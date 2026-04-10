#include "message_manager.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>

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

bool getBool(const nlohmann::json& obj, std::initializer_list<const char*> keys, bool def = false) {
    const auto* value = findField(obj, keys);
    return value != nullptr ? jsonToBool(*value, def) : def;
}

int parseStatus(const nlohmann::json& value, int def = 0) {
    if (value.is_number_integer() || value.is_number_unsigned() || value.is_number_float() || value.is_string()) {
        const int64_t direct = jsonToInt64(value, std::numeric_limits<int64_t>::min());
        if (direct != std::numeric_limits<int64_t>::min()) {
            return static_cast<int>(direct);
        }
    }

    if (value.is_string()) {
        const std::string status = toLower(value.get<std::string>());
        if (status == "recalled" || status == "recall" || status == "revoked") {
            return 1;
        }
        if (status == "deleted" || status == "delete") {
            return 2;
        }
        return 0;
    }

    return def;
}

std::string makeHttpError(const network::HttpResponse& resp) {
    if (!resp.error.empty()) {
        return resp.error;
    }
    return "HTTP " + std::to_string(resp.status_code);
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

    const std::string primary_path = "/messages/history?conversationId=" + urlEncode(conv_id)
        + "&limit=" + std::to_string(limit)
        + (before_timestamp > 0 ? ("&before=" + std::to_string(before_timestamp) + "&startSeq="
                                  + std::to_string(before_timestamp))
                                : "")
        + "&direction=backward";

    auto parse_and_return = [this, conv_id, cb = std::move(callback)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseHttpRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        const nlohmann::json* data = findField(root, { "data" });
        if (data == nullptr || !data->is_object()) {
            cb({}, "missing data");
            return;
        }

        std::vector<Message> messages;
        const nlohmann::json* list = findField(*data, { "messages", "list", "items" });
        if (list != nullptr && list->is_array()) {
            messages.reserve(list->size());
            for (const auto& item : *list) {
                Message msg = parseMessageJson(item, conv_id);
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
    const std::string path = "/messages/offline?lastSeq=" + std::to_string(last_seq) + "&limit="
        + std::to_string(limit);

    http_->get(path, [this, cb = std::move(callback)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (!parseHttpRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        MessageOfflineResult result;
        const nlohmann::json* data = findField(root, { "data" });
        if (data != nullptr && data->is_object()) {
            const nlohmann::json* list = findField(*data, { "messages", "list", "items" });
            if (list != nullptr && list->is_array()) {
                result.messages.reserve(list->size());
                for (const auto& item : *list) {
                    Message msg = parseMessageJson(item);
                    if (msg.message_id.empty()) {
                        continue;
                    }
                    upsertMessageDb(msg);
                    msg_cache_->insert(msg);
                    result.messages.push_back(std::move(msg));
                }
            }
            result.has_more = getBool(*data, { "hasMore", "has_more" }, false);
            result.next_seq = getInt64(*data, { "nextSeq", "next_seq" }, 0);
        }

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

    nlohmann::json body = {
        { "conversationId", conv_id },
        { "messageIds", message_ids },
    };

    http_->post("/messages/ack", body.dump(), [this, conv_id, message_ids, cb = std::move(callback)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (parseHttpRoot(resp, root, err)) {
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

        nlohmann::json fallback_body = {
            { "message_ids", message_ids },
        };
        const std::string path = "/conversations/" + conv_id + "/messages/read";
        http_->post(path, fallback_body.dump(), [cb](network::HttpResponse fallback_resp) {
            nlohmann::json fallback_root;
            std::string fallback_err;
            if (!MessageManagerImpl::parseHttpRoot(fallback_resp, fallback_root, fallback_err)) {
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
        nlohmann::json root;
        std::string err;
        if (!parseHttpRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        const nlohmann::json* data = findField(root, { "data" });
        if (data == nullptr || !data->is_object()) {
            cb({}, "missing data");
            return;
        }

        cb(parseGroupMessageReadState(*data), "");
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
        nlohmann::json root;
        std::string err;
        if (!parseHttpRoot(resp, root, err)) {
            cb({}, err);
            return;
        }

        MessageSearchResult result;
        const nlohmann::json* data = findField(root, { "data" });
        if (data == nullptr) {
            cb({}, "missing data");
            return;
        }

        if (data->is_object()) {
            const nlohmann::json* list = findField(*data, { "messages", "list", "items" });
            if (list != nullptr && list->is_array()) {
                result.messages.reserve(list->size());
                for (const auto& item : *list) {
                    Message msg = parseMessageJson(item);
                    if (msg.message_id.empty()) {
                        continue;
                    }
                    upsertMessageDb(msg);
                    msg_cache_->insert(msg);
                    result.messages.push_back(std::move(msg));
                }
            }
            result.total = getInt64(*data, { "total" }, static_cast<int64_t>(result.messages.size()));
        } else if (data->is_array()) {
            for (const auto& item : *data) {
                Message msg = parseMessageJson(item);
                if (msg.message_id.empty()) {
                    continue;
                }
                upsertMessageDb(msg);
                msg_cache_->insert(msg);
                result.messages.push_back(std::move(msg));
            }
            result.total = static_cast<int64_t>(result.messages.size());
        }

        cb(std::move(result), "");
    });
}

void MessageManagerImpl::recallMessage(const std::string& message_id, MessageCallback callback) {
    if (message_id.empty()) {
        callback(false, "message_id must not be empty");
        return;
    }

    nlohmann::json body = { { "message_id", message_id } };
    http_->post("/messages/recall", body.dump(), [this, message_id, cb = std::move(callback)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (parseHttpRoot(resp, root, err)) {
            msg_cache_->updateMessageById(message_id, [](Message& msg) {
                msg.status = 1;
            });
            updateMessageDbStatusAndContent(message_id, 1, nullptr);
            cb(true, "");
            return;
        }

        if ((resp.status_code == 404 || resp.status_code == 405)) {
            nlohmann::json frame = {
                { "type", "message.recall" },
                { "payload", { { "messageId", message_id } } },
            };
            if (outbound_q_->sendTransient(frame.dump())) {
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
        nlohmann::json root;
        std::string err;
        if (parseHttpRoot(resp, root, err)) {
            msg_cache_->updateMessageById(message_id, [](Message& msg) {
                msg.status = 2;
            });
            updateMessageDbStatusAndContent(message_id, 2, nullptr);
            cb(true, "");
            return;
        }

        if ((resp.status_code == 404 || resp.status_code == 405)) {
            nlohmann::json frame = {
                { "type", "message.delete" },
                { "payload", { { "messageId", message_id } } },
            };
            if (outbound_q_->sendTransient(frame.dump())) {
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

    nlohmann::json body = { { "content", content } };

    http_->patch("/messages/" + message_id, body.dump(), [this, message_id, content, cb = std::move(callback)](network::HttpResponse resp) {
        nlohmann::json root;
        std::string err;
        if (parseHttpRoot(resp, root, err)) {
            Message edited;
            bool has_message = false;
            if (const nlohmann::json* data = findField(root, { "data" }); data != nullptr && data->is_object()) {
                if (const nlohmann::json* message_obj = findField(*data, { "message" });
                    message_obj != nullptr && message_obj->is_object()) {
                    edited = parseMessageJson(*message_obj);
                    has_message = !edited.message_id.empty();
                }
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
            nlohmann::json frame = {
                { "type", "message.edit" },
                { "payload", { { "messageId", message_id }, { "content", content } } },
            };
            if (outbound_q_->sendTransient(frame.dump())) {
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

    nlohmann::json payload = {
        { "conversationId", conversation_id },
        { "typing", typing },
    };
    if (ttl_seconds > 0) {
        payload["ttlSeconds"] = ttl_seconds;
    }

    nlohmann::json frame = {
        { "type", "message.typing" },
        { "payload", payload },
    };

    const bool sent = outbound_q_->sendTransient(frame.dump());
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
        Message msg = parseMessageJson(event.data);
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
    MessageReadReceiptEvent receipt = parseReadReceiptEvent(event.data);
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
    Message msg = parseMessageJson(event.data);
    if (msg.message_id.empty()) {
        msg.message_id = getString(event.data, { "messageId", "message_id" });
    }
    if (msg.message_id.empty()) {
        return;
    }

    msg.status = 1;
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = normalizeEpochMs(getInt64(event.data, { "recalledAt", "recalled_at" }, event.timestamp));
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
    Message msg = parseMessageJson(event.data);
    if (msg.message_id.empty()) {
        msg.message_id = getString(event.data, { "messageId", "message_id" });
    }
    if (msg.message_id.empty()) {
        return;
    }

    msg.status = 2;
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = normalizeEpochMs(getInt64(event.data, { "deletedAt", "deleted_at" }, event.timestamp));
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
    Message msg = parseMessageJson(event.data);
    if (msg.message_id.empty()) {
        msg.message_id = getString(event.data, { "messageId", "message_id" });
    }
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
    MessageTypingEvent typing = parseTypingEvent(event.data);
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
    Message msg = parseMessageJson(event.data);
    if (msg.message_id.empty()) {
        msg.message_id = getString(event.data, { "messageId", "message_id" });
    }
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

bool MessageManagerImpl::parseHttpRoot(const network::HttpResponse& resp, nlohmann::json& root, std::string& err) {
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
    if (!root.is_object()) {
        err = "invalid response body";
        return false;
    }

    if (root.value("code", -1) != 0) {
        err = root.value("message", "server error");
        return false;
    }
    return true;
}

int64_t MessageManagerImpl::normalizeEpochMs(int64_t raw) {
    if (raw == 0) {
        return 0;
    }
    return std::llabs(raw) >= 100000000000LL ? raw : raw * 1000;
}

int64_t MessageManagerImpl::parseTimestampMs(const nlohmann::json& value) {
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

Message MessageManagerImpl::parseMessageJson(const nlohmann::json& item, const std::string& default_conv_id) {
    Message msg;
    msg.message_id = getString(item, { "messageId", "message_id" });
    msg.local_id = getString(item, { "localId", "local_id" });
    msg.conv_id = getString(item, { "conversationId", "conversation_id", "convId", "conv_id" });
    if (msg.conv_id.empty()) {
        msg.conv_id = default_conv_id;
    }
    msg.sender_id = getString(item, { "senderId", "sender_id", "fromUserId", "from_user_id" });
    msg.content_type = getString(item, { "contentType", "content_type" });
    if (msg.content_type.empty()) {
        msg.content_type = "text";
    }
    msg.content = getString(item, { "content" });
    msg.seq = getInt64(item, { "sequence", "seq" }, 0);
    msg.reply_to = getString(item, { "replyTo", "reply_to" });

    if (const nlohmann::json* ts = findField(item, { "timestamp", "sentAt", "sent_at", "createdAt", "created_at" })) {
        msg.timestamp_ms = parseTimestampMs(*ts);
    }

    if (const nlohmann::json* status = findField(item, { "status" })) {
        msg.status = parseStatus(*status, 0);
    }

    msg.send_state = static_cast<int>(getInt64(item, { "sendState", "send_state" }, msg.send_state));
    msg.is_read = getBool(item, { "isRead", "is_read" }, false);
    return msg;
}

void MessageManagerImpl::applyMessagePatch(const nlohmann::json& item, Message& msg) {
    if (const nlohmann::json* value = findField(item, { "conversationId", "conversation_id" })) {
        msg.conv_id = jsonToString(*value);
    }
    if (const nlohmann::json* value = findField(item, { "senderId", "sender_id", "fromUserId", "from_user_id" })) {
        msg.sender_id = jsonToString(*value);
    }
    if (const nlohmann::json* value = findField(item, { "contentType", "content_type" })) {
        msg.content_type = jsonToString(*value);
    }
    if (const nlohmann::json* value = findField(item, { "content" })) {
        msg.content = jsonToString(*value);
    }
    if (const nlohmann::json* value = findField(item, { "sequence", "seq" })) {
        msg.seq = jsonToInt64(*value, msg.seq);
    }
    if (const nlohmann::json* value = findField(item, { "status" })) {
        msg.status = parseStatus(*value, msg.status);
    }
    if (const nlohmann::json* value = findField(item, { "timestamp", "sentAt", "sent_at", "editedAt", "edited_at" })) {
        msg.timestamp_ms = parseTimestampMs(*value);
    }
}

GroupMessageReadState MessageManagerImpl::parseGroupMessageReadState(const nlohmann::json& data) {
    GroupMessageReadState state;
    state.read_count = getInt64(data, { "readCount", "read_count" }, 0);
    state.unread_count = getInt64(data, { "unreadCount", "unread_count" }, 0);

    const nlohmann::json* members = findField(data, { "readMembers", "read_members" });
    if (members != nullptr && members->is_array()) {
        state.read_members.reserve(members->size());
        for (const auto& item : *members) {
            GroupMessageReadMember member;
            member.user_id = getString(item, { "userId", "user_id" });
            member.nickname = getString(item, { "nickname", "name" });
            if (const nlohmann::json* ts = findField(item, { "readAt", "read_at" })) {
                member.read_at_ms = parseTimestampMs(*ts);
            }
            state.read_members.push_back(std::move(member));
        }
    }

    return state;
}

MessageReadReceiptEvent MessageManagerImpl::parseReadReceiptEvent(const nlohmann::json& data) {
    MessageReadReceiptEvent event;
    event.conversation_id = getString(data, { "conversationId", "conversation_id" });
    event.from_user_id = getString(data, { "fromUserId", "from_user_id", "userId", "user_id" });
    event.message_id = getString(data, { "messageId", "message_id" });
    event.last_read_seq = getInt64(data, { "lastReadSeq", "last_read_seq", "readSeq", "read_seq" }, 0);
    event.last_read_message_id =
        getString(data, { "lastReadMessageId", "last_read_message_id", "messageId", "message_id" });

    if (const nlohmann::json* ts = findField(data, { "readAt", "read_at", "timestamp" })) {
        event.read_at_ms = parseTimestampMs(*ts);
    }
    return event;
}

MessageTypingEvent MessageManagerImpl::parseTypingEvent(const nlohmann::json& data) {
    MessageTypingEvent event;
    event.conversation_id = getString(data, { "conversationId", "conversation_id" });
    event.from_user_id = getString(data, { "fromUserId", "from_user_id" });
    event.typing = getBool(data, { "typing" }, false);
    event.device_id = getString(data, { "deviceId", "device_id" });

    if (const nlohmann::json* ts = findField(data, { "expireAt", "expire_at" })) {
        event.expire_at_ms = parseTimestampMs(*ts);
    }

    return event;
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
