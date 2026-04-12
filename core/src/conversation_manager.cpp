#include "conversation_manager.h"

#include "json_common.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat::conversation_manager_detail {

using json_common::ApiEnvelope;
using json_common::nowMs;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusSuccessResponse;
using json_common::parseBoolValue;
using json_common::parseInt32Value;
using json_common::parseInt64Value;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::toLower;
using json_common::writeJson;

struct MarkMessagesReadRequest {
    std::vector<std::string> message_ids{};
};

using MessageContentValue = std::variant<std::string, glz::raw_json>;

struct ConversationPayload {
    std::string conversation_id{};
    std::string conversation_type{};
    std::string target_id{};
    std::string last_message_id{};
    std::string last_message_content{};
    json_common::OptionalTimestampValue last_message_time{};
    std::optional<int32_t> unread_count{};
    std::optional<bool> is_pinned{};
    std::optional<bool> is_muted{};
    std::optional<int32_t> burn_after_reading{};
    std::optional<int32_t> auto_delete_duration{};
    json_common::OptionalTimestampValue pin_time{};
    json_common::OptionalTimestampValue updated_at{};
};

struct ConversationListDataPayload {
    std::optional<std::vector<ConversationPayload>> conversations{};
    std::optional<bool> has_more{};
};

struct TotalUnreadDataPayload {
    std::optional<int32_t> total_unread{};
};

struct MarkMessagesReadPayload {
    std::optional<std::vector<std::string>> accepted_ids{};
    std::optional<std::vector<std::string>> ignored_ids{};
    std::optional<int64_t> advanced_last_read_seq{};
};

