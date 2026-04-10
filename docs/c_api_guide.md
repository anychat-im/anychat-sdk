# AnyChat SDK — C API Guide

## Overview

`anychat_c` is a pure C wrapper over the C++ core library.  It exposes a stable
C ABI so you can link against a pre-built binary from any compiler
(MSVC / GCC / Clang) without C++ ABI compatibility issues.

The design follows the same patterns as SQLite, OpenSSL, and libcurl:

- **Opaque handles** — internal C++ objects are hidden behind `typedef struct … *` pointers.
- **Error codes** — every fallible function returns `int` (0 = `ANYCHAT_OK`).
- **Thread-local last-error** — call `anychat_get_last_error()` after a failure for a human-readable message.
- **C callbacks** — `void (*callback)(void* userdata, …)` replaces `std::function`.
- **Explicit memory ownership** — strings and lists returned by the SDK must be freed with the corresponding `anychat_free_*` function.

---

## Quick Start

```c
#include <anychat_c/anychat_c.h>

AnyChatClientConfig_C config = {
    .gateway_url            = "wss://api.anychat.io",
    .api_base_url           = "https://api.anychat.io/api/v1",
    .device_id              = "my-device-id",
    .db_path                = "./anychat.db",
    .connect_timeout_ms     = 10000,
    .max_reconnect_attempts = 5,
    .auto_reconnect         = 1,
};

AnyChatClientHandle client = anychat_client_create(&config);
anychat_client_connect(client);

AnyChatAuthHandle auth = anychat_client_get_auth(client);
anychat_auth_login(auth, "user@example.com", "password", "web", NULL, my_login_cb);

/* … do work … */

anychat_client_disconnect(client);
anychat_client_destroy(client);
```

---

## Header Files

| Header | Contents |
|--------|----------|
| `anychat_c/anychat_c.h` | Master include (pulls in all sub-headers) |
| `anychat_c/errors_c.h`  | Error codes + `anychat_get_last_error()` |
| `anychat_c/types_c.h`   | All C structs, constants, free functions |
| `anychat_c/client_c.h`  | Client lifecycle + sub-module accessors |
| `anychat_c/auth_c.h`    | Authentication |
| `anychat_c/message_c.h` | Messaging |
| `anychat_c/conversation_c.h` | Conversations |
| `anychat_c/friend_c.h`  | Friends & blacklist |
| `anychat_c/group_c.h`   | Groups |
| `anychat_c/file_c.h`    | File upload / download |
| `anychat_c/user_c.h`    | User profile & settings |
| `anychat_c/call_c.h`     | Calls & meetings |

---

## Error Handling

Every function that can fail returns `int`:

- `ANYCHAT_OK` (0) — success
- non-zero — one of the `ANYCHAT_ERROR_*` constants

On failure, call `anychat_get_last_error()` for a descriptive string.
The string is owned by the SDK and is valid until the next SDK call on the
same thread.

```c
AnyChatClientHandle client = anychat_client_create(&config);
if (!client) {
    fprintf(stderr, "error: %s\n", anychat_get_last_error());
    exit(1);
}
```

---

## Memory Management

### Input strings
Pass `const char*` — the SDK copies the value internally. You retain ownership
of your buffer and it only needs to stay alive for the duration of the call.

### Output strings (future extensions)
Strings returned as `char*` are allocated by the SDK with `malloc`.
Free them with `anychat_free_string(str)`.

