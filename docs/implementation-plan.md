# AnyChat SDK Core — Implementation Plan

## Guiding Principle

**Infrastructure first → Framework skeleton → Business modules**

Build from the inside out:
1. Pure-infrastructure utilities (SQLite, LRU cache) — no business logic, no network
2. Public API surface and wiring (types, interfaces, client scaffolding)
3. Core engines (notifications, sync, outbound queue, heartbeat)
4. Business-logic modules (message, conversation, friend, group, file)
5. Build system wiring and tests

---

## Phase 1 — Infrastructure Layer ✅

### 1.1 SQLite Wrapper (`core/src/db/`)
- `database.h` / `database.cpp` — single worker-thread DB engine
  - All SQL dispatched to `db_thread_` via `std::queue` + `std::condition_variable`
  - `DbValue = std::variant<nullptr_t, int64_t, double, string>` maps to SQLite's 4 types
  - `exec()` / `query()` async (fire-and-forget + callback)
  - `execSync()` / `querySync()` synchronous (block caller via promise/future)
  - `TxScope` inner class: execute SQL inside a running transaction without re-queuing (avoids deadlock)
  - `transactionSync()`: BEGIN → fn(TxScope) → COMMIT/ROLLBACK
  - WAL mode enabled at open

- `migrations.h` / `migrations.cpp` — schema version management
  - `PRAGMA user_version` checked on every open
  - `runMigrations()` applies upgrades sequentially (`kCurrentSchemaVersion = 1`)
  - Schema: `users`, `friends`, `conversations`, `messages`, `groups`, `outbound_queue`, `metadata`

### 1.2 In-Memory Cache (`core/src/cache/`)
- `lru_cache.h` — generic LRU (capacity-bounded), `std::shared_mutex`, O(1) get/put
- `conversation_cache.h` — sorted conversation list; pinned-first, then `last_msg_time_ms` desc
- `message_cache.h` — per-conversation message buckets (max 100 per conv), seq-gap detection

---

## Phase 2 — Public API Surface ✅

### 2.1 Updated Headers (`core/include/anychat/`)
- `types.h` — added `AuthToken`, `Conversation`, `Friend`, `FriendRequest`, `Group`, `GroupMember`, `FileInfo`, `SendState`, `UserProfile`, `UserSettings`, `CallType`, `CallStatus`, `CallSession`, `MeetingRoom`; enriched `Message` with `local_id`, `conv_id`, `seq`, `reply_to`, `send_state`
- `conversation.h` — `ConversationManager` abstract interface
- `friend.h` — `FriendManager` abstract interface
- `group.h` — `GroupManager` abstract interface
- `file.h` — `FileManager` abstract interface
- `user.h` — `UserManager` abstract interface (getProfile, updateProfile, getSettings, updateSettings, updatePushToken, searchUsers, getUserInfo)
- `call.h` — `CallManager` abstract interface (one-to-one calls + meeting rooms + incoming-call notifications)
- `client.h` — added `db_path` to `ClientConfig`; added `conversationMgr()`, `friendMgr()`, `groupMgr()`, `fileMgr()`, `userMgr()`, `callMgr()` pure virtuals

---

## Phase 3 — Core Engines

### 3.1 Notification Manager (`core/src/notification_manager.h/cpp`)
WebSocket message dispatcher with multi-subscriber fan-out:
- Receives raw JSON from WebSocket
- Routes `message.sent` acks to `OutboundQueue`
- Routes `notification` events to **all** registered handlers via `addNotificationHandler()`
  (replaces the former single-slot `setOnNotification()`; each business module
  registers independently without overwriting others)
- Routes `pong` to heartbeat monitor

### 3.2 Sync Engine (`core/src/sync_engine.h/cpp`)
Incremental data sync on connect:
- Reads `last_sync_time` from DB `metadata` table
- POST `/sync` with `lastSyncTime` + per-conversation `lastSeq`
- Merges friends, groups, sessions, messages into DB and caches
- Updates `last_sync_time` in DB after successful sync

### 3.3 Outbound Queue (`core/src/outbound_queue.h/cpp`)
Reliable message delivery:
- Persists unsent messages to `outbound_queue` table
- Sends over WebSocket when connected
- Matches `message.sent` ack by `local_id`, marks as sent
- On reconnect, retries all `pending` rows in the queue

### 3.4 Heartbeat
Integrated into `ConnectionManager`:
- Sends `{"type":"ping"}` every 30 s while connected
- Expects `{"type":"pong"}` within 10 s
- Two missed pongs → treats as disconnect, triggers `onWsDisconnected()`

