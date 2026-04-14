# AnyChat SDK — C API Guide

## Overview

`anychat` exposes the SDK's public C ABI.
The current public headers live under `core/include/anychat/`, and the primary
entry point is:

```c
#include <anychat/anychat.h>
```

Key properties of the current C API:

- Stable C ABI for MSVC / GCC / Clang consumers
- Opaque handles for all manager objects
- Asynchronous request model based on typed callback structs
- Thread-local last-error string via `anychat_get_last_error()`
- UTF-8 for all public strings

The design follows the usual C wrapper patterns used by projects such as
SQLite, OpenSSL, and libcurl, but the concrete API surface is defined by the
current headers in `core/include/anychat/`.

---

## Public Headers

Use `anychat/anychat.h` as the default include. It pulls in all public module
headers.

| Header | Purpose |
|--------|---------|
| `anychat/anychat.h` | Master include for the full C API |
| `anychat/client.h` | Client lifecycle, login/logout, manager accessors |
| `anychat/auth.h` | Authentication APIs and auth listener |
| `anychat/message.h` | Messaging APIs and message listener |
| `anychat/conversation.h` | Conversation APIs and conversation listener |
| `anychat/friend.h` | Friend, request, blacklist APIs and listener |
| `anychat/group.h` | Group APIs and group listener |
| `anychat/file.h` | File upload/download/list APIs |
| `anychat/user.h` | User profile/settings/search APIs and listener |
| `anychat/call.h` | Call and meeting APIs and listener |
| `anychat/version.h` | Version check/report APIs |
| `anychat/types.h` | Opaque handles, POD structs, constants, free helpers |
| `anychat/errors.h` | Error codes and `anychat_get_last_error()` |
| `anychat/export.h` | Export/import macros used by the library build |

---

## Building and Linking

### Building this repository

The repository's standard Native build entry remains:

```bash
git submodule update --init --recursive
python3 scripts/build-native.py --test
```

Build prerequisites relevant to the C API library are the same as the rest of
the SDK:

- `curl`, `glaze`, `googletest`, `libwebsockets` are integrated as submodules
- `sqlite3` is bundled as source in `thirdparty/sqlite3`
- `OpenSSL` must be installed separately on each platform

### Linking from CMake source integration

If you vendor the repository as a subdirectory, link the current library target:

```cmake
add_subdirectory(path/to/anychat-sdk)
target_link_libraries(my_app PRIVATE AnyChat::anychat)
```

Current CMake facts:

- Library target name: `anychat`
- Build-tree alias: `AnyChat::anychat`
- Default library type: static
- Shared-library option: `BUILD_ANYCHAT_SHARED=ON`

For prebuilt-library consumption, add the public include path containing
`anychat/anychat.h` and link the produced `anychat` library.

### Shared library notes

When the library is configured as a shared library:

- The SDK build sets `ANYCHAT_C_EXPORTS` for itself
- The SDK publishes `ANYCHAT_C_SHARED` to consumers
- Consumers should not define `ANYCHAT_C_EXPORTS` manually
- Windows shared builds also require the platform's OpenSSL runtime DLLs

---

## Quick Start

`anychat_client_login()` is the current high-level entry for "log in and bring
up the client session". There are no public `anychat_client_connect()` or
`anychat_client_disconnect()` functions in the current headers.

```c
#include <anychat/anychat.h>
#include <stdio.h>
#include <string.h>

static void on_connection_state(void* userdata, int state) {
    (void)userdata;
    printf("connection state = %d\n", state);
}

static void on_login_success(void* userdata, const AnyChatAuthToken_C* token) {
    (void)userdata;
    printf("login ok, access token = %s\n", token->access_token);
}

static void on_login_error(void* userdata, int code, const char* error) {
    (void)userdata;
    fprintf(stderr, "login failed: code=%d error=%s\n", code, error ? error : "");
}

int main(void) {
    AnyChatClientConfig_C config = {
        .gateway_url = "wss://api.anychat.io",
        .api_base_url = "https://api.anychat.io/api/v1",
        .device_id = "desktop-dev-001",
        .db_path = "./anychat.db",
        .connect_timeout_ms = 10000,
        .max_reconnect_attempts = 5,
        .auto_reconnect = 1,
    };

    AnyChatClientHandle client = anychat_client_create(&config);
    if (!client) {
        fprintf(stderr, "create failed: %s\n", anychat_get_last_error());
        return 1;
    }

    anychat_client_set_connection_callback(client, NULL, on_connection_state);

    AnyChatAuthTokenCallback_C login_cb = {0};
    login_cb.struct_size = sizeof(login_cb);
    login_cb.userdata = NULL;
    login_cb.on_success = on_login_success;
    login_cb.on_error = on_login_error;

    int rc = anychat_client_login(
        client,
        "user@example.com",
        "password",
        "web",
        "1.0.0",
        &login_cb
    );
    if (rc != ANYCHAT_OK) {
        fprintf(stderr, "dispatch failed: %s\n", anychat_get_last_error());
        anychat_client_destroy(client);
        return 1;
    }

    /* Run your event loop here. */

    anychat_client_destroy(client);
    return 0;
}
```

