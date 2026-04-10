#pragma once

#include "notification_manager.h"
#include "outbound_queue.h"

#include "anychat/message.h"

#include "cache/message_cache.h"
#include "db/database.h"
#include "network/http_client.h"

#include <mutex>
#include <string>

namespace anychat {

class MessageManagerImpl : public MessageManager {
public:
    MessageManagerImpl(
        db::Database* db,
        cache::MessageCache* msg_cache,
        OutboundQueue* outbound_q,
        NotificationManager* notif_mgr,
        std::shared_ptr<network::HttpClient> http,
        const std::string& current_user_id
    );

    void sendTextMessage(const std::string& conv_id, const std::string& content, MessageCallback callback) override;

    void getHistory(const std::string& conv_id, int64_t before_timestamp, int limit, MessageListCallback callback)
        override;

    void markAsRead(const std::string& conv_id, const std::string& message_id, MessageCallback callback) override;

    void setOnMessageReceived(OnMessageReceived handler) override;

    // Called by AnyChatClientImpl when current_user_id becomes known after login.
    void setCurrentUserId(const std::string& uid);

private:
    void handleIncomingMessage(const NotificationEvent& event);
    static std::string generateLocalId();

    db::Database* db_;
    cache::MessageCache* msg_cache_;
    OutboundQueue* outbound_q_;
    NotificationManager* notif_mgr_;
    std::shared_ptr<network::HttpClient> http_;

    mutable std::mutex handler_mutex_;
    OnMessageReceived on_message_received_;

    std::mutex uid_mutex_;
    std::string current_user_id_;
};

} // namespace anychat
