#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatAuthTokenSuccessCallback)(void* userdata, const AnyChatAuthToken_C* token);
typedef void (*AnyChatVerificationCodeSuccessCallback)(void* userdata, const AnyChatVerificationCodeResult_C* result);
typedef void (*AnyChatAuthDeviceListSuccessCallback)(void* userdata, const AnyChatAuthDeviceList_C* list);

typedef struct {
    void* userdata;
    AnyChatAuthTokenSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatAuthTokenCallback_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatAuthResultCallback_C;

typedef struct {
    void* userdata;
    AnyChatVerificationCodeSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatVerificationCodeCallback_C;

typedef struct {
    void* userdata;
    AnyChatAuthDeviceListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatAuthDeviceListCallback_C;

/* ---- Auth operations ---- */

/* Login with account (phone / e-mail) and password.
 * device_type: ANYCHAT_DEVICE_TYPE_*
 * client_version: client version string (e.g. "1.0.0"), can be NULL
 * Returns ANYCHAT_OK if the request was dispatched; callback fires asynchronously. */
ANYCHAT_C_API int anychat_auth_login(
    AnyChatAuthHandle handle,
    const char* account,
    const char* password,
    int32_t device_type,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
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
    int32_t device_type,
    const char* nickname,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
);

/* Send verification code.
 * target_type: ANYCHAT_VERIFY_TARGET_*
 * purpose: ANYCHAT_VERIFY_PURPOSE_* */
ANYCHAT_C_API int anychat_auth_send_code(
    AnyChatAuthHandle handle,
    const char* target,
    int32_t target_type,
    int32_t purpose,
    const AnyChatVerificationCodeCallback_C* callback
);

/* Logout the current device. */
ANYCHAT_C_API int anychat_auth_logout(AnyChatAuthHandle handle, const AnyChatAuthResultCallback_C* callback);

/* Exchange a refresh token for a new access token. */
ANYCHAT_C_API int anychat_auth_refresh_token(
    AnyChatAuthHandle handle,
    const char* refresh_token,
    const AnyChatAuthTokenCallback_C* callback
);

/* Change password (requires a valid access token). */
ANYCHAT_C_API int anychat_auth_change_password(
    AnyChatAuthHandle handle,
    const char* old_password,
    const char* new_password,
    const AnyChatAuthResultCallback_C* callback
);

/* Reset password via verification code. */
ANYCHAT_C_API int anychat_auth_reset_password(
    AnyChatAuthHandle handle,
    const char* account,
    const char* verify_code,
    const char* new_password,
    const AnyChatAuthResultCallback_C* callback
);

/* Query current user's device list. */
ANYCHAT_C_API int
anychat_auth_get_device_list(AnyChatAuthHandle handle, const AnyChatAuthDeviceListCallback_C* callback);

/* Force logout specified device. */
ANYCHAT_C_API int anychat_auth_logout_device(
    AnyChatAuthHandle handle,
    const char* device_id,
    const AnyChatAuthResultCallback_C* callback
);

/* ---- State queries ---- */

/* Returns 1 if the user is currently logged in, 0 otherwise. */
ANYCHAT_C_API int anychat_auth_is_logged_in(AnyChatAuthHandle handle);

/* Copy the current auth token into out_token.
 * Returns ANYCHAT_OK on success or ANYCHAT_ERROR_NOT_LOGGED_IN. */
ANYCHAT_C_API int anychat_auth_get_current_token(AnyChatAuthHandle handle, AnyChatAuthToken_C* out_token);

typedef void (*AnyChatAuthExpiredCallback)(void* userdata);

typedef struct {
    void* userdata;
    AnyChatAuthExpiredCallback on_auth_expired;
} AnyChatAuthListener_C;

/* Register auth notification listener.
 * listener == NULL clears the current listener. */
ANYCHAT_C_API int anychat_auth_set_listener(AnyChatAuthHandle handle, const AnyChatAuthListener_C* listener);

#ifdef __cplusplus
}
#endif
