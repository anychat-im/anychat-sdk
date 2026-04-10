#pragma once

#include "anychat/auth.h"

#include "db/database.h"
#include "network/http_client.h"
#include "notification_manager.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace anychat {

class AuthManagerImpl : public AuthManager {
public:
    // |http|      – shared HTTP client (base_url already set to api_base_url).
    // |device_id| – stable identifier for this installation/device.
    // |db|        – optional database for token persistence; may be nullptr.
    AuthManagerImpl(
        std::shared_ptr<network::HttpClient> http,
        std::string device_id,
        db::Database* db,
        NotificationManager* notif_mgr
    );

    void registerUser(
        const std::string& phone_or_email,
        const std::string& password,
        const std::string& verify_code,
        const std::string& device_type,
        const std::string& nickname,
        const std::string& client_version,
        AuthCallback callback
    ) override;

    void sendVerificationCode(
        const std::string& target,
        const std::string& target_type,
        const std::string& purpose,
        SendCodeCallback callback
    ) override;

    void login(
        const std::string& account,
        const std::string& password,
        const std::string& device_type,
        const std::string& client_version,
        AuthCallback callback
    ) override;

    void logout(ResultCallback callback) override;

    void refreshToken(const std::string& refresh_token, AuthCallback callback) override;

    void
    changePassword(const std::string& old_password, const std::string& new_password, ResultCallback callback) override;

    void resetPassword(
        const std::string& account,
        const std::string& verify_code,
        const std::string& new_password,
        ResultCallback callback
    ) override;

    void getDeviceList(DeviceListCallback callback) override;
    void logoutDevice(const std::string& device_id, ResultCallback callback) override;

    bool isLoggedIn() const override;
    AuthToken currentToken() const override;

    void ensureValidToken(ResultCallback cb) override;
    void setOnAuthExpired(std::function<void()> cb) override;

private:
    void handleAuthResponse(network::HttpResponse resp, const AuthCallback& callback);
    void handleResultResponse(network::HttpResponse resp, const std::string& fallback_message, const ResultCallback& cb);
    void handleAuthNotification(const NotificationEvent& event);
    void storeToken(const AuthToken& token);
    void clearToken();

    std::shared_ptr<network::HttpClient> http_;
    std::string device_id_;
    db::Database* db_; // non-owning, may be nullptr
    mutable std::mutex token_mutex_;
    AuthToken token_;
    std::mutex cb_mutex_;
    std::function<void()> on_auth_expired_;
};

// Factory — creates a fully-functional AuthManager.
std::unique_ptr<AuthManager>
createAuthManager(
    std::shared_ptr<network::HttpClient> http,
    const std::string& device_id,
    db::Database* db = nullptr,
    NotificationManager* notif_mgr = nullptr
);

} // namespace anychat
