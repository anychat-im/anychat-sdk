#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatConvListSuccessCallback)(void* userdata, const AnyChatConversationList_C* list);
typedef void (*AnyChatConvInfoSuccessCallback)(void* userdata, const AnyChatConversation_C* conversation);
typedef void (*AnyChatConvTotalUnreadSuccessCallback)(void* userdata, int32_t total_unread);
typedef void (*AnyChatConvUnreadStateSuccessCallback)(void* userdata, const AnyChatConversationUnreadState_C* unread_state);
typedef void (*AnyChatConvReadReceiptListSuccessCallback)(
    void* userdata,
    const AnyChatConversationReadReceiptList_C* list
);
typedef void (*AnyChatConvSequenceSuccessCallback)(void* userdata, int64_t current_seq);
typedef void (*AnyChatConvMarkReadResultSuccessCallback)(void* userdata, const AnyChatConversationMarkReadResult_C* result);

typedef struct {
    void* userdata;
    AnyChatConvListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvListCallback_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvInfoSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvInfoCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvTotalUnreadSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvTotalUnreadCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvUnreadStateSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvUnreadStateCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvReadReceiptListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvReadReceiptListCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvSequenceSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvSequenceCallback_C;

typedef struct {
    void* userdata;
    AnyChatConvMarkReadResultSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatConvMarkReadResultCallback_C;

/* Fired when any conversation is created or updated. */
typedef void (*AnyChatConvUpdatedCallback)(void* userdata, const AnyChatConversation_C* conversation);

typedef struct {
    void* userdata;
    AnyChatConvUpdatedCallback on_conversation_updated;
} AnyChatConvListener_C;

/* ---- Conversation operations ---- */

/* Return cached + DB list (pinned first, then by last_msg_time desc). */
ANYCHAT_C_API int anychat_conv_get_list(AnyChatConvHandle handle, const AnyChatConvListCallback_C* callback);

/* Get total unread count across all conversations. */
ANYCHAT_C_API int
anychat_conv_get_total_unread(AnyChatConvHandle handle, const AnyChatConvTotalUnreadCallback_C* callback);

/* Get one conversation by ID. */
ANYCHAT_C_API int
anychat_conv_get(AnyChatConvHandle handle, const char* conv_id, const AnyChatConvInfoCallback_C* callback);

/* Mark all messages as read for a conversation (POST /read-all). */
ANYCHAT_C_API int
anychat_conv_mark_all_read(AnyChatConvHandle handle, const char* conv_id, const AnyChatConvCallback_C* callback);

/* Batch mark messages read by message IDs. */
ANYCHAT_C_API int anychat_conv_mark_messages_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_id_count,
    const AnyChatConvMarkReadResultCallback_C* callback
);

/* Pin or unpin a conversation (pinned = 1, unpinned = 0). */
ANYCHAT_C_API int anychat_conv_set_pinned(
    AnyChatConvHandle handle,
    const char* conv_id,
    int pinned,
    const AnyChatConvCallback_C* callback
);

/* Mute or unmute a conversation (muted = 1, unmuted = 0). */
ANYCHAT_C_API int anychat_conv_set_muted(
    AnyChatConvHandle handle,
    const char* conv_id,
    int muted,
    const AnyChatConvCallback_C* callback
);

/* Set burn-after-reading duration in seconds (0 = disabled). */
ANYCHAT_C_API int anychat_conv_set_burn_after_reading(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    const AnyChatConvCallback_C* callback
);

/* Set auto-delete duration in seconds (0 = disabled). */
ANYCHAT_C_API int anychat_conv_set_auto_delete(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    const AnyChatConvCallback_C* callback
);

/* Delete a conversation (local + server). */
ANYCHAT_C_API int
anychat_conv_delete(AnyChatConvHandle handle, const char* conv_id, const AnyChatConvCallback_C* callback);

/* Get unread count in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_unread_count(
    AnyChatConvHandle handle,
    const char* conv_id,
    int64_t last_read_seq,
    const AnyChatConvUnreadStateCallback_C* callback
);

/* Get message read receipts in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_read_receipts(
    AnyChatConvHandle handle,
    const char* conv_id,
    const AnyChatConvReadReceiptListCallback_C* callback
);

/* Get latest message sequence in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_sequence(
    AnyChatConvHandle handle,
    const char* conv_id,
    const AnyChatConvSequenceCallback_C* callback
);

/* Register conversation notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_conv_set_listener(AnyChatConvHandle handle, const AnyChatConvListener_C* listener);

#ifdef __cplusplus
}
#endif
