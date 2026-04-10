#include "anychat_c/message_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

void messageToCStruct(const anychat::Message& src, AnyChatMessage_C* dst) {
    anychat_strlcpy(dst->message_id, src.message_id.c_str(), sizeof(dst->message_id));
    anychat_strlcpy(dst->local_id, src.local_id.c_str(), sizeof(dst->local_id));
    anychat_strlcpy(dst->conv_id, src.conv_id.c_str(), sizeof(dst->conv_id));
    anychat_strlcpy(dst->sender_id, src.sender_id.c_str(), sizeof(dst->sender_id));
    anychat_strlcpy(dst->content_type, src.content_type.c_str(), sizeof(dst->content_type));
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

} // namespace

extern "C" {

int anychat_message_send_text(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* content,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id || !content) {
        anychat_set_last_error("conv_id and content must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->sendTextMessage(conv_id, content, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_get_history(
    AnyChatMessageHandle handle,
    const char* conv_id,
    int64_t before_timestamp_ms,
    int limit,
    void* userdata,
    AnyChatMessageListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id) {
        anychat_set_last_error("conv_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getHistory(
        conv_id,
        before_timestamp_ms,
        limit,
        [userdata, callback](const std::vector<anychat::Message>& msgs, const std::string& error) {
            if (!callback) {
                return;
            }

            AnyChatMessageList_C list{};
            fillMessageArray(msgs, &list.items, &list.count);
            callback(userdata, &list, error.empty() ? nullptr : error.c_str());
            anychat_free_message_list(&list);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_mark_read(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id || !message_id) {
        anychat_set_last_error("conv_id and message_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->markAsRead(conv_id, message_id, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_get_offline(
    AnyChatMessageHandle handle,
    int64_t last_seq,
    int limit,
    void* userdata,
    AnyChatOfflineMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getOfflineMessages(last_seq, limit, [userdata, callback](anychat::MessageOfflineResult result, std::string err) {
        if (!callback) {
            return;
        }

        AnyChatOfflineMessageResult_C c_result{};
        fillMessageArray(result.messages, &c_result.items, &c_result.count);
        c_result.has_more = result.has_more ? 1 : 0;
        c_result.next_seq = result.next_seq;

        callback(userdata, &c_result, err.empty() ? nullptr : err.c_str());
        anychat_free_offline_message_result(&c_result);
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_ack(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_count,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conv_id) {
        anychat_set_last_error("conv_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (message_count < 0) {
        anychat_set_last_error("message_count must be >= 0");
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

    handle->impl->ackMessages(conv_id, ids, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_get_group_read_state(
    AnyChatMessageHandle handle,
    const char* group_id,
    const char* message_id,
    void* userdata,
    AnyChatGroupMessageReadStateCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!group_id || !message_id) {
        anychat_set_last_error("group_id and message_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getGroupMessageReadState(
        group_id,
        message_id,
        [userdata, callback](anychat::GroupMessageReadState state, std::string err) {
            if (!callback) {
                return;
            }

            AnyChatGroupMessageReadState_C c_state{};
            groupReadStateToCStruct(state, &c_state);
            callback(userdata, &c_state, err.empty() ? nullptr : err.c_str());
            anychat_free_group_message_read_state(&c_state);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_search(
    AnyChatMessageHandle handle,
    const char* keyword,
    const char* conversation_id,
    const char* content_type,
    int limit,
    int offset,
    void* userdata,
    AnyChatMessageSearchCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!keyword) {
        anychat_set_last_error("keyword must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->searchMessages(
        keyword,
        conversation_id ? conversation_id : "",
        content_type ? content_type : "",
        limit,
        offset,
        [userdata, callback](anychat::MessageSearchResult result, std::string err) {
            if (!callback) {
                return;
            }

            AnyChatMessageSearchResult_C c_result{};
            fillMessageArray(result.messages, &c_result.items, &c_result.count);
            c_result.total = result.total;

            callback(userdata, &c_result, err.empty() ? nullptr : err.c_str());
            anychat_free_message_search_result(&c_result);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_recall(
    AnyChatMessageHandle handle,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id) {
        anychat_set_last_error("message_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->recallMessage(message_id, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_delete(
    AnyChatMessageHandle handle,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id) {
        anychat_set_last_error("message_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->deleteMessage(message_id, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_edit(
    AnyChatMessageHandle handle,
    const char* message_id,
    const char* content,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!message_id || !content) {
        anychat_set_last_error("message_id and content must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->editMessage(message_id, content, [userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, error.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_send_typing(
    AnyChatMessageHandle handle,
    const char* conversation_id,
    int typing,
    int ttl_seconds,
    void* userdata,
    AnyChatMessageCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!conversation_id) {
        anychat_set_last_error("conversation_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->sendTyping(
        conversation_id,
        typing != 0,
        static_cast<int32_t>(ttl_seconds),
        [userdata, callback](bool success, const std::string& error) {
            if (callback) {
                callback(userdata, success ? 1 : 0, error.c_str());
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_message_set_listener(AnyChatMessageHandle handle, const AnyChatMessageListener_C* listener) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        anychat_clear_last_error();
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatMessageListener_C)) {
        anychat_set_last_error("listener struct_size is too small");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatMessageListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CMessageListener>(copied));
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
