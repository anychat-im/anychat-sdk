#include "message_manager.h"
#include "json_common.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

namespace anychat {
namespace {
using json_common::ApiEnvelope;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusSuccessResponse;
using json_common::parseBoolValue;
using json_common::parseInt64Value;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::toLower;
using json_common::writeJson;

using IntegerValue = std::variant<int64_t, double, std::string>;
using OptionalIntegerValue = std::optional<IntegerValue>;
using BooleanValue = std::variant<bool, int64_t, double, std::string>;
using OptionalBooleanValue = std::optional<BooleanValue>;
using MessageContentValue = std::variant<std::string, glz::raw_json>;

int parseIntValue(const OptionalIntegerValue& value, int def = 0) {
    return static_cast<int>(parseInt64Value(value, def));
}


int parseMessageStatus(const OptionalIntegerValue& value, int def = 0) {
    if (!value.has_value()) {
        return def;
    }

    if (const auto* int_value = std::get_if<int64_t>(&*value); int_value != nullptr) {
        return static_cast<int>(*int_value);
    }
    if (const auto* double_value = std::get_if<double>(&*value); double_value != nullptr) {
        return static_cast<int>(*double_value);
    }

    const std::string status = toLower(std::get<std::string>(*value));
    if (status == "recalled" || status == "recall" || status == "revoked") {
        return 1;
    }
    if (status == "deleted" || status == "delete") {
        return 2;
    }
    return parseIntValue(value, def);
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

std::string makeHttpError(const network::HttpResponse& resp) {
    if (!resp.error.empty()) {
        return resp.error;
    }
    return "HTTP " + std::to_string(resp.status_code);
}

struct MessagePayload {
    std::string message_id{};
    std::string local_id{};
    std::string conversation_id{};
    std::string sender_id{};
    std::string content_type{};
    std::optional<MessageContentValue> content{};
    OptionalIntegerValue sequence{};
    std::string reply_to{};
    json_common::OptionalTimestampValue created_at{};
    OptionalIntegerValue status{};
    OptionalIntegerValue send_state{};
    OptionalBooleanValue is_read{};
};

struct MessageListDataPayload {
    std::optional<std::vector<MessagePayload>> messages{};
    OptionalIntegerValue total{};
    OptionalBooleanValue has_more{};
    OptionalIntegerValue next_seq{};
};

struct GroupReadMemberPayload {
    std::string user_id{};
    std::string nickname{};
    std::string name{};
    json_common::OptionalTimestampValue read_at{};
};

struct GroupReadStatePayload {
    OptionalIntegerValue read_count{};
    OptionalIntegerValue unread_count{};
    std::optional<std::vector<GroupReadMemberPayload>> read_members{};
};

struct EditMessageDataPayload {
    std::optional<MessagePayload> message{};
};

using EditMessageDataValue = std::variant<std::monostate, EditMessageDataPayload, MessagePayload>;

struct NotificationMessagePayload {
    std::string message_id{};
    std::string local_id{};
    std::string conversation_id{};
    std::string from_user_id{};
    std::string content_type{};
    std::string content{};
    std::optional<int64_t> seq{};
    std::string reply_to{};
    json_common::OptionalTimestampValue sent_at{};
    json_common::OptionalTimestampValue recalled_at{};
    json_common::OptionalTimestampValue deleted_at{};
    json_common::OptionalTimestampValue edited_at{};
};

struct NotificationReadReceiptPayload {
    std::string conversation_id{};
    std::string reader_user_id{};
    std::optional<int64_t> last_read_seq{};
    json_common::OptionalTimestampValue read_at{};
};

struct NotificationTypingPayload {
    std::string conversation_id{};
    std::string from_user_id{};
    bool typing = false;
    std::string device_id{};
    json_common::OptionalTimestampValue expire_at{};
};

int parseStatusValue(const std::optional<json_common::TimestampValue>& value, int def = 0) {
    if (!value.has_value()) {
        return def;
    }

    if (const auto* int_value = std::get_if<int64_t>(&*value); int_value != nullptr) {
        return static_cast<int>(*int_value);
    }
    if (const auto* double_value = std::get_if<double>(&*value); double_value != nullptr) {
        return static_cast<int>(*double_value);
    }

    const std::string status = toLower(std::get<std::string>(*value));
    if (status == "recalled" || status == "recall" || status == "revoked") {
        return 1;
    }
    if (status == "deleted" || status == "delete") {
        return 2;
    }
    try {
        size_t idx = 0;
        const int64_t parsed = std::stoll(status, &idx);
        if (idx == status.size()) {
            return static_cast<int>(parsed);
        }
    } catch (...) {
    }
    return def;
}

Message parseMessagePayload(const MessagePayload& payload, const std::string& default_conv_id = "") {
    Message msg;
    msg.message_id = payload.message_id;
    msg.local_id = payload.local_id;
    msg.conv_id = payload.conversation_id;
    if (msg.conv_id.empty()) {
        msg.conv_id = default_conv_id;
    }
    msg.sender_id = payload.sender_id;
    msg.content_type = payload.content_type;
    if (msg.content_type.empty()) {
        msg.content_type = "text";
    }
    msg.content = parseMessageContent(payload.content);
    msg.seq = parseInt64Value(payload.sequence, 0);
    msg.reply_to = payload.reply_to;
    msg.timestamp_ms = parseTimestampMs(payload.created_at);

    msg.status = parseMessageStatus(payload.status, 0);
    msg.send_state = parseIntValue(payload.send_state, msg.send_state);
    msg.is_read = parseBoolValue(payload.is_read, false);
    return msg;
}

const std::vector<MessagePayload>* toMessagePayloadList(const MessageListDataPayload& data) {
    return data.messages.has_value() ? &(*data.messages) : nullptr;
}

GroupMessageReadState parseGroupMessageReadState(const GroupReadStatePayload& payload) {
    GroupMessageReadState state;
    state.read_count = parseInt64Value(payload.read_count, 0);
    state.unread_count = parseInt64Value(payload.unread_count, 0);

    if (payload.read_members.has_value()) {
        state.read_members.reserve(payload.read_members->size());
        for (const auto& item : *payload.read_members) {
            GroupMessageReadMember member;
            member.user_id = item.user_id;
            member.nickname = item.nickname.empty() ? item.name : item.nickname;
            member.read_at_ms = parseTimestampMs(item.read_at);
            state.read_members.push_back(std::move(member));
        }
    }

    return state;
}

struct AckMessagesRequest {
    std::string conversation_id{};
    std::vector<std::string> message_ids{};
};

struct MessageIdsRequest {
    std::vector<std::string> message_ids{};
};

struct RecallMessageRequest {
    std::string message_id{};
};

struct EditMessageRequest {
    std::string content{};
};

struct MessageRecallPayload {
    std::string message_id{};
};

struct MessageDeletePayload {
    std::string message_id{};
};

struct MessageEditPayload {
    std::string message_id{};
    std::string content{};
};

struct MessageTypingPayload {
    std::string conversation_id{};
    bool typing = false;
    std::optional<int32_t> ttl_seconds{};
};

template <typename T>
struct TransientFrame {
    std::string type{};
    T payload{};
};

Message parseMessageFromNotification(const NotificationMessagePayload& payload, const std::string& default_conv_id = "") {
    Message msg;
    msg.message_id = payload.message_id;
    msg.local_id = payload.local_id;
    msg.conv_id = payload.conversation_id;
    if (msg.conv_id.empty()) {
        msg.conv_id = default_conv_id;
    }
    msg.sender_id = payload.from_user_id;
    msg.content_type = payload.content_type.empty() ? "text" : payload.content_type;
    msg.content = payload.content;
    msg.seq = payload.seq.value_or(0);
    msg.reply_to = payload.reply_to;

    if (payload.sent_at.has_value()) {
        msg.timestamp_ms = parseTimestampMs(payload.sent_at);
    }
    return msg;
}

MessageReadReceiptEvent parseReadReceiptFromNotification(const NotificationReadReceiptPayload& payload) {
    MessageReadReceiptEvent event;
    event.conversation_id = payload.conversation_id;
    event.from_user_id = payload.reader_user_id;
    event.last_read_seq = payload.last_read_seq.value_or(0);
    event.read_at_ms = parseTimestampMs(payload.read_at);
    return event;
}

MessageTypingEvent parseTypingFromNotification(const NotificationTypingPayload& payload) {
    MessageTypingEvent event;
    event.conversation_id = payload.conversation_id;
    event.from_user_id = payload.from_user_id;
    event.typing = payload.typing;
    event.device_id = payload.device_id;
    if (payload.expire_at.has_value()) {
        event.expire_at_ms = parseTimestampMs(payload.expire_at);
    }
    return event;
}

} // namespace

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
    notif_mgr_->addNotificationHandler([this](const NotificationEvent& event) {
        const std::string& type = event.notification_type;
        if (type == "message.new") {
            handleIncomingMessage(event);
        } else if (type == "message.read_receipt") {
            handleReadReceipt(event);
        } else if (type == "message.recalled") {
            handleMessageRecalled(event);
        } else if (type == "message.deleted") {
            handleMessageDeleted(event);
        } else if (type == "message.edited") {
            handleMessageEdited(event);
        } else if (type == "message.typing") {
            handleTyping(event);
        } else if (type == "message.mentioned") {
            handleMentioned(event);
        }
    });
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MessageManagerImpl::sendTextMessage(
    const std::string& conv_id,
    const std::string& content,
    MessageCallback callback
) {
    const std::string local_id = generateLocalId();
    outbound_q_->enqueue(conv_id, "private", "text", content, local_id, std::move(callback));
}

void MessageManagerImpl::getHistory(
    const std::string& conv_id,
    int64_t before_timestamp,
    int limit,
    MessageListCallback callback
) {
    if (before_timestamp == 0) {
        auto cached = msg_cache_->get(conv_id);
        if (!cached.empty()) {
            callback(cached, "");
            return;
        }
    }

    const std::string primary_path = "/messages/history?conversation_id=" + urlEncode(conv_id)
        + "&limit=" + std::to_string(limit)
        + (before_timestamp > 0 ? ("&before=" + std::to_string(before_timestamp) + "&start_seq="
                                  + std::to_string(before_timestamp))
                                : "")
        + "&direction=backward";

    auto parse_and_return = [this, conv_id, cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<MessageListDataPayload> root;
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "server error", true)) {
            cb({}, err);
            return;
        }

