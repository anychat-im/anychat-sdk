#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatGroupListCallback)(void* userdata, const AnyChatGroupList_C* list, const char* error);

typedef void (*AnyChatGroupCallback)(void* userdata, int success, const char* error);
typedef void (*AnyChatGroupInfoCallback)(void* userdata, const AnyChatGroup_C* group, const char* error);

typedef void (*AnyChatGroupMemberCallback)(void* userdata, const AnyChatGroupMemberList_C* list, const char* error);
typedef void (*AnyChatGroupJoinRequestListCallback)(
    void* userdata,
    const AnyChatGroupJoinRequestList_C* list,
    const char* error
);
typedef void (*AnyChatGroupQRCodeCallback)(void* userdata, const AnyChatGroupQRCode_C* qrcode, const char* error);

/* Fired when the current user is invited to a group. */
typedef void (*AnyChatGroupInvitedCallback)(void* userdata, const AnyChatGroup_C* group, const char* inviter_id);

/* Fired when group metadata changes. */
typedef void (*AnyChatGroupUpdatedCallback)(void* userdata, const AnyChatGroup_C* group);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatGroupInvitedCallback on_group_invited;
    AnyChatGroupUpdatedCallback on_group_updated;
} AnyChatGroupListener_C;

/* ---- Group operations ---- */

ANYCHAT_C_API int anychat_group_get_list(AnyChatGroupHandle handle, void* userdata, AnyChatGroupListCallback callback);
ANYCHAT_C_API int
anychat_group_get_info(AnyChatGroupHandle handle, const char* group_id, void* userdata, AnyChatGroupInfoCallback callback);

/* member_ids: NULL-terminated array of user ID strings.
 * member_count: length of the array (excluding the terminating NULL). */
ANYCHAT_C_API int anychat_group_create(
    AnyChatGroupHandle handle,
    const char* name,
    const char* const* member_ids,
    int member_count,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_join(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* message,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_invite(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* const* user_ids,
    int user_count,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int
anychat_group_quit(AnyChatGroupHandle handle, const char* group_id, void* userdata, AnyChatGroupCallback callback);

ANYCHAT_C_API int
anychat_group_disband(AnyChatGroupHandle handle, const char* group_id, void* userdata, AnyChatGroupCallback callback);

ANYCHAT_C_API int anychat_group_update(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* name,
    const char* avatar_url,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_get_members(
    AnyChatGroupHandle handle,
    const char* group_id,
    int page,
    int page_size,
    void* userdata,
    AnyChatGroupMemberCallback callback
);

ANYCHAT_C_API int anychat_group_remove_member(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_update_member_role(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    const char* role,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_update_nickname(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* nickname,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_transfer_ownership(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* new_owner_id,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_get_join_requests(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* status,
    void* userdata,
    AnyChatGroupJoinRequestListCallback callback
);

ANYCHAT_C_API int anychat_group_handle_join_request(
    AnyChatGroupHandle handle,
    const char* group_id,
    int64_t request_id,
    int accept,
    void* userdata,
    AnyChatGroupCallback callback
);

ANYCHAT_C_API int anychat_group_get_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupQRCodeCallback callback
);

ANYCHAT_C_API int anychat_group_refresh_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupQRCodeCallback callback
);

/* ---- Incoming event listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_group_set_listener(AnyChatGroupHandle handle, const AnyChatGroupListener_C* listener);

#ifdef __cplusplus
}
#endif
