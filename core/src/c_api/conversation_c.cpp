#include "anychat/conversation.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

void convToCStruct(const anychat::Conversation& src, AnyChatConversation_C* dst) {
    anychat_strlcpy(dst->conv_id, src.conv_id.c_str(), sizeof(dst->conv_id));
    anychat_strlcpy(dst->target_id, src.target_id.c_str(), sizeof(dst->target_id));
    anychat_strlcpy(dst->last_msg_id, src.last_msg_id.c_str(), sizeof(dst->last_msg_id));
    anychat_strlcpy(dst->last_msg_text, src.last_msg_text.c_str(), sizeof(dst->last_msg_text));
    dst->conv_type = (src.conv_type == anychat::ConversationType::Private) ? ANYCHAT_CONV_PRIVATE : ANYCHAT_CONV_GROUP;
    dst->last_msg_time_ms = src.last_msg_time_ms;
    dst->unread_count = src.unread_count;
    dst->is_pinned = src.is_pinned ? 1 : 0;
    dst->is_muted = src.is_muted ? 1 : 0;
    dst->burn_after_reading = src.burn_after_reading;
    dst->auto_delete_duration = src.auto_delete_duration;
    dst->updated_at_ms = src.updated_at_ms;
}

void readReceiptToCStruct(const anychat::ConversationReadReceipt& src, AnyChatConversationReadReceipt_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    dst->last_read_seq = src.last_read_seq;
    anychat_strlcpy(dst->last_read_message_id, src.last_read_message_id.c_str(), sizeof(dst->last_read_message_id));
    dst->read_at_ms = src.read_at_ms;
}

class CConversationListener final : public anychat::ConversationListener {
public:
    explicit CConversationListener(const AnyChatConvListener_C& listener)
        : listener_(listener) {}

    void onConversationUpdated(const anychat::Conversation& conv) override {
        if (!listener_.on_conversation_updated) {
            return;
        }
        AnyChatConversation_C c_conv{};
        convToCStruct(conv, &c_conv);
        listener_.on_conversation_updated(listener_.userdata, &c_conv);
    }

private:
    AnyChatConvListener_C listener_{};
};

void markReadResultToCStruct(const anychat::ConversationMarkReadResult& src, AnyChatConversationMarkReadResult_C* dst) {
    dst->accepted_count = static_cast<int>(src.accepted_ids.size());
    dst->accepted_ids = dst->accepted_count > 0
                            ? static_cast<char**>(std::calloc(dst->accepted_count, sizeof(char*)))
                            : nullptr;
    for (int i = 0; i < dst->accepted_count; ++i) {
        dst->accepted_ids[i] = anychat_strdup(src.accepted_ids[static_cast<size_t>(i)].c_str());
    }

    dst->ignored_count = static_cast<int>(src.ignored_ids.size());
    dst->ignored_ids =
        dst->ignored_count > 0 ? static_cast<char**>(std::calloc(dst->ignored_count, sizeof(char*))) : nullptr;
    for (int i = 0; i < dst->ignored_count; ++i) {
        dst->ignored_ids[i] = anychat_strdup(src.ignored_ids[static_cast<size_t>(i)].c_str());
    }

    dst->advanced_last_read_seq = src.advanced_last_read_seq;
}

void freeMarkReadResultStruct(AnyChatConversationMarkReadResult_C* result) {
    anychat_free_conversation_mark_read_result(result);
}

template<typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback && callback->struct_size < sizeof(CallbackStruct)) {
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
void invokeConvError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeConvCallback(const AnyChatConvCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeConvError(callback, code, error);
    };
    return result;
}

template<typename CallbackStruct, typename Value>
anychat::AnyChatValueCallback<Value> makeConvScalarValueCallback(const CallbackStruct& callback) {
    anychat::AnyChatValueCallback<Value> result{};
    result.on_success = [callback](const Value& value) {
        if (!callback.on_success) {
            return;
        }
        callback.on_success(callback.userdata, value);
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeConvError(callback, code, error);
    };
    return result;
}