        std::vector<Message> messages;
        const auto* payloads = toMessagePayloadList(root.data);
        if (payloads != nullptr) {
            messages.reserve(payloads->size());
            for (const auto& item : *payloads) {
                Message msg = parseMessagePayload(item, conv_id);
                if (msg.message_id.empty()) {
                    continue;
                }
                upsertMessageDb(msg);
                msg_cache_->insert(msg);
                messages.push_back(std::move(msg));
            }
        }
        cb(messages, "");
    };

    http_->get(primary_path, [this,
                              conv_id,
                              before_timestamp,
                              limit,
                              parse_and_return = std::move(parse_and_return)](network::HttpResponse resp) mutable {
        if (resp.status_code == 404 || resp.status_code == 405) {
            std::string fallback = "/conversations/" + conv_id + "/messages?limit=" + std::to_string(limit);
            if (before_timestamp > 0) {
                fallback += "&before=" + std::to_string(before_timestamp);
            }
            http_->get(fallback, std::move(parse_and_return));
            return;
        }
        parse_and_return(std::move(resp));
    });
}

void MessageManagerImpl::markAsRead(
    const std::string& conv_id,
    const std::string& message_id,
    MessageCallback callback
) {
    if (message_id.empty()) {
        ackMessages(conv_id, {}, std::move(callback));
        return;
    }
    ackMessages(conv_id, { message_id }, std::move(callback));
}

