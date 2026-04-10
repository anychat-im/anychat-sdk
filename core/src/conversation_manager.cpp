#include "conversation_manager.h"

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {

// ---------------------------------------------------------------------------
// Helper: parse a server session JSON object into a Conversation struct
// ---------------------------------------------------------------------------

/*static*/ Conversation ConversationManagerImpl::parseSession(const nlohmann::json& j) {
    Conversation c;
    c.conv_id = j.value("sessionId", "");
    const std::string stype = j.value("sessionType", "single");
    c.conv_type = (stype == "group") ? ConversationType::Group : ConversationType::Private;
    c.target_id = j.value("targetId", "");
    c.last_msg_id = j.value("lastMessageId", "");
    c.last_msg_text = j.value("lastMessageContent", "");
    // Server timestamps are in Unix seconds; convert to ms.
    c.last_msg_time_ms = j.value("lastMessageTime", int64_t{ 0 }) * 1000;
    c.unread_count = j.value("unreadCount", 0);
    c.is_pinned = j.value("isPinned", false);
    c.is_muted = j.value("isMuted", false);
    c.pin_time_ms = j.value("pinTime", int64_t{ 0 }) * 1000;
    c.updated_at_ms = j.value("updatedAt", int64_t{ 0 }) * 1000;
    return c;
}

// ---------------------------------------------------------------------------
// Helper: build a Conversation from a DB row
// ---------------------------------------------------------------------------

/*static*/ Conversation ConversationManagerImpl::rowToConversation(const db::Row& row) {
    auto get = [&](const std::string& k, const std::string& def = "") -> std::string {
        auto it = row.find(k);
        return (it != row.end()) ? it->second : def;
    };
    auto getI = [&](const std::string& k) -> int64_t {
        auto it = row.find(k);
        if (it == row.end() || it->second.empty())
            return 0;
        try {
            return std::stoll(it->second);
        } catch (...) {
            return 0;
        }
    };

    Conversation c;
    c.conv_id = get("conv_id");
    c.conv_type = (get("conv_type") == "group") ? ConversationType::Group : ConversationType::Private;
    c.target_id = get("target_id");
    c.last_msg_id = get("last_msg_id");
    c.last_msg_text = get("last_msg_text");
    c.last_msg_time_ms = getI("last_msg_time_ms");
    c.unread_count = static_cast<int32_t>(getI("unread_count"));
    c.is_pinned = (getI("is_pinned") != 0);
    c.is_muted = (getI("is_muted") != 0);
    c.pin_time_ms = getI("pin_time_ms");
    c.local_seq = getI("local_seq");
    c.updated_at_ms = getI("updated_at_ms");
    return c;
}

// ---------------------------------------------------------------------------
// Helper: upsert a conversation into the DB
// ---------------------------------------------------------------------------

void ConversationManagerImpl::upsertDb(const Conversation& c) {
    const std::string sql = "INSERT INTO conversations "
                            "(conv_id, conv_type, target_id, last_msg_id, last_msg_text, "
                            " last_msg_time_ms, unread_count, is_pinned, is_muted, pin_time_ms, "
                            " local_seq, updated_at_ms) "
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?) "
                            "ON CONFLICT(conv_id) DO UPDATE SET "
                            " conv_type=excluded.conv_type, target_id=excluded.target_id, "
                            " last_msg_id=excluded.last_msg_id, last_msg_text=excluded.last_msg_text, "
                            " last_msg_time_ms=excluded.last_msg_time_ms, "
                            " unread_count=excluded.unread_count, is_pinned=excluded.is_pinned, "
                            " is_muted=excluded.is_muted, pin_time_ms=excluded.pin_time_ms, "
                            " local_seq=excluded.local_seq, updated_at_ms=excluded.updated_at_ms";

    const std::string conv_type_str = (c.conv_type == ConversationType::Group) ? "group" : "single";

    db_->exec(
        sql,
        { c.conv_id,
          conv_type_str,
          c.target_id,
          c.last_msg_id,
          c.last_msg_text,
          static_cast<int64_t>(c.last_msg_time_ms),
          static_cast<int64_t>(c.unread_count),
          static_cast<int64_t>(c.is_pinned ? 1 : 0),
          static_cast<int64_t>(c.is_muted ? 1 : 0),
          static_cast<int64_t>(c.pin_time_ms),
          static_cast<int64_t>(c.local_seq),
          static_cast<int64_t>(c.updated_at_ms) }
    );
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ConversationManagerImpl::ConversationManagerImpl(
    db::Database* db,
    cache::ConversationCache* conv_cache,
    NotificationManager* notif_mgr,
    std::shared_ptr<network::HttpClient> http
)
    : db_(db)
    , conv_cache_(conv_cache)
    , notif_mgr_(notif_mgr)
    , http_(std::move(http)) {
    // Register a single notification handler that dispatches all
    // session-related notification types.
    notif_mgr_->addNotificationHandler([this](const NotificationEvent& event) {
        const auto& nt = event.notification_type;
        if (nt == "session.unread_updated" || nt == "session.pin_updated" || nt == "session.mute_updated"
            || nt == "session.deleted") {
            handleSessionNotification(event);
        }
    });
}

// ---------------------------------------------------------------------------
// getList
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getList(ConversationListCallback cb) {
    // Fast-path: use cache if already populated.
    auto cached = conv_cache_->getAll();
    if (!cached.empty()) {
        cb(std::move(cached), "");
        return;
    }

    // Fall back to HTTP.
    http_->get("/conversations", [this, cb = std::move(cb)](network::HttpResponse resp) {
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

            // The conversations list may be directly in "data" as an array,
            // or wrapped as {"list": [...]} — handle both.
            std::vector<Conversation> convs;
            nlohmann::json sessions_arr;
            auto& data = root["data"];
            if (data.is_array()) {
                sessions_arr = data;
            } else if (data.contains("list") && data["list"].is_array()) {
                sessions_arr = data["list"];
            }

            for (const auto& item : sessions_arr) {
                Conversation c = parseSession(item);
                upsertDb(c);
                conv_cache_->upsert(c);
                convs.push_back(c);
            }

            // Return the cache-sorted version.
            cb(conv_cache_->getAll(), "");
        } catch (const std::exception& ex) {
            cb({}, std::string("parse error: ") + ex.what());
        }
    });
}

