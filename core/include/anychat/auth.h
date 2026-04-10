#pragma once

#include "types.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace anychat {

// Callback for operations that return an AuthToken (login / refresh / register)
using AuthCallback = std::function<void(bool success, const AuthToken& token, const std::string& error)>;

// Callback for send-code operation.
using SendCodeCallback =
    std::function<void(bool success, const VerificationCodeResult& result, const std::string& error)>;

// Callback for listing user devices.
using DeviceListCallback =
    std::function<void(bool success, const std::vector<AuthDevice>& devices, const std::string& error)>;

// Callback for operations that only indicate success/failure with an optional message
using ResultCallback = std::function<void(bool success, const std::string& error)>;

class AuthListener {
public:
    virtual ~AuthListener() = default;

    virtual void onAuthExpired() {}
};

class AuthManager {
public:
    virtual ~AuthManager() = default;

    // Register a new account.
    // verify_code: SMS / email verification code.
    // nickname: optional display name (pass empty string to skip).
    // client_version: client version string (e.g. "1.0.0")
    virtual void registerUser(
        const std::string& phone_or_email,
        const std::string& password,
        const std::string& verify_code,
        const std::string& device_type,
        const std::string& nickname,
        const std::string& client_version,
        AuthCallback callback
    ) = 0;

    // Send verification code for register/reset_password/bind/change flows.
    // target_type: "sms" | "email"
    virtual void sendVerificationCode(
        const std::string& target,
        const std::string& target_type,
        const std::string& purpose,
        SendCodeCallback callback
    ) = 0;

    // Login with account (phone number or email) + password.
    // device_type: "ios" | "android" | "web" | "pc" | "h5"
    // client_version: client version string (e.g. "1.0.0")
    // The manager uses the device_id provided at construction time.
    virtual void login(
        const std::string& account,
        const std::string& password,
        const std::string& device_type,
        const std::string& client_version,
        AuthCallback callback
    ) = 0;

    // Logout the current device and invalidate its token.
    virtual void logout(ResultCallback callback) = 0;

    // Exchange a refresh_token for a new AccessToken.
    virtual void refreshToken(const std::string& refresh_token, AuthCallback callback) = 0;

    // Change the current user's password (requires a valid access_token).
    virtual void
    changePassword(const std::string& old_password, const std::string& new_password, ResultCallback callback) = 0;

    // Reset password (forgot password flow).
    virtual void resetPassword(
        const std::string& account,
        const std::string& verify_code,
        const std::string& new_password,
        ResultCallback callback
    ) = 0;

    // Query logged-in devices for current user.
    virtual void getDeviceList(DeviceListCallback callback) = 0;

    // Force logout a specific device by device_id.
    virtual void logoutDevice(const std::string& device_id, ResultCallback callback) = 0;

    virtual bool isLoggedIn() const = 0;
    virtual AuthToken currentToken() const = 0;

    // Checks token expiry; if expired (or about to expire) refreshes automatically.
    // Calls cb(true, "") on success, cb(false, reason) on failure.
    virtual void ensureValidToken(ResultCallback cb) = 0;

    // Fired when the access token has expired and cannot be refreshed
    // (e.g. refresh token also invalid). The client must re-login.
    virtual void setListener(std::shared_ptr<AuthListener> listener) = 0;
};

} // namespace anychat
