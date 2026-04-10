#include "sync_engine.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace anychat {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Safe JSON value extraction with a default fallback.
template<typename T>
T jsonGet(const nlohmann::json& obj, const std::string& key, T fallback = T{}) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null())
        return fallback;
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

// Return the current Unix time in milliseconds.
int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SyncEngine::SyncEngine(
    db::Database* db,
    cache::ConversationCache* conv_cache,
    cache::MessageCache* msg_cache,
    std::shared_ptr<network::HttpClient> http
)
    : db_(db)
    , conv_cache_(conv_cache)
    , msg_cache_(msg_cache)
    , http_(std::move(http)) {}

// ---------------------------------------------------------------------------
// sync()
// ---------------------------------------------------------------------------

void SyncEngine::sync() {
    // 1. Read the persisted last_sync_time (0 == full sync on first run).
    int64_t last_sync_time = 0;
    {
        std::string val = db_->getMeta("last_sync_time", "0");
        try {
            last_sync_time = std::stoll(val);
        } catch (...) {
            last_sync_time = 0;
        }
    }

    // 2. Collect all known conversation IDs together with their local_seq so
    //    the server can return only the messages we are missing.
    db::Rows conv_rows = db_->querySync("SELECT conv_id, conv_type, local_seq FROM conversations", {});

    nlohmann::json conv_seqs = nlohmann::json::array();
    for (const auto& row : conv_rows) {
        // Prefer the cache's seq (most up-to-date) over the DB value.
        std::string cid = row.count("conv_id") ? row.at("conv_id") : "";
        std::string ctype = row.count("conv_type") ? row.at("conv_type") : "private";

        if (cid.empty())
            continue;

        int64_t local_seq = 0;
        // Try the in-memory cache first.
        auto cached = conv_cache_->get(cid);
        if (cached.has_value()) {
            local_seq = cached->local_seq;
        } else {
            // Fall back to the DB value.
            if (row.count("local_seq")) {
                try {
                    local_seq = std::stoll(row.at("local_seq"));
                } catch (...) {
                    local_seq = 0;
                }
            }
        }

        // Map DB conv_type to the API enum strings expected by the server.
        std::string api_type = (ctype == "group") ? "group" : "private";

        conv_seqs.push_back({ { "conversationId", cid }, { "conversationType", api_type }, { "lastSeq", local_seq } });
    }

    // 3. Build the POST /sync request body.
    nlohmann::json req_body = { { "lastSyncTime", last_sync_time }, { "conversationSeqs", conv_seqs } };
    std::string body_str = req_body.dump();

    // 4. POST /sync.  The callback runs on the HttpClient worker thread.
    http_->post("/sync", body_str, [this](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            // Transport error — nothing to merge; will retry on next connect.
            return;
        }
        if (resp.status_code != 200) {
            return;
        }
        handleSyncResponse(resp.body);
    });
}

// ---------------------------------------------------------------------------
// handleSyncResponse()
// ---------------------------------------------------------------------------

void SyncEngine::handleSyncResponse(const std::string& body) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(body);
    } catch (...) {
        return;
    }

    // Expect: { "code": 0, "data": { ... } }
    if (jsonGet<int>(root, "code", -1) != 0)
        return;

    auto data_it = root.find("data");
    if (data_it == root.end() || data_it->is_null())
        return;

    const nlohmann::json& data = *data_it;

    // Merge each section if present.
    if (auto it = data.find("friends"); it != data.end() && it->is_object()) {
        auto arr_it = it->find("friends");
        if (arr_it != it->end() && arr_it->is_array()) {
            mergeFriends(*arr_it);
        }
    }

    if (auto it = data.find("groups"); it != data.end() && it->is_object()) {
        auto arr_it = it->find("groups");
        if (arr_it != it->end() && arr_it->is_array()) {
            mergeGroups(*arr_it);
        }
    }

    if (auto it = data.find("sessions"); it != data.end() && it->is_object()) {
        auto arr_it = it->find("sessions");
        if (arr_it != it->end() && arr_it->is_array()) {
            mergeSessions(*arr_it);
        }
    }

    if (auto it = data.find("conversations"); it != data.end() && it->is_array()) {
        mergeConvMessages(*it);
    }

    // Persist the server-reported sync timestamp so the next sync is
    // incremental.
    int64_t sync_time = jsonGet<int64_t>(data, "syncTime", 0);
    if (sync_time > 0) {
        db_->setMeta("last_sync_time", std::to_string(sync_time));
    }
}

