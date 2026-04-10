#pragma once

#include "types_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

/* Fired after login / register / refreshToken.
 * success: 1 on success, 0 on failure.
 * token:   valid only when success == 1.
 * error:   valid only when success == 0 (never NULL). */
typedef void (*AnyChatAuthCallback)(void* userdata, int success, const AnyChatAuthToken_C* token, const char* error);

/* Fired after logout / changePassword.
 * success: 1 on success, 0 on failure.
 * error:   valid only when success == 0 (never NULL). */
typedef void (*AnyChatResultCallback)(void* userdata, int success, const char* error);

/* Fired after send-code.
 * success: 1 on success, 0 on failure.
 * result:  valid only when success == 1. */
typedef void (*AnyChatSendCodeCallback)(
    void* userdata,
    int success,
    const AnyChatVerificationCodeResult_C* result,
    const char* error
);

/* Fired after get-device-list.
 * list: valid only when error == NULL. */
typedef void (*AnyChatAuthDeviceListCallback)(void* userdata, const AnyChatAuthDeviceList_C* list, const char* error);

/* ---- Auth operations ---- */

/* Login with account (phone / e-mail) and password.
 * device_type: "ios" | "android" | "web"
 * client_version: client version string (e.g. "1.0.0"), can be NULL
 * Returns ANYCHAT_OK if the request was dispatched; callback fires asynchronously. */
ANYCHAT_C_API int anychat_auth_login(
    AnyChatAuthHandle handle,
    const char* account,
    const char* password,
    const char* device_type,
    const char* client_version,
    void* userdata,
    AnyChatAuthCallback callback
);

/* Register a new account.
 * verify_code: SMS/e-mail verification code.
 * nickname:    pass empty string or NULL to skip.
 * client_version: client version string (e.g. "1.0.0"), can be NULL */
ANYCHAT_C_API int anychat_auth_register(
    AnyChatAuthHandle handle,
    const char* phone_or_email,
    const char* password,
    const char* verify_code,
    const char* device_type,
    const char* nickname,
    const char* client_version,
    void* userdata,
    AnyChatAuthCallback callback
);

/* Send verification code.
 * target_type: "sms" | "email"
 * purpose: e.g. "register", "reset_password" */
ANYCHAT_C_API int anychat_auth_send_code(
    AnyChatAuthHandle handle,
    const char* target,
    const char* target_type,
    const char* purpose,
    void* userdata,
    AnyChatSendCodeCallback callback
);

/* Logout the current device. */
ANYCHAT_C_API int anychat_auth_logout(AnyChatAuthHandle handle, void* userdata, AnyChatResultCallback callback);

/* Exchange a refresh token for a new access token. */
ANYCHAT_C_API int anychat_auth_refresh_token(
    AnyChatAuthHandle handle,
    const char* refresh_token,
    void* userdata,
    AnyChatAuthCallback callback
);

/* Change password (requires a valid access token). */
ANYCHAT_C_API int anychat_auth_change_password(
    AnyChatAuthHandle handle,
    const char* old_password,
    const char* new_password,
    void* userdata,
    AnyChatResultCallback callback
);

/* Reset password via verification code. */
ANYCHAT_C_API int anychat_auth_reset_password(
    AnyChatAuthHandle handle,
    const char* account,
    const char* verify_code,
    const char* new_password,
    void* userdata,
    AnyChatResultCallback callback
);

/* Query current user's device list. */
ANYCHAT_C_API int
anychat_auth_get_device_list(AnyChatAuthHandle handle, void* userdata, AnyChatAuthDeviceListCallback callback);

/* Force logout specified device. */
ANYCHAT_C_API int anychat_auth_logout_device(
    AnyChatAuthHandle handle,
    const char* device_id,
    void* userdata,
    AnyChatResultCallback callback
);

/* ---- State queries ---- */

/* Returns 1 if the user is currently logged in, 0 otherwise. */
ANYCHAT_C_API int anychat_auth_is_logged_in(AnyChatAuthHandle handle);

/* Copy the current auth token into out_token.
 * Returns ANYCHAT_OK on success or ANYCHAT_ERROR_NOT_LOGGED_IN. */
ANYCHAT_C_API int anychat_auth_get_current_token(AnyChatAuthHandle handle, AnyChatAuthToken_C* out_token);

typedef void (*AnyChatAuthExpiredCallback)(void* userdata);

typedef struct {
    uint32_t struct_size;
    void* userdata;
    AnyChatAuthExpiredCallback on_auth_expired;
} AnyChatAuthListener_C;

/* Register auth notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_auth_set_listener(AnyChatAuthHandle handle, const AnyChatAuthListener_C* listener);

#ifdef __cplusplus
}
#endif
