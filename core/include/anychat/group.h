#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatGroupListSuccessCallback)(void* userdata, const AnyChatGroupList_C* list);
typedef void (*AnyChatGroupInfoSuccessCallback)(void* userdata, const AnyChatGroup_C* group);
typedef void (*AnyChatGroupMemberListSuccessCallback)(void* userdata, const AnyChatGroupMemberList_C* list);
typedef void (*AnyChatGroupJoinRequestListSuccessCallback)(void* userdata, const AnyChatGroupJoinRequestList_C* list);
typedef void (*AnyChatGroupQRCodeSuccessCallback)(void* userdata, const AnyChatGroupQRCode_C* qrcode);

/* Fired when the current user is invited to a group. */
typedef void (*AnyChatGroupInvitedCallback)(void* userdata, const AnyChatGroup_C* group, const char* inviter_id);

/* Fired when group metadata changes. */
typedef void (*AnyChatGroupUpdatedCallback)(void* userdata, const AnyChatGroup_C* group);

typedef struct {
    void* userdata;
    AnyChatGroupInvitedCallback on_group_invited;
    AnyChatGroupUpdatedCallback on_group_updated;
} AnyChatGroupListener_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupCallback_C;

typedef struct {
    void* userdata;
    AnyChatGroupListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupListCallback_C;

typedef struct {
    void* userdata;
    AnyChatGroupInfoSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupInfoCallback_C;

typedef struct {
    void* userdata;
    AnyChatGroupMemberListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupMemberListCallback_C;

typedef struct {
    void* userdata;
    AnyChatGroupJoinRequestListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupJoinRequestListCallback_C;

typedef struct {
    void* userdata;
    AnyChatGroupQRCodeSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatGroupQRCodeCallback_C;

/* ---- Group operations ---- */

ANYCHAT_C_API int anychat_group_get_list(AnyChatGroupHandle handle, const AnyChatGroupListCallback_C* callback);
ANYCHAT_C_API int
anychat_group_get_info(AnyChatGroupHandle handle, const char* group_id, const AnyChatGroupInfoCallback_C* callback);

/* member_ids: NULL-terminated array of user ID strings.
 * member_count: length of the array (excluding the terminating NULL). */
ANYCHAT_C_API int anychat_group_create(
    AnyChatGroupHandle handle,
    const char* name,
    const char* const* member_ids,
    int member_count,
    const AnyChatGroupInfoCallback_C* callback
);

ANYCHAT_C_API int anychat_group_join(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* message,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_invite(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* const* user_ids,
    int user_count,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int
anychat_group_quit(AnyChatGroupHandle handle, const char* group_id, const AnyChatGroupCallback_C* callback);

ANYCHAT_C_API int
anychat_group_disband(AnyChatGroupHandle handle, const char* group_id, const AnyChatGroupCallback_C* callback);

ANYCHAT_C_API int anychat_group_update(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* name,
    const char* avatar_url,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_get_members(
    AnyChatGroupHandle handle,
    const char* group_id,
    int page,
    int page_size,
    const AnyChatGroupMemberListCallback_C* callback
);

ANYCHAT_C_API int anychat_group_remove_member(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_update_member_role(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    int32_t role,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_update_nickname(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* nickname,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_transfer_ownership(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* new_owner_id,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_get_join_requests(
    AnyChatGroupHandle handle,
    const char* group_id,
    int32_t status,
    const AnyChatGroupJoinRequestListCallback_C* callback
);

ANYCHAT_C_API int anychat_group_handle_join_request(
    AnyChatGroupHandle handle,
    const char* group_id,
    int64_t request_id,
    int accept,
    const AnyChatGroupCallback_C* callback
);

ANYCHAT_C_API int anychat_group_get_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    const AnyChatGroupQRCodeCallback_C* callback
);

ANYCHAT_C_API int anychat_group_refresh_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    const AnyChatGroupQRCodeCallback_C* callback
);

/* ---- Incoming event listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_group_set_listener(AnyChatGroupHandle handle, const AnyChatGroupListener_C* listener);

#ifdef __cplusplus
}
#endif
