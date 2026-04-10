#pragma once
#include "notification_manager.h"

#include "anychat/friend.h"

#include "db/database.h"
#include "network/http_client.h"

#include <memory>
#include <mutex>

namespace anychat {

class FriendManagerImpl : public FriendManager {
public:
    FriendManagerImpl(db::Database* db, NotificationManager* notif_mgr, std::shared_ptr<network::HttpClient> http);

    // FriendManager interface
    void getList(FriendListCallback cb) override;
    void sendRequest(const std::string& to_user_id, const std::string& message, FriendCallback cb) override;
    void sendRequest(
        const std::string& to_user_id,
        const std::string& message,
        const std::string& source,
        FriendCallback cb
    ) override;
    void handleRequest(int64_t request_id, bool accept, FriendCallback cb) override;
    void getPendingRequests(FriendRequestListCallback cb) override;
    void getRequests(const std::string& request_type, FriendRequestListCallback cb) override;
    void deleteFriend(const std::string& friend_id, FriendCallback cb) override;
    void updateRemark(const std::string& friend_id, const std::string& remark, FriendCallback cb) override;
    void getBlacklist(BlacklistListCallback cb) override;
    void addToBlacklist(const std::string& user_id, FriendCallback cb) override;
    void removeFromBlacklist(const std::string& user_id, FriendCallback cb) override;
    void setListener(std::shared_ptr<FriendListener> listener) override;

private:
    void handleFriendNotification(const NotificationEvent& event);

    db::Database* db_;
    NotificationManager* notif_mgr_;
    std::shared_ptr<network::HttpClient> http_;

    mutable std::mutex handler_mutex_;
    std::shared_ptr<FriendListener> listener_;
};

} // namespace anychat