void MessageManagerImpl::getOfflineMessages(int64_t last_seq, int limit, MessageOfflineCallback callback) {
    const std::string path = "/messages/offline?last_seq=" + std::to_string(last_seq) + "&limit="
        + std::to_string(limit);

    http_->get(path, [this, cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<MessageListDataPayload> root;
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "server error", true)) {
            cb({}, err);
            return;
        }

        MessageOfflineResult result;
        if (const auto* payloads = toMessagePayloadList(root.data); payloads != nullptr) {
            result.messages.reserve(payloads->size());
            for (const auto& item : *payloads) {
                Message msg = parseMessagePayload(item);
                if (msg.message_id.empty()) {
                    continue;
                }
                upsertMessageDb(msg);
                msg_cache_->insert(msg);
                result.messages.push_back(std::move(msg));
            }
        }
        result.has_more = parseBoolValue(root.data.has_more, false);
        result.next_seq = parseInt64Value(root.data.next_seq, 0);

        cb(std::move(result), "");
    });
}

void MessageManagerImpl::ackMessages(
    const std::string& conv_id,
    const std::vector<std::string>& message_ids,
    MessageCallback callback
) {
    if (conv_id.empty()) {
        callback(false, "conv_id must not be empty");
        return;
    }

    const AckMessagesRequest body{
        .conversation_id = conv_id,
        .message_ids = message_ids,
    };
    std::string body_json;
    std::string body_err;
    if (!writeJson(body, body_json, body_err)) {
        callback(false, body_err);
        return;
    }

    http_->post("/messages/ack", body_json, [this, conv_id, message_ids, cb = std::move(callback)](network::HttpResponse resp) {
        std::string err;
        if (parseApiStatusSuccessResponse(resp, err, "server error", true)) {
            for (const auto& id : message_ids) {
                if (!id.empty()) {
                    msg_cache_->updateMessageById(id, [](Message& m) {
                        m.is_read = true;
                    });
                }
            }
            cb(true, "");
            return;
        }

        if (resp.status_code != 404 && resp.status_code != 405) {
            cb(false, err);
            return;
        }

        if (message_ids.empty()) {
            const std::string path = "/conversations/" + conv_id + "/read-all";
            http_->post(path, "", [cb](network::HttpResponse fallback_resp) {
                if (!fallback_resp.error.empty()) {
                    cb(false, fallback_resp.error);
                    return;
                }
                cb(fallback_resp.status_code == 200, fallback_resp.status_code == 200
                        ? ""
                        : ("HTTP " + std::to_string(fallback_resp.status_code)));
            });
            return;
        }

        const MessageIdsRequest fallback_body{
            .message_ids = message_ids,
        };
        std::string fallback_body_json;
        if (!writeJson(fallback_body, fallback_body_json, err)) {
            cb(false, err);
            return;
        }
        const std::string path = "/conversations/" + conv_id + "/messages/read";
        http_->post(path, fallback_body_json, [cb](network::HttpResponse fallback_resp) {
            std::string fallback_err;
            if (!parseApiStatusSuccessResponse(fallback_resp, fallback_err, "server error", true)) {
                cb(false, fallback_err);
                return;
            }
            cb(true, "");
        });
    });
}

