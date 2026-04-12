#include "auth_manager.h"

#include "json_common.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat::auth_manager_detail {

using json_common::ApiEnvelope;
using json_common::ApiStatus;
using json_common::nowMs;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusResponse;
using json_common::parseBoolValue;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::pickList;
using json_common::writeJson;

constexpr int64_t kTokenRefreshLeewayMs = 5 * 60 * 1000;

using BoolValue = std::variant<bool, int64_t, double, std::string>;
using OptionalBoolValue = std::optional<BoolValue>;

struct RegisterUserRequest {
    std::string password{};
    std::string verify_code{};
    std::string device_id{};
    std::string device_type{};
    std::string client_version{};
    std::optional<std::string> email{};
    std::optional<std::string> phone_number{};
    std::optional<std::string> nickname{};
};

struct SendVerificationCodeRequest {
    std::string target{};
    std::string target_type{};
    std::string purpose{};
    std::string device_id{};
};

struct LoginRequest {
    std::string account{};
    std::string password{};
    std::string device_id{};
    std::string device_type{};
    std::string client_version{};
};

struct LogoutRequest {
    std::string device_id{};
};

struct LogoutDeviceRequest {
    std::string device_id{};
};

struct RefreshTokenRequest {
    std::string refresh_token{};
};

struct ChangePasswordRequest {
    std::string device_id{};
    std::string old_password{};
    std::string new_password{};
};

struct ResetPasswordRequest {
    std::string account{};
    std::string verify_code{};
    std::string new_password{};
};

struct AuthTokenPayload {
    std::string access_token{};
    std::string refresh_token{};
    int64_t expires_in = 0;
};

struct VerificationCodePayload {
    std::string code_id{};
    int64_t expires_in = 0;
};

struct AuthDevicePayload {
    std::string device_id{};
    std::string device_type{};
    std::string client_version{};
    std::string last_login_ip{};
    json_common::OptionalTimestampValue last_login_at{};
    OptionalBoolValue is_current{};
};

struct ForceLogoutPayload {
    std::string device_id{};
};

struct DeviceListDataPayload {
    std::optional<std::vector<AuthDevicePayload>> devices{};
};

const std::vector<AuthDevicePayload>* toDevicePayloadList(const DeviceListDataPayload& data) {
    return pickList(data.devices);
}

} // namespace anychat::auth_manager_detail

namespace anychat {
using namespace auth_manager_detail;


AuthManagerImpl::AuthManagerImpl(
    std::shared_ptr<network::HttpClient> http,
    std::string device_id,
    db::Database* db,
    NotificationManager* notif_mgr
)
    : http_(std::move(http))
    , device_id_(std::move(device_id))
    , db_(db) {
    if (db_) {
        const std::string at = db_->getMeta("auth.access_token", "");
        const std::string rt = db_->getMeta("auth.refresh_token", "");
        const std::string exp = db_->getMeta("auth.expires_at_ms", "0");
        if (!at.empty()) {
            std::lock_guard<std::mutex> lock(token_mutex_);
            token_.access_token = at;
            token_.refresh_token = rt;
            token_.expires_at_ms = std::strtoll(exp.c_str(), nullptr, 10);
            http_->setAuthToken(at);
        }
    }

    if (notif_mgr) {
        notif_mgr->addNotificationHandler([this](const NotificationEvent& event) {
            handleAuthNotification(event);
        });
    }
}

void AuthManagerImpl::registerUser(
    const std::string& phone_or_email,
    const std::string& password,
    const std::string& verify_code,
    const std::string& device_type,
    const std::string& nickname,
    const std::string& client_version,
    AuthCallback callback
) {
    RegisterUserRequest body{
        .password = password,
        .verify_code = verify_code,
        .device_id = device_id_,
        .device_type = device_type,
        .client_version = client_version,
    };

    if (phone_or_email.find('@') != std::string::npos) {
        body.email = phone_or_email;
    } else {
        body.phone_number = phone_or_email;
    }
    if (!nickname.empty()) {
        body.nickname = nickname;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/auth/register", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

void AuthManagerImpl::sendVerificationCode(
    const std::string& target,
    const std::string& target_type,
    const std::string& purpose,
    SendCodeCallback callback
) {
    SendVerificationCodeRequest body{
        .target = target,
        .target_type = target_type,
        .purpose = purpose,
        .device_id = device_id_,
    };

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/auth/send-code", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<VerificationCodePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "send code failed")) {
            cb(false, {}, err);
            return;
        }

        VerificationCodeResult result;
        result.code_id = root.data.code_id;
        result.expires_in = root.data.expires_in;
        cb(true, result, "");
    });
}

