#include "anychat/message.h"

#include "message_manager.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

void messageToCStruct(const anychat::Message& src, AnyChatMessage_C* dst) {
    anychat_strlcpy(dst->message_id, src.message_id.c_str(), sizeof(dst->message_id));
    anychat_strlcpy(dst->local_id, src.local_id.c_str(), sizeof(dst->local_id));
    anychat_strlcpy(dst->conv_id, src.conv_id.c_str(), sizeof(dst->conv_id));
    anychat_strlcpy(dst->sender_id, src.sender_id.c_str(), sizeof(dst->sender_id));
    dst->content_type = src.content_type;
    anychat_strlcpy(dst->reply_to, src.reply_to.c_str(), sizeof(dst->reply_to));

    dst->type = static_cast<int>(src.type);
    dst->seq = src.seq;
    dst->timestamp_ms = src.timestamp_ms;
    dst->status = src.status;
    dst->send_state = src.send_state;
    dst->is_read = src.is_read ? 1 : 0;

    dst->content = anychat_strdup(src.content.c_str());
}

void fillMessageArray(const std::vector<anychat::Message>& messages, AnyChatMessage_C** items, int* count) {
    *count = static_cast<int>(messages.size());
    *items = *count > 0 ? static_cast<AnyChatMessage_C*>(std::calloc(*count, sizeof(AnyChatMessage_C))) : nullptr;
    for (int i = 0; i < *count; ++i) {
        messageToCStruct(messages[static_cast<size_t>(i)], &(*items)[i]);
    }
}

void readReceiptToCStruct(const anychat::MessageReadReceiptEvent& src, AnyChatMessageReadReceiptEvent_C* dst) {
    anychat_strlcpy(dst->conversation_id, src.conversation_id.c_str(), sizeof(dst->conversation_id));
    anychat_strlcpy(dst->from_user_id, src.from_user_id.c_str(), sizeof(dst->from_user_id));
    anychat_strlcpy(dst->message_id, src.message_id.c_str(), sizeof(dst->message_id));
    anychat_strlcpy(
        dst->last_read_message_id,
        src.last_read_message_id.c_str(),
        sizeof(dst->last_read_message_id)
    );
    dst->last_read_seq = src.last_read_seq;
    dst->read_at_ms = src.read_at_ms;
}

void typingToCStruct(const anychat::MessageTypingEvent& src, AnyChatMessageTypingEvent_C* dst) {
    anychat_strlcpy(dst->conversation_id, src.conversation_id.c_str(), sizeof(dst->conversation_id));
    anychat_strlcpy(dst->from_user_id, src.from_user_id.c_str(), sizeof(dst->from_user_id));
    anychat_strlcpy(dst->device_id, src.device_id.c_str(), sizeof(dst->device_id));
    dst->typing = src.typing ? 1 : 0;
    dst->expire_at_ms = src.expire_at_ms;
}

void groupReadStateToCStruct(const anychat::GroupMessageReadState& src, AnyChatGroupMessageReadState_C* dst) {
    dst->read_count = src.read_count;
    dst->unread_count = src.unread_count;
    dst->count = static_cast<int>(src.read_members.size());
    dst->items = dst->count > 0
        ? static_cast<AnyChatGroupMessageReadMember_C*>(
              std::calloc(static_cast<size_t>(dst->count), sizeof(AnyChatGroupMessageReadMember_C))
          )
        : nullptr;

    for (int i = 0; i < dst->count; ++i) {
        const auto& member = src.read_members[static_cast<size_t>(i)];
        anychat_strlcpy(dst->items[i].user_id, member.user_id.c_str(), sizeof(dst->items[i].user_id));
        anychat_strlcpy(dst->items[i].nickname, member.nickname.c_str(), sizeof(dst->items[i].nickname));
        dst->items[i].read_at_ms = member.read_at_ms;
    }
}

