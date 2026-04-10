#include "conversation_manager.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

const nlohmann::json* findField(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

std::string toLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string jsonToString(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<int64_t>());
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<uint64_t>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    return "";
}

int64_t jsonToInt64(const nlohmann::json& value, int64_t def = 0) {
    try {
        if (value.is_number_integer()) {
            return value.get<int64_t>();
        }
        if (value.is_number_unsigned()) {
            return static_cast<int64_t>(value.get<uint64_t>());
        }
        if (value.is_number_float()) {
            return static_cast<int64_t>(value.get<double>());
        }
        if (value.is_string()) {
            const std::string s = value.get<std::string>();
            if (!s.empty()) {
                size_t idx = 0;
                const int64_t out = std::stoll(s, &idx);
                if (idx == s.size()) {
                    return out;
                }
            }
        }
    } catch (...) {
        return def;
    }
    return def;
}

bool jsonToBool(const nlohmann::json& value, bool def = false) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        return jsonToInt64(value, 0) != 0;
    }
    if (value.is_string()) {
        const std::string s = toLower(value.get<std::string>());
        if (s == "true" || s == "1" || s == "yes" || s == "on") {
            return true;
        }
        if (s == "false" || s == "0" || s == "no" || s == "off") {
            return false;
        }
    }
    return def;
}

std::string getString(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    return value != nullptr ? jsonToString(*value) : "";
}

int64_t getInt64(const nlohmann::json& obj, std::initializer_list<const char*> keys, int64_t def = 0) {
    const auto* value = findField(obj, keys);
    return value != nullptr ? jsonToInt64(*value, def) : def;
}

int32_t getInt32(const nlohmann::json& obj, std::initializer_list<const char*> keys, int32_t def = 0) {
    return static_cast<int32_t>(getInt64(obj, keys, def));
}

bool getBool(const nlohmann::json& obj, std::initializer_list<const char*> keys, bool def = false) {
    const auto* value = findField(obj, keys);
    return value != nullptr ? jsonToBool(*value, def) : def;
}

// Server-side timestamps may be in seconds or milliseconds. Keep milliseconds internally.
int64_t normalizeEpochMs(int64_t raw) {
    if (raw == 0) {
        return 0;
    }
    return std::llabs(raw) >= 100000000000LL ? raw : raw * 1000;
}

int64_t parseTimestampMs(const nlohmann::json& value) {
    if (value.is_object()) {
        const int64_t secs = getInt64(value, { "seconds", "Seconds" }, 0);
        const int64_t nanos = getInt64(value, { "nanos", "Nanos" }, 0);
        if (secs != 0 || nanos != 0) {
            return secs * 1000 + nanos / 1000000;
        }
        return 0;
    }
    return normalizeEpochMs(jsonToInt64(value, 0));
}

int64_t getTimestampMs(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    return value != nullptr ? parseTimestampMs(*value) : 0;
}

