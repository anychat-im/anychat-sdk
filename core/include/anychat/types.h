#pragma once

#include "errors.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Opaque handle types ---- */
typedef struct AnyChatClient_T* AnyChatClientHandle;
typedef struct AnyChatAuthManager_T* AnyChatAuthHandle;
typedef struct AnyChatMessage_T* AnyChatMessageHandle;
typedef struct AnyChatConversation_T* AnyChatConvHandle;
typedef struct AnyChatFriend_T* AnyChatFriendHandle;
typedef struct AnyChatGroup_T* AnyChatGroupHandle;
typedef struct AnyChatFile_T* AnyChatFileHandle;
typedef struct AnyChatUser_T* AnyChatUserHandle;
typedef struct AnyChatCall_T* AnyChatCallHandle;
typedef struct AnyChatVersion_T* AnyChatVersionHandle;

/* ---- Common callback types ---- */
typedef void (*AnyChatErrorCallback)(void* userdata, int code, const char* error);
typedef void (*AnyChatSuccessCallback)(void* userdata);

/* ---- Connection states ---- */
#define ANYCHAT_STATE_DISCONNECTED 0
#define ANYCHAT_STATE_CONNECTING 1
#define ANYCHAT_STATE_CONNECTED 2
#define ANYCHAT_STATE_RECONNECTING 3

/* ---- Message types ---- */
#define ANYCHAT_MSG_TEXT 0
#define ANYCHAT_MSG_IMAGE 1
#define ANYCHAT_MSG_FILE 2
#define ANYCHAT_MSG_AUDIO 3
#define ANYCHAT_MSG_VIDEO 4

/* ---- Message content types ---- */
#define ANYCHAT_MESSAGE_CONTENT_TYPE_UNSPECIFIED 0
#define ANYCHAT_MESSAGE_CONTENT_TYPE_TEXT 1
#define ANYCHAT_MESSAGE_CONTENT_TYPE_IMAGE 2
#define ANYCHAT_MESSAGE_CONTENT_TYPE_VIDEO 3
#define ANYCHAT_MESSAGE_CONTENT_TYPE_AUDIO 4
#define ANYCHAT_MESSAGE_CONTENT_TYPE_FILE 5
#define ANYCHAT_MESSAGE_CONTENT_TYPE_LOCATION 6
#define ANYCHAT_MESSAGE_CONTENT_TYPE_CARD 7

/* ---- Conversation types ---- */
#define ANYCHAT_CONV_PRIVATE 0
#define ANYCHAT_CONV_GROUP 1

/* ---- Message send states ---- */
#define ANYCHAT_SEND_PENDING 0
#define ANYCHAT_SEND_SENT 1
#define ANYCHAT_SEND_FAILED 2

/* ---- Call types ---- */
#define ANYCHAT_CALL_AUDIO 0
#define ANYCHAT_CALL_VIDEO 1

/* ---- Call status ---- */
#define ANYCHAT_CALL_STATUS_RINGING 0
#define ANYCHAT_CALL_STATUS_CONNECTED 1
#define ANYCHAT_CALL_STATUS_ENDED 2
#define ANYCHAT_CALL_STATUS_REJECTED 3
#define ANYCHAT_CALL_STATUS_MISSED 4
#define ANYCHAT_CALL_STATUS_CANCELLED 5

/* ---- Group roles ---- */
#define ANYCHAT_GROUP_ROLE_UNSPECIFIED 0
#define ANYCHAT_GROUP_ROLE_OWNER 1
#define ANYCHAT_GROUP_ROLE_ADMIN 2
#define ANYCHAT_GROUP_ROLE_MEMBER 3

/* ---- Group join request status ---- */
#define ANYCHAT_GROUP_JOIN_REQUEST_STATUS_UNSPECIFIED 0
#define ANYCHAT_GROUP_JOIN_REQUEST_STATUS_PENDING 1
#define ANYCHAT_GROUP_JOIN_REQUEST_STATUS_ACCEPTED 2
#define ANYCHAT_GROUP_JOIN_REQUEST_STATUS_REJECTED 3

/* ---- Auth enums ---- */
#define ANYCHAT_DEVICE_TYPE_UNSPECIFIED 0
#define ANYCHAT_DEVICE_TYPE_IOS 1
#define ANYCHAT_DEVICE_TYPE_ANDROID 2
#define ANYCHAT_DEVICE_TYPE_WEB 3
#define ANYCHAT_DEVICE_TYPE_PC 4
#define ANYCHAT_DEVICE_TYPE_H5 5