---

## Error Handling

Current public error codes in `anychat/errors.h`:

- `ANYCHAT_OK`
- `ANYCHAT_ERROR_INVALID_PARAM`
- `ANYCHAT_ERROR_AUTH`
- `ANYCHAT_ERROR_NETWORK`
- `ANYCHAT_ERROR_TIMEOUT`
- `ANYCHAT_ERROR_NOT_FOUND`
- `ANYCHAT_ERROR_ALREADY_EXISTS`
- `ANYCHAT_ERROR_INTERNAL`
- `ANYCHAT_ERROR_NOT_LOGGED_IN`
- `ANYCHAT_ERROR_TOKEN_EXPIRED`

The general pattern is:

1. A function returns `ANYCHAT_OK` when the request was accepted or dispatched.
2. Completion then arrives asynchronously through the callback you provided.
3. Immediate argument/dispatch failures can be inspected with `anychat_get_last_error()`.

`anychat_get_last_error()` returns a thread-local string owned by the SDK. The
pointer remains valid until the next SDK call on the same thread.

---

## Callback Conventions

Most asynchronous APIs use a typed callback struct defined in the matching
module header.

Example:

```c
AnyChatUserProfileCallback_C cb = {0};
cb.struct_size = sizeof(cb);
cb.userdata = my_context;
cb.on_success = on_profile;
cb.on_error = on_profile_error;
```

Current callback rules:

- `struct_size` must be set to at least `sizeof(the_callback_struct)`
- `userdata` is passed back unchanged
- Omitted handlers may be left as `NULL`
- Passing `NULL` for the whole callback struct is allowed for most async APIs if
  you do not need completion notification
- Listener registration APIs clear the current listener when you pass `NULL`

The connection-state callback is the main exception: it is a raw function
pointer plus a raw `userdata` pointer, not a typed callback struct.

---

## Threading and Ownership

### Threading

Callbacks and listeners are invoked on SDK-managed internal threads, not on the
thread that initiated the request.

Practical rules:

- Do not block SDK callback threads for long periods
- Protect shared mutable state in your application
- If your UI requires a main thread, marshal callback results yourself

### Lifetime and ownership

The current implementation uses callback-scoped payloads.

- Input strings passed into SDK functions are copied during the call
- Callback payload pointers are only valid for the duration of that callback
- Listener payload pointers are also callback-scoped
- If you need to keep any payload, copy it before the callback returns

Important current details:

- `AnyChatMessage_C.content` in message listeners is allocated internally and
  freed by the SDK immediately after the listener returns
- List/result wrappers delivered to async success callbacks are temporary; the
  SDK releases their backing storage after your callback returns
- Callback string arguments such as error strings, download URLs, IDs, and other
  transient pointers should also be treated as callback-scoped
- Synchronous output structs such as `AnyChatAuthToken_C out_token` in
  `anychat_client_get_current_token()` and `anychat_auth_get_current_token()`
  are caller-owned because the caller provides the storage

`anychat/types.h` also exposes `anychat_free_*` helpers, but the current
callback-based C API does not transfer ownership of the normal payloads it
delivers to your callbacks.

---

## Client Lifecycle

Current client lifecycle shape:

- Create a client with `anychat_client_create()`
- Log in with `anychat_client_login()` or work with the auth manager directly
- Access module handles with `anychat_client_get_*()`
- Log out with `anychat_client_logout()` when needed
- Destroy the client with `anychat_client_destroy()`

Sub-module handles returned by `anychat_client_get_auth()`,
`anychat_client_get_message()`, and the other accessor functions are owned by
the client. Do not destroy them separately.

---

## Module Reference

The following lists reflect the current public header surface.

### Client

