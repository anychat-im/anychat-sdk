#pragma once
#include "notification_manager.h"

#include "anychat/conversation.h"

#include "cache/conversation_cache.h"
#include "db/database.h"
#include "network/http_client.h"

#include <memory>
#include <mutex>

namespace anychat {

class ConversationManagerImpl : public ConversationManager {
public:
    ConversationManagerImpl(
        db::Database* db,
        cache::ConversationCache* conv_cache,
        NotificationManager* notif_mgr,
        std::shared_ptr<network::HttpClient> http
    );

    // ConversationManager interface
    void getList(ConversationListCallback cb) override;
    void getTotalUnread(ConversationTotalUnreadCallback cb) override;
    void getConversation(const std::string& conv_id, ConversationDetailCallback cb) override;
    void markAllRead(const std::string& conv_id, ConversationCallback cb) override;
    void markMessagesRead(
        const std::string& conv_id,
        const std::vector<std::string>& message_ids,
        ConversationMarkReadResultCallback cb
    ) override;
    void setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) override;
    void setMuted(const std::string& conv_id, bool muted, ConversationCallback cb) override;
    void setBurnAfterReading(const std::string& conv_id, int32_t duration, ConversationCallback cb) override;
    void setAutoDelete(const std::string& conv_id, int32_t duration, ConversationCallback cb) override;
    void deleteConv(const std::string& conv_id, ConversationCallback cb) override;
    void
    getMessageUnreadCount(const std::string& conv_id, int64_t last_read_seq, ConversationUnreadStateCallback cb)
        override;
    void getMessageReadReceipts(const std::string& conv_id, ConversationReadReceiptListCallback cb) override;
    void getMessageSequence(const std::string& conv_id, ConversationSequenceCallback cb) override;
    void setListener(std::shared_ptr<ConversationListener> listener) override;

private:
    // Parse a single conversation JSON object into a Conversation struct.
    static Conversation parseConversation(const nlohmann::json& j);

    // Persist a conversation to the DB (upsert).
    void upsertDb(const Conversation& conv);

    // Populate a Conversation from a DB row.
    static Conversation rowToConversation(const db::Row& row);

    // Notification handler for conversation-related events.
    void handleConversationNotification(const NotificationEvent& event);

    db::Database* db_;
    cache::ConversationCache* conv_cache_;
    NotificationManager* notif_mgr_;
    std::shared_ptr<network::HttpClient> http_;

    mutable std::mutex handler_mutex_;
    std::shared_ptr<ConversationListener> listener_;
};

} // namespace anychat