class CMessageListener final : public anychat::MessageListener {
public:
    explicit CMessageListener(const AnyChatMessageListener_C& listener)
        : listener_(listener) {}

    void onMessageReceived(const anychat::Message& msg) override {
        if (!listener_.on_message_received) {
            return;
        }
        AnyChatMessage_C c_msg{};
        messageToCStruct(msg, &c_msg);
        listener_.on_message_received(listener_.userdata, &c_msg);
        std::free(c_msg.content);
    }

    void onMessageReadReceipt(const anychat::MessageReadReceiptEvent& event) override {
        if (!listener_.on_message_read_receipt) {
            return;
        }
        AnyChatMessageReadReceiptEvent_C c_event{};
        readReceiptToCStruct(event, &c_event);
        listener_.on_message_read_receipt(listener_.userdata, &c_event);
    }

    void onMessageRecalled(const anychat::Message& msg) override {
        if (!listener_.on_message_recalled) {
            return;
        }
        AnyChatMessage_C c_msg{};
        messageToCStruct(msg, &c_msg);
        listener_.on_message_recalled(listener_.userdata, &c_msg);
        std::free(c_msg.content);
    }

    void onMessageDeleted(const anychat::Message& msg) override {
        if (!listener_.on_message_deleted) {
            return;
        }
        AnyChatMessage_C c_msg{};
        messageToCStruct(msg, &c_msg);
        listener_.on_message_deleted(listener_.userdata, &c_msg);
        std::free(c_msg.content);
    }

    void onMessageEdited(const anychat::Message& msg) override {
        if (!listener_.on_message_edited) {
            return;
        }
        AnyChatMessage_C c_msg{};
        messageToCStruct(msg, &c_msg);
        listener_.on_message_edited(listener_.userdata, &c_msg);
        std::free(c_msg.content);
    }

    void onMessageTyping(const anychat::MessageTypingEvent& event) override {
        if (!listener_.on_message_typing) {
            return;
        }
        AnyChatMessageTypingEvent_C c_event{};
        typingToCStruct(event, &c_event);
        listener_.on_message_typing(listener_.userdata, &c_event);
    }

    void onMessageMentioned(const anychat::Message& msg) override {
        if (!listener_.on_message_mentioned) {
            return;
        }
        AnyChatMessage_C c_msg{};
        messageToCStruct(msg, &c_msg);
        listener_.on_message_mentioned(listener_.userdata, &c_msg);
        std::free(c_msg.content);
    }

private:
    AnyChatMessageListener_C listener_{};
};

template<typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback) {
        return false;
    }
    return true;
}

template<typename CallbackStruct>
CallbackStruct copyCallbackStruct(const CallbackStruct* callback) {
    CallbackStruct callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }
    return callback_copy;
}

