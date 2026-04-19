#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatCallSessionSuccessCallback)(void* userdata, const AnyChatCallSession_C* session);
typedef void (*AnyChatCallListSuccessCallback)(void* userdata, const AnyChatCallList_C* list);
typedef void (*AnyChatMeetingSuccessCallback)(void* userdata, const AnyChatMeetingRoom_C* room);
typedef void (*AnyChatMeetingListSuccessCallback)(void* userdata, const AnyChatMeetingList_C* list);

typedef struct {
    void* userdata;
    AnyChatCallSessionSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatCallSessionCallback_C;

typedef struct {
    void* userdata;
    AnyChatCallListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatCallListCallback_C;

typedef struct {
    void* userdata;
    AnyChatMeetingSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatMeetingCallback_C;

typedef struct {
    void* userdata;
    AnyChatMeetingListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatMeetingListCallback_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatCallCallback_C;

/* Fired when an incoming call arrives. */
typedef void (*AnyChatIncomingCallCallback)(void* userdata, const AnyChatCallSession_C* session);

/* Fired when the status of an ongoing call changes. */
typedef void (*AnyChatCallStatusChangedCallback)(
    void* userdata,
    const char* call_id,
    int status /* ANYCHAT_CALL_STATUS_* */
);

typedef struct {
    void* userdata;
    AnyChatIncomingCallCallback on_incoming_call;
    AnyChatCallStatusChangedCallback on_call_status_changed;
} AnyChatCallListener_C;

/* ---- One-to-one call operations ---- */

/* call_type: ANYCHAT_CALL_AUDIO or ANYCHAT_CALL_VIDEO */
ANYCHAT_C_API int anychat_call_initiate_call(
    AnyChatCallHandle handle,
    const char* callee_id,
    int call_type,
    const AnyChatCallSessionCallback_C* callback
);

ANYCHAT_C_API int
anychat_call_join_call(AnyChatCallHandle handle, const char* call_id, const AnyChatCallSessionCallback_C* callback);

ANYCHAT_C_API int anychat_call_reject_call(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallCallback_C* callback
);

ANYCHAT_C_API int
anychat_call_end_call(AnyChatCallHandle handle, const char* call_id, const AnyChatCallCallback_C* callback);

ANYCHAT_C_API int anychat_call_get_call_session(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallSessionCallback_C* callback
);

ANYCHAT_C_API int anychat_call_get_call_logs(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    const AnyChatCallListCallback_C* callback
);

/* ---- Meeting operations ---- */

/* password: pass NULL or empty string for a meeting without password. */
ANYCHAT_C_API int anychat_call_create_meeting(
    AnyChatCallHandle handle,
    const char* title,
    const char* password,
    int max_participants,
    const AnyChatMeetingCallback_C* callback
);

ANYCHAT_C_API int anychat_call_join_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const char* password,
    const AnyChatMeetingCallback_C* callback
);

ANYCHAT_C_API int anychat_call_end_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const AnyChatCallCallback_C* callback
);

ANYCHAT_C_API int
anychat_call_get_meeting(AnyChatCallHandle handle, const char* room_id, const AnyChatMeetingCallback_C* callback);

ANYCHAT_C_API int anychat_call_list_meetings(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    const AnyChatMeetingListCallback_C* callback
);

/* ---- Incoming event listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_call_set_listener(AnyChatCallHandle handle, const AnyChatCallListener_C* listener);

#ifdef __cplusplus
}
#endif
