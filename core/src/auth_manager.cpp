#include "auth_manager.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

constexpr int64_t kTokenRefreshLeewayMs = 5 * 60 * 1000;
// Some non-token API fields may still return Unix seconds; normalize them to ms.
constexpr int64_t kUnixMsThreshold = 1000000000000LL;

int64_t unixNowMs() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
}

int64_t secondsToMsIfNeeded(int64_t value) {
    if (value > 0 && value < kUnixMsThreshold) {
        return value * 1000;
    }
    return value;
}

int64_t jsonInt64(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        return value.get<int64_t>();
    }
    if (value.is_number_unsigned()) {
        return static_cast<int64_t>(value.get<uint64_t>());
    }
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text.empty()) {
            return 0;
        }
        char* end = nullptr;
        const auto parsed = std::strtoll(text.c_str(), &end, 10);
        if (end != nullptr && *end == '\0') {
            return parsed;
        }
    }
    return 0;
}

std::string jsonString(
    const nlohmann::json& obj,
    std::initializer_list<const char*> keys,
    const std::string& fallback = ""
) {
    if (!obj.is_object()) {
        return fallback;
    }
    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return fallback;
}

bool jsonBool(const nlohmann::json& obj, std::initializer_list<const char*> keys, bool fallback = false) {
    if (!obj.is_object()) {
        return fallback;
    }
    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            continue;
        }
        if (it->is_boolean()) {
            return it->get<bool>();
        }
        if (it->is_number_integer() || it->is_number_unsigned()) {
            return jsonInt64(*it) != 0;
        }
        if (it->is_string()) {
            std::string v = it->get<std::string>();
            for (char& ch : v) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (v == "1" || v == "true" || v == "yes") {
                return true;
            }
            if (v == "0" || v == "false" || v == "no") {
                return false;
            }
        }
    }
    return fallback;
}

int64_t jsonTimestampMs(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return 0;
    }
    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it == obj.end()) {
            continue;
        }
        if (it->is_object()) {
            const auto sec_it = it->find("seconds");
            int64_t seconds = sec_it != it->end() ? jsonInt64(*sec_it) : 0;
            const auto ns_it = it->find("nanos");
            int64_t nanos = ns_it != it->end() ? jsonInt64(*ns_it) : 0;
            if (seconds > 0 || nanos > 0) {
                return seconds * 1000 + nanos / 1000000;
            }
            continue;
        }
        return secondsToMsIfNeeded(jsonInt64(*it));
    }
    return 0;
}

std::string extractResponseMessage(const nlohmann::json& json, const std::string& fallback) {
    if (json.is_object()) {
        if (json.contains("message") && json["message"].is_string()) {
            return json["message"].get<std::string>();
        }
        if (json.contains("error") && json["error"].is_string()) {
            return json["error"].get<std::string>();
        }
    }
    return fallback;
}

