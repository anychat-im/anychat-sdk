#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatMessageCallback)(void* userdata, int success, const char* error);

typedef void (*AnyChatMessageListCallback)(void* userdata, const AnyChatMessageList_C* list, const char* error);

/* Invoked on the SDK's internal thread each time a new message arrives. */
typedef void (*AnyChatMessageReceivedCallback)(void* userdata, const AnyChatMessage_C* message);

/* ---- Message operations ---- */

/* Send a plain-text message to a conversation.
 * Returns ANYCHAT_OK if the request was dispatched. */
ANYCHAT_C_API int anychat_message_send_text(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* content,
    void* userdata,
    AnyChatMessageCallback callback
);

/* Fetch message history before a given timestamp.
 * before_timestamp_ms: 0 means "fetch the most recent messages".
 * limit: maximum number of messages to return. */
ANYCHAT_C_API int anychat_message_get_history(
    AnyChatMessageHandle handle,
    const char* conv_id,
    int64_t before_timestamp_ms,
    int limit,
    void* userdata,
    AnyChatMessageListCallback callback
);

/* Mark a message as read.
 * Returns ANYCHAT_OK if the request was dispatched. */
ANYCHAT_C_API int anychat_message_mark_read(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
);

/* ---- Incoming message handler ---- */

/* Register a callback invoked for every incoming message.
 * Only one callback can be registered at a time; pass NULL to clear. */
ANYCHAT_C_API void anychat_message_set_received_callback(
    AnyChatMessageHandle handle,
    void* userdata,
    AnyChatMessageReceivedCallback callback
);

#ifdef __cplusplus
}
#endif
