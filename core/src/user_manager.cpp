#include "user_manager.h"

#include "json_common.h"
#include "notification_manager.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace anychat {
namespace {
using json_common::ApiEnvelope;
using json_common::formatIso8601Utc;
using json_common::normalizeUnixEpochMs;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusSuccessResponse;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::writeJson;

struct UserProfilePayload {
    std::string user_id{};
    std::string nickname{};
    std::string avatar{};
    std::string phone{};
    std::string email{};
    std::string signature{};
    std::string region{};
    int32_t gender = 0;
    std::string qrcode_url{};
    json_common::OptionalTimestampValue birthday{};
    json_common::OptionalTimestampValue created_at{};
};

struct UserSettingsPayload {
    std::string user_id{};
    bool notification_enabled = true;
    bool sound_enabled = true;
    bool vibration_enabled = true;
    bool message_preview_enabled = true;
    bool friend_verify_required = false;
    bool search_by_phone = true;
    bool search_by_id = true;
    std::string language{};
};

struct UserInfoPayload {
    std::string user_id{};
    std::string nickname{};
    std::string avatar{};
    std::string signature{};
    int32_t gender = 0;
    std::string region{};
    bool is_friend = false;
    bool is_blocked = false;
};

struct UserQRCodePayload {
    std::string qrcode_url{};
    json_common::OptionalTimestampValue expires_at{};
};

struct BindPhoneResultPayload {
    std::string phone_number{};
    bool is_primary = false;
};

struct ChangePhoneResultPayload {
    std::string old_phone_number{};
    std::string new_phone_number{};
};

struct BindEmailResultPayload {
    std::string email{};
    bool is_primary = false;
};

struct ChangeEmailResultPayload {
    std::string old_email{};
    std::string new_email{};
};

struct SearchUsersDataPayload {
    int64_t total = 0;
    std::vector<UserInfoPayload> users{};
};

struct UpdateProfileRequestPayload {
    std::optional<std::string> nickname{};
    std::optional<std::string> avatar{};
    std::optional<std::string> signature{};
    std::optional<std::string> region{};
    std::optional<int32_t> gender{};
    std::optional<std::string> birthday{};
};

struct UpdateSettingsRequestPayload {
    bool notification_enabled = true;
    bool sound_enabled = true;
    bool vibration_enabled = true;
    bool message_preview_enabled = true;
    bool friend_verify_required = false;
    bool search_by_phone = true;
    bool search_by_id = true;
    std::optional<std::string> language{};
};

struct UpdatePushTokenRequestPayload {
    std::string push_token{};
    std::string platform{};
    std::optional<std::string> device_id{};
};

struct BindPhoneRequestPayload {
    std::string phone_number{};
    std::string verify_code{};
};

struct ChangePhoneRequestPayload {
    std::string old_phone_number{};
    std::string new_phone_number{};
    std::string new_verify_code{};
    std::optional<std::string> old_verify_code{};
    std::optional<std::string> device_id{};
};

struct BindEmailRequestPayload {
    std::string email{};
    std::string verify_code{};
};

struct ChangeEmailRequestPayload {
    std::string old_email{};
    std::string new_email{};
    std::string new_verify_code{};
    std::optional<std::string> old_verify_code{};
    std::optional<std::string> device_id{};
};

struct NotificationUserInfoPayload {
    std::string user_id{};
    std::string nickname{};
    std::string avatar{};
    std::string signature{};
    int32_t gender = 0;
    std::string region{};
    bool is_friend = false;
    bool is_blocked = false;
};

struct NotificationUserStatusPayload {
    std::string user_id{};
    std::string status{};
    json_common::OptionalTimestampValue last_active_at{};
    std::string platform{};
};

UserInfo parseNotificationUserInfo(const std::string& data) {
    UserInfo info;
    NotificationUserInfoPayload payload{};
    std::string err;
    if (!parseJsonObject(data, payload, err)) {
        return info;
    }

    info.user_id = payload.user_id;
    info.username = payload.nickname;
    info.avatar_url = payload.avatar;
    info.signature = payload.signature;
    info.gender = payload.gender;
    info.region = payload.region;
    info.is_friend = payload.is_friend;
    info.is_blocked = payload.is_blocked;
    return info;
}

UserStatusEvent parseNotificationStatusEvent(const std::string& data) {
    UserStatusEvent event;
    NotificationUserStatusPayload payload{};
    std::string err;
    if (!parseJsonObject(data, payload, err)) {
        return event;
    }

    event.user_id = payload.user_id;
    event.status = payload.status;
    event.last_active_at_ms = parseTimestampMs(payload.last_active_at);
    event.platform = payload.platform;
    return event;
}

UserProfile toUserProfile(const UserProfilePayload& payload) {
    UserProfile p;
    p.user_id = payload.user_id;
    p.nickname = payload.nickname;
    p.avatar_url = payload.avatar;
    p.phone = payload.phone;
    p.email = payload.email;
    p.signature = payload.signature;
    p.region = payload.region;
    p.gender = payload.gender;
    p.qrcode_url = payload.qrcode_url;
    p.birthday_ms = parseTimestampMs(payload.birthday);
    p.created_at_ms = parseTimestampMs(payload.created_at);
    return p;
}

UserSettings toUserSettings(const UserSettingsPayload& payload) {
    UserSettings s;
    s.user_id = payload.user_id;
    s.notification_enabled = payload.notification_enabled;
    s.sound_enabled = payload.sound_enabled;
    s.vibration_enabled = payload.vibration_enabled;
    s.message_preview_enabled = payload.message_preview_enabled;
    s.friend_verify_required = payload.friend_verify_required;
    s.search_by_phone = payload.search_by_phone;
    s.search_by_id = payload.search_by_id;
    s.language = payload.language;
    return s;
}

UserInfo toUserInfo(const UserInfoPayload& payload) {
    UserInfo u;
    u.user_id = payload.user_id;
    u.username = payload.nickname;
    u.avatar_url = payload.avatar;
    u.signature = payload.signature;
    u.gender = payload.gender;
    u.region = payload.region;
    u.is_friend = payload.is_friend;
    u.is_blocked = payload.is_blocked;
    return u;
}

UserQRCode toUserQRCode(const UserQRCodePayload& payload) {
    UserQRCode code;
    code.qrcode_url = payload.qrcode_url;
    code.expires_at_ms = parseTimestampMs(payload.expires_at);
    return code;
}

BindPhoneResult toBindPhoneResult(const BindPhoneResultPayload& payload) {
    BindPhoneResult result;
    result.phone_number = payload.phone_number;
    result.is_primary = payload.is_primary;
    return result;
}

ChangePhoneResult toChangePhoneResult(const ChangePhoneResultPayload& payload) {
    ChangePhoneResult result;
    result.old_phone_number = payload.old_phone_number;
    result.new_phone_number = payload.new_phone_number;
    return result;
}

BindEmailResult toBindEmailResult(const BindEmailResultPayload& payload) {
    BindEmailResult result;
    result.email = payload.email;
    result.is_primary = payload.is_primary;
    return result;
}

ChangeEmailResult toChangeEmailResult(const ChangeEmailResultPayload& payload) {
    ChangeEmailResult result;
    result.old_email = payload.old_email;
    result.new_email = payload.new_email;
    return result;
}

std::shared_ptr<UserListener> snapshotListener(std::mutex& mutex, const std::shared_ptr<UserListener>& listener) {
    std::lock_guard<std::mutex> lock(mutex);
    return listener;
}

} // namespace