#define ANYCHAT_VERIFY_TARGET_UNSPECIFIED 0
#define ANYCHAT_VERIFY_TARGET_SMS 1
#define ANYCHAT_VERIFY_TARGET_EMAIL 2

#define ANYCHAT_VERIFY_PURPOSE_UNSPECIFIED 0
#define ANYCHAT_VERIFY_PURPOSE_REGISTER 1
#define ANYCHAT_VERIFY_PURPOSE_LOGIN 2
#define ANYCHAT_VERIFY_PURPOSE_RESET_PASSWORD 3
#define ANYCHAT_VERIFY_PURPOSE_BIND_PHONE 4
#define ANYCHAT_VERIFY_PURPOSE_CHANGE_PHONE 5
#define ANYCHAT_VERIFY_PURPOSE_BIND_EMAIL 6
#define ANYCHAT_VERIFY_PURPOSE_CHANGE_EMAIL 7

/* ---- Friend request enums ---- */
#define ANYCHAT_FRIEND_SOURCE_UNSPECIFIED 0
#define ANYCHAT_FRIEND_SOURCE_SEARCH 1
#define ANYCHAT_FRIEND_SOURCE_QRCODE 2
#define ANYCHAT_FRIEND_SOURCE_GROUP 3
#define ANYCHAT_FRIEND_SOURCE_CONTACTS 4

#define ANYCHAT_FRIEND_REQUEST_STATUS_UNSPECIFIED 0
#define ANYCHAT_FRIEND_REQUEST_STATUS_PENDING 1
#define ANYCHAT_FRIEND_REQUEST_STATUS_ACCEPTED 2
#define ANYCHAT_FRIEND_REQUEST_STATUS_REJECTED 3
#define ANYCHAT_FRIEND_REQUEST_STATUS_EXPIRED 4

#define ANYCHAT_FRIEND_REQUEST_ACTION_UNSPECIFIED 0
#define ANYCHAT_FRIEND_REQUEST_ACTION_ACCEPT 1
#define ANYCHAT_FRIEND_REQUEST_ACTION_REJECT 2

#define ANYCHAT_FRIEND_REQUEST_QUERY_TYPE_UNSPECIFIED 0
#define ANYCHAT_FRIEND_REQUEST_QUERY_TYPE_RECEIVED 1
#define ANYCHAT_FRIEND_REQUEST_QUERY_TYPE_SENT 2

/* ---- User push platform ---- */
#define ANYCHAT_PUSH_PLATFORM_UNSPECIFIED 0
#define ANYCHAT_PUSH_PLATFORM_IOS 1
#define ANYCHAT_PUSH_PLATFORM_ANDROID 2

/* ---- File types ---- */
#define ANYCHAT_FILE_TYPE_UNSPECIFIED 0
#define ANYCHAT_FILE_TYPE_IMAGE 1
#define ANYCHAT_FILE_TYPE_VIDEO 2
#define ANYCHAT_FILE_TYPE_AUDIO 3
#define ANYCHAT_FILE_TYPE_FILE 4
#define ANYCHAT_FILE_TYPE_LOG 5

/* ---- User status ---- */
#define ANYCHAT_USER_STATUS_OFFLINE 0
#define ANYCHAT_USER_STATUS_ONLINE 1
#define ANYCHAT_USER_STATUS_AWAY 2

/* ---- Version enums ---- */
#define ANYCHAT_VERSION_PLATFORM_UNSPECIFIED 0
#define ANYCHAT_VERSION_PLATFORM_IOS 1
#define ANYCHAT_VERSION_PLATFORM_ANDROID 2
#define ANYCHAT_VERSION_PLATFORM_PC 3
#define ANYCHAT_VERSION_PLATFORM_WEB 4
#define ANYCHAT_VERSION_PLATFORM_H5 5

#define ANYCHAT_VERSION_RELEASE_TYPE_UNSPECIFIED 0
#define ANYCHAT_VERSION_RELEASE_TYPE_STABLE 1
#define ANYCHAT_VERSION_RELEASE_TYPE_BETA 2
#define ANYCHAT_VERSION_RELEASE_TYPE_ALPHA 3

/* ---- Plain-old-data structs ---- */

typedef struct {
    char access_token[512];
    char refresh_token[512];
    int64_t expires_at_ms;
} AnyChatAuthToken_C;