struct UnreadStateMessagePayload {
    std::string message_id{};
    std::string conversation_id{};
    std::string sender_id{};
    std::string content_type{};
    std::optional<MessageContentValue> content{};
    std::optional<int64_t> sequence{};
    json_common::OptionalTimestampValue timestamp{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<int32_t> status{};
};

struct ConversationUnreadStatePayload {
    std::optional<int64_t> unread_count{};
    std::optional<int64_t> last_message_seq{};
    std::optional<UnreadStateMessagePayload> last_message{};
};

struct ReceiptUserInfoPayload {
    std::string user_id{};
    std::string nickname{};
    std::string avatar{};
    std::optional<std::string> phone{};
    std::optional<std::string> email{};
};

struct ConversationReadReceiptPayload {
    std::string user_id{};
    std::optional<int64_t> last_read_seq{};
    std::string last_read_message_id{};
    json_common::OptionalTimestampValue read_at{};
    std::optional<ReceiptUserInfoPayload> user_info{};
};

struct ConversationReadReceiptListDataPayload {
    std::optional<std::vector<ConversationReadReceiptPayload>> receipts{};
};

struct MessageSequencePayload {
    std::optional<int64_t> current_seq{};
};

struct SetPinnedRequest {
    bool pinned = false;
};

struct SetMutedRequest {
    bool muted = false;
};

struct DurationRequest {
    int32_t duration = 0;
};

struct NotificationConversationPayload {
    std::optional<std::string> conversation_id{};
    std::optional<int32_t> unread_count{};
    std::optional<int32_t> total_unread{};
    std::optional<bool> is_pinned{};
    std::optional<bool> is_muted{};
    std::optional<int32_t> burn_after_reading{};
    std::optional<int32_t> auto_delete_duration{};
};

Conversation parseConversationPayload(const ConversationPayload& payload) {
    Conversation c;
    c.conv_id = payload.conversation_id;

    const std::string conv_type = toLower(payload.conversation_type);
    c.conv_type = (conv_type == "group") ? ConversationType::Group : ConversationType::Private;

    c.target_id = payload.target_id;
    c.last_msg_id = payload.last_message_id;
    c.last_msg_text = payload.last_message_content;
    c.last_msg_time_ms = parseTimestampMs(payload.last_message_time);
    c.unread_count = parseInt32Value(payload.unread_count, 0);
    c.is_pinned = parseBoolValue(payload.is_pinned, false);
    c.is_muted = parseBoolValue(payload.is_muted, false);
    c.burn_after_reading = parseInt32Value(payload.burn_after_reading, 0);
    c.auto_delete_duration = parseInt32Value(payload.auto_delete_duration, 0);
    c.pin_time_ms = parseTimestampMs(payload.pin_time);
    c.updated_at_ms = parseTimestampMs(payload.updated_at);
    return c;
}

const std::vector<ConversationPayload>* toConversationPayloadList(const ConversationListDataPayload& data) {
    return data.conversations.has_value() ? &(*data.conversations) : nullptr;
}

int64_t parseMessageSequenceNumber(const MessageSequencePayload& data, int64_t fallback = 0) {
    return parseInt64Value(data.current_seq, fallback);
}

int64_t parseTotalUnreadNumber(const TotalUnreadDataPayload& data, int64_t fallback = 0) {
    return parseInt64Value(data.total_unread, fallback);
}

std::string parseMessageContent(const std::optional<MessageContentValue>& value) {
    if (!value.has_value()) {
        return "";
    }
    if (const auto* text = std::get_if<std::string>(&*value); text != nullptr) {
        return *text;
    }

    const auto& raw = std::get<glz::raw_json>(*value).str;
    if (raw.empty()) {
        return "";
    }
    const auto parsed = glz::read_json<std::string>(raw);
    return parsed ? *parsed : raw;
}

Message parseUnreadStateMessage(const UnreadStateMessagePayload& payload, const std::string& conv_id) {
    Message msg;
    msg.message_id = payload.message_id;
    msg.conv_id = payload.conversation_id.empty() ? conv_id : payload.conversation_id;
    msg.sender_id = payload.sender_id;
    msg.content_type = payload.content_type;
    msg.content = parseMessageContent(payload.content);
    msg.seq = parseInt64Value(payload.sequence, 0);
    msg.timestamp_ms = payload.timestamp.has_value() ? parseTimestampMs(payload.timestamp)
                                                     : parseTimestampMs(payload.created_at);
    msg.status = static_cast<int32_t>(parseInt64Value(payload.status, 0));
    return msg;
}

UserInfo parseReceiptUserInfo(const ReceiptUserInfoPayload& payload) {
    UserInfo info;
    info.user_id = payload.user_id;
    info.username = payload.nickname;
    info.avatar_url = payload.avatar;
    return info;
}

ConversationReadReceipt parseReadReceipt(const ConversationReadReceiptPayload& payload) {
    ConversationReadReceipt receipt;
    receipt.user_id = payload.user_id;
    receipt.last_read_seq = parseInt64Value(payload.last_read_seq, 0);
    receipt.last_read_message_id = payload.last_read_message_id;
    receipt.read_at_ms = parseTimestampMs(payload.read_at);
    if (payload.user_info.has_value()) {
        receipt.user_info = parseReceiptUserInfo(*payload.user_info);
    }
    if (receipt.user_info.user_id.empty()) {
        receipt.user_info.user_id = receipt.user_id;
    }
    return receipt;
}

const std::vector<ConversationReadReceiptPayload>* toReadReceiptPayloadList(const ConversationReadReceiptListDataPayload& data) {
    return data.receipts.has_value() ? &(*data.receipts) : nullptr;
}

std::string notificationConversationId(const NotificationConversationPayload& payload) {
    return (payload.conversation_id.has_value() && !payload.conversation_id->empty()) ? *payload.conversation_id : "";
}

void applyNotificationPatch(const NotificationConversationPayload& payload, Conversation& conv) {
    const std::string conv_id = notificationConversationId(payload);
    if (!conv_id.empty()) {
        conv.conv_id = conv_id;
    }

    if (payload.unread_count.has_value()) {
        conv.unread_count = *payload.unread_count;
    }
    if (payload.is_pinned.has_value()) {
        conv.is_pinned = *payload.is_pinned;
    }
    if (payload.is_muted.has_value()) {
        conv.is_muted = *payload.is_muted;
    }
    if (payload.burn_after_reading.has_value()) {
        conv.burn_after_reading = *payload.burn_after_reading;
    }
    if (payload.auto_delete_duration.has_value()) {
        conv.auto_delete_duration = *payload.auto_delete_duration;
    }
}

bool isConversationNotification(const std::string& notification_type) {
    return notification_type == "conversation.unread_updated" || notification_type == "conversation.pin_updated"
        || notification_type == "conversation.mute_updated" || notification_type == "conversation.deleted"
        || notification_type == "conversation.burn_updated"
        || notification_type == "conversation.auto_delete_updated";
}

} // namespace anychat::conversation_manager_detail

namespace anychat {
using namespace conversation_manager_detail;


/*static*/ Conversation ConversationManagerImpl::rowToConversation(const db::Row& row) {
    auto get = [&](const std::string& k, const std::string& def = "") -> std::string {
        const auto it = row.find(k);
        return (it != row.end()) ? it->second : def;
    };
    auto getI = [&](const std::string& k) -> int64_t {
        const auto it = row.find(k);
        if (it == row.end() || it->second.empty()) {
            return 0;
        }
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
        c.last_msg_time_ms = json_common::normalizeUnixEpochMs(getI("last_msg_time"));
    }
    c.unread_count = static_cast<int32_t>(getI("unread_count"));
    c.is_pinned = (getI("is_pinned") != 0);
    c.is_muted = (getI("is_muted") != 0);
    c.burn_after_reading = static_cast<int32_t>(getI("burn_after_reading"));
    c.auto_delete_duration = static_cast<int32_t>(getI("auto_delete_duration"));
    c.pin_time_ms = getI("pin_time_ms");
    if (c.pin_time_ms == 0) {
        c.pin_time_ms = json_common::normalizeUnixEpochMs(getI("pin_time"));
    }
    c.local_seq = getI("local_seq");
    c.updated_at_ms = getI("updated_at_ms");
    if (c.updated_at_ms == 0) {
        c.updated_at_ms = json_common::normalizeUnixEpochMs(getI("updated_at"));
    }
    return c;
}

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

