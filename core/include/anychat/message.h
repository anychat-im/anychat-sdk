#pragma once

#include "types.h"

#include <functional>
#include <memory>
#include <vector>

namespace anychat {

using MessageCallback = std::function<void(bool success, const std::string& error)>;
using MessageListCallback = std::function<void(const std::vector<Message>& messages, const std::string& error)>;
using MessageOfflineCallback = std::function<void(MessageOfflineResult result, std::string err)>;
using MessageSearchCallback = std::function<void(MessageSearchResult result, std::string err)>;
using GroupMessageReadStateCallback = std::function<void(GroupMessageReadState state, std::string err)>;

class MessageListener {
public:
    virtual ~MessageListener() = default;

    virtual void onMessageReceived(const Message& message) {
        (void) message;
    }

    virtual void onMessageReadReceipt(const MessageReadReceiptEvent& event) {
        (void) event;
    }

    virtual void onMessageRecalled(const Message& message) {
        (void) message;
    }

    virtual void onMessageDeleted(const Message& message) {
        (void) message;
    }

    virtual void onMessageEdited(const Message& message) {
        (void) message;
    }

    virtual void onMessageTyping(const MessageTypingEvent& event) {
        (void) event;
    }

    virtual void onMessageMentioned(const Message& message) {
        (void) message;
    }
};

class MessageManager {
public:
    virtual ~MessageManager() = default;

    virtual void
    sendTextMessage(const std::string& conv_id, const std::string& content, MessageCallback callback) = 0;

    virtual void
    getHistory(const std::string& conv_id, int64_t before_timestamp, int limit, MessageListCallback callback) = 0;

    virtual void markAsRead(const std::string& conv_id, const std::string& message_id, MessageCallback callback) = 0;

    // GET /messages/offline?lastSeq={last_seq}&limit={limit}
    virtual void getOfflineMessages(int64_t last_seq, int limit, MessageOfflineCallback callback) = 0;

    // POST /messages/ack
    virtual void ackMessages(
        const std::string& conv_id,
        const std::vector<std::string>& message_ids,
        MessageCallback callback
    ) = 0;

    // GET /groups/{id}/messages/{msgId}/reads
    virtual void getGroupMessageReadState(
        const std::string& group_id,
        const std::string& message_id,
        GroupMessageReadStateCallback callback
    ) = 0;

    // GET /messages/search
    virtual void searchMessages(
        const std::string& keyword,
        const std::string& conversation_id,
        const std::string& content_type,
        int limit,
        int offset,
        MessageSearchCallback callback
    ) = 0;

    // Message operations (aligns with ws/http capabilities).
    virtual void recallMessage(const std::string& message_id, MessageCallback callback) = 0;
    virtual void deleteMessage(const std::string& message_id, MessageCallback callback) = 0;
    virtual void editMessage(const std::string& message_id, const std::string& content, MessageCallback callback) = 0;
    virtual void
    sendTyping(const std::string& conversation_id, bool typing, int32_t ttl_seconds, MessageCallback callback) = 0;

    virtual void setListener(std::shared_ptr<MessageListener> listener) = 0;
};

} // namespace anychat