typedef struct {
    char code_id[128];
    int64_t expires_in;
} AnyChatVerificationCodeResult_C;

typedef struct {
    char device_id[128];
    int32_t device_type; /* ANYCHAT_DEVICE_TYPE_* */
    char client_version[64];
    char last_login_ip[64];
    int64_t last_login_at_ms;
    int is_current;
} AnyChatAuthDevice_C;

typedef struct {
    AnyChatAuthDevice_C* items;
    int count;
} AnyChatAuthDeviceList_C;

typedef struct {
    char user_id[64];
    char username[128];
    char avatar_url[512];
    char signature[256];
    int32_t gender; /* 0=unknown, 1=male, 2=female */
    char region[64];
    int is_friend;
    int is_blocked;
} AnyChatUserInfo_C;

typedef struct {
    char message_id[64];
    char local_id[64];
    char conv_id[64];
    char sender_id[64];
    int32_t content_type; /* ANYCHAT_MESSAGE_CONTENT_TYPE_* */
    int type; /* ANYCHAT_MSG_* */
    char* content; /* heap-allocated; free via anychat_free_message() */
    int64_t seq;
    char reply_to[64];
    int64_t timestamp_ms;
    int status; /* 0=normal, 1=recalled, 2=deleted */
    int send_state; /* ANYCHAT_SEND_* */
    int is_read;
} AnyChatMessage_C;

typedef struct {
    AnyChatMessage_C* items;
    int count;
} AnyChatMessageList_C;

typedef struct {
    AnyChatMessage_C* items;
    int count;
    int has_more;
    int64_t next_seq;
} AnyChatOfflineMessageResult_C;

typedef struct {
    AnyChatMessage_C* items;
    int count;
    int64_t total;
} AnyChatMessageSearchResult_C;

typedef struct {
    char user_id[64];
    char nickname[128];
    int64_t read_at_ms;
} AnyChatGroupMessageReadMember_C;

typedef struct {
    AnyChatGroupMessageReadMember_C* items;
    int count;
    int64_t read_count;
    int64_t unread_count;
} AnyChatGroupMessageReadState_C;

typedef struct {
    char conversation_id[64];
    char from_user_id[64];
    char message_id[64];
    int64_t last_read_seq;
    char last_read_message_id[64];
    int64_t read_at_ms;
} AnyChatMessageReadReceiptEvent_C;

typedef struct {
    char conversation_id[64];
    char from_user_id[64];
    int typing;
    int64_t expire_at_ms;
    char device_id[64];
} AnyChatMessageTypingEvent_C;

typedef struct {
    char conv_id[64];
    int conv_type; /* ANYCHAT_CONV_* */
    char target_id[64];
    char last_msg_id[64];
    char last_msg_text[512];
    int64_t last_msg_time_ms;
    int32_t unread_count;
    int is_pinned;
    int is_muted;
    int32_t burn_after_reading; /* seconds, 0 = disabled */
    int32_t auto_delete_duration; /* seconds, 0 = disabled */
    int64_t updated_at_ms;
} AnyChatConversation_C;

typedef struct {
    AnyChatConversation_C* items;
    int count;
} AnyChatConversationList_C;

typedef struct {
    int64_t unread_count;
    int64_t last_message_seq;
} AnyChatConversationUnreadState_C;

typedef struct {
    char** accepted_ids;
    int accepted_count;
    char** ignored_ids;
    int ignored_count;
    int64_t advanced_last_read_seq;
} AnyChatConversationMarkReadResult_C;

typedef struct {
    char user_id[64];
    int64_t last_read_seq;
    char last_read_message_id[64];
    int64_t read_at_ms;
} AnyChatConversationReadReceipt_C;

typedef struct {
    AnyChatConversationReadReceipt_C* items;
    int count;
} AnyChatConversationReadReceiptList_C;

typedef struct {
    char user_id[64];
    char remark[128];
    int64_t updated_at_ms;
    int is_deleted;
    AnyChatUserInfo_C user_info;
} AnyChatFriend_C;

typedef struct {
    AnyChatFriend_C* items;
    int count;
} AnyChatFriendList_C;

typedef struct {
    int64_t request_id;
    char from_user_id[64];
    char to_user_id[64];
    char message[256];
    int32_t source; /* ANYCHAT_FRIEND_SOURCE_* */
    int32_t status; /* ANYCHAT_FRIEND_REQUEST_STATUS_* */
    int64_t created_at_ms;
    AnyChatUserInfo_C from_user_info;
} AnyChatFriendRequest_C;