    const std::string conv_type_str = (c.conv_type == ConversationType::Group) ? "group" : "private";

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

void ConversationManagerImpl::getList(ConversationListCallback cb) {
    auto cached = conv_cache_->getAll();
    if (!cached.empty()) {
        cb(std::move(cached), "");
        return;
    }

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

    http_->get("/conversations", [this, cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<ConversationListDataPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get conversations failed", true)) {
            cb({}, err);
            return;
        }

        std::vector<Conversation> convs;
        const auto* payloads = toConversationPayloadList(root.data);
        if (payloads != nullptr) {
            convs.reserve(payloads->size());
            for (const auto& payload : *payloads) {
                Conversation c = parseConversationPayload(payload);
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

void ConversationManagerImpl::getTotalUnread(ConversationTotalUnreadCallback cb) {
    http_->get("/conversations/unread/total", [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<TotalUnreadDataPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get total unread failed", true)) {
            cb(0, err);
            return;
        }

        const int32_t total = static_cast<int32_t>(parseTotalUnreadNumber(root.data, 0));
        cb(total, "");
    });
}

void ConversationManagerImpl::getConversation(const std::string& conv_id, ConversationDetailCallback cb) {
    const std::string path = "/conversations/" + conv_id;
    http_->get(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        ApiEnvelope<ConversationPayload> root{};
        if (!parseApiEnvelopeResponse(resp, root, err, "get conversation failed", true)) {
            cb({}, err);
            return;
        }
        Conversation conv = parseConversationPayload(root.data);

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

void ConversationManagerImpl::markAllRead(const std::string& conv_id, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/read-all";
    http_->post(path, "", [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "mark read failed", true)) {
            cb(false, err);
            return;
        }

        conv_cache_->clearUnread(conv_id);
        db_->exec("UPDATE conversations SET unread_count=0 WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

void ConversationManagerImpl::markMessagesRead(
    const std::string& conv_id,
    const std::vector<std::string>& message_ids,
    ConversationMarkReadResultCallback cb
) {
    if (message_ids.empty()) {
        cb({}, "message_ids is empty");
        return;
    }

    const MarkMessagesReadRequest body{.message_ids = message_ids};
    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        cb({}, err);
        return;
    }

    const std::string path = "/conversations/" + conv_id + "/messages/read";
    http_->post(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<MarkMessagesReadPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "mark messages read failed", true)) {
            cb({}, err);
            return;
        }

        ConversationMarkReadResult result;
        if (root.data.accepted_ids.has_value()) {
            for (const auto& id : *root.data.accepted_ids) {
                if (!id.empty()) {
                    result.accepted_ids.push_back(id);
                }
            }
        }
        if (root.data.ignored_ids.has_value()) {
            for (const auto& id : *root.data.ignored_ids) {
                if (!id.empty()) {
                    result.ignored_ids.push_back(id);
                }
            }
        }
        result.advanced_last_read_seq = parseInt64Value(root.data.advanced_last_read_seq, 0);

        cb(std::move(result), "");
    });
}

void ConversationManagerImpl::setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) {
    const SetPinnedRequest body{.pinned = pinned};
    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        cb(false, err);
        return;
    }

    const std::string path = "/conversations/" + conv_id + "/pin";
    http_->put(path, body_json, [this, conv_id, pinned, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "set pinned failed", true)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
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

void ConversationManagerImpl::setMuted(const std::string& conv_id, bool muted, ConversationCallback cb) {
    const SetMutedRequest body{.muted = muted};
    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        cb(false, err);
        return;
    }

    const std::string path = "/conversations/" + conv_id + "/mute";
    http_->put(path, body_json, [this, conv_id, muted, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "set muted failed", true)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
            Conversation c = *opt;
            c.is_muted = muted;
            c.updated_at_ms = nowMs();
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

void ConversationManagerImpl::setBurnAfterReading(const std::string& conv_id, int32_t duration, ConversationCallback cb)
{
    const DurationRequest body{.duration = duration};
    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        cb(false, err);
        return;
    }

    const std::string path = "/conversations/" + conv_id + "/burn";
    http_->put(path, body_json, [this, conv_id, duration, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "set burn_after_reading failed", true)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
            Conversation c = *opt;
            c.burn_after_reading = duration;
            c.updated_at_ms = nowMs();
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

void ConversationManagerImpl::setAutoDelete(const std::string& conv_id, int32_t duration, ConversationCallback cb) {
    const DurationRequest body{.duration = duration};
    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        cb(false, err);
        return;
    }

    const std::string path = "/conversations/" + conv_id + "/auto_delete";
    http_->put(path, body_json, [this, conv_id, duration, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "set auto_delete failed", true)) {
            cb(false, err);
            return;
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
            Conversation c = *opt;
            c.auto_delete_duration = duration;
            c.updated_at_ms = nowMs();
            conv_cache_->upsert(c);
            upsertDb(c);
        }
        cb(true, "");
    });
}

void ConversationManagerImpl::deleteConv(const std::string& conv_id, ConversationCallback cb) {
    const std::string path = "/conversations/" + conv_id;
    http_->del(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        std::string err;
        if (!parseApiStatusSuccessResponse(resp, err, "delete conversation failed", true)) {
            cb(false, err);
            return;
        }

        conv_cache_->remove(conv_id);
        db_->exec("DELETE FROM conversations WHERE conv_id=?", { conv_id });
        cb(true, "");
    });
}

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
        ApiEnvelope<ConversationUnreadStatePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get unread count failed", true)) {
            cb({}, err);
            return;
        }

