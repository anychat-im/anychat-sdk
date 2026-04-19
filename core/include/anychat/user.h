#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatUserProfileSuccessCallback)(void* userdata, const AnyChatUserProfile_C* profile);
typedef void (*AnyChatUserSettingsSuccessCallback)(void* userdata, const AnyChatUserSettings_C* settings);
typedef void (*AnyChatUserInfoSuccessCallback)(void* userdata, const AnyChatUserInfo_C* info);
typedef void (*AnyChatUserListSuccessCallback)(void* userdata, const AnyChatUserList_C* list);
typedef void (*AnyChatUserQRCodeSuccessCallback)(void* userdata, const AnyChatUserQRCode_C* qrcode);
typedef void (*AnyChatBindPhoneSuccessCallback)(void* userdata, const AnyChatBindPhoneResult_C* result);
typedef void (*AnyChatChangePhoneSuccessCallback)(void* userdata, const AnyChatChangePhoneResult_C* result);
typedef void (*AnyChatBindEmailSuccessCallback)(void* userdata, const AnyChatBindEmailResult_C* result);
typedef void (*AnyChatChangeEmailSuccessCallback)(void* userdata, const AnyChatChangeEmailResult_C* result);

typedef struct {
    void* userdata;
    AnyChatUserProfileSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserProfileCallback_C;

typedef struct {
    void* userdata;
    AnyChatUserSettingsSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserSettingsCallback_C;

typedef struct {
    void* userdata;
    AnyChatUserInfoSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserInfoCallback_C;

typedef struct {
    void* userdata;
    AnyChatUserListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserListCallback_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserCallback_C;

typedef struct {
    void* userdata;
    AnyChatUserQRCodeSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatUserQRCodeCallback_C;

typedef struct {
    void* userdata;
    AnyChatBindPhoneSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatBindPhoneCallback_C;

typedef struct {
    void* userdata;
    AnyChatChangePhoneSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatChangePhoneCallback_C;

typedef struct {
    void* userdata;
    AnyChatBindEmailSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatBindEmailCallback_C;

typedef struct {
    void* userdata;
    AnyChatChangeEmailSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatChangeEmailCallback_C;

typedef void (*AnyChatUserProfileUpdatedCallback)(void* userdata, const AnyChatUserInfo_C* info);
typedef void (*AnyChatUserFriendProfileChangedCallback)(void* userdata, const AnyChatUserInfo_C* info);
typedef void (*AnyChatUserStatusChangedCallback)(void* userdata, const AnyChatUserStatusEvent_C* event);

typedef struct {
    void* userdata;
    AnyChatUserProfileUpdatedCallback on_profile_updated;
    AnyChatUserFriendProfileChangedCallback on_friend_profile_changed;
    AnyChatUserStatusChangedCallback on_status_changed;
} AnyChatUserListener_C;

/* ---- User operations ---- */

ANYCHAT_C_API int
anychat_user_get_profile(AnyChatUserHandle handle, const AnyChatUserProfileCallback_C* callback);

ANYCHAT_C_API int anychat_user_update_profile(
    AnyChatUserHandle handle,
    const AnyChatUserProfile_C* profile,
    const AnyChatUserProfileCallback_C* callback
);

ANYCHAT_C_API int
anychat_user_get_settings(AnyChatUserHandle handle, const AnyChatUserSettingsCallback_C* callback);

ANYCHAT_C_API int anychat_user_update_settings(
    AnyChatUserHandle handle,
    const AnyChatUserSettings_C* settings,
    const AnyChatUserSettingsCallback_C* callback
);

/* Update the push notification token for this device.
 * platform: ANYCHAT_PUSH_PLATFORM_* */
ANYCHAT_C_API int anychat_user_update_push_token(
    AnyChatUserHandle handle,
    const char* push_token,
    int32_t platform,
    const AnyChatUserCallback_C* callback
);

/* Same as anychat_user_update_push_token but allows overriding device_id. */
ANYCHAT_C_API int anychat_user_update_push_token_with_device(
    AnyChatUserHandle handle,
    const char* push_token,
    int32_t platform,
    const char* device_id,
    const AnyChatUserCallback_C* callback
);

/* Search for users by keyword (username / phone / e-mail). */
ANYCHAT_C_API int anychat_user_search(
    AnyChatUserHandle handle,
    const char* keyword,
    int page,
    int page_size,
    const AnyChatUserListCallback_C* callback
);

/* Fetch public info for a specific user. */
ANYCHAT_C_API int
anychat_user_get_info(AnyChatUserHandle handle, const char* user_id, const AnyChatUserInfoCallback_C* callback);

/* Bind/change phone */
ANYCHAT_C_API int anychat_user_bind_phone(
    AnyChatUserHandle handle,
    const char* phone_number,
    const char* verify_code,
    const AnyChatBindPhoneCallback_C* callback
);

ANYCHAT_C_API int anychat_user_change_phone(
    AnyChatUserHandle handle,
    const char* old_phone_number,
    const char* new_phone_number,
    const char* new_verify_code,
    const char* old_verify_code,
    const AnyChatChangePhoneCallback_C* callback
);

/* Bind/change email */
ANYCHAT_C_API int anychat_user_bind_email(
    AnyChatUserHandle handle,
    const char* email,
    const char* verify_code,
    const AnyChatBindEmailCallback_C* callback
);

ANYCHAT_C_API int anychat_user_change_email(
    AnyChatUserHandle handle,
    const char* old_email,
    const char* new_email,
    const char* new_verify_code,
    const char* old_verify_code,
    const AnyChatChangeEmailCallback_C* callback
);

/* QR code operations */
ANYCHAT_C_API int
anychat_user_refresh_qrcode(AnyChatUserHandle handle, const AnyChatUserQRCodeCallback_C* callback);

ANYCHAT_C_API int anychat_user_get_by_qrcode(
    AnyChatUserHandle handle,
    const char* qrcode,
    const AnyChatUserInfoCallback_C* callback
);

/* User WebSocket notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_user_set_listener(AnyChatUserHandle handle, const AnyChatUserListener_C* listener);

#ifdef __cplusplus
}
#endif