// ---------------------------------------------------------------------------
// markRead
// ---------------------------------------------------------------------------

void ConversationManagerImpl::markRead(const std::string& conv_id, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/read";
    http_->post(path, "", [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        if (resp.status_code != 200) {
            cb(false, "HTTP " + std::to_string(resp.status_code));
            return;
        }
        conv_cache_->clearUnread(conv_id);
        db_->exec("UPDATE conversations SET unread_count=0 WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// setPinned
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/pin";
    nlohmann::json body_j = { { "pinned", pinned } };
    http_->put(path, body_j.dump(), [this, conv_id, pinned, cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        if (resp.status_code != 200) {
            cb(false, "HTTP " + std::to_string(resp.status_code));
            return;
        }

        // Update cache entry.
        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.is_pinned = pinned;
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// setMuted
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setMuted(const std::string& conv_id, bool muted, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/mute";
    nlohmann::json body_j = { { "muted", muted } };
    http_->put(path, body_j.dump(), [this, conv_id, muted, cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        if (resp.status_code != 200) {
            cb(false, "HTTP " + std::to_string(resp.status_code));
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.is_muted = muted;
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// deleteConv
// ---------------------------------------------------------------------------

void ConversationManagerImpl::deleteConv(const std::string& conv_id, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id;
    http_->del(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        if (resp.status_code != 200) {
            cb(false, "HTTP " + std::to_string(resp.status_code));
            return;
        }
        conv_cache_->remove(conv_id);
        db_->exec("DELETE FROM conversations WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// setOnConversationUpdated
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setOnConversationUpdated(OnConversationUpdated handler) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    on_updated_ = std::move(handler);
}

// ---------------------------------------------------------------------------
// handleSessionNotification  (private)
// ---------------------------------------------------------------------------

void ConversationManagerImpl::handleSessionNotification(const NotificationEvent& event) {
    try {
        const auto& d = event.data;
        const auto& nt = event.notification_type;

        if (nt == "session.deleted") {
            Conversation removed;
            removed.conv_id = d.value("sessionId", "");
            if (!removed.conv_id.empty()) {
                auto existing = conv_cache_->get(removed.conv_id);
                if (existing) {
                    removed = *existing;
                }
                conv_cache_->remove(removed.conv_id);
                db_->exec("DELETE FROM conversations WHERE conv_id=?", { removed.conv_id });
            }

            OnConversationUpdated handler;
            {
                std::lock_guard<std::mutex> lk(handler_mutex_);
                handler = on_updated_;
            }
            if (handler) {
                handler(removed);
            }
            return;
        }

        // For all other session events the data carries the updated session object.
        // Try to parse it as a session.
        Conversation c = parseSession(d);
        if (c.conv_id.empty())
            return;

        // Merge with the existing cached entry so we don't lose fields the
        // notification doesn't include.
        auto existing = conv_cache_->get(c.conv_id);
        if (existing) {
            Conversation merged = *existing;
            // Apply the deltas carried by each notification type.
            if (nt == "session.unread_updated") {
                merged.unread_count = d.value("unreadCount", merged.unread_count);
            } else if (nt == "session.pin_updated") {
                merged.is_pinned = d.value("isPinned", merged.is_pinned);
                merged.pin_time_ms = d.value("pinTime", int64_t{ 0 }) * 1000;
            } else if (nt == "session.mute_updated") {
                merged.is_muted = d.value("isMuted", merged.is_muted);
            }
            c = merged;
        }

        conv_cache_->upsert(c);
        upsertDb(c);

        // Fire the user-facing handler outside the lock.
        OnConversationUpdated handler;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            handler = on_updated_;
        }
        if (handler) {
            handler(c);
        }
    } catch (const std::exception&) {
        // Malformed notification — silently ignore.
    }
}

} // namespace anychat
