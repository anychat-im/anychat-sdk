#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatUserProfileCallback)(
    void* userdata,
    int success,
    const AnyChatUserProfile_C* profile,
    const char* error
);

typedef void (*AnyChatUserSettingsCallback)(
    void* userdata,
    int success,
    const AnyChatUserSettings_C* settings,
    const char* error
);

typedef void (*AnyChatUserInfoCallback)(void* userdata, int success, const AnyChatUserInfo_C* info, const char* error);

typedef void (*AnyChatUserListCallback)(void* userdata, const AnyChatUserList_C* list, const char* error);

typedef void (*AnyChatUserResultCallback)(void* userdata, int success, const char* error);
typedef void (*AnyChatUserQRCodeCallback)(
    void* userdata,
    int success,
    const AnyChatUserQRCode_C* qrcode,
    const char* error
);
typedef void (*AnyChatBindPhoneCallback)(
    void* userdata,
    int success,
    const AnyChatBindPhoneResult_C* result,
    const char* error
);
typedef void (*AnyChatChangePhoneCallback)(
    void* userdata,
    int success,
    const AnyChatChangePhoneResult_C* result,
    const char* error
);
typedef void (*AnyChatBindEmailCallback)(
    void* userdata,
    int success,
    const AnyChatBindEmailResult_C* result,
    const char* error
);
typedef void (*AnyChatChangeEmailCallback)(
    void* userdata,
    int success,
    const AnyChatChangeEmailResult_C* result,
    const char* error
);
typedef void (*AnyChatUserProfileUpdatedCallback)(void* userdata, const AnyChatUserInfo_C* info);
typedef void (*AnyChatUserFriendProfileChangedCallback)(void* userdata, const AnyChatUserInfo_C* info);
typedef void (*AnyChatUserStatusChangedCallback)(void* userdata, const AnyChatUserStatusEvent_C* event);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatUserProfileUpdatedCallback on_profile_updated;
    AnyChatUserFriendProfileChangedCallback on_friend_profile_changed;
    AnyChatUserStatusChangedCallback on_status_changed;
} AnyChatUserListener_C;

/* ---- User operations ---- */

ANYCHAT_C_API int
anychat_user_get_profile(AnyChatUserHandle handle, void* userdata, AnyChatUserProfileCallback callback);

ANYCHAT_C_API int anychat_user_update_profile(
    AnyChatUserHandle handle,
    const AnyChatUserProfile_C* profile,
    void* userdata,
    AnyChatUserProfileCallback callback
);

ANYCHAT_C_API int
anychat_user_get_settings(AnyChatUserHandle handle, void* userdata, AnyChatUserSettingsCallback callback);

ANYCHAT_C_API int anychat_user_update_settings(
    AnyChatUserHandle handle,
    const AnyChatUserSettings_C* settings,
    void* userdata,
    AnyChatUserSettingsCallback callback
);

/* Update the push notification token for this device.
 * platform: "ios" | "android" | "web" */
ANYCHAT_C_API int anychat_user_update_push_token(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    void* userdata,
    AnyChatUserResultCallback callback
);

/* Same as anychat_user_update_push_token but allows overriding device_id. */
ANYCHAT_C_API int anychat_user_update_push_token_with_device(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    const char* device_id,
    void* userdata,
    AnyChatUserResultCallback callback
);

/* Search for users by keyword (username / phone / e-mail). */
ANYCHAT_C_API int anychat_user_search(
    AnyChatUserHandle handle,
    const char* keyword,
    int page,
    int page_size,
    void* userdata,
    AnyChatUserListCallback callback
);

/* Fetch public info for a specific user. */
ANYCHAT_C_API int
anychat_user_get_info(AnyChatUserHandle handle, const char* user_id, void* userdata, AnyChatUserInfoCallback callback);

/* Bind/change phone */
ANYCHAT_C_API int anychat_user_bind_phone(
    AnyChatUserHandle handle,
    const char* phone_number,
    const char* verify_code,
    void* userdata,
    AnyChatBindPhoneCallback callback
);

ANYCHAT_C_API int anychat_user_change_phone(
    AnyChatUserHandle handle,
    const char* old_phone_number,
    const char* new_phone_number,
    const char* new_verify_code,
    const char* old_verify_code,
    void* userdata,
    AnyChatChangePhoneCallback callback
);

/* Bind/change email */
ANYCHAT_C_API int anychat_user_bind_email(
    AnyChatUserHandle handle,
    const char* email,
    const char* verify_code,
    void* userdata,
    AnyChatBindEmailCallback callback
);

ANYCHAT_C_API int anychat_user_change_email(
    AnyChatUserHandle handle,
    const char* old_email,
    const char* new_email,
    const char* new_verify_code,
    const char* old_verify_code,
    void* userdata,
    AnyChatChangeEmailCallback callback
);

/* QR code operations */
ANYCHAT_C_API int
anychat_user_refresh_qrcode(AnyChatUserHandle handle, void* userdata, AnyChatUserQRCodeCallback callback);

ANYCHAT_C_API int anychat_user_get_by_qrcode(
    AnyChatUserHandle handle,
    const char* qrcode,
    void* userdata,
    AnyChatUserInfoCallback callback
);

/* User WebSocket notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_user_set_listener(AnyChatUserHandle handle, const AnyChatUserListener_C* listener);

#ifdef __cplusplus
}
#endif
