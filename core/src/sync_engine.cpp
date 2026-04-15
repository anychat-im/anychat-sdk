#include "sync_engine.h"

#include "json_common.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat::sync_engine_detail {

using json_common::ApiEnvelope;
using json_common::nowMs;
using json_common::parseBoolValue;
using json_common::parseInt64Value;
using json_common::parseTimestampMs;
using json_common::readJsonRelaxed;
using json_common::toLower;
using json_common::writeJson;

using IntegerValue = std::variant<int64_t, double, std::string>;
using OptionalIntegerValue = std::optional<IntegerValue>;
using BooleanValue = std::variant<bool, int64_t, double, std::string>;
using OptionalBooleanValue = std::optional<BooleanValue>;
using MessageContentValue = std::variant<std::string, glz::raw_json>;

struct ConversationSeqRequest {
    std::string conversation_id{};
    std::string conversation_type{};
    int64_t last_seq = 0;
};

struct SyncRequestPayload {
    int64_t last_sync_time = 0;
    std::vector<ConversationSeqRequest> conversation_seqs{};
};

struct FriendDeltaPayload {
    std::string user_id{};
    std::string remark{};
    OptionalIntegerValue updated_at{};
    OptionalBooleanValue is_deleted{};
};

struct FriendListPayload {
    std::optional<std::vector<FriendDeltaPayload>> friends{};
    OptionalIntegerValue total{};
};

struct GroupDeltaPayload {
    std::string group_id{};
    std::string name{};
    std::string avatar{};
    OptionalIntegerValue updated_at{};
    OptionalIntegerValue member_count{};
};

struct GroupListPayload {
    std::optional<std::vector<GroupDeltaPayload>> groups{};
    OptionalIntegerValue total{};
};

struct ConversationDeltaPayload {
    std::string conversation_id{};
    std::string conversation_type = "single";
    std::string target_id{};
    std::optional<MessageContentValue> last_message_content{};
    json_common::OptionalTimestampValue last_message_time{};
    OptionalIntegerValue unread_count{};
    OptionalBooleanValue is_pinned{};
    OptionalBooleanValue is_muted{};
};

struct ConversationDeltaListPayload {
    std::optional<std::vector<ConversationDeltaPayload>> conversations{};
    OptionalBooleanValue has_more{};
};

struct MessageDeltaPayload {
    std::string message_id{};
    std::string sender_id{};
    std::string content_type = "text";
    std::optional<MessageContentValue> content{};
    OptionalIntegerValue sequence{};
    std::string reply_to{};
    OptionalIntegerValue status{};
    json_common::OptionalTimestampValue created_at{};
};

struct ConversationMessagesPayload {
    std::string conversation_id{};
    std::string conversation_type = "private";
    std::optional<std::vector<MessageDeltaPayload>> messages{};
    OptionalBooleanValue has_more{};
};

struct SyncResponseDataPayload {
    std::optional<FriendListPayload> friends{};
    std::optional<GroupListPayload> groups{};
    std::optional<ConversationDeltaListPayload> conversation_data{};
    std::optional<std::vector<ConversationMessagesPayload>> conversations{};
    OptionalIntegerValue sync_time{};
};

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

const std::vector<FriendDeltaPayload>* toFriendPayloadList(const std::optional<FriendListPayload>& value) {
    return (value.has_value() && value->friends.has_value()) ? &(*value->friends) : nullptr;
}

const std::vector<GroupDeltaPayload>* toGroupPayloadList(const std::optional<GroupListPayload>& value) {
    return (value.has_value() && value->groups.has_value()) ? &(*value->groups) : nullptr;
}

const std::vector<ConversationDeltaPayload>*
toConversationDeltaPayloadList(const std::optional<ConversationDeltaListPayload>& value) {
    return (value.has_value() && value->conversations.has_value()) ? &(*value->conversations) : nullptr;
}

const std::vector<ConversationMessagesPayload>*
toConversationPayloadList(const std::optional<std::vector<ConversationMessagesPayload>>& value) {
    return value.has_value() ? &(*value) : nullptr;
}

