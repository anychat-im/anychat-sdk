#pragma once

#include "types.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace anychat {

using ConversationListCallback = std::function<void(std::vector<Conversation> list, std::string err)>;
using ConversationDetailCallback = std::function<void(Conversation conv, std::string err)>;
using ConversationCallback = std::function<void(bool ok, std::string err)>;
using ConversationTotalUnreadCallback = std::function<void(int32_t total_unread, std::string err)>;
using ConversationUnreadStateCallback = std::function<void(ConversationUnreadState state, std::string err)>;
using ConversationReadReceiptListCallback =
    std::function<void(std::vector<ConversationReadReceipt> list, std::string err)>;
using ConversationSequenceCallback = std::function<void(int64_t current_seq, std::string err)>;
using ConversationMarkReadResultCallback = std::function<void(ConversationMarkReadResult result, std::string err)>;
using OnConversationUpdated = std::function<void(const Conversation& conv)>;

class ConversationManager {
public:
    virtual ~ConversationManager() = default;

    // Returns cached + DB sorted list (pinned first, then by last_msg_time desc)
    virtual void getList(ConversationListCallback cb) = 0;

    // GET /conversations/unread/total
    virtual void getTotalUnread(ConversationTotalUnreadCallback cb) = 0;

    // GET /conversations/{id}
    virtual void getConversation(const std::string& conv_id, ConversationDetailCallback cb) = 0;

    // POST /conversations/{id}/read-all
    virtual void markAllRead(const std::string& conv_id, ConversationCallback cb) = 0;

    // Backward-compatible alias for markAllRead().
    virtual void markRead(const std::string& conv_id, ConversationCallback cb) {
        markAllRead(conv_id, std::move(cb));
    }

    // POST /conversations/{id}/messages/read
    virtual void markMessagesRead(
        const std::string& conv_id,
        const std::vector<std::string>& message_ids,
        ConversationMarkReadResultCallback cb
    ) = 0;

    // Toggle pinned (local + PUT /conversations/{id}/pin)
    virtual void setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) = 0;

    // Toggle muted (local + PUT /conversations/{id}/mute)
    virtual void setMuted(const std::string& conv_id, bool muted, ConversationCallback cb) = 0;

    // PUT /conversations/{id}/burn
    virtual void setBurnAfterReading(const std::string& conv_id, int32_t duration, ConversationCallback cb) = 0;

    // PUT /conversations/{id}/auto_delete
    virtual void setAutoDelete(const std::string& conv_id, int32_t duration, ConversationCallback cb) = 0;

    // Delete conversation (local + DELETE /conversations/{id})
    virtual void deleteConv(const std::string& conv_id, ConversationCallback cb) = 0;

    // GET /conversations/{id}/messages/unread-count
    // last_read_seq < 0 means "not provided".
    virtual void
    getMessageUnreadCount(const std::string& conv_id, int64_t last_read_seq, ConversationUnreadStateCallback cb) = 0;

    virtual void getMessageUnreadCount(const std::string& conv_id, ConversationUnreadStateCallback cb) {
        getMessageUnreadCount(conv_id, -1, std::move(cb));
    }

    // GET /conversations/{id}/messages/read-receipts
    virtual void getMessageReadReceipts(const std::string& conv_id, ConversationReadReceiptListCallback cb) = 0;

    // GET /conversations/{id}/messages/sequence
    virtual void getMessageSequence(const std::string& conv_id, ConversationSequenceCallback cb) = 0;

    // Callback fired whenever a conversation is updated (new message, read, etc.)
    virtual void setOnConversationUpdated(OnConversationUpdated handler) = 0;
};

} // namespace anychat