template<typename CallbackStruct, typename Value, typename CValue, typename ConvertFn>
anychat::AnyChatValueCallback<Value> makeConvPtrValueCallback(const CallbackStruct& callback, ConvertFn convert) {
    anychat::AnyChatValueCallback<Value> result{};
    result.on_success = [callback, convert](const Value& value) {
        if (!callback.on_success) {
            return;
        }
        CValue converted{};
        convert(value, &converted);
        callback.on_success(callback.userdata, &converted);
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeConvError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

int anychat_conv_get_list(AnyChatConvHandle handle, const AnyChatConvListCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvListCallback_C callback_copy = copyCallbackStruct(callback);

    anychat::AnyChatValueCallback<std::vector<anychat::Conversation>> cb{};
    cb.on_success = [callback_copy](const std::vector<anychat::Conversation>& list) {
        if (!callback_copy.on_success) {
            return;
        }
        const int count = static_cast<int>(list.size());
        AnyChatConversationList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0
                           ? static_cast<AnyChatConversation_C*>(std::calloc(count, sizeof(AnyChatConversation_C)))
                           : nullptr;
        for (int i = 0; i < count; ++i) {
            convToCStruct(list[static_cast<size_t>(i)], &c_list.items[i]);
        }
        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        invokeConvError(callback_copy, code, error);
    };
    handle->impl->getConversationList(std::move(cb));

    return ANYCHAT_OK;
}

int anychat_conv_get_total_unread(AnyChatConvHandle handle, const AnyChatConvTotalUnreadCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvTotalUnreadCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getTotalUnread(makeConvScalarValueCallback<AnyChatConvTotalUnreadCallback_C, int32_t>(callback_copy));

    return ANYCHAT_OK;
}

int anychat_conv_get(AnyChatConvHandle handle, const char* conv_id, const AnyChatConvInfoCallback_C* callback) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvInfoCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getConversation(
        conv_id,
        makeConvPtrValueCallback<AnyChatConvInfoCallback_C, anychat::Conversation, AnyChatConversation_C>(
            callback_copy,
            convToCStruct
        )
    );

    return ANYCHAT_OK;
}

int anychat_conv_mark_all_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    const AnyChatConvCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->markAllRead(conv_id, makeConvCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_conv_mark_messages_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_id_count,
    const AnyChatConvMarkReadResultCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id || !message_ids || message_id_count <= 0) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    std::vector<std::string> ids;
    ids.reserve(static_cast<size_t>(message_id_count));
    for (int i = 0; i < message_id_count; ++i) {
        if (message_ids[i] && message_ids[i][0] != '\0') {
            ids.emplace_back(message_ids[i]);
        }
    }

    if (ids.empty()) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvMarkReadResultCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->markMessagesRead(
        conv_id,
        ids,
        anychat::AnyChatValueCallback<anychat::ConversationMarkReadResult>{
            .on_success =
                [callback_copy](const anychat::ConversationMarkReadResult& result) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatConversationMarkReadResult_C c_result{};
                    markReadResultToCStruct(result, &c_result);
                    callback_copy.on_success(callback_copy.userdata, &c_result);
                    freeMarkReadResultStruct(&c_result);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeConvError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_conv_set_pinned(
    AnyChatConvHandle handle,
    const char* conv_id,
    int pinned,
    const AnyChatConvCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->setPinned(conv_id, pinned != 0, makeConvCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_conv_set_muted(
    AnyChatConvHandle handle,
    const char* conv_id,
    int muted,
    const AnyChatConvCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->setMuted(conv_id, muted != 0, makeConvCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_conv_set_burn_after_reading(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    const AnyChatConvCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->setBurnAfterReading(conv_id, duration, makeConvCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_conv_set_auto_delete(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    const AnyChatConvCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->setAutoDelete(conv_id, duration, makeConvCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_conv_delete(AnyChatConvHandle handle, const char* conv_id, const AnyChatConvCallback_C* callback) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->deleteConversation(conv_id, makeConvCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_conv_get_message_unread_count(
    AnyChatConvHandle handle,
    const char* conv_id,
    int64_t last_read_seq,
    const AnyChatConvUnreadStateCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvUnreadStateCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getMessageUnreadCount(
        conv_id,
        last_read_seq,
        anychat::AnyChatValueCallback<anychat::ConversationUnreadState>{
            .on_success =
                [callback_copy](const anychat::ConversationUnreadState& state) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatConversationUnreadState_C c_state{};
                    c_state.unread_count = state.unread_count;
                    c_state.last_message_seq = state.last_message_seq;
                    callback_copy.on_success(callback_copy.userdata, &c_state);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeConvError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_conv_get_message_read_receipts(
    AnyChatConvHandle handle,
    const char* conv_id,
    const AnyChatConvReadReceiptListCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvReadReceiptListCallback_C callback_copy = copyCallbackStruct(callback);
    anychat::AnyChatValueCallback<std::vector<anychat::ConversationReadReceipt>> cb{};
    cb.on_success = [callback_copy](const std::vector<anychat::ConversationReadReceipt>& list) {
        if (!callback_copy.on_success) {
            return;
        }

        AnyChatConversationReadReceiptList_C c_list{};
        c_list.count = static_cast<int>(list.size());
        c_list.items = c_list.count > 0
                           ? static_cast<AnyChatConversationReadReceipt_C*>(
                                 std::calloc(static_cast<size_t>(c_list.count), sizeof(AnyChatConversationReadReceipt_C))
                             )
                           : nullptr;

        for (int i = 0; i < c_list.count; ++i) {
            readReceiptToCStruct(list[static_cast<size_t>(i)], &c_list.items[i]);
        }

        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        invokeConvError(callback_copy, code, error);
    };
    handle->impl->getMessageReadReceipts(conv_id, std::move(cb));

    return ANYCHAT_OK;
}

int anychat_conv_get_message_sequence(
    AnyChatConvHandle handle,
    const char* conv_id,
    const AnyChatConvSequenceCallback_C* callback
) {
    if (!handle || !handle->impl || !conv_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatConvSequenceCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getMessageSequence(
        conv_id,
        makeConvScalarValueCallback<AnyChatConvSequenceCallback_C, int64_t>(callback_copy)
    );

    return ANYCHAT_OK;
}

int anychat_conv_set_listener(AnyChatConvHandle handle, const AnyChatConvListener_C* listener) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatConvListener_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatConvListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CConversationListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