template<typename CallbackStruct>
void invokeMessageError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeMessageCallback(const AnyChatMessageCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeMessageError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

int anychat_message_send_text(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* content,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id || !content) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->sendTextMessage(conv_id, content, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_get_history(
    AnyChatMessageHandle handle,
    const char* conv_id,
    int64_t before_timestamp_ms,
    int limit,
    const AnyChatMessageListCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageListCallback_C callback_copy = copyCallbackStruct(callback);

    impl->getHistory(
        conv_id,
        before_timestamp_ms,
        limit,
        anychat::AnyChatValueCallback<std::vector<anychat::Message>>{
            .on_success =
                [callback_copy](const std::vector<anychat::Message>& msgs) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    AnyChatMessageList_C list{};
                    fillMessageArray(msgs, &list.items, &list.count);
                    callback_copy.on_success(callback_copy.userdata, &list);
                    anychat_free_message_list(&list);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeMessageError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_message_mark_read(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id || !message_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->markAsRead(conv_id, message_id, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_get_offline(
    AnyChatMessageHandle handle,
    int64_t last_seq,
    int limit,
    const AnyChatOfflineMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatOfflineMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->getOfflineMessages(
        last_seq,
        limit,
        anychat::AnyChatValueCallback<anychat::MessageOfflineResult>{
            .on_success =
                [callback_copy](const anychat::MessageOfflineResult& result) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    AnyChatOfflineMessageResult_C c_result{};
                    fillMessageArray(result.messages, &c_result.items, &c_result.count);
                    c_result.has_more = result.has_more ? 1 : 0;
                    c_result.next_seq = result.next_seq;

                    callback_copy.on_success(callback_copy.userdata, &c_result);
                    anychat_free_offline_message_result(&c_result);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeMessageError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_message_ack(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_count,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (message_count < 0) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    std::vector<std::string> ids;
    if (message_ids && message_count > 0) {
        ids.reserve(static_cast<size_t>(message_count));
        for (int i = 0; i < message_count; ++i) {
            if (message_ids[i] != nullptr && message_ids[i][0] != '\0') {
                ids.emplace_back(message_ids[i]);
            }
        }
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->ackMessages(conv_id, ids, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_get_group_read_state(
    AnyChatMessageHandle handle,
    const char* group_id,
    const char* message_id,
    const AnyChatGroupMessageReadStateCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!group_id || !message_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupMessageReadStateCallback_C callback_copy = copyCallbackStruct(callback);

    impl->getGroupMessageReadState(
        group_id,
        message_id,
        anychat::AnyChatValueCallback<anychat::GroupMessageReadState>{
            .on_success =
                [callback_copy](const anychat::GroupMessageReadState& state) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    AnyChatGroupMessageReadState_C c_state{};
                    groupReadStateToCStruct(state, &c_state);
                    callback_copy.on_success(callback_copy.userdata, &c_state);
                    anychat_free_group_message_read_state(&c_state);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeMessageError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_message_search(
    AnyChatMessageHandle handle,
    const char* keyword,
    const char* conversation_id,
    int32_t content_type,
    int limit,
    int offset,
    const AnyChatMessageSearchCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!keyword) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (content_type < ANYCHAT_MESSAGE_CONTENT_TYPE_UNSPECIFIED
        || content_type > ANYCHAT_MESSAGE_CONTENT_TYPE_CARD) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageSearchCallback_C callback_copy = copyCallbackStruct(callback);

    impl->searchMessages(
        keyword,
        conversation_id ? conversation_id : "",
        content_type,
        limit,
        offset,
        anychat::AnyChatValueCallback<anychat::MessageSearchResult>{
            .on_success =
                [callback_copy](const anychat::MessageSearchResult& result) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    AnyChatMessageSearchResult_C c_result{};
                    fillMessageArray(result.messages, &c_result.items, &c_result.count);
                    c_result.total = result.total;

                    callback_copy.on_success(callback_copy.userdata, &c_result);
                    anychat_free_message_search_result(&c_result);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeMessageError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_message_recall(
    AnyChatMessageHandle handle,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->recallMessage(message_id, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_delete(
    AnyChatMessageHandle handle,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->deleteMessage(message_id, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_edit(
    AnyChatMessageHandle handle,
    const char* message_id,
    const char* content,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id || !content) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->editMessage(message_id, content, makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_send_typing(
    AnyChatMessageHandle handle,
    const char* conversation_id,
    int typing,
    int ttl_seconds,
    const AnyChatMessageCallback_C* callback
) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conversation_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatMessageCallback_C callback_copy = copyCallbackStruct(callback);

    impl->sendTyping(conversation_id, typing != 0, static_cast<int32_t>(ttl_seconds), makeMessageCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_message_set_listener(AnyChatMessageHandle handle, const AnyChatMessageListener_C* listener) {
    auto* impl = static_cast<anychat::MessageManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        impl->setListener(nullptr);
        return ANYCHAT_OK;
    }

    AnyChatMessageListener_C copied = *listener;
    impl->setListener(std::make_shared<CMessageListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
