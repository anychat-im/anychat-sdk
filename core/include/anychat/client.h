#pragma once

#include "auth.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Client configuration ---- */
typedef struct {
    const char* gateway_url; /* WebSocket gateway, e.g. "wss://api.anychat.io" */
    const char* api_base_url; /* HTTP API base, e.g. "https://api.anychat.io/api/v1" */
    const char* device_id; /* Unique device identifier */
    const char* db_path; /* SQLite database file path */
    int connect_timeout_ms; /* default: 10000 */
    int max_reconnect_attempts; /* default: 5 */
    int auto_reconnect; /* 1 = enabled (default), 0 = disabled */
} AnyChatClientConfig_C;

/* Connection state change callback.
 * state: one of ANYCHAT_STATE_* constants */
typedef void (*AnyChatConnectionStateCallback)(void* userdata, int state);

/* ---- Lifecycle ---- */

/* Create a new client. Returns NULL on failure.
 * The caller owns the handle and must destroy it with anychat_client_destroy(). */
ANYCHAT_C_API AnyChatClientHandle anychat_client_create(const AnyChatClientConfig_C* config);

/* Destroy the client and release all resources.
 * Must not be called while callbacks are in flight on other threads. */
ANYCHAT_C_API void anychat_client_destroy(AnyChatClientHandle handle);

/* ---- Authentication & Connection ---- */

/* Login with account/password and automatically establish WebSocket connection.
 * Note: WebSocket auto-reconnect is handled internally by the SDK.
 * client_version: client version string (e.g. "1.0.0"), can be NULL */
ANYCHAT_C_API int anychat_client_login(
    AnyChatClientHandle handle,
    const char* account,
    const char* password,
    const char* device_type,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
);

/* Logout: disconnect WebSocket and call HTTP logout endpoint. */
ANYCHAT_C_API int anychat_client_logout(AnyChatClientHandle handle, const AnyChatAuthResultCallback_C* callback);

/* Check if the user is currently logged in. Returns 1 if logged in, 0 otherwise. */
ANYCHAT_C_API int anychat_client_is_logged_in(AnyChatClientHandle handle);

/* Get current auth token (if logged in). Returns 0 on success, -1 if not logged in. */
ANYCHAT_C_API int anychat_client_get_current_token(AnyChatClientHandle handle, AnyChatAuthToken_C* out_token);

/* Returns the current connection state (ANYCHAT_STATE_*). */
ANYCHAT_C_API int anychat_client_get_connection_state(AnyChatClientHandle handle);

/* Register a callback for connection state changes.
 * Pass NULL to clear a previously registered callback.
 * userdata is passed through unchanged. */
ANYCHAT_C_API void anychat_client_set_connection_callback(
    AnyChatClientHandle handle,
    void* userdata,
    AnyChatConnectionStateCallback callback
);

/* ---- Sub-module accessors ----
 * The returned handles are owned by the client; do NOT destroy them separately. */
ANYCHAT_C_API AnyChatAuthHandle anychat_client_get_auth(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatMessageHandle anychat_client_get_message(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatConvHandle anychat_client_get_conversation(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatFriendHandle anychat_client_get_friend(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatGroupHandle anychat_client_get_group(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatFileHandle anychat_client_get_file(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatUserHandle anychat_client_get_user(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatCallHandle anychat_client_get_call(AnyChatClientHandle handle);
ANYCHAT_C_API AnyChatVersionHandle anychat_client_get_version(AnyChatClientHandle handle);

#ifdef __cplusplus
}
#endif
