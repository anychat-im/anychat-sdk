#include "anychat_c/message_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

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

    /* Heap-allocate content; caller must free via anychat_free_message(). */
    dst->content = anychat_strdup(src.content.c_str());
}

/* ---- Per-handle callback state ---- */
struct MsgCallbackState {
    std::mutex mutex;
    void* userdata = nullptr;
    AnyChatMessageReceivedCallback callback = nullptr;
};

} // namespace

/* We store one callback state per MessageManager handle.
 * Since handles are embedded in AnyChatClient_T and never re-created,
 * we attach extra state via a map keyed on the impl pointer. */
#include <unordered_map>
static std::mutex g_msg_cb_map_mutex;
static std::unordered_map<anychat::MessageManager*, MsgCallbackState*> g_msg_cb_map;

static MsgCallbackState* getOrCreateState(anychat::MessageManager* impl) {
    std::lock_guard<std::mutex> lock(g_msg_cb_map_mutex);
    auto it = g_msg_cb_map.find(impl);
    if (it != g_msg_cb_map.end())
        return it->second;
    auto* s = new MsgCallbackState();
    g_msg_cb_map[impl] = s;
    return s;
}

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
        if (callback)
            callback(userdata, success ? 1 : 0, error.c_str());
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
            if (!callback)
                return;
            int count = static_cast<int>(msgs.size());
            AnyChatMessageList_C c_list{};
            c_list.count = count;
            c_list.items =
                count > 0 ? static_cast<AnyChatMessage_C*>(std::calloc(count, sizeof(AnyChatMessage_C))) : nullptr;
            for (int i = 0; i < count; ++i)
                messageToCStruct(msgs[i], &c_list.items[i]);
            callback(userdata, &c_list, error.empty() ? nullptr : error.c_str());
            /* Free items and their heap-allocated content fields. */
            anychat_free_message_list(&c_list);
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
        if (callback)
            callback(userdata, success ? 1 : 0, error.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

void anychat_message_set_received_callback(
    AnyChatMessageHandle handle,
    void* userdata,
    AnyChatMessageReceivedCallback callback
) {
    if (!handle || !handle->impl)
        return;

    MsgCallbackState* state = getOrCreateState(handle->impl);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->userdata = userdata;
        state->callback = callback;
    }

    if (callback) {
        handle->impl->setOnMessageReceived([state](const anychat::Message& msg) {
            AnyChatMessageReceivedCallback cb;
            void* ud;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                cb = state->callback;
                ud = state->userdata;
            }
            if (!cb)
                return;
            AnyChatMessage_C c_msg{};
            messageToCStruct(msg, &c_msg);
            cb(ud, &c_msg);
            /* Free the heap-allocated content field. */
            std::free(c_msg.content);
        });
    } else {
        handle->impl->setOnMessageReceived(nullptr);
    }
}

} // extern "C"