void applyConversationPatch(const nlohmann::json& j, Conversation& c) {
    if (const auto* value = findField(j, { "conversationId", "conversation_id", "sessionId", "session_id", "convId", "conv_id" })) {
        const std::string conv_id = jsonToString(*value);
        if (!conv_id.empty()) {
            c.conv_id = conv_id;
        }
    }

    if (const auto* value = findField(j, { "conversationType", "conversation_type", "sessionType", "session_type", "convType", "conv_type" })) {
        const std::string t = toLower(jsonToString(*value));
        if (!t.empty()) {
            c.conv_type = (t == "group") ? ConversationType::Group : ConversationType::Private;
        }
    }

    if (const auto* value = findField(j, { "targetId", "target_id" })) {
        c.target_id = jsonToString(*value);
    }
    if (const auto* value = findField(j, { "lastMessageId", "last_message_id", "lastMsgId", "last_msg_id" })) {
        c.last_msg_id = jsonToString(*value);
    }
    if (const auto* value = findField(
            j,
            { "lastMessageContent", "last_message_content", "lastMessageText", "last_message_text", "lastMsgText", "last_msg_text" }
        )) {
        c.last_msg_text = jsonToString(*value);
    }
    if (const auto* value = findField(j, { "lastMessageTime", "last_message_time", "lastMsgTime", "last_msg_time" })) {
        c.last_msg_time_ms = parseTimestampMs(*value);
    }
    if (const auto* value = findField(j, { "unreadCount", "unread_count" })) {
        c.unread_count = static_cast<int32_t>(jsonToInt64(*value, 0));
    }
    if (const auto* value = findField(j, { "isPinned", "is_pinned" })) {
        c.is_pinned = jsonToBool(*value, false);
    }
    if (const auto* value = findField(j, { "isMuted", "is_muted" })) {
        c.is_muted = jsonToBool(*value, false);
    }
    if (const auto* value = findField(j, { "burnAfterReading", "burn_after_reading" })) {
        c.burn_after_reading = static_cast<int32_t>(jsonToInt64(*value, 0));
    }
    if (const auto* value = findField(j, { "autoDeleteDuration", "auto_delete_duration" })) {
        c.auto_delete_duration = static_cast<int32_t>(jsonToInt64(*value, 0));
    }
    if (const auto* value = findField(j, { "pinTime", "pin_time" })) {
        c.pin_time_ms = parseTimestampMs(*value);
    }
    if (const auto* value = findField(j, { "localSeq", "local_seq" })) {
        c.local_seq = jsonToInt64(*value, c.local_seq);
    }
    if (const auto* value = findField(j, { "updatedAt", "updated_at" })) {
        c.updated_at_ms = parseTimestampMs(*value);
    }
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool parseResponseRoot(const network::HttpResponse& resp, nlohmann::json& root, std::string& err) {
    if (!resp.error.empty()) {
        err = resp.error;
        return false;
    }
    if (resp.status_code != 200) {
        err = "HTTP " + std::to_string(resp.status_code);
        return false;
    }
    try {
        root = nlohmann::json::parse(resp.body);
    } catch (const std::exception& ex) {
        err = std::string("parse error: ") + ex.what();
        return false;
    }

    if (root.value("code", -1) != 0) {
        err = root.value("message", "server error");
        return false;
    }
    return true;
}

const nlohmann::json* getDataNode(const nlohmann::json& root) {
    auto it = root.find("data");
    if (it == root.end()) {
        return nullptr;
    }
    return &(*it);
}

const nlohmann::json* pickArray(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end() && it->is_array()) {
            return &(*it);
        }
    }
    return nullptr;
}

bool isConversationNotification(const std::string& notification_type) {
    return notification_type == "session.unread_updated" || notification_type == "session.pin_updated"
        || notification_type == "session.mute_updated" || notification_type == "session.deleted"
        || notification_type == "conversation.unread_updated" || notification_type == "conversation.pin_updated"
        || notification_type == "conversation.mute_updated" || notification_type == "conversation.deleted"
        || notification_type == "conversation.burn_updated"
        || notification_type == "conversation.auto_delete_updated";
}

} // namespace

// ---------------------------------------------------------------------------
// Helper: parse a server conversation JSON object into a Conversation struct
// ---------------------------------------------------------------------------

/*static*/ Conversation ConversationManagerImpl::parseConversation(const nlohmann::json& j) {
    Conversation c;
    applyConversationPatch(j, c);
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
    if (c.last_msg_time_ms == 0) {
        c.last_msg_time_ms = normalizeEpochMs(getI("last_msg_time"));
    }
    c.unread_count = static_cast<int32_t>(getI("unread_count"));
    c.is_pinned = (getI("is_pinned") != 0);
    c.is_muted = (getI("is_muted") != 0);
    c.burn_after_reading = static_cast<int32_t>(getI("burn_after_reading"));
    c.auto_delete_duration = static_cast<int32_t>(getI("auto_delete_duration"));
    c.pin_time_ms = getI("pin_time_ms");
    if (c.pin_time_ms == 0) {
        c.pin_time_ms = normalizeEpochMs(getI("pin_time"));
    }
    c.local_seq = getI("local_seq");
    c.updated_at_ms = getI("updated_at_ms");
    if (c.updated_at_ms == 0) {
        c.updated_at_ms = normalizeEpochMs(getI("updated_at"));
    }
    return c;
}

// ---------------------------------------------------------------------------
// Helper: upsert a conversation into the DB
// ---------------------------------------------------------------------------

