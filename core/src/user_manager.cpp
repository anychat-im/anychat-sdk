#include "user_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

int64_t normalizeUnixEpochMs(int64_t raw) {
    // Server payloads usually use Unix seconds. Keep millisecond inputs as-is.
    return (raw > 0 && raw < 1'000'000'000'000LL) ? raw * 1000LL : raw;
}

time_t utcTimeFromTm(std::tm* tm_utc) {
#if defined(_WIN32)
    return _mkgmtime(tm_utc);
#else
    return timegm(tm_utc);
#endif
}

int parseTwoDigits(const std::string& text, size_t pos) {
    if (pos + 2 > text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))
        || !std::isdigit(static_cast<unsigned char>(text[pos + 1]))) {
        return -1;
    }
    return (text[pos] - '0') * 10 + (text[pos + 1] - '0');
}

std::string formatIso8601Utc(int64_t unix_ms) {
    if (unix_ms <= 0) {
        return "";
    }

    const std::time_t unix_seconds = static_cast<std::time_t>(unix_ms / 1000LL);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &unix_seconds);
#else
    gmtime_r(&unix_seconds, &tm_utc);
#endif

    char buffer[32] = { 0 };
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        return "";
    }
    return std::string(buffer);
}

template <typename Callback>
void callbackWithParseError(const Callback& cb, const std::exception& e) {
    cb(false, {}, std::string("parse error: ") + e.what());
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/*static*/ int64_t UserManagerImpl::parseTimestampMs(const nlohmann::json& value) {
    if (value.is_null()) {
        return 0;
    }
    if (value.is_number_integer()) {
        return normalizeUnixEpochMs(value.get<int64_t>());
    }
    if (value.is_number_unsigned()) {
        return normalizeUnixEpochMs(static_cast<int64_t>(value.get<uint64_t>()));
    }
    if (!value.is_string()) {
        return 0;
    }

    const std::string text = value.get<std::string>();
    if (text.empty()) {
        return 0;
    }

    const bool all_digits = std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
    if (all_digits) {
        try {
            return normalizeUnixEpochMs(std::stoll(text));
        } catch (...) {
            return 0;
        }
    }

    // Parse RFC3339-like timestamps, e.g. 2026-04-10T09:41:37Z.
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6
        && std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }

    std::tm tm_utc{};
    tm_utc.tm_year = year - 1900;
    tm_utc.tm_mon = month - 1;
    tm_utc.tm_mday = day;
    tm_utc.tm_hour = hour;
    tm_utc.tm_min = minute;
    tm_utc.tm_sec = second;

    time_t epoch_seconds = utcTimeFromTm(&tm_utc);
    if (epoch_seconds == static_cast<time_t>(-1)) {
        return 0;
    }

    int ms = 0;
    const size_t dot_pos = text.find('.', 19);
    if (dot_pos != std::string::npos) {
        int scale = 100;
        for (size_t i = dot_pos + 1; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) && scale > 0;
             ++i) {
            ms += (text[i] - '0') * scale;
            scale /= 10;
        }
    }

    // RFC3339 timezone handling: "Z" or +/-HH[:MM]
    int tz_offset_seconds = 0;
    size_t tz_pos = text.find_first_of("+-Z", 19);
    if (tz_pos != std::string::npos && text[tz_pos] != 'Z') {
        const int hh = parseTwoDigits(text, tz_pos + 1);
        if (hh >= 0) {
            int mm = 0;
            if (tz_pos + 3 < text.size() && text[tz_pos + 3] == ':') {
                const int parsed = parseTwoDigits(text, tz_pos + 4);
                if (parsed >= 0) {
                    mm = parsed;
                }
            } else if (tz_pos + 3 < text.size() && std::isdigit(static_cast<unsigned char>(text[tz_pos + 3]))) {
                const int parsed = parseTwoDigits(text, tz_pos + 3);
                if (parsed >= 0) {
                    mm = parsed;
                }
            }
            tz_offset_seconds = hh * 3600 + mm * 60;
            if (text[tz_pos] == '-') {
                tz_offset_seconds = -tz_offset_seconds;
            }
        }
    }

    const int64_t adjusted_ms = static_cast<int64_t>(epoch_seconds - tz_offset_seconds) * 1000LL + ms;
    return adjusted_ms;
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