UserManagerImpl::UserManagerImpl(
    std::shared_ptr<network::HttpClient> http,
    NotificationManager* notif_mgr,
    std::string device_id
)
    : http_(std::move(http))
    , notif_mgr_(notif_mgr)
    , device_id_(std::move(device_id)) {
    if (notif_mgr_) {
        notif_mgr_->addNotificationHandler([this](const NotificationEvent& event) {
            handleUserNotification(event);
        });
    }
}

/*static*/ std::string UserManagerImpl::urlEncode(const std::string& input) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : input) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

void UserManagerImpl::getProfile(ProfileCallback callback) {
    http_->get("/users/me", [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserProfilePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserProfile(root.data), "");
    });
}

void UserManagerImpl::updateProfile(const UserProfile& profile, ProfileCallback callback) {
    UpdateProfileRequestPayload body{};
    if (!profile.nickname.empty()) {
        body.nickname = profile.nickname;
    }
    if (!profile.avatar_url.empty()) {
        body.avatar = profile.avatar_url;
    }
    if (!profile.signature.empty()) {
        body.signature = profile.signature;
    }
    if (!profile.region.empty()) {
        body.region = profile.region;
    }
    if (profile.gender != 0) {
        body.gender = profile.gender;
    }
    if (profile.birthday_ms > 0) {
        const std::string birthday = formatIso8601Utc(profile.birthday_ms);
        if (!birthday.empty()) {
            body.birthday = birthday;
        }
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->put("/users/me", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserProfilePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserProfile(root.data), "");
    });
}

void UserManagerImpl::getSettings(SettingsCallback callback) {
    http_->get("/users/me/settings", [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserSettingsPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserSettings(root.data), "");
    });
}

void UserManagerImpl::updateSettings(const UserSettings& settings, SettingsCallback callback) {
    UpdateSettingsRequestPayload body{};
    body.notification_enabled = settings.notification_enabled;
    body.sound_enabled = settings.sound_enabled;
    body.vibration_enabled = settings.vibration_enabled;
    body.message_preview_enabled = settings.message_preview_enabled;
    body.friend_verify_required = settings.friend_verify_required;
    body.search_by_phone = settings.search_by_phone;
    body.search_by_id = settings.search_by_id;
    if (!settings.language.empty()) {
        body.language = settings.language;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->put("/users/me/settings", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserSettingsPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserSettings(root.data), "");
    });
}

