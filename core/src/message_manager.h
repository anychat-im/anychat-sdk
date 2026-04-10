#pragma once

#include "notification_manager.h"
#include "outbound_queue.h"

#include "anychat/message.h"

#include "cache/message_cache.h"
#include "db/database.h"
#include "network/http_client.h"

#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
    void getOfflineMessages(int64_t last_seq, int limit, MessageOfflineCallback callback) override;
    void ackMessages(
        const std::string& conv_id,
        const std::vector<std::string>& message_ids,
        MessageCallback callback
    ) override;
    void getGroupMessageReadState(
        const std::string& group_id,
        const std::string& message_id,
        GroupMessageReadStateCallback callback
    ) override;
    void searchMessages(
        const std::string& keyword,
        const std::string& conversation_id,
        const std::string& content_type,
        int limit,
        int offset,
        MessageSearchCallback callback
    ) override;
    void recallMessage(const std::string& message_id, MessageCallback callback) override;
    void deleteMessage(const std::string& message_id, MessageCallback callback) override;
    void editMessage(const std::string& message_id, const std::string& content, MessageCallback callback) override;
    void sendTyping(const std::string& conversation_id, bool typing, int32_t ttl_seconds, MessageCallback callback)
        override;

    void setOnMessageReceived(OnMessageReceived handler) override;
    void setOnMessageReadReceipt(OnMessageReadReceipt handler) override;
    void setOnMessageRecalled(OnMessageChanged handler) override;
    void setOnMessageDeleted(OnMessageChanged handler) override;
    void setOnMessageEdited(OnMessageChanged handler) override;
    void setOnMessageTyping(OnMessageTyping handler) override;
    void setOnMessageMentioned(OnMessageChanged handler) override;

    // Called by AnyChatClientImpl when current_user_id becomes known after login.
    void setCurrentUserId(const std::string& uid);

private:
    struct HandlerBundle {
        OnMessageReceived on_message_received;
        OnMessageReadReceipt on_message_read_receipt;
        OnMessageChanged on_message_recalled;
        OnMessageChanged on_message_deleted;
        OnMessageChanged on_message_edited;
        OnMessageTyping on_message_typing;
        OnMessageChanged on_message_mentioned;
    };

    static std::string urlEncode(const std::string& input);
    static bool parseHttpRoot(const network::HttpResponse& resp, nlohmann::json& root, std::string& err);
    static int64_t normalizeEpochMs(int64_t raw);
    static int64_t parseTimestampMs(const nlohmann::json& value);
    static Message parseMessageJson(const nlohmann::json& item, const std::string& default_conv_id = "");
    static void applyMessagePatch(const nlohmann::json& item, Message& msg);
    static GroupMessageReadState parseGroupMessageReadState(const nlohmann::json& data);
    static MessageReadReceiptEvent parseReadReceiptEvent(const nlohmann::json& data);
    static MessageTypingEvent parseTypingEvent(const nlohmann::json& data);
    void upsertMessageDb(const Message& msg);
    void updateMessageDbStatusAndContent(const std::string& message_id, int status, const std::string* content);
    void dispatchMessageChanged(const Message& msg, const OnMessageChanged& handler);

    void handleIncomingMessage(const NotificationEvent& event);
    void handleReadReceipt(const NotificationEvent& event);
    void handleMessageRecalled(const NotificationEvent& event);
    void handleMessageDeleted(const NotificationEvent& event);
    void handleMessageEdited(const NotificationEvent& event);
    void handleTyping(const NotificationEvent& event);
    void handleMentioned(const NotificationEvent& event);
    static std::string generateLocalId();

    db::Database* db_;
    cache::MessageCache* msg_cache_;
    OutboundQueue* outbound_q_;
    NotificationManager* notif_mgr_;
    std::shared_ptr<network::HttpClient> http_;

    mutable std::mutex handler_mutex_;
    HandlerBundle handlers_;

    std::mutex uid_mutex_;
    std::string current_user_id_;
};

} // namespace anychat