/*static*/ UserProfile UserManagerImpl::parseProfile(const nlohmann::json& j) {
    UserProfile p;
    p.user_id = j.value("userId", "");
    p.nickname = j.value("nickname", "");
    p.avatar_url = j.value("avatar", j.value("avatarUrl", ""));
    p.phone = j.value("phone", "");
    p.email = j.value("email", "");
    p.signature = j.value("signature", "");
    p.region = j.value("region", "");
    p.gender = j.value("gender", 0);
    p.qrcode_url = j.value("qrcodeUrl", "");
    if (j.contains("birthday")) {
        p.birthday_ms = parseTimestampMs(j["birthday"]);
    }
    if (j.contains("createdAt")) {
        p.created_at_ms = parseTimestampMs(j["createdAt"]);
    }
    return p;
}

/*static*/ UserSettings UserManagerImpl::parseSettings(const nlohmann::json& j) {
    UserSettings s;
    s.user_id = j.value("userId", "");
    s.notification_enabled = j.value("notificationEnabled", true);
    s.sound_enabled = j.value("soundEnabled", true);
    s.vibration_enabled = j.value("vibrationEnabled", true);
    s.message_preview_enabled = j.value("messagePreviewEnabled", true);
    s.friend_verify_required = j.value("friendVerifyRequired", false);
    s.search_by_phone = j.value("searchByPhone", true);
    s.search_by_id = j.value("searchById", true);
    s.language = j.value("language", "");
    return s;
}

/*static*/ UserInfo UserManagerImpl::parseUserInfo(const nlohmann::json& j) {
    UserInfo u;
    u.user_id = j.value("userId", "");
    u.username = j.value("nickname", j.value("username", ""));
    u.avatar_url = j.value("avatar", j.value("avatarUrl", ""));
    u.signature = j.value("signature", "");
    u.gender = j.value("gender", 0);
    u.region = j.value("region", "");
    u.is_friend = j.value("isFriend", false);
    u.is_blocked = j.value("isBlocked", false);
    return u;
}

/*static*/ UserQRCode UserManagerImpl::parseQRCode(const nlohmann::json& j) {
    UserQRCode qrcode;
    qrcode.qrcode_url = j.value("qrcodeUrl", "");
    if (j.contains("expiresAt")) {
        qrcode.expires_at_ms = parseTimestampMs(j["expiresAt"]);
    }
    return qrcode;
}

/*static*/ BindPhoneResult UserManagerImpl::parseBindPhoneResult(const nlohmann::json& j) {
    BindPhoneResult result;
    result.phone_number = j.value("phoneNumber", "");
    result.is_primary = j.value("isPrimary", false);
    return result;
}

/*static*/ ChangePhoneResult UserManagerImpl::parseChangePhoneResult(const nlohmann::json& j) {
    ChangePhoneResult result;
    result.old_phone_number = j.value("oldPhoneNumber", "");
    result.new_phone_number = j.value("newPhoneNumber", "");
    return result;
}

/*static*/ BindEmailResult UserManagerImpl::parseBindEmailResult(const nlohmann::json& j) {
    BindEmailResult result;
    result.email = j.value("email", "");
    result.is_primary = j.value("isPrimary", false);
    return result;
}

/*static*/ ChangeEmailResult UserManagerImpl::parseChangeEmailResult(const nlohmann::json& j) {
    ChangeEmailResult result;
    result.old_email = j.value("oldEmail", "");
    result.new_email = j.value("newEmail", "");
    return result;
}

/*static*/ UserStatusEvent UserManagerImpl::parseUserStatusEvent(const nlohmann::json& j) {
    UserStatusEvent event;
    event.user_id = j.value("userId", "");
    event.status = j.value("status", "");
    event.platform = j.value("platform", "");
    if (j.contains("lastActiveAt")) {
        event.last_active_at_ms = parseTimestampMs(j["lastActiveAt"]);
    }
    return event;
}