void UserManagerImpl::updatePushToken(const std::string& push_token, const std::string& platform, ResultCallback callback) {
    updatePushToken(push_token, platform, device_id_, std::move(callback));
}

void UserManagerImpl::updatePushToken(
    const std::string& push_token,
    const std::string& platform,
    const std::string& device_id,
    ResultCallback callback
) {
    UpdatePushTokenRequestPayload body{};
    body.push_token = push_token;
    body.platform = platform;
    if (!device_id.empty()) {
        body.device_id = device_id;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post("/users/me/push-token", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        std::string err;
        const bool ok = parseApiStatusSuccessResponse(resp, err);
        cb(ok, ok ? "" : err);
    });
}

void UserManagerImpl::searchUsers(const std::string& keyword, int page, int page_size, UserListCallback callback) {
    const std::string path = "/users/search?keyword=" + urlEncode(keyword) + "&page=" + std::to_string(page)
        + "&page_size=" + std::to_string(page_size);

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<SearchUsersDataPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb({}, 0, err);
            return;
        }

        std::vector<UserInfo> users;
        users.reserve(root.data.users.size());
        for (const auto& item : root.data.users) {
            users.push_back(toUserInfo(item));
        }
        cb(users, root.data.total, "");
    });
}

void UserManagerImpl::getUserInfo(const std::string& user_id, UserInfoCallback callback) {
    http_->get("/users/" + user_id, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserInfoPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserInfo(root.data), "");
    });
}

void UserManagerImpl::bindPhone(
    const std::string& phone_number,
    const std::string& verify_code,
    BindPhoneCallback callback
) {
    BindPhoneRequestPayload body{};
    body.phone_number = phone_number;
    body.verify_code = verify_code;

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/users/me/phone/bind", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<BindPhoneResultPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toBindPhoneResult(root.data), "");
    });
}

void UserManagerImpl::changePhone(
    const std::string& old_phone_number,
    const std::string& new_phone_number,
    const std::string& new_verify_code,
    const std::string& old_verify_code,
    ChangePhoneCallback callback
) {
    ChangePhoneRequestPayload body{};
    body.old_phone_number = old_phone_number;
    body.new_phone_number = new_phone_number;
    body.new_verify_code = new_verify_code;
    if (!old_verify_code.empty()) {
        body.old_verify_code = old_verify_code;
    }
    if (!device_id_.empty()) {
        body.device_id = device_id_;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/users/me/phone/change", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<ChangePhoneResultPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toChangePhoneResult(root.data), "");
    });
}

void UserManagerImpl::bindEmail(const std::string& email, const std::string& verify_code, BindEmailCallback callback) {
    BindEmailRequestPayload body{};
    body.email = email;
    body.verify_code = verify_code;

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/users/me/email/bind", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<BindEmailResultPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toBindEmailResult(root.data), "");
    });
}

void UserManagerImpl::changeEmail(
    const std::string& old_email,
    const std::string& new_email,
    const std::string& new_verify_code,
    const std::string& old_verify_code,
    ChangeEmailCallback callback
) {
    ChangeEmailRequestPayload body{};
    body.old_email = old_email;
    body.new_email = new_email;
    body.new_verify_code = new_verify_code;
    if (!old_verify_code.empty()) {
        body.old_verify_code = old_verify_code;
    }
    if (!device_id_.empty()) {
        body.device_id = device_id_;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/users/me/email/change", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<ChangeEmailResultPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toChangeEmailResult(root.data), "");
    });
}

void UserManagerImpl::refreshQRCode(QRCodeCallback callback) {
    http_->post("/users/me/qrcode/refresh", "{}", [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserQRCodePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserQRCode(root.data), "");
    });
}

void UserManagerImpl::getUserByQRCode(const std::string& qrcode, UserInfoCallback callback) {
    http_->get("/users/qrcode?qrcode=" + urlEncode(qrcode), [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<UserInfoPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toUserInfo(root.data), "");
    });
}

void UserManagerImpl::setListener(std::shared_ptr<UserListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

void UserManagerImpl::handleUserNotification(const NotificationEvent& event) {
    const std::string& type = event.notification_type;

    if (type == "user.profile_updated") {
        auto listener = snapshotListener(handler_mutex_, listener_);
        if (!listener) {
            return;
        }

        try {
            listener->onProfileUpdated(parseNotificationUserInfo(event.data));
        } catch (...) {
        }
        return;
    }

    if (type == "user.friend_profile_changed") {
        auto listener = snapshotListener(handler_mutex_, listener_);
        if (!listener) {
            return;
        }

        try {
            listener->onFriendProfileChanged(parseNotificationUserInfo(event.data));
        } catch (...) {
        }
        return;
    }

    if (type == "user.status_changed") {
        auto listener = snapshotListener(handler_mutex_, listener_);
        if (!listener) {
            return;
        }

        try {
            listener->onUserStatusChanged(parseNotificationStatusEvent(event.data));
        } catch (...) {
        }
    }
}

} // namespace anychat