const nlohmann::json* locateListArray(const nlohmann::json& data) {
    if (data.is_array()) {
        return &data;
    }
    if (!data.is_object()) {
        return nullptr;
    }
    static constexpr const char* kCandidates[] = { "list", "devices", "items", "rows" };
    for (const char* key : kCandidates) {
        auto it = data.find(key);
        if (it != data.end() && it->is_array()) {
            return &(*it);
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AuthManagerImpl::AuthManagerImpl(
    std::shared_ptr<network::HttpClient> http,
    std::string device_id,
    db::Database* db,
    NotificationManager* notif_mgr
)
    : http_(std::move(http))
    , device_id_(std::move(device_id))
    , db_(db) {
    // If a database is provided, attempt to restore a previously persisted token.
    if (db_) {
        std::string at = db_->getMeta("auth.access_token", "");
        std::string rt = db_->getMeta("auth.refresh_token", "");
        std::string exp = db_->getMeta("auth.expires_at_ms", "0");
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

// ---------------------------------------------------------------------------
// registerUser
// ---------------------------------------------------------------------------

void AuthManagerImpl::registerUser(
    const std::string& phone_or_email,
    const std::string& password,
    const std::string& verify_code,
    const std::string& device_type,
    const std::string& nickname,
    const std::string& client_version,
    AuthCallback callback
) {
    nlohmann::json body;
    body["password"] = password;
    body["verifyCode"] = verify_code;
    body["deviceId"] = device_id_;
    body["deviceType"] = device_type;
    body["clientVersion"] = client_version;

    // Heuristic: if the value contains '@' treat it as email.
    if (phone_or_email.find('@') != std::string::npos)
        body["email"] = phone_or_email;
    else
        body["phoneNumber"] = phone_or_email;

    if (!nickname.empty())
        body["nickname"] = nickname;

    http_->post("/auth/register", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

// ---------------------------------------------------------------------------
// sendVerificationCode
// ---------------------------------------------------------------------------

void AuthManagerImpl::sendVerificationCode(
    const std::string& target,
    const std::string& target_type,
    const std::string& purpose,
    SendCodeCallback callback
) {
    nlohmann::json body;
    body["target"] = target;
    body["targetType"] = target_type;
    body["purpose"] = purpose;
    body["deviceId"] = device_id_;

    http_->post("/auth/send-code", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }

        try {
            const auto json = nlohmann::json::parse(resp.body);
            if (json.value("code", -1) != 0) {
                cb(false, {}, extractResponseMessage(json, "send code failed"));
                return;
            }

            VerificationCodeResult result;
            const auto data = json.value("data", nlohmann::json::object());
            result.code_id = jsonString(data, { "codeId", "code_id" });
            result.expires_in = jsonInt64(data.value("expiresIn", data.value("expires_in", 0)));
            cb(true, result, "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("JSON parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// login
// ---------------------------------------------------------------------------

void AuthManagerImpl::login(
    const std::string& account,
    const std::string& password,
    const std::string& device_type,
    const std::string& client_version,
    AuthCallback callback
) {
    nlohmann::json body;
    body["account"] = account;
    body["password"] = password;
    body["deviceId"] = device_id_;
    body["deviceType"] = device_type;
    body["clientVersion"] = client_version;

    http_->post("/auth/login", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

// ---------------------------------------------------------------------------
// logout
// ---------------------------------------------------------------------------

void AuthManagerImpl::logout(ResultCallback callback) {
    nlohmann::json body;
    body["deviceId"] = device_id_;

    http_->post("/auth/logout", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
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

// ---------------------------------------------------------------------------
// getDeviceList
// ---------------------------------------------------------------------------

void AuthManagerImpl::getDeviceList(DeviceListCallback callback) {
    auto cb = std::make_shared<DeviceListCallback>(std::move(callback));

    auto parse_response = [cb](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            (*cb)(false, {}, resp.error);
            return;
        }
        try {
            const auto json = nlohmann::json::parse(resp.body);
            if (json.value("code", -1) != 0) {
                (*cb)(false, {}, extractResponseMessage(json, "get device list failed"));
                return;
            }

            std::vector<AuthDevice> devices;
            const auto data = json.value("data", nlohmann::json::object());
            const auto* arr = locateListArray(data);
            if (arr) {
                devices.reserve(arr->size());
                for (const auto& item : *arr) {
                    if (!item.is_object()) {
                        continue;
                    }
                    AuthDevice device;
                    device.device_id = jsonString(item, { "deviceId", "device_id" });
                    device.device_type = jsonString(item, { "deviceType", "device_type" });
                    device.client_version = jsonString(item, { "clientVersion", "client_version" });
                    device.last_login_ip = jsonString(item, { "lastLoginIp", "last_login_ip" });
                    device.last_login_at_ms = jsonTimestampMs(
                        item,
                        { "lastLoginAtMs", "last_login_at_ms", "lastLoginAt", "last_login_at" }
                    );
                    device.is_current = jsonBool(item, { "isCurrent", "is_current", "currentDevice", "current" });
                    devices.push_back(std::move(device));
                }
            }
            (*cb)(true, devices, "");
        } catch (const std::exception& e) {
            (*cb)(false, {}, std::string("JSON parse error: ") + e.what());
        }
    };

    http_->post("/auth/device/list", "{}", [this, cb, parse_response](network::HttpResponse resp) {
        if (resp.error.empty() && (resp.status_code == 404 || resp.status_code == 405)) {
            http_->get("/auth/devices", [cb, parse_response](network::HttpResponse fallback_resp) {
                parse_response(std::move(fallback_resp));
            });
            return;
        }
        parse_response(std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// logoutDevice
// ---------------------------------------------------------------------------

void AuthManagerImpl::logoutDevice(const std::string& device_id, ResultCallback callback) {
    nlohmann::json body;
    body["deviceId"] = device_id;

    http_->post(
        "/auth/device/logout",
        body.dump(),
        [this, device_id, cb = std::move(callback)](network::HttpResponse resp) {
            if (resp.error.empty() && (resp.status_code == 404 || resp.status_code == 405)) {
                // Backward compatibility with older route naming.
                nlohmann::json fallback_body;
                fallback_body["deviceId"] = device_id;
                http_->post(
                    "/auth/devices/logout",
                    fallback_body.dump(),
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

// ---------------------------------------------------------------------------
// refreshToken
// ---------------------------------------------------------------------------

void AuthManagerImpl::refreshToken(const std::string& refresh_token, AuthCallback callback) {
    nlohmann::json body;
    body["refreshToken"] = refresh_token;

    http_->post("/auth/refresh", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleAuthResponse(std::move(resp), cb);
    });
}

// ---------------------------------------------------------------------------
// changePassword
// ---------------------------------------------------------------------------

void AuthManagerImpl::changePassword(
    const std::string& old_password,
    const std::string& new_password,
    ResultCallback callback
) {
    nlohmann::json body;
    body["deviceId"] = device_id_;
    body["oldPassword"] = old_password;
    body["newPassword"] = new_password;

    http_->post("/auth/password/change", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleResultResponse(std::move(resp), "change password failed", cb);
    });
}

// ---------------------------------------------------------------------------
// resetPassword
// ---------------------------------------------------------------------------

void AuthManagerImpl::resetPassword(
    const std::string& account,
    const std::string& verify_code,
    const std::string& new_password,
    ResultCallback callback
) {
    nlohmann::json body;
    body["account"] = account;
    body["verifyCode"] = verify_code;
    body["newPassword"] = new_password;

    http_->post("/auth/password/reset", body.dump(), [this, cb = std::move(callback)](network::HttpResponse resp) {
        handleResultResponse(std::move(resp), "reset password failed", cb);
    });
}

// ---------------------------------------------------------------------------
// State accessors
// ---------------------------------------------------------------------------

bool AuthManagerImpl::isLoggedIn() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return !token_.access_token.empty() && token_.expires_at_ms > unixNowMs();
}

AuthToken AuthManagerImpl::currentToken() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return token_;
}

// ---------------------------------------------------------------------------
// ensureValidToken
// ---------------------------------------------------------------------------

void AuthManagerImpl::ensureValidToken(ResultCallback cb) {
    AuthToken token_snapshot;
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        token_snapshot = token_;
    }

    const int64_t now_ms = unixNowMs();
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

// ---------------------------------------------------------------------------
// setListener
// ---------------------------------------------------------------------------

void AuthManagerImpl::setListener(std::shared_ptr<AuthListener> listener) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    listener_ = std::move(listener);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void AuthManagerImpl::handleResultResponse(
    network::HttpResponse resp,
    const std::string& fallback_message,
    const ResultCallback& cb
) {
    if (!cb) {
        return;
    }
    if (!resp.error.empty()) {
        cb(false, resp.error);
        return;
    }

    try {
        const auto json = nlohmann::json::parse(resp.body);
        if (json.value("code", -1) == 0) {
            cb(true, "");
            return;
        }
        cb(false, extractResponseMessage(json, fallback_message));
    } catch (const std::exception& e) {
        cb(false, std::string("JSON parse error: ") + e.what());
    }
}

void AuthManagerImpl::handleAuthResponse(network::HttpResponse resp, const AuthCallback& callback) {
    if (!callback) {
        return;
    }
    if (!resp.error.empty()) {
        callback(false, {}, resp.error);
        return;
    }
    try {
        const auto json = nlohmann::json::parse(resp.body);
        const int code = json.value("code", -1);
        if (code != 0) {
            callback(false, {}, extractResponseMessage(json, "auth failed"));
            return;
        }

        const auto& data = json.value("data", nlohmann::json::object());
        AuthToken token;
        token.access_token = jsonString(data, { "accessToken", "access_token" });
        token.refresh_token = jsonString(data, { "refreshToken", "refresh_token" });
        const int64_t expires_in = jsonInt64(data.value("expiresIn", data.value("expires_in", 0)));
        token.expires_at_ms = unixNowMs() + expires_in * 1000;

        storeToken(token);
        http_->setAuthToken(token.access_token);

        callback(true, token, "");
    } catch (const std::exception& e) {
        callback(false, {}, std::string("JSON parse error: ") + e.what());
    }
}

void AuthManagerImpl::handleAuthNotification(const NotificationEvent& event) {
    if (event.notification_type != "auth.force_logout") {
        return;
    }

    const std::string target_device_id = jsonString(event.data, { "deviceId", "device_id", "deviceID" });

    // If server specifies target device, only react on current device.
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

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

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