// ---------------------------------------------------------------------------
// getProfile / updateProfile
// ---------------------------------------------------------------------------

void UserManagerImpl::getProfile(ProfileCallback callback) {
    http_->get("/users/me", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseProfile(root["data"]), "");
        } catch (const std::exception& e) {
            callbackWithParseError(cb, e);
        }
    });
}

void UserManagerImpl::updateProfile(const UserProfile& profile, ProfileCallback callback) {
    nlohmann::json body;
    if (!profile.nickname.empty()) {
        body["nickname"] = profile.nickname;
    }
    if (!profile.avatar_url.empty()) {
        body["avatar"] = profile.avatar_url;
    }
    if (!profile.signature.empty()) {
        body["signature"] = profile.signature;
    }
    if (!profile.region.empty()) {
        body["region"] = profile.region;
    }
    if (profile.gender != 0) {
        body["gender"] = profile.gender;
    }
    if (profile.birthday_ms > 0) {
        const std::string birthday = formatIso8601Utc(profile.birthday_ms);
        if (!birthday.empty()) {
            body["birthday"] = birthday;
        }
    }

    http_->put("/users/me", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseProfile(root["data"]), "");
        } catch (const std::exception& e) {
            callbackWithParseError(cb, e);
        }
    });
}

// ---------------------------------------------------------------------------
// getSettings / updateSettings
// ---------------------------------------------------------------------------

void UserManagerImpl::getSettings(SettingsCallback callback) {
    http_->get("/users/me/settings", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseSettings(root["data"]), "");
        } catch (const std::exception& e) {
            callbackWithParseError(cb, e);
        }
    });
}

void UserManagerImpl::updateSettings(const UserSettings& settings, SettingsCallback callback) {
    nlohmann::json body;
    body["notificationEnabled"] = settings.notification_enabled;
    body["soundEnabled"] = settings.sound_enabled;
    body["vibrationEnabled"] = settings.vibration_enabled;
    body["messagePreviewEnabled"] = settings.message_preview_enabled;
    body["friendVerifyRequired"] = settings.friend_verify_required;
    body["searchByPhone"] = settings.search_by_phone;
    body["searchById"] = settings.search_by_id;
    if (!settings.language.empty()) {
        body["language"] = settings.language;
    }

    http_->put("/users/me/settings", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseSettings(root["data"]), "");
        } catch (const std::exception& e) {
            callbackWithParseError(cb, e);
        }
    });
}

// ---------------------------------------------------------------------------
// updatePushToken
// ---------------------------------------------------------------------------

void UserManagerImpl::updatePushToken(const std::string& push_token, const std::string& platform, ResultCallback callback) {
    updatePushToken(push_token, platform, device_id_, std::move(callback));
}

void UserManagerImpl::updatePushToken(
    const std::string& push_token,
    const std::string& platform,
    const std::string& device_id,
    ResultCallback callback
) {
    nlohmann::json body;
    body["pushToken"] = push_token;
    body["platform"] = platform;
    if (!device_id.empty()) {
        body["deviceId"] = device_id;
    }

    http_->post("/users/me/push-token", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            const bool ok = (root.value("code", -1) == 0);
            cb(ok, ok ? "" : root.value("message", "server error"));
        } catch (const std::exception& e) {
            cb(false, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// searchUsers / getUserInfo
// ---------------------------------------------------------------------------

void UserManagerImpl::searchUsers(const std::string& keyword, int page, int page_size, UserListCallback callback) {
    const std::string path = "/users/search?keyword=" + urlEncode(keyword) + "&page=" + std::to_string(page)
        + "&pageSize=" + std::to_string(page_size);

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb({}, 0, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb({}, 0, root.value("message", "server error"));
                return;
            }
            const auto& data = root["data"];
            const int64_t total = data.value("total", int64_t{ 0 });
            std::vector<UserInfo> users;
            if (data.contains("users") && data["users"].is_array()) {
                for (const auto& item : data["users"]) {
                    users.push_back(parseUserInfo(item));
                }
            }
            cb(users, total, "");
        } catch (const std::exception& e) {
            cb({}, 0, std::string("parse error: ") + e.what());
        }
    });
}