// ---------------------------------------------------------------------------
// mergeFriends()
// ---------------------------------------------------------------------------

void SyncEngine::mergeFriends(const nlohmann::json& friends_arr) {
    // Each element in friends_arr represents a friend delta entry from the
    // server.  We upsert into the `friends` table (schema assumed to exist
    // from migrations.cpp).
    for (const auto& f : friends_arr) {
        if (!f.is_object())
            continue;

        std::string user_id = jsonGet<std::string>(f, "userId", "");
        std::string remark = jsonGet<std::string>(f, "remark", "");
        int64_t updated_at = jsonGet<int64_t>(f, "updatedAt", 0LL);
        bool is_deleted = jsonGet<bool>(f, "isDeleted", false);

        if (user_id.empty())
            continue;

        db_->execSync(
            "INSERT INTO friends (user_id, remark, updated_at_ms, is_deleted) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(user_id) DO UPDATE SET "
            "  remark       = excluded.remark, "
            "  updated_at_ms = excluded.updated_at_ms, "
            "  is_deleted   = excluded.is_deleted",
            { user_id, remark, updated_at, static_cast<int64_t>(is_deleted ? 1 : 0) }
        );
    }
}

// ---------------------------------------------------------------------------
// mergeGroups()
// ---------------------------------------------------------------------------

void SyncEngine::mergeGroups(const nlohmann::json& groups_arr) {
    // Each element represents a group the current user belongs to.
    // We upsert into the `groups` table (schema assumed to exist from
    // migrations.cpp).
    for (const auto& g : groups_arr) {
        if (!g.is_object())
            continue;

        std::string group_id = jsonGet<std::string>(g, "groupId", "");
        std::string name = jsonGet<std::string>(g, "name", "");
        std::string avatar = jsonGet<std::string>(g, "avatar", "");
        int64_t updated_at = jsonGet<int64_t>(g, "updatedAt", 0LL);
        int32_t member_cnt = jsonGet<int32_t>(g, "memberCount", 0);

        if (group_id.empty())
            continue;

        db_->execSync(
            "INSERT INTO groups (group_id, name, avatar, member_count, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(group_id) DO UPDATE SET "
            "  name          = excluded.name, "
            "  avatar        = excluded.avatar, "
            "  member_count  = excluded.member_count, "
            "  updated_at_ms = excluded.updated_at_ms",
            { group_id, name, avatar, static_cast<int64_t>(member_cnt), updated_at }
        );
    }
}

// ---------------------------------------------------------------------------
// mergeSessions()
// ---------------------------------------------------------------------------

void SyncEngine::mergeSessions(const nlohmann::json& sessions_arr) {
    // API shape (session object from /sync → data.sessions.sessions):
    //   sessionId, sessionType, targetId, lastMessageContent,
    //   lastMessageTime, unreadCount, isPinned, isMuted
    for (const auto& s : sessions_arr) {
        if (!s.is_object())
            continue;

        std::string conv_id = jsonGet<std::string>(s, "sessionId", "");
        std::string conv_type = jsonGet<std::string>(s, "sessionType", "private");
        std::string target_id = jsonGet<std::string>(s, "targetId", "");
        std::string last_msg_txt = jsonGet<std::string>(s, "lastMessageContent", "");
        int64_t last_msg_time = jsonGet<int64_t>(s, "lastMessageTime", 0LL);
        int32_t unread_count = jsonGet<int32_t>(s, "unreadCount", 0);
        bool is_pinned = jsonGet<bool>(s, "isPinned", false);
        bool is_muted = jsonGet<bool>(s, "isMuted", false);

        if (conv_id.empty())
            continue;

        int64_t now = nowMs();

        // Upsert into the conversations DB table.
        db_->execSync(
            "INSERT INTO conversations "
            "  (conv_id, conv_type, target_id, last_msg_text, "
            "   last_msg_time_ms, unread_count, is_pinned, is_muted, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(conv_id) DO UPDATE SET "
            "  conv_type        = excluded.conv_type, "
            "  target_id        = excluded.target_id, "
            "  last_msg_text    = excluded.last_msg_text, "
            "  last_msg_time_ms = excluded.last_msg_time_ms, "
            "  unread_count     = excluded.unread_count, "
            "  is_pinned        = excluded.is_pinned, "
            "  is_muted         = excluded.is_muted, "
            "  updated_at_ms    = excluded.updated_at_ms",
            { conv_id,
              conv_type,
              target_id,
              last_msg_txt,
              last_msg_time,
              static_cast<int64_t>(unread_count),
              static_cast<int64_t>(is_pinned ? 1 : 0),
              static_cast<int64_t>(is_muted ? 1 : 0),
              now }
        );

        // Upsert into the in-memory cache.
        Conversation conv;
        conv.conv_id = conv_id;
        conv.conv_type = (conv_type == "group") ? ConversationType::Group : ConversationType::Private;
        conv.target_id = target_id;
        conv.last_msg_text = last_msg_txt;
        conv.last_msg_time_ms = last_msg_time;
        conv.unread_count = unread_count;
        conv.is_pinned = is_pinned;
        conv.is_muted = is_muted;
        conv.updated_at_ms = now;

        conv_cache_->upsert(std::move(conv));
    }
}