void mergeFriends(db::Database* db, const std::vector<FriendDeltaPayload>& friends) {
    for (const auto& payload : friends) {
        if (payload.user_id.empty()) {
            continue;
        }

        db->execSync(
            "INSERT INTO friends (user_id, remark, updated_at_ms, is_deleted) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(user_id) DO UPDATE SET "
            "  remark       = excluded.remark, "
            "  updated_at_ms = excluded.updated_at_ms, "
            "  is_deleted   = excluded.is_deleted",
            { payload.user_id,
              payload.remark,
              parseInt64Value(payload.updated_at, 0),
              static_cast<int64_t>(parseBoolValue(payload.is_deleted, false) ? 1 : 0) }
        );
    }
}

void mergeGroups(db::Database* db, const std::vector<GroupDeltaPayload>& groups) {
    for (const auto& payload : groups) {
        if (payload.group_id.empty()) {
            continue;
        }

        db->execSync(
            "INSERT INTO groups (group_id, name, avatar_url, member_count, updated_at_ms) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(group_id) DO UPDATE SET "
            "  name          = excluded.name, "
            "  avatar_url    = excluded.avatar_url, "
            "  member_count  = excluded.member_count, "
            "  updated_at_ms = excluded.updated_at_ms",
            { payload.group_id,
              payload.name,
              payload.avatar,
              parseInt64Value(payload.member_count, 0),
              parseInt64Value(payload.updated_at, 0) }
        );
    }
}

void mergeSessions(
    db::Database* db,
    cache::ConversationCache* conv_cache,
    const std::vector<ConversationDeltaPayload>& sessions
) {
    for (const auto& payload : sessions) {
        if (payload.conversation_id.empty()) {
            continue;
        }

        const std::string conversation_type = toLower(payload.conversation_type);
        const std::string session_type = (conversation_type == "group") ? "group" : "private";
        std::string last_msg_text = parseMessageContent(payload.last_message_content);

        const int64_t last_msg_time = parseTimestampMs(payload.last_message_time);
        const int32_t unread_count = static_cast<int32_t>(parseInt64Value(payload.unread_count, 0));
        const bool is_pinned = parseBoolValue(payload.is_pinned, false);
        const bool is_muted = parseBoolValue(payload.is_muted, false);
        const int64_t now = nowMs();

        db->execSync(
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
            { payload.conversation_id,
              session_type,
              payload.target_id,
              last_msg_text,
              last_msg_time,
              static_cast<int64_t>(unread_count),
              static_cast<int64_t>(is_pinned ? 1 : 0),
              static_cast<int64_t>(is_muted ? 1 : 0),
              now }
        );

        Conversation conv;
        conv.conv_id = payload.conversation_id;
        conv.conv_type = (session_type == "group") ? ConversationType::Group : ConversationType::Private;
        conv.target_id = payload.target_id;
        conv.last_msg_text = last_msg_text;
        conv.last_msg_time_ms = last_msg_time;
        conv.unread_count = unread_count;
        conv.is_pinned = is_pinned;
        conv.is_muted = is_muted;
        conv.updated_at_ms = now;

        conv_cache->upsert(std::move(conv));
    }
}