void UserManagerImpl::getUserInfo(const std::string& user_id, UserInfoCallback callback) {
    http_->get("/users/" + user_id, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseUserInfo(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// phone / email operations
// ---------------------------------------------------------------------------

void UserManagerImpl::bindPhone(
    const std::string& phone_number,
    const std::string& verify_code,
    BindPhoneCallback callback
) {
    nlohmann::json body;
    body["phoneNumber"] = phone_number;
    body["verifyCode"] = verify_code;

    http_->post("/users/me/phone/bind", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseBindPhoneResult(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void UserManagerImpl::changePhone(
    const std::string& old_phone_number,
    const std::string& new_phone_number,
    const std::string& new_verify_code,
    const std::string& old_verify_code,
    ChangePhoneCallback callback
) {
    nlohmann::json body;
    body["oldPhoneNumber"] = old_phone_number;
    body["newPhoneNumber"] = new_phone_number;
    body["newVerifyCode"] = new_verify_code;
    if (!old_verify_code.empty()) {
        body["oldVerifyCode"] = old_verify_code;
    }
    if (!device_id_.empty()) {
        body["deviceId"] = device_id_;
    }

    http_->post("/users/me/phone/change", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseChangePhoneResult(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void UserManagerImpl::bindEmail(const std::string& email, const std::string& verify_code, BindEmailCallback callback) {
    nlohmann::json body;
    body["email"] = email;
    body["verifyCode"] = verify_code;

    http_->post("/users/me/email/bind", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseBindEmailResult(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void UserManagerImpl::changeEmail(
    const std::string& old_email,
    const std::string& new_email,
    const std::string& new_verify_code,
    const std::string& old_verify_code,
    ChangeEmailCallback callback
) {
    nlohmann::json body;
    body["oldEmail"] = old_email;
    body["newEmail"] = new_email;
    body["newVerifyCode"] = new_verify_code;
    if (!old_verify_code.empty()) {
        body["oldVerifyCode"] = old_verify_code;
    }
    if (!device_id_.empty()) {
        body["deviceId"] = device_id_;
    }

    http_->post("/users/me/email/change", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseChangeEmailResult(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// qrcode
// ---------------------------------------------------------------------------

void UserManagerImpl::refreshQRCode(QRCodeCallback callback) {
    http_->post("/users/me/qrcode/refresh", "{}", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseQRCode(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void UserManagerImpl::getUserByQRCode(const std::string& qrcode, UserInfoCallback callback) {
    http_->get("/users/qrcode?qrcode=" + urlEncode(qrcode), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseUserInfo(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// notification callbacks
// ---------------------------------------------------------------------------

void UserManagerImpl::setListener(std::shared_ptr<UserListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

void UserManagerImpl::handleUserNotification(const NotificationEvent& event) {
    const std::string& type = event.notification_type;

    if (type == "user.profile_updated" || type == "notification.user.profile_updated") {
        std::shared_ptr<UserListener> listener;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            listener = listener_;
        }
        if (!listener) {
            return;
        }
        try {
            listener->onProfileUpdated(parseUserInfo(event.data));
        } catch (...) {
        }
        return;
    }

    if (type == "user.friend_profile_changed" || type == "notification.user.friend_profile_changed") {
        std::shared_ptr<UserListener> listener;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            listener = listener_;
        }
        if (!listener) {
            return;
        }
        try {
            listener->onFriendProfileChanged(parseUserInfo(event.data));
        } catch (...) {
        }
        return;
    }

    if (type == "user.status_changed" || type == "notification.user.status_changed") {
        std::shared_ptr<UserListener> listener;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            listener = listener_;
        }
        if (!listener) {
            return;
        }
        try {
            listener->onUserStatusChanged(parseUserStatusEvent(event.data));
        } catch (...) {
        }
    }
}

} // namespace anychat