void MessageManagerImpl::getGroupMessageReadState(
    const std::string& group_id,
    const std::string& message_id,
    GroupMessageReadStateCallback callback
) {
    if (group_id.empty() || message_id.empty()) {
        callback({}, "group_id and message_id must not be empty");
        return;
    }

    const std::string path = "/groups/" + group_id + "/messages/" + message_id + "/reads";
    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<GroupReadStatePayload> root;
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "server error", true)) {
            cb({}, err);
            return;
        }

        cb(parseGroupMessageReadState(root.data), "");
    });
}

void MessageManagerImpl::searchMessages(
    const std::string& keyword,
    const std::string& conversation_id,
    const std::string& content_type,
    int limit,
    int offset,
    MessageSearchCallback callback
) {
    if (keyword.empty()) {
        callback({}, "keyword must not be empty");
        return;
    }

    std::string path = "/messages/search?keyword=" + urlEncode(keyword);
    if (!conversation_id.empty()) {
        path += "&conversation_id=" + urlEncode(conversation_id);
    }
    if (!content_type.empty()) {
        path += "&content_type=" + urlEncode(content_type);
    }
    if (limit > 0) {
        path += "&limit=" + std::to_string(limit);
    }
    if (offset > 0) {
        path += "&offset=" + std::to_string(offset);
    }

    http_->get(path, [this, cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<MessageListDataPayload> root;
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err, "server error", true)) {
            cb({}, err);
            return;
        }

        MessageSearchResult result;
        if (const auto* payloads = toMessagePayloadList(root.data); payloads != nullptr) {
            for (const auto& item : *payloads) {
                Message msg = parseMessagePayload(item);
                if (msg.message_id.empty()) {
                    continue;
                }
                upsertMessageDb(msg);
                msg_cache_->insert(msg);
                result.messages.push_back(std::move(msg));
            }
        }
        result.total = parseInt64Value(root.data.total, static_cast<int64_t>(result.messages.size()));

        cb(std::move(result), "");
    });
}

