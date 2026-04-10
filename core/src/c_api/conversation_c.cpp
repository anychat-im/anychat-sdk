#include "anychat_c/conversation_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <mutex>
#include <unordered_map>
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

struct ConvCallbackState {
    std::mutex mutex;
    void* userdata = nullptr;
    AnyChatConvUpdatedCallback callback = nullptr;
};

} // namespace

static std::mutex g_conv_cb_map_mutex;
static std::unordered_map<anychat::ConversationManager*, ConvCallbackState*> g_conv_cb_map;

static ConvCallbackState* getOrCreateConvState(anychat::ConversationManager* impl) {
    std::lock_guard<std::mutex> lock(g_conv_cb_map_mutex);
    auto it = g_conv_cb_map.find(impl);
    if (it != g_conv_cb_map.end())
        return it->second;
    auto* s = new ConvCallbackState();
    g_conv_cb_map[impl] = s;
    return s;
}

extern "C" {

int anychat_conv_get_list(AnyChatConvHandle handle, void* userdata, AnyChatConvListCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getList([userdata, callback](std::vector<anychat::Conversation> list, std::string err) {
        if (!callback)
            return;
        int count = static_cast<int>(list.size());
        AnyChatConversationList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0
                           ? static_cast<AnyChatConversation_C*>(std::calloc(count, sizeof(AnyChatConversation_C)))
                           : nullptr;
        for (int i = 0; i < count; ++i)
            convToCStruct(list[i], &c_list.items[i]);
        callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
        std::free(c_list.items);
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_get_total_unread(AnyChatConvHandle handle, void* userdata, AnyChatConvTotalUnreadCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getTotalUnread([userdata, callback](int32_t total_unread, std::string err) {
        if (callback) {
            callback(userdata, total_unread, err.empty() ? nullptr : err.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_get(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvInfoCallback callback) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getConversation(conv_id, [userdata, callback](anychat::Conversation conv, std::string err) {
        if (!callback) {
            return;
        }
        if (!err.empty()) {
            callback(userdata, nullptr, err.c_str());
            return;
        }
        AnyChatConversation_C c_conv{};
        convToCStruct(conv, &c_conv);
        callback(userdata, &c_conv, nullptr);
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_mark_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->markRead(conv_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_mark_all_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->markAllRead(conv_id, [userdata, callback](bool ok, std::string err) {
        if (callback) {
            callback(userdata, ok ? 1 : 0, err.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_mark_messages_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_id_count,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id || !message_ids || message_id_count <= 0) {
        anychat_set_last_error("invalid arguments");
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
        anychat_set_last_error("message_ids is empty");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->markMessagesRead(
        conv_id,
        ids,
        [userdata, callback](anychat::ConversationMarkReadResult /*unused*/, std::string err) {
            if (callback) {
                callback(userdata, err.empty() ? 1 : 0, err.c_str());
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_set_pinned(
    AnyChatConvHandle handle,
    const char* conv_id,
    int pinned,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->setPinned(conv_id, pinned != 0, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_set_muted(
    AnyChatConvHandle handle,
    const char* conv_id,
    int muted,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->setMuted(conv_id, muted != 0, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_set_burn_after_reading(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->setBurnAfterReading(conv_id, duration, [userdata, callback](bool ok, std::string err) {
        if (callback) {
            callback(userdata, ok ? 1 : 0, err.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_set_auto_delete(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    void* userdata,
    AnyChatConvCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->setAutoDelete(conv_id, duration, [userdata, callback](bool ok, std::string err) {
        if (callback) {
            callback(userdata, ok ? 1 : 0, err.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_delete(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvCallback callback) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->deleteConv(conv_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_get_message_unread_count(
    AnyChatConvHandle handle,
    const char* conv_id,
    int64_t last_read_seq,
    void* userdata,
    AnyChatConvUnreadStateCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getMessageUnreadCount(
        conv_id,
        last_read_seq,
        [userdata, callback](anychat::ConversationUnreadState state, std::string err) {
            if (!callback) {
                return;
            }
            if (!err.empty()) {
                callback(userdata, nullptr, err.c_str());
                return;
            }
            AnyChatConversationUnreadState_C c_state{};
            c_state.unread_count = state.unread_count;
            c_state.last_message_seq = state.last_message_seq;
            callback(userdata, &c_state, nullptr);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_get_message_read_receipts(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvReadReceiptListCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getMessageReadReceipts(
        conv_id,
        [userdata, callback](std::vector<anychat::ConversationReadReceipt> list, std::string err) {
            if (!callback) {
                return;
            }
            if (!err.empty()) {
                callback(userdata, nullptr, err.c_str());
                return;
            }

            AnyChatConversationReadReceiptList_C c_list{};
            c_list.count = static_cast<int>(list.size());
            c_list.items = c_list.count > 0 ? static_cast<AnyChatConversationReadReceipt_C*>(
                                                 std::calloc(static_cast<size_t>(c_list.count), sizeof(AnyChatConversationReadReceipt_C))
                                             )
                                             : nullptr;

            for (int i = 0; i < c_list.count; ++i) {
                readReceiptToCStruct(list[static_cast<size_t>(i)], &c_list.items[i]);
            }

            callback(userdata, &c_list, nullptr);
            std::free(c_list.items);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_conv_get_message_sequence(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvSequenceCallback callback
) {
    if (!handle || !handle->impl || !conv_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getMessageSequence(conv_id, [userdata, callback](int64_t current_seq, std::string err) {
        if (callback) {
            callback(userdata, current_seq, err.empty() ? nullptr : err.c_str());
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

void anychat_conv_set_updated_callback(AnyChatConvHandle handle, void* userdata, AnyChatConvUpdatedCallback callback) {
    if (!handle || !handle->impl)
        return;

    ConvCallbackState* state = getOrCreateConvState(handle->impl);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->userdata = userdata;
        state->callback = callback;
    }

    if (callback) {
        handle->impl->setOnConversationUpdated([state](const anychat::Conversation& conv) {
            AnyChatConvUpdatedCallback cb;
            void* ud;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                cb = state->callback;
                ud = state->userdata;
            }
            if (!cb)
                return;
            AnyChatConversation_C c_conv{};
            convToCStruct(conv, &c_conv);
            cb(ud, &c_conv);
        });
    } else {
        handle->impl->setOnConversationUpdated(nullptr);
    }
}

} // extern "C"
