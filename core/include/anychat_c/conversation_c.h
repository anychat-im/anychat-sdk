#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatConvListCallback)(void* userdata, const AnyChatConversationList_C* list, const char* error);

typedef void (*AnyChatConvCallback)(void* userdata, int success, const char* error);

typedef void (*AnyChatConvInfoCallback)(void* userdata, const AnyChatConversation_C* conversation, const char* error);

typedef void (*AnyChatConvTotalUnreadCallback)(void* userdata, int32_t total_unread, const char* error);

typedef void (*AnyChatConvUnreadStateCallback)(
    void* userdata,
    const AnyChatConversationUnreadState_C* unread_state,
    const char* error
);

typedef void (*AnyChatConvReadReceiptListCallback)(
    void* userdata,
    const AnyChatConversationReadReceiptList_C* list,
    const char* error
);

typedef void (*AnyChatConvSequenceCallback)(void* userdata, int64_t current_seq, const char* error);

/* Fired when any conversation is created or updated. */
typedef void (*AnyChatConvUpdatedCallback)(void* userdata, const AnyChatConversation_C* conversation);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatConvUpdatedCallback on_conversation_updated;
} AnyChatConvListener_C;

/* ---- Conversation operations ---- */

/* Return cached + DB list (pinned first, then by last_msg_time desc). */
ANYCHAT_C_API int anychat_conv_get_list(AnyChatConvHandle handle, void* userdata, AnyChatConvListCallback callback);

/* Get total unread count across all conversations. */
ANYCHAT_C_API int
anychat_conv_get_total_unread(AnyChatConvHandle handle, void* userdata, AnyChatConvTotalUnreadCallback callback);

/* Get one conversation by ID. */
ANYCHAT_C_API int
anychat_conv_get(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvInfoCallback callback);

/* Mark a conversation as read. */
ANYCHAT_C_API int
anychat_conv_mark_read(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvCallback callback);

/* Mark all messages as read for a conversation (POST /read-all). */
ANYCHAT_C_API int
anychat_conv_mark_all_read(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvCallback callback);

/* Batch mark messages read by message IDs. */
ANYCHAT_C_API int anychat_conv_mark_messages_read(
    AnyChatConvHandle handle,
    const char* conv_id,
    const char* const* message_ids,
    int message_id_count,
    void* userdata,
    AnyChatConvCallback callback
);

/* Pin or unpin a conversation (pinned = 1, unpinned = 0). */
ANYCHAT_C_API int anychat_conv_set_pinned(
    AnyChatConvHandle handle,
    const char* conv_id,
    int pinned,
    void* userdata,
    AnyChatConvCallback callback
);

/* Mute or unmute a conversation (muted = 1, unmuted = 0). */
ANYCHAT_C_API int anychat_conv_set_muted(
    AnyChatConvHandle handle,
    const char* conv_id,
    int muted,
    void* userdata,
    AnyChatConvCallback callback
);

/* Set burn-after-reading duration in seconds (0 = disabled). */
ANYCHAT_C_API int anychat_conv_set_burn_after_reading(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    void* userdata,
    AnyChatConvCallback callback
);

/* Set auto-delete duration in seconds (0 = disabled). */
ANYCHAT_C_API int anychat_conv_set_auto_delete(
    AnyChatConvHandle handle,
    const char* conv_id,
    int32_t duration,
    void* userdata,
    AnyChatConvCallback callback
);

/* Delete a conversation (local + server). */
ANYCHAT_C_API int
anychat_conv_delete(AnyChatConvHandle handle, const char* conv_id, void* userdata, AnyChatConvCallback callback);

/* Get unread count in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_unread_count(
    AnyChatConvHandle handle,
    const char* conv_id,
    int64_t last_read_seq,
    void* userdata,
    AnyChatConvUnreadStateCallback callback
);

/* Get message read receipts in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_read_receipts(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvReadReceiptListCallback callback
);

/* Get latest message sequence in one conversation. */
ANYCHAT_C_API int anychat_conv_get_message_sequence(
    AnyChatConvHandle handle,
    const char* conv_id,
    void* userdata,
    AnyChatConvSequenceCallback callback
);

/* Register conversation notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_conv_set_listener(AnyChatConvHandle handle, const AnyChatConvListener_C* listener);

#ifdef __cplusplus
}
#endif
