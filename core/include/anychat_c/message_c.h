#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatMessageCallback)(void* userdata, int success, const char* error);

typedef void (*AnyChatMessageListCallback)(void* userdata, const AnyChatMessageList_C* list, const char* error);
typedef void (*AnyChatOfflineMessageCallback)(
    void* userdata,
    const AnyChatOfflineMessageResult_C* result,
    const char* error
);
typedef void (*AnyChatMessageSearchCallback)(
    void* userdata,
    const AnyChatMessageSearchResult_C* result,
    const char* error
);
typedef void (*AnyChatGroupMessageReadStateCallback)(
    void* userdata,
    const AnyChatGroupMessageReadState_C* state,
    const char* error
);

/* Invoked on the SDK's internal thread each time a new message arrives. */
typedef void (*AnyChatMessageReceivedCallback)(void* userdata, const AnyChatMessage_C* message);
typedef void (*AnyChatMessageReadReceiptCallback)(void* userdata, const AnyChatMessageReadReceiptEvent_C* event);
typedef void (*AnyChatMessageTypingCallback)(void* userdata, const AnyChatMessageTypingEvent_C* event);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatMessageReceivedCallback on_message_received;
    AnyChatMessageReadReceiptCallback on_message_read_receipt;
    AnyChatMessageReceivedCallback on_message_recalled;
    AnyChatMessageReceivedCallback on_message_deleted;
    AnyChatMessageReceivedCallback on_message_edited;
    AnyChatMessageTypingCallback on_message_typing;
    AnyChatMessageReceivedCallback on_message_mentioned;
} AnyChatMessageListener_C;

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

/* Fetch offline messages after the given sequence. */
ANYCHAT_C_API int anychat_message_get_offline(
    AnyChatMessageHandle handle,
    int64_t last_seq,
    int limit,
    void* userdata,
    AnyChatOfflineMessageCallback callback
);

/* Ack one or more read messages in a conversation. */
ANYCHAT_C_API int anychat_message_ack(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_count,
    void* userdata,
    AnyChatMessageCallback callback
);

/* Query group message read state. */
ANYCHAT_C_API int anychat_message_get_group_read_state(
    AnyChatMessageHandle handle,
    const char* group_id,
    const char* message_id,
    void* userdata,
    AnyChatGroupMessageReadStateCallback callback
);

/* Search messages by keyword in a conversation scope. */
ANYCHAT_C_API int anychat_message_search(
    AnyChatMessageHandle handle,
    const char* keyword,
    const char* conversation_id,
    const char* content_type,
    int limit,
    int offset,
    void* userdata,
    AnyChatMessageSearchCallback callback
);

/* Recall / delete / edit messages. */
ANYCHAT_C_API int anychat_message_recall(
    AnyChatMessageHandle handle,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
);

ANYCHAT_C_API int anychat_message_delete(
    AnyChatMessageHandle handle,
    const char* message_id,
    void* userdata,
    AnyChatMessageCallback callback
);

ANYCHAT_C_API int anychat_message_edit(
    AnyChatMessageHandle handle,
    const char* message_id,
    const char* content,
    void* userdata,
    AnyChatMessageCallback callback
);

/* Send typing status via WebSocket. */
ANYCHAT_C_API int anychat_message_send_typing(
    AnyChatMessageHandle handle,
    const char* conversation_id,
    int typing,
    int ttl_seconds,
    void* userdata,
    AnyChatMessageCallback callback
);

/* ---- Incoming message listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_message_set_listener(AnyChatMessageHandle handle, const AnyChatMessageListener_C* listener);

#ifdef __cplusplus
}
#endif