        ConversationUnreadState state;
        state.unread_count = parseInt64Value(root.data.unread_count, 0);
        state.last_message_seq = parseInt64Value(root.data.last_message_seq, 0);
        if (root.data.last_message.has_value()) {
            state.has_last_message = true;
            state.last_message = parseUnreadStateMessage(*root.data.last_message, conv_id);
        }

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
            Conversation c = *opt;
            c.unread_count = static_cast<int32_t>(state.unread_count);
            conv_cache_->upsert(c);
            upsertDb(c);
        }

        cb(state, "");
    });
}

void ConversationManagerImpl::getMessageReadReceipts(const std::string& conv_id, ConversationReadReceiptListCallback cb)
{
    const std::string path = "/conversations/" + conv_id + "/messages/read-receipts";
    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<ConversationReadReceiptListDataPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get message read receipts failed", true)) {
            cb({}, err);
            return;
        }

        std::vector<ConversationReadReceipt> receipts;
        const auto* payloads = toReadReceiptPayloadList(root.data);
        if (payloads != nullptr) {
            receipts.reserve(payloads->size());
            for (const auto& payload : *payloads) {
                receipts.push_back(parseReadReceipt(payload));
            }
        }

        cb(std::move(receipts), "");
    });
}

void ConversationManagerImpl::getMessageSequence(const std::string& conv_id, ConversationSequenceCallback cb) {
    const std::string path = "/conversations/" + conv_id + "/messages/sequence";
    http_->get(path, [this, conv_id, cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<MessageSequencePayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "get message sequence failed", true)) {
            cb(0, err);
            return;
        }

        const int64_t seq = parseMessageSequenceNumber(root.data, 0);

        auto opt = conv_cache_->get(conv_id);
        if (opt.has_value()) {
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

void ConversationManagerImpl::setListener(std::shared_ptr<ConversationListener> listener) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    listener_ = std::move(listener);
}

void ConversationManagerImpl::handleConversationNotification(const NotificationEvent& event) {
    try {
        NotificationConversationPayload payload{};
        std::string err;
        if (!parseJsonObject(event.data, payload, err)) {
            return;
        }

        const std::string conv_id = notificationConversationId(payload);
        if (conv_id.empty()) {
            return;
        }

        const bool is_deleted = (event.notification_type == "conversation.deleted");
        if (is_deleted) {
            Conversation removed;
            removed.conv_id = conv_id;
            if (auto existing = conv_cache_->get(conv_id); existing.has_value()) {
                removed = *existing;
            }

            conv_cache_->remove(conv_id);
            db_->exec("DELETE FROM conversations WHERE conv_id=?", { conv_id });

            std::shared_ptr<ConversationListener> listener;
            {
                std::lock_guard<std::mutex> lk(handler_mutex_);
                listener = listener_;
            }
            if (listener) {
                listener->onConversationUpdated(removed);
            }
            return;
        }

        Conversation conv;
        if (auto existing = conv_cache_->get(conv_id); existing.has_value()) {
            conv = *existing;
        }
        conv.conv_id = conv_id;
        applyNotificationPatch(payload, conv);

        if (event.notification_type == "conversation.pin_updated") {
            const bool pinned = payload.is_pinned.value_or(conv.is_pinned);
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

        std::shared_ptr<ConversationListener> listener;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            listener->onConversationUpdated(conv);
        }
    } catch (const std::exception&) {
    }
}

} // namespace anychat