void ConversationManagerImpl::upsertDb(const Conversation& c) {
    const std::string sql = "INSERT INTO conversations "
                            "(conv_id, conv_type, target_id, last_msg_id, last_msg_text, "
                            " last_msg_time_ms, unread_count, is_pinned, is_muted, "
                            " burn_after_reading, auto_delete_duration, pin_time_ms, "
                            " local_seq, updated_at_ms) "
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                            "ON CONFLICT(conv_id) DO UPDATE SET "
                            " conv_type=excluded.conv_type, target_id=excluded.target_id, "
                            " last_msg_id=excluded.last_msg_id, last_msg_text=excluded.last_msg_text, "
                            " last_msg_time_ms=excluded.last_msg_time_ms, "
                            " unread_count=excluded.unread_count, is_pinned=excluded.is_pinned, "
                            " is_muted=excluded.is_muted, "
                            " burn_after_reading=excluded.burn_after_reading, "
                            " auto_delete_duration=excluded.auto_delete_duration, "
                            " pin_time_ms=excluded.pin_time_ms, "
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
          static_cast<int64_t>(c.burn_after_reading),
          static_cast<int64_t>(c.auto_delete_duration),
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
    notif_mgr_->addNotificationHandler([this](const NotificationEvent& event) {
        if (isConversationNotification(event.notification_type)) {
            handleConversationNotification(event);
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

    // Fall back to DB snapshot before HTTP.
    db::Rows rows = db_->querySync(
        "SELECT conv_id, conv_type, target_id, last_msg_id, last_msg_text, "
        "       last_msg_time_ms, unread_count, is_pinned, is_muted, "
        "       burn_after_reading, auto_delete_duration, pin_time_ms, local_seq, updated_at_ms "
        "FROM conversations "
        "ORDER BY is_pinned DESC, pin_time_ms DESC, last_msg_time_ms DESC",
        {}
    );

    if (!rows.empty()) {
        std::vector<Conversation> from_db;
        from_db.reserve(rows.size());
        for (const auto& row : rows) {
            from_db.push_back(rowToConversation(row));
        }
        conv_cache_->setAll(from_db);
        cb(std::move(from_db), "");
        return;
    }

    // Fall back to HTTP.
    http_->get("/conversations", [this, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        std::vector<Conversation> convs;
        const nlohmann::json* data = getDataNode(root);
        const nlohmann::json* arr = nullptr;

        if (data != nullptr) {
            if (data->is_array()) {
                arr = data;
            } else {
                arr = pickArray(*data, { "conversations", "sessions", "list", "items" });
            }
        }

        if (arr != nullptr) {
            convs.reserve(arr->size());
            for (const auto& item : *arr) {
                if (!item.is_object()) {
                    continue;
                }
                Conversation c = parseConversation(item);
                if (c.conv_id.empty()) {
                    continue;
                }
                if (c.updated_at_ms == 0) {
                    c.updated_at_ms = nowMs();
                }
                upsertDb(c);
                convs.push_back(c);
            }
        }

        conv_cache_->setAll(convs);
        cb(conv_cache_->getAll(), "");
    });
}

// ---------------------------------------------------------------------------
// getTotalUnread
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getTotalUnread(ConversationTotalUnreadCallback cb) {
    http_->get("/conversations/unread/total", [cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(0, err);
            return;
        }

        int32_t total = 0;
        const nlohmann::json* data = getDataNode(root);
        if (data != nullptr) {
            if (data->is_number()) {
                total = static_cast<int32_t>(jsonToInt64(*data, 0));
            } else {
                total = getInt32(*data, { "totalUnread", "total_unread", "total" }, 0);
            }
        }

        cb(total, "");
    });
}

// ---------------------------------------------------------------------------
// getConversation
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getConversation(const std::string& conv_id, ConversationDetailCallback cb) {
    const std::string path = "/conversations/" + conv_id;
    http_->get(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        const nlohmann::json* data = getDataNode(root);
        if (data == nullptr || !data->is_object()) {
            cb({}, "invalid response data");
            return;
        }

        Conversation conv = parseConversation(*data);
        if (conv.conv_id.empty()) {
            conv.conv_id = conv_id;
        }
        if (conv.updated_at_ms == 0) {
            conv.updated_at_ms = nowMs();
        }

        conv_cache_->upsert(conv);
        upsertDb(conv);
        cb(conv, "");
    });
}

// ---------------------------------------------------------------------------
// markAllRead
// ---------------------------------------------------------------------------

void ConversationManagerImpl::markAllRead(const std::string& conv_id, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/read-all";
    http_->post(path, "", [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        conv_cache_->clearUnread(conv_id);
        db_->exec("UPDATE conversations SET unread_count=0 WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// markMessagesRead
// ---------------------------------------------------------------------------

void ConversationManagerImpl::markMessagesRead(
    const std::string& conv_id,
    const std::vector<std::string>& message_ids,
    ConversationMarkReadResultCallback cb
) {
    if (message_ids.empty()) {
        cb({}, "message_ids is empty");
        return;
    }

    nlohmann::json body;
    body["message_ids"] = message_ids;

    const std::string path = "/conversations/" + conv_id + "/messages/read";
    http_->post(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        ConversationMarkReadResult result;
        const nlohmann::json* data = getDataNode(root);
        if (data != nullptr && data->is_object()) {
            if (const auto* accepted = pickArray(*data, { "acceptedIds", "accepted_ids" })) {
                for (const auto& item : *accepted) {
                    const std::string id = jsonToString(item);
                    if (!id.empty()) {
                        result.accepted_ids.push_back(id);
                    }
                }
            }
            if (const auto* ignored = pickArray(*data, { "ignoredIds", "ignored_ids" })) {
                for (const auto& item : *ignored) {
                    const std::string id = jsonToString(item);
                    if (!id.empty()) {
                        result.ignored_ids.push_back(id);
                    }
                }
            }
            result.advanced_last_read_seq =
                getInt64(*data, { "advancedLastReadSeq", "advanced_last_read_seq" }, 0);
        }

        cb(std::move(result), "");
    });
}

// ---------------------------------------------------------------------------
// setPinned
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/pin";
    nlohmann::json body_j = { { "pinned", pinned } };
    http_->put(path, body_j.dump(), [this, conv_id, pinned, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.is_pinned = pinned;
            c.pin_time_ms = pinned ? nowMs() : 0;
            c.updated_at_ms = nowMs();
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
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.is_muted = muted;
            c.updated_at_ms = nowMs();
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// setBurnAfterReading
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setBurnAfterReading(const std::string& conv_id, int32_t duration, ConversationCallback cb)
{
    const std::string path = "/conversations/" + conv_id + "/burn";
    nlohmann::json body_j = { { "duration", duration } };
    http_->put(path, body_j.dump(), [this, conv_id, duration, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.burn_after_reading = duration;
            c.updated_at_ms = nowMs();
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// setAutoDelete
// ---------------------------------------------------------------------------

void ConversationManagerImpl::setAutoDelete(const std::string& conv_id, int32_t duration, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/auto_delete";
    nlohmann::json body_j = { { "duration", duration } };
    http_->put(path, body_j.dump(), [this, conv_id, duration, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.auto_delete_duration = duration;
            c.updated_at_ms = nowMs();
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
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(false, err);
            return;
        }

        conv_cache_->remove(conv_id);
        db_->exec("DELETE FROM conversations WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

// ---------------------------------------------------------------------------
// getMessageUnreadCount
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getMessageUnreadCount(
    const std::string& conv_id,
    int64_t last_read_seq,
    ConversationUnreadStateCallback cb
) {
    std::string path = "/conversations/" + conv_id + "/messages/unread-count";
    if (last_read_seq >= 0) {
        path += "?last_read_seq=" + std::to_string(last_read_seq);
    }

    http_->get(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        ConversationUnreadState state;
        const nlohmann::json* data = getDataNode(root);
        if (data != nullptr) {
            state.unread_count = getInt64(*data, { "unreadCount", "unread_count" }, 0);
            state.last_message_seq = getInt64(*data, { "lastMessageSeq", "last_message_seq" }, 0);

            const auto* last_msg = findField(*data, { "lastMessage", "last_message" });
            if (last_msg != nullptr && last_msg->is_object()) {
                state.has_last_message = true;
                Message msg;
                msg.message_id = getString(*last_msg, { "messageId", "message_id" });
                msg.conv_id = getString(*last_msg, { "conversationId", "conversation_id" });
                if (msg.conv_id.empty()) {
                    msg.conv_id = conv_id;
                }
                msg.sender_id = getString(*last_msg, { "senderId", "sender_id" });
                msg.content_type = getString(*last_msg, { "contentType", "content_type" });
                if (const auto* content_val = findField(*last_msg, { "content" }); content_val != nullptr) {
                    msg.content = content_val->is_string() ? content_val->get<std::string>() : content_val->dump();
                }
                msg.seq = getInt64(*last_msg, { "sequence", "seq" }, 0);
                msg.timestamp_ms = getTimestampMs(*last_msg, { "timestamp", "createdAt", "created_at" });
                msg.status = getInt32(*last_msg, { "status" }, 0);
                state.last_message = std::move(msg);
            }
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            c.unread_count = static_cast<int32_t>(state.unread_count);
            conv_cache_->upsert(c);
            upsertDb(c);
        }

        cb(state, "");
    });
}

// ---------------------------------------------------------------------------
// getMessageReadReceipts
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getMessageReadReceipts(const std::string& conv_id, ConversationReadReceiptListCallback cb)
{
    const std::string path = "/conversations/" + conv_id + "/messages/read-receipts";
    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        std::vector<ConversationReadReceipt> receipts;
        const nlohmann::json* data = getDataNode(root);
        if (data != nullptr && data->is_object()) {
            if (const auto* arr = pickArray(*data, { "receipts", "list", "items" })) {
                receipts.reserve(arr->size());
                for (const auto& item : *arr) {
                    if (!item.is_object()) {
                        continue;
                    }

                    ConversationReadReceipt receipt;
                    receipt.user_id = getString(item, { "userId", "user_id" });
                    receipt.last_read_seq = getInt64(item, { "lastReadSeq", "last_read_seq" }, 0);
                    receipt.last_read_message_id =
                        getString(item, { "lastReadMessageId", "last_read_message_id" });
                    receipt.read_at_ms = getTimestampMs(item, { "readAt", "read_at" });

                    const auto* user_info = findField(item, { "userInfo", "user_info" });
                    if (user_info != nullptr && user_info->is_object()) {
                        receipt.user_info.user_id = getString(*user_info, { "userId", "user_id" });
                        receipt.user_info.username = getString(*user_info, { "username", "nickname" });
                        receipt.user_info.avatar_url = getString(*user_info, { "avatar", "avatarUrl", "avatar_url" });
                        receipt.user_info.signature = getString(*user_info, { "signature" });
                        receipt.user_info.gender = getInt32(*user_info, { "gender" }, 0);
                        receipt.user_info.region = getString(*user_info, { "region" });
                    }

                    receipts.push_back(std::move(receipt));
                }
            }
        }

        cb(std::move(receipts), "");
    });
}

// ---------------------------------------------------------------------------
// getMessageSequence
// ---------------------------------------------------------------------------

void ConversationManagerImpl::getMessageSequence(const std::string& conv_id, ConversationSequenceCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/messages/sequence";
    http_->get(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseResponseRoot(resp, root, err)) {
            cb(0, err);
            return;
        }

        int64_t seq = 0;
        const nlohmann::json* data = getDataNode(root);
        if (data != nullptr) {
            if (data->is_number()) {
                seq = jsonToInt64(*data, 0);
            } else {
                seq = getInt64(*data, { "currentSeq", "current_seq", "sequence" }, 0);
            }
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt) {
            Conversation c = *opt;
            if (seq > c.local_seq) {
                c.local_seq = seq;
                conv_cache_->upsert(c);
                upsertDb(c);
            }
        } else if (seq > 0) {
            db_->exec("UPDATE conversations SET local_seq=MAX(local_seq, ?) WHERE conv_id=?", { seq, conv_id });
        }

        cb(seq, "");
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
// handleConversationNotification  (private)
// ---------------------------------------------------------------------------

void ConversationManagerImpl::handleConversationNotification(const NotificationEvent& event) {
    try {
        const auto& d = event.data;
        const auto& nt = event.notification_type;

        const std::string conv_id =
            getString(d, { "conversationId", "conversation_id", "sessionId", "session_id", "convId", "conv_id" });
        if (conv_id.empty()) {
            return;
        }

        const bool is_deleted = (nt == "session.deleted" || nt == "conversation.deleted");
        if (is_deleted) {
            Conversation removed;
            removed.conv_id = conv_id;
            if (auto existing = conv_cache_->get(conv_id)) {
                removed = *existing;
            }
            conv_cache_->remove(conv_id);
            db_->exec("DELETE FROM conversations WHERE conv_id=?", { conv_id });

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

        Conversation conv;
        if (auto existing = conv_cache_->get(conv_id)) {
            conv = *existing;
        }
        conv.conv_id = conv_id;
        applyConversationPatch(d, conv);

        if (nt == "session.pin_updated" || nt == "conversation.pin_updated") {
            const bool pinned = getBool(d, { "isPinned", "is_pinned" }, conv.is_pinned);
            if (!pinned) {
                conv.pin_time_ms = 0;
            } else if (conv.pin_time_ms == 0) {
                conv.pin_time_ms = nowMs();
            }
        }

        if (conv.updated_at_ms == 0) {
            conv.updated_at_ms = nowMs();
        }

        conv_cache_->upsert(conv);
        upsertDb(conv);

        OnConversationUpdated handler;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            handler = on_updated_;
        }
        if (handler) {
            handler(conv);
        }
    } catch (const std::exception&) {
        // Malformed notification — silently ignore.
    }
}

} // namespace anychat