```c
AnyChatClientHandle anychat_client_create(const AnyChatClientConfig_C* config);
void anychat_client_destroy(AnyChatClientHandle handle);

int anychat_client_login(handle, account, password, device_type, client_version, callback);
int anychat_client_logout(handle, callback);
int anychat_client_is_logged_in(handle);
int anychat_client_get_current_token(handle, out_token);
int anychat_client_get_connection_state(handle);
void anychat_client_set_connection_callback(handle, userdata, callback);

AnyChatAuthHandle anychat_client_get_auth(handle);
AnyChatMessageHandle anychat_client_get_message(handle);
AnyChatConvHandle anychat_client_get_conversation(handle);
AnyChatFriendHandle anychat_client_get_friend(handle);
AnyChatGroupHandle anychat_client_get_group(handle);
AnyChatFileHandle anychat_client_get_file(handle);
AnyChatUserHandle anychat_client_get_user(handle);
AnyChatCallHandle anychat_client_get_call(handle);
AnyChatVersionHandle anychat_client_get_version(handle);
```

### Auth

```c
int anychat_auth_login(handle, account, password, device_type, client_version, callback);
int anychat_auth_register(handle, phone_or_email, password, verify_code, device_type, nickname, client_version, callback);
int anychat_auth_send_code(handle, target, target_type, purpose, callback);
int anychat_auth_logout(handle, callback);
int anychat_auth_refresh_token(handle, refresh_token, callback);
int anychat_auth_change_password(handle, old_password, new_password, callback);
int anychat_auth_reset_password(handle, account, verify_code, new_password, callback);
int anychat_auth_get_device_list(handle, callback);
int anychat_auth_logout_device(handle, device_id, callback);
int anychat_auth_is_logged_in(handle);
int anychat_auth_get_current_token(handle, out_token);
int anychat_auth_set_listener(handle, listener);
```

Notes:

- `device_type` currently follows string values such as `ios`, `android`, `web`
- `anychat_auth_set_listener(NULL)` clears the auth listener

### Message

```c
int anychat_message_send_text(handle, conv_id, content, callback);
int anychat_message_get_history(handle, conv_id, before_timestamp_ms, limit, callback);
int anychat_message_mark_read(handle, conv_id, message_id, callback);
int anychat_message_get_offline(handle, last_seq, limit, callback);
int anychat_message_ack(handle, conv_id, message_ids, message_count, callback);
int anychat_message_get_group_read_state(handle, group_id, message_id, callback);
int anychat_message_search(handle, keyword, conversation_id, content_type, limit, offset, callback);
int anychat_message_recall(handle, message_id, callback);
int anychat_message_delete(handle, message_id, callback);
int anychat_message_edit(handle, message_id, content, callback);
int anychat_message_send_typing(handle, conversation_id, typing, ttl_seconds, callback);
int anychat_message_set_listener(handle, listener);
```

### Conversation

```c
int anychat_conv_get_list(handle, callback);
int anychat_conv_get_total_unread(handle, callback);
int anychat_conv_get(handle, conv_id, callback);
int anychat_conv_mark_all_read(handle, conv_id, callback);
int anychat_conv_mark_messages_read(handle, conv_id, message_ids, message_id_count, callback);
int anychat_conv_set_pinned(handle, conv_id, pinned, callback);
int anychat_conv_set_muted(handle, conv_id, muted, callback);
int anychat_conv_set_burn_after_reading(handle, conv_id, duration, callback);
int anychat_conv_set_auto_delete(handle, conv_id, duration, callback);
int anychat_conv_delete(handle, conv_id, callback);
int anychat_conv_get_message_unread_count(handle, conv_id, last_read_seq, callback);
int anychat_conv_get_message_read_receipts(handle, conv_id, callback);
int anychat_conv_get_message_sequence(handle, conv_id, callback);
int anychat_conv_set_listener(handle, listener);
```

### Friend

```c
int anychat_friend_get_list(handle, callback);
int anychat_friend_add(handle, to_user_id, message, source, callback);
int anychat_friend_accept_request(handle, request_id, callback);
int anychat_friend_reject_request(handle, request_id, callback);
int anychat_friend_get_requests(handle, request_type, callback);
int anychat_friend_delete(handle, friend_id, callback);
int anychat_friend_update_remark(handle, friend_id, remark, callback);
int anychat_friend_add_to_blacklist(handle, user_id, callback);
int anychat_friend_remove_from_blacklist(handle, user_id, callback);
int anychat_friend_get_blacklist(handle, callback);
int anychat_friend_set_listener(handle, listener);
```

Notes:

- `request_type` currently accepts `received` or `sent`

