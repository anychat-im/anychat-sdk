#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatFriendListCallback)(void* userdata, const AnyChatFriendList_C* list, const char* error);

typedef void (*AnyChatFriendRequestListCallback)(
    void* userdata,
    const AnyChatFriendRequestList_C* list,
    const char* error
);
typedef void (*AnyChatBlacklistListCallback)(void* userdata, const AnyChatBlacklistList_C* list, const char* error);

typedef void (*AnyChatFriendCallback)(void* userdata, int success, const char* error);

/* ---- Incoming event callbacks ---- */

typedef void (*AnyChatFriendAddedCallback)(void* userdata, const AnyChatFriend_C* friend_info);
typedef void (*AnyChatFriendDeletedCallback)(void* userdata, const char* user_id);
typedef void (*AnyChatFriendInfoChangedCallback)(void* userdata, const AnyChatFriend_C* friend_info);
typedef void (*AnyChatBlacklistAddedCallback)(void* userdata, const AnyChatBlacklistItem_C* item);
typedef void (*AnyChatBlacklistRemovedCallback)(void* userdata, const char* blocked_user_id);
typedef void (*AnyChatFriendRequestReceivedCallback)(void* userdata, const AnyChatFriendRequest_C* request);
typedef void (*AnyChatFriendRequestDeletedCallback)(void* userdata, const AnyChatFriendRequest_C* request);
typedef void (*AnyChatFriendRequestAcceptedCallback)(void* userdata, const AnyChatFriendRequest_C* request);
typedef void (*AnyChatFriendRequestRejectedCallback)(void* userdata, const AnyChatFriendRequest_C* request);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatFriendAddedCallback on_friend_added;
    AnyChatFriendDeletedCallback on_friend_deleted;
    AnyChatFriendInfoChangedCallback on_friend_info_changed;
    AnyChatBlacklistAddedCallback on_blacklist_added;
    AnyChatBlacklistRemovedCallback on_blacklist_removed;
    AnyChatFriendRequestReceivedCallback on_friend_request_received;
    AnyChatFriendRequestDeletedCallback on_friend_request_deleted;
    AnyChatFriendRequestAcceptedCallback on_friend_request_accepted;
    AnyChatFriendRequestRejectedCallback on_friend_request_rejected;
} AnyChatFriendListener_C;

/* ---- Friend operations ---- */

ANYCHAT_C_API int
anychat_friend_get_list(AnyChatFriendHandle handle, void* userdata, AnyChatFriendListCallback callback);

ANYCHAT_C_API int anychat_friend_send_request(
    AnyChatFriendHandle handle,
    const char* to_user_id,
    const char* message,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_send_request_with_source(
    AnyChatFriendHandle handle,
    const char* to_user_id,
    const char* message,
    const char* source,
    void* userdata,
    AnyChatFriendCallback callback
);

/* accept: 1 to accept, 0 to reject */
ANYCHAT_C_API int anychat_friend_handle_request(
    AnyChatFriendHandle handle,
    int64_t request_id,
    int accept,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_get_pending_requests(
    AnyChatFriendHandle handle,
    void* userdata,
    AnyChatFriendRequestListCallback callback
);

/* request_type: "received" | "sent", NULL defaults to "received" */
ANYCHAT_C_API int anychat_friend_get_requests(
    AnyChatFriendHandle handle,
    const char* request_type,
    void* userdata,
    AnyChatFriendRequestListCallback callback
);

ANYCHAT_C_API int anychat_friend_delete(
    AnyChatFriendHandle handle,
    const char* friend_id,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_update_remark(
    AnyChatFriendHandle handle,
    const char* friend_id,
    const char* remark,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_add_to_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_remove_from_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    void* userdata,
    AnyChatFriendCallback callback
);

ANYCHAT_C_API int anychat_friend_get_blacklist(
    AnyChatFriendHandle handle,
    void* userdata,
    AnyChatBlacklistListCallback callback
);

/* ---- Incoming event listener ----
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int
anychat_friend_set_listener(AnyChatFriendHandle handle, const AnyChatFriendListener_C* listener);

#ifdef __cplusplus
}
#endif