// ---------------------------------------------------------------------------
// mergeConvMessages()
// ---------------------------------------------------------------------------

void SyncEngine::mergeConvMessages(const nlohmann::json& conversations_arr) {
    // Each element:
    //   { "conversationId": "...", "conversationType": "private",
    //     "messages": [ { messageId, senderId, contentType, content,
    //                      sequence, replyTo, status, timestamp } ... ],
    //     "hasMore": false }
    for (const auto& conv_obj : conversations_arr) {
        if (!conv_obj.is_object())
            continue;

        std::string conv_id = jsonGet<std::string>(conv_obj, "conversationId", "");
        if (conv_id.empty())
            continue;

        auto msgs_it = conv_obj.find("messages");
        if (msgs_it == conv_obj.end() || !msgs_it->is_array())
            continue;

        int64_t max_seq_seen = 0;

        for (const auto& m : *msgs_it) {
            if (!m.is_object())
                continue;

            std::string message_id = jsonGet<std::string>(m, "messageId", "");
            std::string sender_id = jsonGet<std::string>(m, "senderId", "");
            std::string content_type = jsonGet<std::string>(m, "contentType", "text");
            std::string content = jsonGet<std::string>(m, "content", "");
            int64_t seq = jsonGet<int64_t>(m, "sequence", 0LL);
            std::string reply_to = jsonGet<std::string>(m, "replyTo", "");
            int32_t status = jsonGet<int32_t>(m, "status", 0);
            int64_t timestamp_ms = jsonGet<int64_t>(m, "timestamp", 0LL);
            // Server may send seconds; convert to ms if plausibly in seconds.
            if (timestamp_ms > 0 && timestamp_ms < 1e12) {
                timestamp_ms *= 1000;
            }
            std::string local_id = jsonGet<std::string>(m, "localId", "");

            if (message_id.empty())
                continue;

            // Insert into the messages DB table (ignore if already present).
            db_->execSync(
                "INSERT OR IGNORE INTO messages "
                "  (message_id, conv_id, sender_id, content_type, content, "
                "   seq, reply_to, status, send_state, timestamp_ms, local_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                { message_id,
                  conv_id,
                  sender_id,
                  content_type,
                  content,
                  seq,
                  reply_to,
                  static_cast<int64_t>(status),
                  static_cast<int64_t>(1), // send_state=1 (sent, from server)
                  timestamp_ms,
                  local_id }
            );

            // Insert into the in-memory message cache.
            Message msg;
            msg.message_id = message_id;
            msg.local_id = local_id;
            msg.conv_id = conv_id;
            msg.sender_id = sender_id;
            msg.content_type = content_type;
            msg.content = content;
            msg.seq = seq;
            msg.reply_to = reply_to;
            msg.status = status;
            msg.send_state = 1; // received from server
            msg.timestamp_ms = timestamp_ms;
            msg_cache_->insert(msg);

            if (seq > max_seq_seen)
                max_seq_seen = seq;
        }

        // Update local_seq in the conversations table so the next sync
        // can request only newer messages.
        if (max_seq_seen > 0) {
            db_->execSync(
                "UPDATE conversations SET local_seq = MAX(local_seq, ?) "
                "WHERE conv_id = ?",
                { max_seq_seen, conv_id }
            );

            // Also update the cache entry's local_seq.
            auto cached = conv_cache_->get(conv_id);
            if (cached.has_value() && max_seq_seen > cached->local_seq) {
                cached->local_seq = max_seq_seen;
                conv_cache_->upsert(std::move(*cached));
            }
        }
    }
}

} // namespace anychat
