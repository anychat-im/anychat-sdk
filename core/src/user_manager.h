#pragma once

#include "anychat/user.h"

#include "network/http_client.h"
#include "notification_manager.h"

#include <memory>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {

class UserManagerImpl : public UserManager {
public:
    explicit UserManagerImpl(
        std::shared_ptr<network::HttpClient> http,
        NotificationManager* notif_mgr = nullptr,
        std::string device_id = {}
    );

    void getProfile(ProfileCallback callback) override;
    void updateProfile(const UserProfile& profile, ProfileCallback callback) override;
    void getSettings(SettingsCallback callback) override;
    void updateSettings(const UserSettings& settings, SettingsCallback callback) override;
    void updatePushToken(const std::string& push_token, const std::string& platform, ResultCallback callback) override;
    void updatePushToken(
        const std::string& push_token,
        const std::string& platform,
        const std::string& device_id,
        ResultCallback callback
    ) override;
    void searchUsers(const std::string& keyword, int page, int page_size, UserListCallback callback) override;
    void getUserInfo(const std::string& user_id, UserInfoCallback callback) override;
    void bindPhone(
        const std::string& phone_number,
        const std::string& verify_code,
        BindPhoneCallback callback
    ) override;
    void changePhone(
        const std::string& old_phone_number,
        const std::string& new_phone_number,
        const std::string& new_verify_code,
        const std::string& old_verify_code,
        ChangePhoneCallback callback
    ) override;
    void bindEmail(const std::string& email, const std::string& verify_code, BindEmailCallback callback) override;
    void changeEmail(
        const std::string& old_email,
        const std::string& new_email,
        const std::string& new_verify_code,
        const std::string& old_verify_code,
        ChangeEmailCallback callback
    ) override;
    void refreshQRCode(QRCodeCallback callback) override;
    void getUserByQRCode(const std::string& qrcode, UserInfoCallback callback) override;
    void setListener(std::shared_ptr<UserListener> listener) override;

private:
    static UserProfile parseProfile(const nlohmann::json& j);
    static UserSettings parseSettings(const nlohmann::json& j);
    static UserInfo parseUserInfo(const nlohmann::json& j);
    static UserQRCode parseQRCode(const nlohmann::json& j);
    static BindPhoneResult parseBindPhoneResult(const nlohmann::json& j);
    static ChangePhoneResult parseChangePhoneResult(const nlohmann::json& j);
    static BindEmailResult parseBindEmailResult(const nlohmann::json& j);
    static ChangeEmailResult parseChangeEmailResult(const nlohmann::json& j);
    static UserStatusEvent parseUserStatusEvent(const nlohmann::json& j);
    static int64_t parseTimestampMs(const nlohmann::json& value);
    static std::string urlEncode(const std::string& input);
    void handleUserNotification(const NotificationEvent& event);

    std::shared_ptr<network::HttpClient> http_;
    NotificationManager* notif_mgr_ = nullptr;
    std::string device_id_;

    mutable std::mutex handler_mutex_;
    std::shared_ptr<UserListener> listener_;
};

} // namespace anychat