typedef struct {
    AnyChatFriendRequest_C* items;
    int count;
} AnyChatFriendRequestList_C;

typedef struct {
    int64_t id;
    char user_id[64];
    char blocked_user_id[64];
    int64_t created_at_ms;
    AnyChatUserInfo_C blocked_user_info;
} AnyChatBlacklistItem_C;

typedef struct {
    AnyChatBlacklistItem_C* items;
    int count;
} AnyChatBlacklistList_C;

typedef struct {
    char group_id[64];
    char name[128];
    char avatar_url[512];
    char owner_id[64];
    int32_t member_count;
    int my_role; /* ANYCHAT_GROUP_ROLE_* */
    int join_verify;
    int64_t updated_at_ms;
    char display_name[128];
    char announcement[512];
    char description[512];
    char group_remark[64];
    int32_t max_members;
    int is_muted;
    int64_t created_at_ms;
} AnyChatGroup_C;

typedef struct {
    AnyChatGroup_C* items;
    int count;
} AnyChatGroupList_C;

typedef struct {
    char user_id[64];
    char group_nickname[128];
    int role; /* ANYCHAT_GROUP_ROLE_* */
    int is_muted;
    int64_t muted_until_ms;
    int64_t joined_at_ms;
    AnyChatUserInfo_C user_info;
} AnyChatGroupMember_C;

typedef struct {
    AnyChatGroupMember_C* items;
    int count;
} AnyChatGroupMemberList_C;

typedef struct {
    int64_t request_id;
    char group_id[64];
    char user_id[64];
    char inviter_id[64];
    char message[256];
    int32_t status; /* ANYCHAT_GROUP_JOIN_REQUEST_STATUS_* */
    int64_t created_at_ms;
    AnyChatUserInfo_C user_info;
} AnyChatGroupJoinRequest_C;

typedef struct {
    AnyChatGroupJoinRequest_C* items;
    int count;
} AnyChatGroupJoinRequestList_C;

typedef struct {
    char group_id[64];
    char token[128];
    char deep_link[512];
    int64_t expire_at_ms;
} AnyChatGroupQRCode_C;

typedef struct {
    char file_id[64];
    char file_name[256];
    int32_t file_type; /* ANYCHAT_FILE_TYPE_* */
    int64_t file_size_bytes;
    char mime_type[128];
    char download_url[1024];
    int64_t created_at_ms;
} AnyChatFileInfo_C;

typedef struct {
    AnyChatFileInfo_C* items;
    int count;
    int64_t total;
    int32_t page;
    int32_t page_size;
} AnyChatFileList_C;

typedef struct {
    char user_id[64];
    char nickname[128];
    char avatar_url[512];
    char phone[32];
    char email[128];
    char signature[256];
    char region[64];
    int32_t gender; /* 0=unknown, 1=male, 2=female */
    int64_t birthday_ms;
    char qrcode_url[512];
    int64_t created_at_ms;
} AnyChatUserProfile_C;

typedef struct {
    char user_id[64];
    int notification_enabled;
    int sound_enabled;
    int vibration_enabled;
    int message_preview_enabled;
    int friend_verify_required;
    int search_by_phone;
    int search_by_id;
    char language[16];
} AnyChatUserSettings_C;

typedef struct {
    AnyChatUserInfo_C* items;
    int count;
    int64_t total;
} AnyChatUserList_C;

typedef struct {
    char qrcode_url[512];
    int64_t expires_at_ms;
} AnyChatUserQRCode_C;

typedef struct {
    char phone_number[32];
    int is_primary;
} AnyChatBindPhoneResult_C;

typedef struct {
    char old_phone_number[32];
    char new_phone_number[32];
} AnyChatChangePhoneResult_C;

typedef struct {
    char email[128];
    int is_primary;
} AnyChatBindEmailResult_C;

typedef struct {
    char old_email[128];
    char new_email[128];
} AnyChatChangeEmailResult_C;

typedef struct {
    char user_id[64];
    int32_t status; /* ANYCHAT_USER_STATUS_* */
    int64_t last_active_at_ms;
    int32_t platform; /* ANYCHAT_PUSH_PLATFORM_* */
} AnyChatUserStatusEvent_C;

