#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatMsgListSuccessCallback)(void* userdata, const AnyChatMessageList_C* list);
typedef void (*AnyChatMsgOfflineSuccessCallback)(void* userdata, const AnyChatOfflineMessageResult_C* result);
typedef void (*AnyChatMsgSearchSuccessCallback)(void* userdata, const AnyChatMessageSearchResult_C* result);
typedef void (*AnyChatMsgGroupReadStateSuccessCallback)(void* userdata, const AnyChatGroupMessageReadState_C* state);

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatMessageCallback_C;

typedef struct {
    void* userdata;
    AnyChatMsgListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatMessageListCallback_C;

typedef struct {
    void* userdata;
    AnyChatMsgOfflineSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatOfflineMessageCallback_C;

typedef struct {
    void* userdata;
    AnyChatMsgSearchSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatMessageSearchCallback_C;

typedef struct {
    void* userdata;
    AnyChatMsgGroupReadStateSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupMessageReadStateCallback_C;

/* Invoked on the SDK's internal thread each time a new message arrives. */
typedef void (*AnyChatMessageReceivedCallback)(void* userdata, const AnyChatMessage_C* message);
typedef void (*AnyChatMessageReadReceiptCallback)(void* userdata, const AnyChatMessageReadReceiptEvent_C* event);
typedef void (*AnyChatMessageTypingCallback)(void* userdata, const AnyChatMessageTypingEvent_C* event);

typedef struct {
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
    const AnyChatMessageCallback_C* callback
);

/* Fetch message history before a given timestamp.
 * before_timestamp_ms: 0 means "fetch the most recent messages".
 * limit: maximum number of messages to return. */
ANYCHAT_C_API int anychat_message_get_history(
    AnyChatMessageHandle handle,
    const char* conv_id,
    int64_t before_timestamp_ms,
    int limit,
    const AnyChatMessageListCallback_C* callback
);

/* Mark a message as read.
 * Returns ANYCHAT_OK if the request was dispatched. */
ANYCHAT_C_API int anychat_message_mark_read(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
);

/* Fetch offline messages after the given sequence. */
ANYCHAT_C_API int anychat_message_get_offline(
    AnyChatMessageHandle handle,
    int64_t last_seq,
    int limit,
    const AnyChatOfflineMessageCallback_C* callback
);

/* Ack one or more read messages in a conversation. */
ANYCHAT_C_API int anychat_message_ack(
    AnyChatMessageHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_count,
    const AnyChatMessageCallback_C* callback
);

/* Query group message read state. */
ANYCHAT_C_API int anychat_message_get_group_read_state(
    AnyChatMessageHandle handle,
    const char* group_id,
    const char* message_id,
    const AnyChatGroupMessageReadStateCallback_C* callback
);

/* Search messages by keyword in a conversation scope. */
ANYCHAT_C_API int anychat_message_search(
    AnyChatMessageHandle handle,
    const char* keyword,
    const char* conversation_id,
    int32_t content_type,
    int limit,
    int offset,
    const AnyChatMessageSearchCallback_C* callback
);

/* Recall / delete / edit messages. */
ANYCHAT_C_API int anychat_message_recall(
    AnyChatMessageHandle handle,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
);

ANYCHAT_C_API int anychat_message_delete(
    AnyChatMessageHandle handle,
    const char* message_id,
    const AnyChatMessageCallback_C* callback
);

ANYCHAT_C_API int anychat_message_edit(
    AnyChatMessageHandle handle,
    const char* message_id,
    const char* content,
    const AnyChatMessageCallback_C* callback
);

/* Send typing status via WebSocket. */
ANYCHAT_C_API int anychat_message_send_typing(
    AnyChatMessageHandle handle,
    const char* conversation_id,
    int typing,
    int ttl_seconds,
    const AnyChatMessageCallback_C* callback
);

/* ---- Incoming message listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_message_set_listener(AnyChatMessageHandle handle, const AnyChatMessageListener_C* listener);

#ifdef __cplusplus
}
#endif