void MessageManagerImpl::recallMessage(const std::string& message_id, MessageCallback callback) {
    if (message_id.empty()) {
        callback(false, "message_id must not be empty");
        return;
    }

    const RecallMessageRequest body{
        .message_id = message_id,
    };
    std::string body_json;
    std::string body_err;
    if (!writeJson(body, body_json, body_err)) {
        callback(false, body_err);
        return;
    }

    http_->post("/messages/recall", body_json, [this, message_id, cb = std::move(callback)](network::HttpResponse resp) {
        std::string err;
        if (parseApiStatusSuccessResponse(resp, err, "server error", true)) {
            msg_cache_->updateMessageById(message_id, [](Message& msg) {
                msg.status = 1;
            });
            updateMessageDbStatusAndContent(message_id, 1, nullptr);
            cb(true, "");
            return;
        }

        if ((resp.status_code == 404 || resp.status_code == 405)) {
            const TransientFrame<MessageRecallPayload> frame{
                .type = "message.recall",
                .payload =
                    MessageRecallPayload{
                        .message_id = message_id,
                    },
            };
            std::string frame_json;
            if (!writeJson(frame, frame_json, err)) {
                cb(false, err);
                return;
            }
            if (outbound_q_->sendTransient(frame_json)) {
                cb(true, "");
                return;
            }
        }

        cb(false, err.empty() ? makeHttpError(resp) : err);
    });
}

void MessageManagerImpl::deleteMessage(const std::string& message_id, MessageCallback callback) {
    if (message_id.empty()) {
        callback(false, "message_id must not be empty");
        return;
    }

    http_->del("/messages/" + message_id, [this, message_id, cb = std::move(callback)](network::HttpResponse resp) {
        std::string err;
        if (parseApiStatusSuccessResponse(resp, err, "server error", true)) {
            msg_cache_->updateMessageById(message_id, [](Message& msg) {
                msg.status = 2;
            });
            updateMessageDbStatusAndContent(message_id, 2, nullptr);
            cb(true, "");
            return;
        }

        if ((resp.status_code == 404 || resp.status_code == 405)) {
            const TransientFrame<MessageDeletePayload> frame{
                .type = "message.delete",
                .payload =
                    MessageDeletePayload{
                        .message_id = message_id,
                    },
            };
            std::string frame_json;
            if (!writeJson(frame, frame_json, err)) {
                cb(false, err);
                return;
            }
            if (outbound_q_->sendTransient(frame_json)) {
                cb(true, "");
                return;
            }
        }

        cb(false, err.empty() ? makeHttpError(resp) : err);
    });
}

void MessageManagerImpl::editMessage(const std::string& message_id, const std::string& content, MessageCallback callback) {
    if (message_id.empty()) {
        callback(false, "message_id must not be empty");
        return;
    }

    const EditMessageRequest body{
        .content = content,
    };
    std::string body_json;
    std::string body_err;
    if (!writeJson(body, body_json, body_err)) {
        callback(false, body_err);
        return;
    }

    http_->patch("/messages/" + message_id, body_json, [this, message_id, content, cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<EditMessageDataValue> root;
        std::string err;
        if (parseApiEnvelopeResponse(resp, root, err, "server error", true)) {
            Message edited;
            bool has_message = false;
            if (const auto* wrapped = std::get_if<EditMessageDataPayload>(&root.data); wrapped != nullptr) {
                if (wrapped->message.has_value()) {
                    edited = parseMessagePayload(*wrapped->message);
                    has_message = !edited.message_id.empty();
                }
            } else if (const auto* direct = std::get_if<MessagePayload>(&root.data); direct != nullptr) {
                    edited = parseMessagePayload(*direct);
                    has_message = !edited.message_id.empty();
            }

            if (!has_message) {
                edited.message_id = message_id;
                edited.content = content;
            }

            msg_cache_->updateMessageById(message_id, [&](Message& msg) {
                msg.content = edited.content;
                if (edited.status != 0) {
                    msg.status = edited.status;
                }
            });
            updateMessageDbStatusAndContent(message_id, edited.status, &edited.content);
            cb(true, "");
            return;
        }

        if ((resp.status_code == 404 || resp.status_code == 405)) {
            const TransientFrame<MessageEditPayload> frame{
                .type = "message.edit",
                .payload =
                    MessageEditPayload{
                        .message_id = message_id,
                        .content = content,
                    },
            };
            std::string frame_json;
            if (!writeJson(frame, frame_json, err)) {
                cb(false, err);
                return;
            }
            if (outbound_q_->sendTransient(frame_json)) {
                cb(true, "");
                return;
            }
        }

        cb(false, err.empty() ? makeHttpError(resp) : err);
    });
}

