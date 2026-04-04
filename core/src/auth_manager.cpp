#include "auth_manager.h"

#include <string>

#include <nlohmann/json.hpp>

#include <ctime>

namespace anychat {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AuthManagerImpl::AuthManagerImpl(std::shared_ptr<network::HttpClient> http, std::string device_id, db::Database* db)
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
            token_.expires_at_ms = std::stoll(exp);
            http_->setAuthToken(at);
        }
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
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            auto json = nlohmann::json::parse(resp.body);
            bool ok = (json.value("code", -1) == 0);
            if (ok) {
                clearToken();
                http_->clearAuthToken();
            }
            cb(ok, ok ? "" : json.value("message", "logout failed"));
        } catch (const std::exception& e) {
            cb(false, std::string("JSON parse error: ") + e.what());
        }
    });
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
    body["oldPassword"] = old_password;
    body["newPassword"] = new_password;

    http_->post("/auth/password/change", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            auto json = nlohmann::json::parse(resp.body);
            bool ok = (json.value("code", -1) == 0);
            cb(ok, ok ? "" : json.value("message", "change password failed"));
        } catch (const std::exception& e) {
            cb(false, std::string("JSON parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// State accessors
// ---------------------------------------------------------------------------

bool AuthManagerImpl::isLoggedIn() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return !token_.access_token.empty() && token_.expires_at_ms > static_cast<int64_t>(std::time(nullptr));
}

AuthToken AuthManagerImpl::currentToken() const {
    std::lock_guard<std::mutex> lock(token_mutex_);
    return token_;
}

// ---------------------------------------------------------------------------
// ensureValidToken
// ---------------------------------------------------------------------------

void AuthManagerImpl::ensureValidToken(ResultCallback cb) {
    if (isLoggedIn()) {
        cb(true, "");
        return;
    }

    std::string rt;
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        rt = token_.refresh_token;
    }

    if (rt.empty()) {
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            if (on_auth_expired_)
                on_auth_expired_();
        }
        cb(false, "no refresh token");
        return;
    }

    refreshToken(rt, [this, cb = std::move(cb)](bool ok, const AuthToken& /*token*/, const std::string& err) {
        if (!ok) {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            if (on_auth_expired_)
                on_auth_expired_();
        }
        cb(ok, err);
    });
}

// ---------------------------------------------------------------------------
// setOnAuthExpired
// ---------------------------------------------------------------------------

void AuthManagerImpl::setOnAuthExpired(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    on_auth_expired_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void AuthManagerImpl::handleAuthResponse(network::HttpResponse resp, const AuthCallback& callback) {
    if (!resp.error.empty()) {
        callback(false, {}, resp.error);
        return;
    }
    try {
        auto json = nlohmann::json::parse(resp.body);
        int code = json.value("code", -1);
        if (code != 0) {
            callback(false, {}, json.value("message", "unknown error"));
            return;
        }

        const auto& data = json.at("data");
        AuthToken token;
        token.access_token = data.value("accessToken", "");
        token.refresh_token = data.value("refreshToken", "");
        auto expires_in = data.value("expiresIn", int64_t{ 0 });
        token.expires_at_ms = static_cast<int64_t>(std::time(nullptr)) + expires_in;

        storeToken(token);
        http_->setAuthToken(token.access_token);

        callback(true, token, "");
    } catch (const std::exception& e) {
        callback(false, {}, std::string("JSON parse error: ") + e.what());
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
createAuthManager(std::shared_ptr<network::HttpClient> http, const std::string& device_id, db::Database* db) {
    return std::make_unique<AuthManagerImpl>(std::move(http), device_id, db);
}

} // namespace anychat