typedef struct {
    char call_id[64];
    char caller_id[64];
    char callee_id[64];
    int call_type; /* ANYCHAT_CALL_* */
    int status; /* ANYCHAT_CALL_STATUS_* */
    char room_name[128];
    char token[512];
    int64_t started_at;
    int64_t connected_at;
    int64_t ended_at;
    int32_t duration; /* seconds */
} AnyChatCallSession_C;

typedef struct {
    AnyChatCallSession_C* items;
    int count;
    int64_t total;
} AnyChatCallList_C;

typedef struct {
    char room_id[64];
    char creator_id[64];
    char title[128];
    char room_name[128];
    char token[512];
    int has_password;
    int32_t max_participants;
    int is_active;
    int64_t started_at;
    int64_t created_at_ms;
} AnyChatMeetingRoom_C;

typedef struct {
    AnyChatMeetingRoom_C* items;
    int count;
    int64_t total;
} AnyChatMeetingList_C;

typedef struct {
    char title[128];
    char content[2048];
    char download_url[1024];
    int64_t file_size;
    char file_hash[128];
} AnyChatVersionUpdateInfo_C;

typedef struct {
    int has_update;
    char latest_version[64];
    int32_t latest_build_number;
    int force_update;
    char min_version[64];
    int32_t min_build_number;
    AnyChatVersionUpdateInfo_C update_info;
} AnyChatVersionCheckResult_C;

typedef struct {
    int64_t id;
    int32_t platform; /* ANYCHAT_VERSION_PLATFORM_* */
    char version[64];
    int32_t build_number;
    int32_t version_code;
    char min_version[64];
    int32_t min_build_number;
    int force_update;
    int32_t release_type; /* ANYCHAT_VERSION_RELEASE_TYPE_* */
    char title[128];
    char content[2048];
    char download_url[1024];
    int64_t file_size;
    char file_hash[128];
    int64_t published_at_ms;
} AnyChatVersionInfo_C;

typedef struct {
    AnyChatVersionInfo_C* items;
    int count;
    int64_t total;
    int32_t page;
    int32_t page_size;
} AnyChatVersionList_C;

/* ---- Memory management ---- */

/* Free a string allocated by the SDK. */
ANYCHAT_C_API void anychat_free_string(char* str);

/* Free the content field of a single message struct. */
ANYCHAT_C_API void anychat_free_message(AnyChatMessage_C* msg);

/* Free a list allocated by the SDK. The structs themselves are also freed. */
ANYCHAT_C_API void anychat_free_message_list(AnyChatMessageList_C* list);
ANYCHAT_C_API void anychat_free_offline_message_result(AnyChatOfflineMessageResult_C* result);
ANYCHAT_C_API void anychat_free_message_search_result(AnyChatMessageSearchResult_C* result);
ANYCHAT_C_API void anychat_free_group_message_read_state(AnyChatGroupMessageReadState_C* state);
ANYCHAT_C_API void anychat_free_conversation_list(AnyChatConversationList_C* list);
ANYCHAT_C_API void anychat_free_conversation_mark_read_result(AnyChatConversationMarkReadResult_C* result);
ANYCHAT_C_API void anychat_free_conversation_read_receipt_list(AnyChatConversationReadReceiptList_C* list);
ANYCHAT_C_API void anychat_free_friend_list(AnyChatFriendList_C* list);
ANYCHAT_C_API void anychat_free_friend_request_list(AnyChatFriendRequestList_C* list);
ANYCHAT_C_API void anychat_free_blacklist_list(AnyChatBlacklistList_C* list);
ANYCHAT_C_API void anychat_free_group_list(AnyChatGroupList_C* list);
ANYCHAT_C_API void anychat_free_group_member_list(AnyChatGroupMemberList_C* list);
ANYCHAT_C_API void anychat_free_group_join_request_list(AnyChatGroupJoinRequestList_C* list);
ANYCHAT_C_API void anychat_free_user_list(AnyChatUserList_C* list);
ANYCHAT_C_API void anychat_free_call_list(AnyChatCallList_C* list);
ANYCHAT_C_API void anychat_free_meeting_list(AnyChatMeetingList_C* list);
ANYCHAT_C_API void anychat_free_auth_device_list(AnyChatAuthDeviceList_C* list);
ANYCHAT_C_API void anychat_free_file_list(AnyChatFileList_C* list);
ANYCHAT_C_API void anychat_free_version_list(AnyChatVersionList_C* list);

#ifdef __cplusplus
}
#endif