void MessageManagerImpl::sendTyping(
    const std::string& conversation_id,
    bool typing,
    int32_t ttl_seconds,
    MessageCallback callback
) {
    if (conversation_id.empty()) {
        callback(false, "conversation_id must not be empty");
        return;
    }

    const TransientFrame<MessageTypingPayload> frame{
        .type = "message.typing",
        .payload =
            MessageTypingPayload{
                .conversation_id = conversation_id,
                .typing = typing,
                .ttl_seconds = ttl_seconds > 0 ? std::optional<int32_t>{ ttl_seconds } : std::nullopt,
            },
    };

    std::string frame_json;
    std::string err;
    if (!writeJson(frame, frame_json, err)) {
        callback(false, err);
        return;
    }

    const bool sent = outbound_q_->sendTransient(frame_json);
    callback(sent, sent ? "" : "websocket not connected");
}

void MessageManagerImpl::setListener(std::shared_ptr<MessageListener> listener) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    listener_ = std::move(listener);
}

void MessageManagerImpl::setCurrentUserId(const std::string& uid) {
    std::lock_guard<std::mutex> lk(uid_mutex_);
    current_user_id_ = uid;
}

// ---------------------------------------------------------------------------
// Notification handling
// ---------------------------------------------------------------------------

void MessageManagerImpl::handleIncomingMessage(const NotificationEvent& event) {
    try {
        NotificationMessagePayload payload{};
        std::string err;
        if (!parseJsonObject(event.data, payload, err)) {
            return;
        }

        Message msg = parseMessageFromNotification(payload);
        if (msg.message_id.empty()) {
            return;
        }
        if (msg.timestamp_ms == 0) {
            msg.timestamp_ms = normalizeEpochMs(event.timestamp);
        }

        msg_cache_->insert(msg);
        upsertMessageDb(msg);

        std::shared_ptr<MessageListener> listener;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            listener->onMessageReceived(msg);
        }
    } catch (const std::exception&) {
        // Ignore malformed notification payload.
    }
}

void MessageManagerImpl::handleReadReceipt(const NotificationEvent& event) {
    NotificationReadReceiptPayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    MessageReadReceiptEvent receipt = parseReadReceiptFromNotification(payload);
    if (receipt.read_at_ms == 0) {
        receipt.read_at_ms = normalizeEpochMs(event.timestamp);
    }

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageReadReceipt(receipt);
    }
}

void MessageManagerImpl::handleMessageRecalled(const NotificationEvent& event) {
    NotificationMessagePayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    Message msg = parseMessageFromNotification(payload);
    if (msg.message_id.empty()) {
        return;
    }

    msg.status = 1;
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = payload.recalled_at.has_value() ? json_common::parseTimestampMs(payload.recalled_at)
                                                            : normalizeEpochMs(event.timestamp);
    }

    bool found = msg_cache_->updateMessageById(msg.message_id, [&](Message& cached) {
        if (!msg.conv_id.empty()) {
            cached.conv_id = msg.conv_id;
        }
        if (!msg.content.empty()) {
            cached.content = msg.content;
        }
        cached.status = 1;
        msg = cached;
    });
    if (!found && !msg.conv_id.empty()) {
        msg_cache_->insert(msg);
    }

    const std::string* content = msg.content.empty() ? nullptr : &msg.content;
    updateMessageDbStatusAndContent(msg.message_id, 1, content);

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageRecalled(msg);
    }
}

void MessageManagerImpl::handleMessageDeleted(const NotificationEvent& event) {
    NotificationMessagePayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    Message msg = parseMessageFromNotification(payload);
    if (msg.message_id.empty()) {
        return;
    }

    msg.status = 2;
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = payload.deleted_at.has_value() ? json_common::parseTimestampMs(payload.deleted_at)
                                                           : normalizeEpochMs(event.timestamp);
    }

    bool found = msg_cache_->updateMessageById(msg.message_id, [&](Message& cached) {
        if (!msg.conv_id.empty()) {
            cached.conv_id = msg.conv_id;
        }
        cached.status = 2;
        msg = cached;
    });
    if (!found && !msg.conv_id.empty()) {
        msg_cache_->insert(msg);
    }

    updateMessageDbStatusAndContent(msg.message_id, 2, nullptr);

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageDeleted(msg);
    }
}

