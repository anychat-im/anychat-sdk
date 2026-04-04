#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatCallCallback)(
    void* userdata,
    int success,
    const AnyChatCallSession_C* session,
    const char* error
);

typedef void (*AnyChatCallListCallback)(void* userdata, const AnyChatCallList_C* list, const char* error);

typedef void (*AnyChatMeetingCallback)(
    void* userdata,
    int success,
    const AnyChatMeetingRoom_C* room,
    const char* error
);

typedef void (*AnyChatMeetingListCallback)(void* userdata, const AnyChatMeetingList_C* list, const char* error);

typedef void (*AnyChatCallResultCallback)(void* userdata, int success, const char* error);

/* Fired when an incoming call arrives. */
typedef void (*AnyChatIncomingCallCallback)(void* userdata, const AnyChatCallSession_C* session);

/* Fired when the status of an ongoing call changes. */
typedef void (*AnyChatCallStatusChangedCallback)(
    void* userdata,
    const char* call_id,
    int status /* ANYCHAT_CALL_STATUS_* */
);

/* ---- One-to-one call operations ---- */

/* call_type: ANYCHAT_CALL_AUDIO or ANYCHAT_CALL_VIDEO */
ANYCHAT_C_API int anychat_call_initiate_call(
    AnyChatCallHandle handle,
    const char* callee_id,
    int call_type,
    void* userdata,
    AnyChatCallCallback callback
);

ANYCHAT_C_API int
anychat_call_join_call(AnyChatCallHandle handle, const char* call_id, void* userdata, AnyChatCallCallback callback);

ANYCHAT_C_API int anychat_call_reject_call(
    AnyChatCallHandle handle,
    const char* call_id,
    void* userdata,
    AnyChatCallResultCallback callback
);

ANYCHAT_C_API int
anychat_call_end_call(AnyChatCallHandle handle, const char* call_id, void* userdata, AnyChatCallResultCallback callback);

ANYCHAT_C_API int anychat_call_get_call_session(
    AnyChatCallHandle handle,
    const char* call_id,
    void* userdata,
    AnyChatCallCallback callback
);

ANYCHAT_C_API int anychat_call_get_call_logs(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    void* userdata,
    AnyChatCallListCallback callback
);

/* ---- Meeting operations ---- */

/* password: pass NULL or empty string for a meeting without password. */
ANYCHAT_C_API int anychat_call_create_meeting(
    AnyChatCallHandle handle,
    const char* title,
    const char* password,
    int max_participants,
    void* userdata,
    AnyChatMeetingCallback callback
);

ANYCHAT_C_API int anychat_call_join_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const char* password,
    void* userdata,
    AnyChatMeetingCallback callback
);

ANYCHAT_C_API int anychat_call_end_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    void* userdata,
    AnyChatCallResultCallback callback
);

ANYCHAT_C_API int
anychat_call_get_meeting(AnyChatCallHandle handle, const char* room_id, void* userdata, AnyChatMeetingCallback callback);

ANYCHAT_C_API int anychat_call_list_meetings(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    void* userdata,
    AnyChatMeetingListCallback callback
);

/* ---- Incoming event handlers ---- */

ANYCHAT_C_API void
anychat_call_set_incoming_call_callback(AnyChatCallHandle handle, void* userdata, AnyChatIncomingCallCallback callback);

ANYCHAT_C_API void anychat_call_set_call_status_changed_callback(
    AnyChatCallHandle handle,
    void* userdata,
    AnyChatCallStatusChangedCallback callback
);

#ifdef __cplusplus
}
#endif