### 3.5 Auth — Token Persistence
Updated `auth_manager.cpp` / `auth_manager.h` (renamed from `auth.cpp` / `auth_impl.h`):
- `createAuthManager` accepts `db::Database*`
- On construct: load token from DB `metadata` and set on `HttpClient`
- `storeToken()`: persist to DB
- `clearToken()`: remove from DB

---

## Phase 4 — Business Modules ✅

### 4.1 Message (`core/src/message_manager.h/cpp`)
- `sendTextMessage`: generate `local_id`, enqueue in `OutboundQueue`, return optimistic message
- `getHistory`: try cache → DB → HTTP fallback
- Incoming `message.new` notification handler: dedup by `message_id`, seq-gap check

### 4.2 Conversation (`core/src/conversation_manager.h/cpp`)
- `ConversationManagerImpl`: reads from `ConversationCache`; HTTP for missing items
- `markRead` → POST `/sessions/{id}/read`
- `setPinned` / `setMuted` → HTTP + cache update
- Receives session updates from `NotificationManager`

### 4.3 Friend (`core/src/friend_manager.h/cpp`)
- `FriendManagerImpl`: get list (cache → DB), send/handle requests, blacklist
- Receives `friend.*` notifications, updates DB and emits callbacks

### 4.4 Group (`core/src/group_manager.h/cpp`)
- `GroupManagerImpl`: get list (DB), create/join/invite/quit
- Receives `group.*` notifications

### 4.5 File (`core/src/file_manager.h/cpp`)
- `FileManagerImpl`: 3-step upload (POST `/files/upload-token` → PUT presigned URL → POST `/files/{id}/complete`)
- `getDownloadUrl` → GET `/files/{id}/download`

### 4.6 User (`core/src/user_manager.h/cpp`)
- `UserManagerImpl`: profile and settings CRUD + push token + user search
- `getProfile` / `updateProfile` → GET/PUT `/users/me`
- `getSettings` / `updateSettings` → GET/PUT `/users/me/settings`
- `updatePushToken` → POST `/users/me/push-token`
- `searchUsers` → GET `/users/search?keyword=...`
- `getUserInfo` → GET `/users/{userId}`

### 4.7 Call (`core/src/call_manager.h/cpp`)
- `CallManagerImpl`: one-to-one calls + meeting rooms
- Calls: `initiateCall` / `joinCall` / `rejectCall` / `endCall` / `getCallSession` / `getCallLogs`
- Meetings: `createMeeting` / `joinMeeting` / `endMeeting` / `getMeeting` / `listMeetings`
- Registers `addNotificationHandler` on `NotificationManager` at construction time to receive `livekit.call_invite`, `livekit.call_status`, `livekit.call_rejected`
- Exposes `setOnIncomingCall` and `setOnCallStatusChanged` to the application layer

---

## Phase 5 — Build System & Tests ✅

### 5.1 `core/CMakeLists.txt`
- All `.cpp` source files added to `anychat_core` target
- `find_package(SQLite3 REQUIRED)` + linked to target
- `CPMAddPackage` for nlohmann/json

### 5.2 `core/tests/CMakeLists.txt`
- All test sources listed

### 5.3 Test Files
- `test_types.cpp` — struct construction, enum values
- `test_client.cpp` — AnyChatClient factory validation
- `test_connection_manager.cpp` — state machine transitions
- `test_database.cpp` — open/migrate/read-write/transaction
- `test_cache.cpp` — LRU eviction, ConversationCache sort, MessageCache gap detection
- `test_auth.cpp` — token persistence across restarts
- `test_notification_manager.cpp` — dispatch routing, multi-subscriber fan-out
- `test_outbound_queue.cpp` — retry on reconnect
- `test_sync_engine.cpp` — construction and no-crash sync call
- `test_message_manager.cpp` — send / history / dedup
- `test_conversation_manager.cpp` — session notification routing
- `test_friend_manager.cpp` — friend.request / friend.deleted notification routing
- `test_group_manager.cpp` — group.invited / group.info_updated notification routing
- `test_file_manager.cpp` — upload error path, download URL, delete
- `test_user_manager.cpp` — profile / settings / search no-crash
- `test_call_manager.cpp` — livekit call_invite / call_status / call_rejected routing

---

## Dependency Graph

```
Phase 1 (db, cache)                                         ✅
    └── Phase 2 (public headers, ClientConfig.db_path)      ✅
            └── Phase 3 (engines: notif, sync, outbound,    ✅
                         heartbeat, auth persistence)
                    └── Phase 4 (business modules:          ✅
                                 message, conv, friend,
                                 group, file, user, call)
                            └── Phase 5 (CMakeLists,        ✅
                                         tests)
```