void MessageManagerImpl::handleMessageEdited(const NotificationEvent& event) {
    NotificationMessagePayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    Message msg = parseMessageFromNotification(payload);
    if (msg.message_id.empty()) {
        return;
    }

    bool found = msg_cache_->updateMessageById(msg.message_id, [&](Message& cached) {
        if (!msg.content.empty()) {
            cached.content = msg.content;
        }
        if (!msg.content_type.empty()) {
            cached.content_type = msg.content_type;
        }
        if (msg.status != 0) {
            cached.status = msg.status;
        }
        msg = cached;
    });
    if (!found && !msg.conv_id.empty()) {
        msg_cache_->insert(msg);
    }

    const std::string* content = msg.content.empty() ? nullptr : &msg.content;
    updateMessageDbStatusAndContent(msg.message_id, msg.status, content);

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageEdited(msg);
    }
}

void MessageManagerImpl::handleTyping(const NotificationEvent& event) {
    NotificationTypingPayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    MessageTypingEvent typing = parseTypingFromNotification(payload);
    if (typing.expire_at_ms == 0 && event.timestamp > 0) {
        typing.expire_at_ms = normalizeEpochMs(event.timestamp);
    }

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageTyping(typing);
    }
}

void MessageManagerImpl::handleMentioned(const NotificationEvent& event) {
    NotificationMessagePayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    Message msg = parseMessageFromNotification(payload);
    if (msg.message_id.empty()) {
        return;
    }
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = normalizeEpochMs(event.timestamp);
    }

    msg_cache_->insert(msg);
    upsertMessageDb(msg);

    std::shared_ptr<MessageListener> listener;
    {
        std::lock_guard<std::mutex> lk(handler_mutex_);
        listener = listener_;
    }
    if (listener) {
        listener->onMessageMentioned(msg);
    }
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::string MessageManagerImpl::urlEncode(const std::string& input) {
    std::ostringstream oss;
    oss.fill('0');
    oss << std::hex << std::uppercase;

    for (unsigned char ch : input) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-'
            || ch == '_' || ch == '.' || ch == '~') {
            oss << static_cast<char>(ch);
        } else {
            oss << '%' << std::setw(2) << static_cast<int>(ch);
        }
    }

    return oss.str();
}

int64_t MessageManagerImpl::normalizeEpochMs(int64_t raw) {
    if (raw == 0) {
        return 0;
    }
    return std::llabs(raw) >= 100000000000LL ? raw : raw * 1000;
}

void MessageManagerImpl::upsertMessageDb(const Message& msg) {
    if (msg.message_id.empty() || msg.conv_id.empty()) {
        return;
    }

    const int64_t created_at_s = msg.timestamp_ms > 0 ? msg.timestamp_ms / 1000 : 0;
    db_->exec(
        "INSERT INTO messages (msg_id, local_id, conv_id, sender_id, content_type, content, seq, reply_to, "
        "status, send_state, is_read, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(msg_id) DO UPDATE SET "
        "local_id=excluded.local_id, conv_id=excluded.conv_id, sender_id=excluded.sender_id, "
        "content_type=excluded.content_type, content=excluded.content, seq=excluded.seq, reply_to=excluded.reply_to, "
        "status=excluded.status, send_state=excluded.send_state, is_read=excluded.is_read, created_at=excluded.created_at",
        { msg.message_id,
          msg.local_id,
          msg.conv_id,
          msg.sender_id,
          msg.content_type,
          msg.content,
          msg.seq,
          msg.reply_to,
          static_cast<int64_t>(msg.status),
          static_cast<int64_t>(msg.send_state),
          static_cast<int64_t>(msg.is_read ? 1 : 0),
          created_at_s }
    );
}

void MessageManagerImpl::updateMessageDbStatusAndContent(
    const std::string& message_id,
    int status,
    const std::string* content
) {
    if (message_id.empty()) {
        return;
    }

    if (content == nullptr) {
        db_->exec("UPDATE messages SET status = ? WHERE msg_id = ?", { static_cast<int64_t>(status), message_id });
        return;
    }

    db_->exec(
        "UPDATE messages SET status = ?, content = ? WHERE msg_id = ?",
        { static_cast<int64_t>(status), *content, message_id }
    );
}

std::string MessageManagerImpl::generateLocalId() {
    static std::atomic<int64_t> counter{ 1 };
    return "local_" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

} // namespace anychat