void AuthManagerImpl::login(
    const std::string& account,
    const std::string& password,
    const std::string& device_type,
    const std::string& client_version,
    AuthCallback callback
) {
    LoginRequest body{
        .account = account,
        .password = password,
        .device_id = device_id_,
        .device_type = device_type,
        .client_version = client_version,
    };

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/auth/login", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

void AuthManagerImpl::logout(ResultCallback callback) {
    const LogoutRequest body{.device_id = device_id_};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post("/auth/logout", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleResultResponse(
            std::move(resp),
            "logout failed",
            [this, cb = std::move(cb)](bool success, const std::string& error) {
                if (success) {
                    clearToken();
                    http_->clearAuthToken();
                }
                cb(success, error);
            }
        );
    });
}

void AuthManagerImpl::getDeviceList(DeviceListCallback callback) {
    auto cb = std::make_shared<DeviceListCallback>(std::move(callback));

    auto parse_response = [cb](network::HttpResponse resp) {
        ApiEnvelope<DeviceListDataPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get device list failed")) {
            (*cb)(false, {}, err);
            return;
        }

        std::vector<AuthDevice> devices;
        const auto* payloads = toDevicePayloadList(root.data);
        if (payloads != nullptr) {
            devices.reserve(payloads->size());
            for (const auto& payload : *payloads) {
                AuthDevice device;
                device.device_id = payload.device_id;
                device.device_type = payload.device_type;
                device.client_version = payload.client_version;
                device.last_login_ip = payload.last_login_ip;
                device.last_login_at_ms = parseTimestampMs(payload.last_login_at);
                device.is_current = parseBoolValue(payload.is_current);
                devices.push_back(std::move(device));
            }
        }

        (*cb)(true, devices, "");
    };

    http_->post("/auth/device/list", "{}", [this, parse_response](network::HttpResponse resp) {
        if (resp.error.empty() && (resp.status_code == 404 || resp.status_code == 405)) {
            http_->get("/auth/devices", [parse_response](network::HttpResponse fallback_resp) {
                parse_response(std::move(fallback_resp));
            });
            return;
        }
        parse_response(std::move(resp));
    });
}

void AuthManagerImpl::logoutDevice(const std::string& device_id, ResultCallback callback) {
    const LogoutDeviceRequest body{.device_id = device_id};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post(
        "/auth/device/logout",
        body_json,
        [this, body_json, cb = std::move(callback)](network::HttpResponse resp) {
            if (resp.error.empty() && (resp.status_code == 404 || resp.status_code == 405)) {
                http_->post(
                    "/auth/devices/logout",
                    body_json,
                    [this, cb = std::move(cb)](network::HttpResponse fallback_resp) {
                        handleResultResponse(std::move(fallback_resp), "logout device failed", cb);
                    }
                );
                return;
            }
            handleResultResponse(std::move(resp), "logout device failed", cb);
        }
    );
}

void AuthManagerImpl::refreshToken(const std::string& refresh_token, AuthCallback callback) {
    const RefreshTokenRequest body{.refresh_token = refresh_token};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, {}, err);
        return;
    }

    http_->post("/auth/refresh", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

void AuthManagerImpl::changePassword(
    const std::string& old_password,
    const std::string& new_password,
    ResultCallback callback
) {
    const ChangePasswordRequest body{
        .device_id = device_id_,
        .old_password = old_password,
        .new_password = new_password,
    };

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post("/auth/password/change", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleResultResponse(std::move(resp), "change password failed", cb);
    });
}

void AuthManagerImpl::resetPassword(
    const std::string& account,
    const std::string& verify_code,
    const std::string& new_password,
    ResultCallback callback
) {
    const ResetPasswordRequest body{
        .account = account,
        .verify_code = verify_code,
        .new_password = new_password,
    };

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post("/auth/password/reset", body_json, [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleResultResponse(std::move(resp), "reset password failed", cb);
    });
}