### Group

```c
int anychat_group_get_list(handle, callback);
int anychat_group_get_info(handle, group_id, callback);
int anychat_group_create(handle, name, member_ids, member_count, callback);
int anychat_group_join(handle, group_id, message, callback);
int anychat_group_invite(handle, group_id, user_ids, user_count, callback);
int anychat_group_quit(handle, group_id, callback);
int anychat_group_disband(handle, group_id, callback);
int anychat_group_update(handle, group_id, name, avatar_url, callback);
int anychat_group_get_members(handle, group_id, page, page_size, callback);
int anychat_group_remove_member(handle, group_id, user_id, callback);
int anychat_group_update_member_role(handle, group_id, user_id, role, callback);
int anychat_group_update_nickname(handle, group_id, nickname, callback);
int anychat_group_transfer_ownership(handle, group_id, new_owner_id, callback);
int anychat_group_get_join_requests(handle, group_id, status, callback);
int anychat_group_handle_join_request(handle, group_id, request_id, accept, callback);
int anychat_group_get_qrcode(handle, group_id, callback);
int anychat_group_refresh_qrcode(handle, group_id, callback);
int anychat_group_set_listener(handle, listener);
```

Notes:

- `role` is currently parsed from strings such as `owner`, `admin`, `member`

### File

```c
int anychat_file_upload(handle, local_path, file_type, on_progress, on_done);
int anychat_file_get_download_url(handle, file_id, callback);
int anychat_file_get_info(handle, file_id, callback);
int anychat_file_list(handle, file_type, page, page_size, callback);
int anychat_file_upload_log(handle, local_path, expires_hours, on_progress, on_done);
int anychat_file_delete(handle, file_id, callback);
```

Notes:

- `file_type` currently accepts `image`, `video`, `audio`, `file`
- `on_progress` may be `NULL`

### User

```c
int anychat_user_get_profile(handle, callback);
int anychat_user_update_profile(handle, profile, callback);
int anychat_user_get_settings(handle, callback);
int anychat_user_update_settings(handle, settings, callback);
int anychat_user_update_push_token(handle, push_token, platform, callback);
int anychat_user_update_push_token_with_device(handle, push_token, platform, device_id, callback);
int anychat_user_search(handle, keyword, page, page_size, callback);
int anychat_user_get_info(handle, user_id, callback);
int anychat_user_bind_phone(handle, phone_number, verify_code, callback);
int anychat_user_change_phone(handle, old_phone_number, new_phone_number, new_verify_code, old_verify_code, callback);
int anychat_user_bind_email(handle, email, verify_code, callback);
int anychat_user_change_email(handle, old_email, new_email, new_verify_code, old_verify_code, callback);
int anychat_user_refresh_qrcode(handle, callback);
int anychat_user_get_by_qrcode(handle, qrcode, callback);
int anychat_user_set_listener(handle, listener);
```

Notes:

- Push-token `platform` currently uses strings such as `ios`, `android`, `web`

### Call

```c
int anychat_call_initiate_call(handle, callee_id, call_type, callback);
int anychat_call_join_call(handle, call_id, callback);
int anychat_call_reject_call(handle, call_id, callback);
int anychat_call_end_call(handle, call_id, callback);
int anychat_call_get_call_session(handle, call_id, callback);
int anychat_call_get_call_logs(handle, page, page_size, callback);
int anychat_call_create_meeting(handle, title, password, max_participants, callback);
int anychat_call_join_meeting(handle, room_id, password, callback);
int anychat_call_end_meeting(handle, room_id, callback);
int anychat_call_get_meeting(handle, room_id, callback);
int anychat_call_list_meetings(handle, page, page_size, callback);
int anychat_call_set_listener(handle, listener);
```

Notes:

- `call_type` uses `ANYCHAT_CALL_AUDIO` or `ANYCHAT_CALL_VIDEO`

### Version

```c
int anychat_version_check(handle, platform, version, build_number, callback);
int anychat_version_get_latest(handle, platform, release_type, callback);
int anychat_version_list(handle, platform, release_type, page, page_size, callback);
int anychat_version_report(handle, platform, version, build_number, device_id, os_version, sdk_version, callback);
```

---

## Summary

The current C API documentation should now be read with these three facts in
mind:

- Public include path is `anychat/...`, not `anychat_c/...`
- Public library target is `anychat` / `AnyChat::anychat`, not `anychat_c`
- Current callback payloads are temporary and must be copied if you need them
  after the callback returns