void mergeConvMessages(
    db::Database* db,
    cache::ConversationCache* conv_cache,
    cache::MessageCache* msg_cache,
    const std::vector<ConversationMessagesPayload>& conversations
) {
    for (const auto& conv : conversations) {
        if (conv.conversation_id.empty()) {
            continue;
        }
        if (!conv.messages.has_value()) {
            continue;
        }

        const std::string& conv_id = conv.conversation_id;
        int64_t max_seq_seen = 0;

        for (const auto& payload : *conv.messages) {
            if (payload.message_id.empty()) {
                continue;
            }

            const int64_t sequence = parseInt64Value(payload.sequence, 0);
            const int64_t timestamp_ms = parseTimestampMs(payload.created_at);
            const std::string content = parseMessageContent(payload.content);
            const int status = static_cast<int>(parseInt64Value(payload.status, 0));

            db->execSync(
                "INSERT OR IGNORE INTO messages "
                "  (message_id, conv_id, sender_id, content_type, content, "
                "   seq, reply_to, status, send_state, timestamp_ms, local_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                { payload.message_id,
                  conv_id,
                  payload.sender_id,
                  payload.content_type,
                  content,
                  sequence,
                  payload.reply_to,
                  static_cast<int64_t>(status),
                  static_cast<int64_t>(1),
                  timestamp_ms,
                  std::string{} }
            );

            Message msg;
            msg.message_id = payload.message_id;
            msg.conv_id = conv_id;
            msg.sender_id = payload.sender_id;
            msg.content_type = payload.content_type;
            msg.content = content;
            msg.seq = sequence;
            msg.reply_to = payload.reply_to;
            msg.status = status;
            msg.send_state = 1;
            msg.timestamp_ms = timestamp_ms;
            msg_cache->insert(msg);

            if (sequence > max_seq_seen) {
                max_seq_seen = sequence;
            }
        }

        if (max_seq_seen > 0) {
            db->execSync(
                "UPDATE conversations SET local_seq = MAX(local_seq, ?) "
                "WHERE conv_id = ?",
                { max_seq_seen, conv_id }
            );

            auto cached = conv_cache->get(conv_id);
            if (cached.has_value() && max_seq_seen > cached->local_seq) {
                cached->local_seq = max_seq_seen;
                conv_cache->upsert(std::move(*cached));
            }
        }
    }
}

} // namespace anychat::sync_engine_detail

namespace anychat {
using namespace sync_engine_detail;


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

void SyncEngine::sync() {
    int64_t last_sync_time = 0;
    {
        const std::string val = db_->getMeta("last_sync_time", "0");
        try {
            last_sync_time = std::stoll(val);
        } catch (...) {
            last_sync_time = 0;
        }
    }

    const db::Rows conv_rows = db_->querySync("SELECT conv_id, conv_type, local_seq FROM conversations", {});
    std::vector<ConversationSeqRequest> conversation_seqs;
    conversation_seqs.reserve(conv_rows.size());

    for (const auto& row : conv_rows) {
        const std::string cid = row.count("conv_id") ? row.at("conv_id") : "";
        const std::string ctype = row.count("conv_type") ? row.at("conv_type") : "private";
        if (cid.empty()) {
            continue;
        }

        int64_t local_seq = 0;
        const auto cached = conv_cache_->get(cid);
        if (cached.has_value()) {
            local_seq = cached->local_seq;
        } else if (row.count("local_seq")) {
            try {
                local_seq = std::stoll(row.at("local_seq"));
            } catch (...) {
                local_seq = 0;
            }
        }

        ConversationSeqRequest request{};
        request.conversation_id = cid;
        request.conversation_type = (ctype == "group") ? "group" : "single";
        request.last_seq = local_seq;
        conversation_seqs.push_back(std::move(request));
    }

    SyncRequestPayload req_body{};
    req_body.last_sync_time = last_sync_time;
    req_body.conversation_seqs = std::move(conversation_seqs);

    std::string body_str;
    std::string err;
    if (!writeJson(req_body, body_str, err)) {
        return;
    }

    http_->post("/sync", body_str, [this](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            return;
        }
        if (resp.status_code != 200) {
            return;
        }
        handleSyncResponse(resp.body);
    });
}

void SyncEngine::handleSyncResponse(const std::string& body) {
    ApiEnvelope<SyncResponseDataPayload> root{};
    std::string err;
    if (!readJsonRelaxed(body, root, err)) {
        return;
    }
    if (root.code != 0) {
        return;
    }

    if (const auto* friends = toFriendPayloadList(root.data.friends); friends != nullptr) {
        mergeFriends(db_, *friends);
    }

    if (const auto* groups = toGroupPayloadList(root.data.groups); groups != nullptr) {
        mergeGroups(db_, *groups);
    }

    if (const auto* sessions = toConversationDeltaPayloadList(root.data.conversation_data); sessions != nullptr) {
        mergeSessions(db_, conv_cache_, *sessions);
    }

    if (const auto* conversations = toConversationPayloadList(root.data.conversations); conversations != nullptr) {
        mergeConvMessages(db_, conv_cache_, msg_cache_, *conversations);
    }

    const int64_t sync_time = parseInt64Value(root.data.sync_time, 0);
    if (sync_time > 0) {
        db_->setMeta("last_sync_time", std::to_string(sync_time));
    }
}

} // namespace anychat