### Lists
Lists returned via callbacks are **stack-allocated wrappers** with a
**heap-allocated `items` array**.  The callback fires synchronously (from the
SDK's internal thread) and the list is freed by the SDK after the callback
returns.  **Do not store the pointer** — copy what you need inside the callback.

```c
static void on_friends(void* ud, const AnyChatFriendList_C* list, const char* err) {
    for (int i = 0; i < list->count; ++i) {
        /* OK — read data during the callback */
        printf("%s\n", list->items[i].user_id);
    }
    /* Do NOT save list or list->items — they are freed after this returns */
}
```

### Message content field
`AnyChatMessage_C.content` is heap-allocated.  When a message is delivered
via the incoming-message callback, the SDK frees `content` after your callback
returns.  If you need the string later, `strdup()` it inside the callback.

---

## Callbacks and Threading

All callbacks are invoked from the **SDK's internal worker thread**, not the
calling thread.  Rules:

1. Do not call blocking SDK functions from within a callback.
2. Protect shared state with a mutex.
3. You may call **read-only** SDK queries (e.g. `anychat_auth_is_logged_in`)
   from a callback.

---

## Platform Visibility Macros

When building `anychat_c` as a shared library, define `ANYCHAT_C_EXPORTS`
for the SDK translation units.  Consumers do **not** define it.

| Platform | Export | Import |
|----------|--------|--------|
| Windows  | `__declspec(dllexport)` | `__declspec(dllimport)` |
| Linux / macOS | `__attribute__((visibility("default")))` | (same) |

CMake handles this automatically when you set `BUILD_ANYCHAT_C_SHARED=ON`.

---

## CMake Integration

### Static library (default)

```cmake
add_subdirectory(anychat-sdk)

target_link_libraries(my_app PRIVATE anychat_c)
```

### Shared library

```cmake
cmake -DBUILD_ANYCHAT_C_SHARED=ON ..
```

Then link against `libanychat_c.so` / `anychat_c.dll`.

---

## String Encoding

All strings exchanged through the C API are **UTF-8**.

---

## Module Reference

### Client

```c
AnyChatClientHandle anychat_client_create(const AnyChatClientConfig_C* config);
void                anychat_client_destroy(AnyChatClientHandle handle);
void                anychat_client_connect(AnyChatClientHandle handle);
void                anychat_client_disconnect(AnyChatClientHandle handle);
int                 anychat_client_get_connection_state(AnyChatClientHandle handle);
```

Sub-module handles (do **not** destroy individually):
```c
AnyChatAuthHandle    anychat_client_get_auth(AnyChatClientHandle);
AnyChatMessageHandle anychat_client_get_message(AnyChatClientHandle);
AnyChatConvHandle    anychat_client_get_conversation(AnyChatClientHandle);
AnyChatFriendHandle  anychat_client_get_friend(AnyChatClientHandle);
AnyChatGroupHandle   anychat_client_get_group(AnyChatClientHandle);
AnyChatFileHandle    anychat_client_get_file(AnyChatClientHandle);
AnyChatUserHandle    anychat_client_get_user(AnyChatClientHandle);
AnyChatCallHandle     anychat_client_get_call(AnyChatClientHandle);
```

### Auth

```c
int anychat_auth_login(handle, account, password, device_type, userdata, callback);
int anychat_auth_register(handle, phone_or_email, password, verify_code,
                           device_type, nickname, userdata, callback);
int anychat_auth_logout(handle, userdata, callback);
int anychat_auth_refresh_token(handle, refresh_token, userdata, callback);
int anychat_auth_change_password(handle, old_password, new_password, userdata, callback);
int anychat_auth_is_logged_in(handle);
int anychat_auth_get_current_token(handle, out_token);
int anychat_auth_set_listener(handle, listener /* AnyChatAuthListener_C* */);
```

### Message

```c
int  anychat_message_send_text(handle, conv_id, content, userdata, callback);
int  anychat_message_get_history(handle, conv_id, before_ms, limit, userdata, callback);
int  anychat_message_mark_read(handle, conv_id, message_id, userdata, callback);
int  anychat_message_get_offline(handle, last_seq, limit, userdata, callback);
int  anychat_message_ack(handle, conv_id, message_ids, message_count, userdata, callback);
int  anychat_message_get_group_read_state(handle, group_id, message_id, userdata, callback);
int  anychat_message_search(handle, keyword, conversation_id, content_type, limit, offset, userdata, callback);
int  anychat_message_recall(handle, message_id, userdata, callback);
int  anychat_message_delete(handle, message_id, userdata, callback);
int  anychat_message_edit(handle, message_id, content, userdata, callback);
int  anychat_message_send_typing(handle, conversation_id, typing, ttl_seconds, userdata, callback);
int  anychat_message_set_listener(handle, listener /* AnyChatMessageListener_C* */);
```

### Conversation

```c
int  anychat_conv_get_list(handle, userdata, callback);
int  anychat_conv_mark_read(handle, conv_id, userdata, callback);
int  anychat_conv_set_pinned(handle, conv_id, pinned, userdata, callback);
int  anychat_conv_set_muted(handle, conv_id, muted, userdata, callback);
int  anychat_conv_delete(handle, conv_id, userdata, callback);
int  anychat_conv_set_listener(handle, listener /* AnyChatConvListener_C* */);
```

### Friend

```c
int  anychat_friend_get_list(handle, userdata, callback);
int  anychat_friend_send_request(handle, to_user_id, message, userdata, callback);
int  anychat_friend_send_request_with_source(handle, to_user_id, message, source, userdata, callback);
int  anychat_friend_handle_request(handle, request_id, accept, userdata, callback);
int  anychat_friend_get_pending_requests(handle, userdata, callback);
int  anychat_friend_get_requests(handle, request_type, userdata, callback);
int  anychat_friend_delete(handle, friend_id, userdata, callback);
int  anychat_friend_update_remark(handle, friend_id, remark, userdata, callback);
int  anychat_friend_get_blacklist(handle, userdata, callback);
int  anychat_friend_add_to_blacklist(handle, user_id, userdata, callback);
int  anychat_friend_remove_from_blacklist(handle, user_id, userdata, callback);
int  anychat_friend_set_listener(handle, listener /* AnyChatFriendListener_C* */);
```

### Group

```c
int  anychat_group_get_list(handle, userdata, callback);
int  anychat_group_get_info(handle, group_id, userdata, callback);
int  anychat_group_create(handle, name, member_ids, member_count, userdata, callback);
int  anychat_group_join(handle, group_id, message, userdata, callback);
int  anychat_group_invite(handle, group_id, user_ids, user_count, userdata, callback);
int  anychat_group_quit(handle, group_id, userdata, callback);
int  anychat_group_disband(handle, group_id, userdata, callback);
int  anychat_group_update(handle, group_id, name, avatar_url, userdata, callback);
int  anychat_group_get_members(handle, group_id, page, page_size, userdata, callback);
int  anychat_group_remove_member(handle, group_id, user_id, userdata, callback);
int  anychat_group_update_member_role(handle, group_id, user_id, role, userdata, callback);
int  anychat_group_update_nickname(handle, group_id, nickname, userdata, callback);
int  anychat_group_transfer_ownership(handle, group_id, new_owner_id, userdata, callback);
int  anychat_group_get_join_requests(handle, group_id, status, userdata, callback);
int  anychat_group_handle_join_request(handle, group_id, request_id, accept, userdata, callback);
int  anychat_group_get_qrcode(handle, group_id, userdata, callback);
int  anychat_group_refresh_qrcode(handle, group_id, userdata, callback);
int  anychat_group_set_listener(handle, listener /* AnyChatGroupListener_C* */);
```

### File

```c
int anychat_file_upload(handle, local_path, file_type, userdata, on_progress, on_done);
int anychat_file_get_download_url(handle, file_id, userdata, callback);
int anychat_file_get_info(handle, file_id, userdata, callback);
int anychat_file_list(handle, file_type, page, page_size, userdata, callback);
int anychat_file_upload_log(handle, local_path, expires_hours, userdata, on_progress, on_done);
int anychat_file_delete(handle, file_id, userdata, callback);
```

### User

```c
int anychat_user_get_profile(handle, userdata, callback);
int anychat_user_update_profile(handle, profile, userdata, callback);
int anychat_user_get_settings(handle, userdata, callback);
int anychat_user_update_settings(handle, settings, userdata, callback);
int anychat_user_update_push_token(handle, push_token, platform, userdata, callback);
int anychat_user_search(handle, keyword, page, page_size, userdata, callback);
int anychat_user_get_info(handle, user_id, userdata, callback);
int anychat_user_set_listener(handle, listener /* AnyChatUserListener_C* */);
```

### Call

```c
int  anychat_call_initiate_call(handle, callee_id, call_type, userdata, callback);
int  anychat_call_join_call(handle, call_id, userdata, callback);
int  anychat_call_reject_call(handle, call_id, userdata, callback);
int  anychat_call_end_call(handle, call_id, userdata, callback);
int  anychat_call_get_call_session(handle, call_id, userdata, callback);
int  anychat_call_get_call_logs(handle, page, page_size, userdata, callback);
int  anychat_call_create_meeting(handle, title, password, max_participants, userdata, callback);
int  anychat_call_join_meeting(handle, room_id, password, userdata, callback);
int  anychat_call_end_meeting(handle, room_id, userdata, callback);
int  anychat_call_get_meeting(handle, room_id, userdata, callback);
int  anychat_call_list_meetings(handle, page, page_size, userdata, callback);
int  anychat_call_set_listener(handle, listener /* AnyChatCallListener_C* */);
```

---

## Memory Leak Detection

```bash
valgrind --leak-check=full --show-leak-kinds=all \
    ./build/bin/c_example
```

---

## ABI Verification

After building, confirm all exported symbols use C linkage (no mangling):

```bash
# Linux
nm -D build/bin/libanychat_c.so | grep ' T ' | grep anychat

# macOS
nm -gU build/bin/libanychat_c.dylib | grep anychat
```

All symbols should appear as plain `anychat_*` without C++ name-mangling.
