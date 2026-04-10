#include "message_manager.h"

#include <atomic>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MessageManagerImpl::MessageManagerImpl(
    db::Database* db,
    cache::MessageCache* msg_cache,
    OutboundQueue* outbound_q,
    NotificationManager* notif_mgr,
    std::shared_ptr<network::HttpClient> http,
    const std::string& current_user_id
)
    : db_(db)
    , msg_cache_(msg_cache)
    , outbound_q_(outbound_q)
    , notif_mgr_(notif_mgr)
    , http_(std::move(http))
    , current_user_id_(current_user_id) {
    // Register a "notification" handler that filters on notificationType ==
    // "message.new" and forwards to handleIncomingMessage.
    notif_mgr_->addNotificationHandler([this](const NotificationEvent& event) {
        if (event.notification_type == "message.new") {
            handleIncomingMessage(event);
        }
    });
}

// ---------------------------------------------------------------------------
// sendTextMessage
// ---------------------------------------------------------------------------

void MessageManagerImpl::sendTextMessage(
    const std::string& conv_id,
    const std::string& content,
    MessageCallback callback
) {
    const std::string local_id = generateLocalId();
    outbound_q_->enqueue(conv_id, "private", "text", content, local_id, std::move(callback));
}

// ---------------------------------------------------------------------------
// getHistory
// ---------------------------------------------------------------------------

void MessageManagerImpl::getHistory(
    const std::string& conv_id,
    int64_t before_timestamp,
    int limit,
    MessageListCallback callback
) {
    // Fast-path: return cache when caller asks for "latest" (before_timestamp
    // == 0) and the cache has something.
    if (before_timestamp == 0) {
        auto cached = msg_cache_->get(conv_id);
        if (!cached.empty()) {
            callback(cached, "");
            return;
        }
    }

    // Build the query path.
    std::string path = "/conversations/" + conv_id + "/messages?limit=" + std::to_string(limit);
    if (before_timestamp > 0) {
        path += "&before=" + std::to_string(before_timestamp);
    }

    http_->get(path, [this, conv_id, cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb({}, resp.error);
            return;
        }
        if (resp.status_code != 200) {
            cb({}, "HTTP " + std::to_string(resp.status_code));
            return;
        }

        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb({}, root.value("message", "server error"));
                return;
            }

            auto& data = root["data"];
            auto& list = data["list"];

            std::vector<Message> messages;
            messages.reserve(list.size());

            for (const auto& item : list) {
                Message msg;
                msg.message_id = item.value("messageId", "");
                msg.conv_id = item.value("conversationId", conv_id);
                msg.sender_id = item.value("senderId", "");
                msg.content_type = item.value("contentType", "text");
                msg.content = item.value("content", "");
                msg.seq = item.value("sequence", int64_t{ 0 });
                // Server returns Unix seconds; convert to ms.
                msg.timestamp_ms = item.value("timestamp", int64_t{ 0 }) * 1000;
                msg.status = item.value("status", 0);

                msg_cache_->insert(msg);
                messages.push_back(msg);
            }

            cb(messages, "");
        } catch (const std::exception& ex) {
            cb({}, std::string("parse error: ") + ex.what());
        }
    });
}

// ---------------------------------------------------------------------------
// markAsRead
// ---------------------------------------------------------------------------

void MessageManagerImpl::markAsRead(
    const std::string& conv_id,
    const std::string& /*message_id*/,
    MessageCallback callback
) {
    const std::string path = "/conversations/" + conv_id + "/read-all";
    http_->post(path, "", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        if (resp.status_code == 200) {
            cb(true, "");
        } else {
            cb(false, "HTTP " + std::to_string(resp.status_code));
        }
    });
}

// ---------------------------------------------------------------------------
// setOnMessageReceived
// ---------------------------------------------------------------------------

void MessageManagerImpl::setOnMessageReceived(OnMessageReceived handler) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    on_message_received_ = std::move(handler);
}

// ---------------------------------------------------------------------------
// setCurrentUserId
// ---------------------------------------------------------------------------

void MessageManagerImpl::setCurrentUserId(const std::string& uid) {
    std::lock_guard<std::mutex> lk(uid_mutex_);
    current_user_id_ = uid;
}

// ---------------------------------------------------------------------------
// handleIncomingMessage  (private)
// ---------------------------------------------------------------------------

void MessageManagerImpl::handleIncomingMessage(const NotificationEvent& event) {
    try {
        const auto& d = event.data;

        Message msg;
        msg.message_id = d.value("messageId", "");
        msg.conv_id = d.value("conversationId", "");
        msg.sender_id = d.value("senderId", d.value("fromUserId", ""));
        msg.content_type = d.value("contentType", "text");
        msg.content = d.value("content", "");
        msg.seq = d.value("sequence", d.value("seq", int64_t{ 0 }));
        const int64_t timestamp = d.value("timestamp", d.value("sentAt", event.timestamp));
        msg.timestamp_ms = timestamp * 1000;

        // Insert into cache (dedup by message_id is handled inside insert()).
        msg_cache_->insert(msg);

        // Fire the user-facing handler outside the lock to avoid deadlock.
        OnMessageReceived handler;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            handler = on_message_received_;
        }
        if (handler) {
            handler(msg);
        }
    } catch (const std::exception&) {
        // Malformed notification — silently ignore.
    }
}

// ---------------------------------------------------------------------------
// generateLocalId  (private static)
// ---------------------------------------------------------------------------

/*static*/ std::string MessageManagerImpl::generateLocalId() {
    static std::atomic<int64_t> counter{ 1 };
    return "local_" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

} // namespace anychat