bool AuthManagerImpl::isLoggedIn() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return !token_.access_token.empty() && token_.expires_at_ms > nowMs();
}

AuthToken AuthManagerImpl::currentToken() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return token_;
}

void AuthManagerImpl::ensureValidToken(ResultCallback cb) {
    AuthToken token_snapshot;
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        token_snapshot = token_;
    }

    const int64_t now_ms = nowMs();
    if (!token_snapshot.access_token.empty() && token_snapshot.expires_at_ms > now_ms + kTokenRefreshLeewayMs) {
        cb(true, "");
        return;
    }

    if (token_snapshot.refresh_token.empty()) {
        std::shared_ptr<AuthListener> listener;
        {
            std::lock_guard<std::mutex> lock(listener_mutex_);
            listener = listener_;
        }
        if (listener) {
            listener->onAuthExpired();
        }
        cb(false, "no refresh token");
        return;
    }

    refreshToken(token_snapshot.refresh_token, [this, cb = std::move(cb)](bool ok, const AuthToken&, const std::string& err) {
        if (!ok) {
            std::shared_ptr<AuthListener> listener;
            {
                std::lock_guard<std::mutex> lock(listener_mutex_);
                listener = listener_;
            }
            if (listener) {
                listener->onAuthExpired();
            }
        }
        cb(ok, err);
    });
}

void AuthManagerImpl::setListener(std::shared_ptr<AuthListener> listener) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    listener_ = std::move(listener);
}

void AuthManagerImpl::handleResultResponse(
    network::HttpResponse resp,
    const std::string& fallback_message,
    const ResultCallback& cb
) {
    if (!cb) {
        return;
    }

    ApiStatus status{};
    std::string err;
    if (!parseApiStatusResponse(resp, status, err)) {
        cb(false, err);
        return;
    }

    if (status.code == 0) {
        cb(true, "");
        return;
    }
    cb(false, status.message.empty() ? fallback_message : status.message);
}

void AuthManagerImpl::handleAuthResponse(network::HttpResponse resp, const AuthCallback& callback) {
    if (!callback) {
        return;
    }

    ApiEnvelope<AuthTokenPayload> root{};
    std::string err;
    if (!parseApiEnvelopeResponse(resp, root, err, "auth failed")) {
        callback(false, {}, err);
        return;
    }

    AuthToken token;
    token.access_token = root.data.access_token;
    token.refresh_token = root.data.refresh_token;
    token.expires_at_ms = root.data.expires_in > 0 ? (nowMs() + root.data.expires_in * 1000) : 0;

    if (token.access_token.empty()) {
        callback(false, {}, "auth failed: access_token is empty");
        return;
    }

    storeToken(token);
    http_->setAuthToken(token.access_token);

    callback(true, token, "");
}

void AuthManagerImpl::handleAuthNotification(const NotificationEvent& event) {
    if (event.notification_type != "auth.force_logout") {
        return;
    }

    std::string target_device_id;
    ForceLogoutPayload payload{};
    std::string err;
    if (parseJsonObject(event.data, payload, err)) {
        target_device_id = payload.device_id;
    }

    if (!target_device_id.empty() && target_device_id != device_id_) {
        return;
    }

    clearToken();
    http_->clearAuthToken();

    std::shared_ptr<AuthListener> listener;
    {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onAuthExpired();
    }
}

void AuthManagerImpl::storeToken(const AuthToken& token) {
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        token_ = token;
    }
    if (db_) {
        db_->setMeta("auth.access_token", token.access_token);
        db_->setMeta("auth.refresh_token", token.refresh_token);
        db_->setMeta("auth.expires_at_ms", std::to_string(token.expires_at_ms));
    }
}

void AuthManagerImpl::clearToken() {
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        token_ = {};
    }
    if (db_) {
        db_->setMeta("auth.access_token", "");
        db_->setMeta("auth.refresh_token", "");
        db_->setMeta("auth.expires_at_ms", "0");
    }
}

std::unique_ptr<AuthManager>
createAuthManager(
    std::shared_ptr<network::HttpClient> http,
    const std::string& device_id,
    db::Database* db,
    NotificationManager* notif_mgr
) {
    return std::make_unique<AuthManagerImpl>(std::move(http), device_id, db, notif_mgr);
}

} // namespace anychat
