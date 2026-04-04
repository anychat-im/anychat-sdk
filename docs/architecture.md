# AnyChat SDK Architecture

## 层次结构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│            Flutter / Web / Android Native / iOS Native           │
├─────────────────────────────────────────────────────────────────┤
│                  Platform SDK  (packages/)                        │
│        Dart · TypeScript · Kotlin · Swift · C++ header-only      │
│   职责：UI 友好的 API 封装、响应式状态适配、平台推送集成            │
├─────────────────────────────────────────────────────────────────┤
│               Platform Binding Layer  (packages/)                │
│          Dart FFI · Emscripten postMessage · JNI · ObjC          │
│   职责：跨语言调用桥接、线程调度、平台路径/存储注入                 │
├──────────────────────────────┬──────────────────────────────────┤
│    C++ Core SDK  (core/)     │   Web fallback (packages/web/)    │
│                              │   TypeScript 原生实现              │
│  所有平台共用的业务逻辑        │   (浏览器无法运行 C++ Core)         │
│  ┌─────────────────────┐    │                                    │
│  │   Public API Layer  │    │                                    │
│  ├─────────────────────┤    │                                    │
│  │  Business Logic     │    │                                    │
│  │  - Auth & Token     │    │                                    │
│  │  - Sync Engine      │    │                                    │
│  │  - Message Queue    │    │                                    │
│  │  - Notification Mgr │    │                                    │
│  │  - Message / Conv   │    │                                    │
│  │  - Friend / Group   │    │                                    │
│  │  - File / User      │    │                                    │
│  │  - Call (calls +    │    │                                    │
│  │    meetings)        │    │                                    │
│  ├─────────────────────┤    │                                    │
│  │  In-Memory Cache    │    │                                    │
│  ├─────────────────────┤    │                                    │
│  │  Persistence (SQLite│    │                                    │
│  │  via injected path) │    │                                    │
│  ├─────────────────────┤    │                                    │
│  │  Network Layer      │    │                                    │
│  │  - HTTP (libcurl)   │    │                                    │
│  │  - WebSocket (lws)  │    │                                    │
│  └─────────────────────┘    │                                    │
└──────────────────────────────┴──────────────────────────────────┘
                           │
                    AnyChat Server
              (HTTP gateway + WebSocket)
```

---

## 设计原则

- **Core 包含所有平台无关的业务逻辑**，一次实现，多端复用。
- **平台差异通过注入接口解决**，不用条件编译。
- **SDK 不依赖 UI 框架**，也不持有 UI 层引用。
- **离线优先**：读操作优先命中缓存/DB，网络操作在后台静默同步。

---

## 数据持久化（SQLite）

### 决策：由 Core 统一实现 SQLite 逻辑

所有平台的消息存储逻辑完全相同（Schema、SQL、事务），差异只有**数据库文件的路径**和 Web 平台的替代方案。

### 平台路径差异

| 平台 | 路径来源 | 典型路径 |
|------|---------|---------|
| Android | `Context.getDatabasePath()` | `/data/data/<pkg>/databases/anychat.db` |
| iOS | `NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory)` | `<AppSupport>/anychat.db` |
| Flutter | `path_provider` 插件的 `getApplicationDocumentsDirectory()` | 各平台对应的 documents 目录 |
| Desktop (Linux/macOS/Windows) | 应用启动时由调用方传入 | `~/.local/share/anychat/anychat.db` |
| Web | **不使用 C++ Core**，Web SDK 独立实现，见下节 | — |

**注入方式**：`ClientConfig` 增加 `db_path` 字段，各平台 binding 在初始化时填写。

```cpp
struct ClientConfig {
    std::string gateway_url;
    std::string api_base_url;
    std::string db_path;          // 由平台 binding 注入，Core 不猜测路径
    std::string device_id;        // 平台生成的唯一设备标识
    // ...
};
```

### Web 平台的替代方案

浏览器无法运行 C++ Core（Emscripten 可行但体积巨大，ROI 低），Web SDK (`packages/web/`) 采用独立 TypeScript 实现：
- **持久化**：IndexedDB（通过 `idb` 等轻量封装）
- **接口**：与 C++ Core 暴露的 API 保持一致的命名和语义

### 数据库 Schema

```sql
-- 用户信息（本地缓存，来自服务器）
CREATE TABLE users (
    user_id     TEXT PRIMARY KEY,
    nickname    TEXT,
    avatar_url  TEXT,
    signature   TEXT,
    updated_at  INTEGER         -- Unix ms，用于增量更新判断
);

-- 好友关系
CREATE TABLE friends (
    user_id     TEXT,
    friend_id   TEXT,
    remark      TEXT,
    updated_at  INTEGER,
    is_deleted  INTEGER DEFAULT 0,
    PRIMARY KEY (user_id, friend_id)
);

-- 会话列表（对应服务端 Session）
CREATE TABLE conversations (
    conv_id         TEXT PRIMARY KEY,
    conv_type       TEXT,           -- 'private' | 'group'
    target_id       TEXT,
    last_msg_id     TEXT,
    last_msg_text   TEXT,
    last_msg_time   INTEGER,
    unread_count    INTEGER DEFAULT 0,
    is_pinned       INTEGER DEFAULT 0,
    is_muted        INTEGER DEFAULT 0,
    local_seq       INTEGER DEFAULT 0,  -- 本地已收到的最大 seq
    updated_at      INTEGER
);

-- 消息
CREATE TABLE messages (
    msg_id          TEXT PRIMARY KEY,
    local_id        TEXT UNIQUE,    -- 客户端发送时生成，服务端确认后更新 msg_id
    conv_id         TEXT NOT NULL,
    sender_id       TEXT,
    content_type    TEXT,           -- 'text' | 'image' | 'audio' | 'video' | 'file'
    content         TEXT,           -- JSON string
    seq             INTEGER,        -- 会话内序列号
    reply_to        TEXT,
    status          INTEGER DEFAULT 0, -- 0=正常 1=撤回 2=本地删除
    send_state      INTEGER DEFAULT 0, -- 0=已收到 1=发送中 2=发送失败（仅本地消息用）
    is_read         INTEGER DEFAULT 0,
    created_at      INTEGER,        -- Unix ms
    FOREIGN KEY (conv_id) REFERENCES conversations(conv_id)
);
CREATE INDEX idx_messages_conv_seq ON messages (conv_id, seq);

-- 群组信息
CREATE TABLE groups (
    group_id    TEXT PRIMARY KEY,
    name        TEXT,
    avatar_url  TEXT,
    owner_id    TEXT,
    member_count INTEGER,
    my_role     TEXT,
    updated_at  INTEGER
);

-- 离线发送队列（网络恢复后重发）
CREATE TABLE outbound_queue (
    local_id        TEXT PRIMARY KEY,
    conv_id         TEXT,
    conv_type       TEXT,
    content_type    TEXT,
    content         TEXT,
    retry_count     INTEGER DEFAULT 0,
    created_at      INTEGER
);

-- SDK 元数据（kv 存储）
CREATE TABLE metadata (
    key     TEXT PRIMARY KEY,
    value   TEXT
);
-- 用于存储：last_sync_time、token、device_id 等
```

### 数据库版本与迁移

Core 内置简单的 Schema 版本管理：

```cpp
// 每次更改 Schema 时递增
static constexpr int kCurrentSchemaVersion = 1;

// 启动时检查 PRAGMA user_version，按版本依次执行迁移 SQL
```

---

## 内存缓存

### 决策：在 C++ Core 内实现

**理由：**
- 消息翻页、会话列表排序、未读数聚合等逻辑在所有平台完全相同，放在 Core 可以保证行为一致。
- 避免各平台 binding 分别实现一遍缓存逻辑（否则 Kotlin / Swift / Dart 各写一套，维护成本高）。
- 平台层（Flutter Provider、Android ViewModel）持有的是**响应式 UI 状态**，而 Core 的缓存是**数据层读穿缓存**，两者职责不同，可以共存。

**SDK 缓存的数据：**

| 缓存对象 | 结构 | 容量控制 |
|---------|------|---------|
| 会话列表 | `std::vector<Conversation>`，按 pin+time 排序 | 全量（上限约数百条） |
| 消息（最近若干条） | 按 `conv_id` 分桶的 LRU，每桶保留最新 N 条 | 每会话最近 100 条 |
| 用户信息 | `std::unordered_map<user_id, UserInfo>` + LRU 淘汰 | 500 个用户 |
| 好友列表 | `std::unordered_map<friend_id, Friend>` | 全量 |

**缓存策略：读穿（Read-Through）+ 写穿（Write-Through）**

```
读消息：内存命中 → 直接返回
       内存未命中 → 查 SQLite → 填充内存 → 返回
写消息：写内存 + 写 SQLite（同步事务）
       成功后触发 onConversationUpdated 回调
```

**平台层的响应式适配（由 Platform SDK 实现）：**

```
Core 触发回调 → Platform SDK 将数据转换为平台对象
             → Flutter: 更新 ChangeNotifier / Riverpod Provider
             → Android: 更新 LiveData / StateFlow
             → iOS/macOS: 调用 Combine Publisher / @Published
```

---

## 应放入 SDK Core 的公共逻辑

以下逻辑所有平台完全一致，统一放在 C++ Core：

### 1. Token 全生命周期管理

- 登录后将 access_token + refresh_token 写入 SQLite metadata 表（加密可选）
- 每次 HTTP 请求前检查 `expires_at`，提前 5 分钟静默调用 `/auth/refresh`
- refresh 失败（token 过期/被吊销）触发 `onAuthExpired` 回调，通知应用层跳转登录页
- 应用冷启动时从 DB 恢复 token，无需重新登录

### 2. 增量同步引擎（Sync Engine）

管理与服务端的数据同步，减少全量拉取：

```
WebSocket 连接建立后：
  1. 读取 DB 中 last_sync_time
  2. 调用 POST /sync { lastSyncTime, conversationSeqs }
  3. 将返回的 friends / groups / sessions 增量合并到 DB + 缓存
  4. 将离线消息写入 messages 表
  5. 更新 last_sync_time
  6. 触发 onSyncCompleted 回调（平台层刷新 UI）
```

### 3. 消息发送队列（离线消息队列）

- 调用 `sendMessage()` 时立即将消息写入本地（status=发送中），UI 立刻可见
- 将消息放入内存发送队列
- 网络可用时从队列取出，通过 WebSocket 发送
- 收到 `message.sent` 确认后，用服务端 msg_id 替换本地 local_id，更新状态为已发送
- 断网期间消息持久化在 `outbound_queue` 表，重启后恢复发送，最多重试 3 次

### 4. 消息序列号管理与补齐

- 每个会话维护 `local_seq`（本地最大已收到序列号）
- 收到 WebSocket 推送时，若 `seq > local_seq + 1`，说明有消息丢失
- 自动调用 `POST /sync/messages` 补齐缺失的消息
- 保证消息的顺序性和完整性

### 5. WebSocket 通知路由（Notification Manager）

将 WebSocket 收到的 `notification` 消息按 `notificationType` 路由到所有已注册的处理器。

`NotificationManager` 支持多订阅者：各业务模块在构造时各自调用 `addNotificationHandler()` 注册，互不覆盖。

```
message.new         → MessageManagerImpl       → 写 DB，触发 onMessageReceived
message.recalled    → MessageManagerImpl       → 更新 DB，触发 onMessageRecalled
friend.request      → FriendManagerImpl        → 触发 onFriendRequest
friend.deleted      → FriendManagerImpl        → 触发 onFriendListChanged
group.invited       → GroupManagerImpl         → 触发 onGroupInvited
group.info_updated  → GroupManagerImpl         → 触发 onGroupUpdated
auth.force_logout   → AuthManagerImpl          → 清除 token，触发 onAuthExpired
session.*           → ConversationManagerImpl  → 更新会话状态，触发 onConversationUpdated
livekit.call_invite → CallManagerImpl           → 触发 onIncomingCall（含 Call token）
livekit.call_status → CallManagerImpl           → 触发 onCallStatusChanged
livekit.call_rejected→ CallManagerImpl          → 触发 onCallStatusChanged(Rejected)
pong                → ConnectionManager        → 重置心跳计时器
message.sent        → OutboundQueue            → 确认发送成功，更新 local_id→msg_id
```

### 6. 消息去重

- 发送侧：`local_id`（UUID）保证重试时不重复
- 接收侧：先查 DB 是否存在相同 `msg_id`，存在则丢弃

### 7. 未读数管理

- 收消息时本地 `conversations.unread_count += 1`（is_muted=true 时不增加）
- 调用 `markAsRead()` 后本地置 0，同时调用 `POST /sessions/{id}/read`
- `GET /sessions/unread/total` 定期轮询或由 `session.unread_updated` 通知触发

### 8. 文件上传抽象

三步流程封装在 SDK：

```
1. POST /files/upload-token → 获取预签名 URL 和 file_id
2. PUT <presigned_url> → 直接上传到 MinIO（HTTP PUT）
3. POST /files/{fileId}/complete → 通知服务端激活
```

上传进度通过 libcurl 的 progress callback 暴露给上层。

### 9. 心跳与重连

- WebSocket 每 30 秒发送 `{"type":"ping"}`，收到 `pong` 则重置计时器
- 连续 2 次无响应触发重连
- 重连采用指数退避（1s → 2s → 4s → 8s → 16s），最多 5 次
- 重连成功后自动执行增量同步（步骤 2）

---

## 网络连接管理

### 问题

网络可达性检测是**平台强相关**的（ConnectivityManager / NWPathMonitor / netlink），但连接状态机的逻辑（何时连接、何时暂停重连、何时恢复）在所有平台完全相同，因此采用"注入接口 + Core 实现逻辑"的分层方案。

### 组件结构

```
Platform Binding
  └── 实现 NetworkMonitor（平台相关）
        │ 注入
        ▼
  ConnectionManager（Core 内部，src/connection_manager.h/cpp）
        │ 订阅状态变化
        ├── NetworkMonitor   → 网络可达性事件
        ├── WebSocketClient  → 连接/断开/错误事件
        └── on_ready 钩子   → 连接建立后触发 SyncEngine
```

### NetworkMonitor 接口（平台注入）

```cpp
// core/include/anychat/network_monitor.h
class NetworkMonitor {
    virtual NetworkStatus currentStatus() const = 0;
    virtual void setOnStatusChanged(StatusChangedCallback cb) = 0;
    virtual void start() = 0;
    virtual void stop()  = 0;
};
```

各平台实现参考：

| 平台 | 实现方式 |
|------|---------|
| Android | `ConnectivityManager.registerNetworkCallback()` |
| iOS 12+ | `NWPathMonitor`（推荐）|
| iOS < 12 | `SCNetworkReachabilityCreateWithAddress` |
| Linux | `netlink` socket 监听 `RTM_NEWROUTE` / `RTM_DELROUTE` |
| Web | `navigator.onLine` + `'online'` / `'offline'` 事件 |

无论何种实现，当状态变化时调用注册的 `StatusChangedCallback`，由 ConnectionManager 接管后续逻辑，**平台侧无需关心重连策略**。

### ConnectionManager 状态机

```
               ┌──────────────────────────────────────────────────┐
               │                  connect() 调用                   │
               ▼                                                  │
         Disconnected  ──── 网络可用 ────►  Connecting            │
               ▲                               │                  │
               │                     ws_connected                 │
               │                               ▼                  │
      所有重试耗尽          disconnect()   Connected              │
      或 disconnect()   ◄───────────────────   │                  │
               │                               │ ws_disconnected  │
               │            Reconnecting ◄─────┘  （非主动断开）   │
               │                 │                                │
               │     网络丢失    │ 定时器到期                      │
               └─────────────────┘  且网络可用                    │
                                        └────────────────────────►│
```

**两层重连机制：**

| 层级 | 触发条件 | 间隔 | 最大次数 |
|------|---------|------|---------|
| WebSocketClient 内层 | 单次连接失败 / 超时 | 1s → 2s → 4s → 8s → 16s | 5 次 |
| ConnectionManager 外层 | WebSocket 内层重试全部耗尽后 | 30s → 60s → 120s → 240s → 480s | 5 次 |

内层重试处理**瞬时网络抖动**；外层重试处理**持续性连接失败**（服务不可用、长时间弱网）。

**网络变化处理：**
- 网络丢失 → 立即调用 `ws.disconnect()`，取消所有待重连计划，避免无谓的连接尝试消耗电量
- 网络恢复 → 若用户意图为"已连接"，重置重试计数，立即发起连接

### 与其他组件的集成

```
ConnectionManager.on_ready 钩子
  → SyncEngine.sync()      （增量同步离线数据）
  → OutboundQueue.flush()  （重发待发送消息）
  → TokenManager.check()   （检查 token 是否需要刷新）
```

### ClientConfig 新增字段

```cpp
struct ClientConfig {
    std::string  gateway_url;
    std::string  api_base_url;
    std::string  device_id;          // 平台注入，唯一设备标识
    std::string  db_path;            // 平台注入，SQLite 文件路径
    std::shared_ptr<NetworkMonitor> network_monitor;  // 可选，nullptr=始终视网络可用
    int          connect_timeout_ms     = 10'000;
    int          max_reconnect_attempts = 5;
    bool         auto_reconnect         = true;
};
```

---

## 应由平台层（Platform SDK）实现的逻辑

以下逻辑与平台特性强耦合，不适合放入 C++ Core：

| 功能 | 原因 |
|------|------|
| **推送通知（APNs / FCM）** | 需要系统级权限和平台 SDK，C++ 无法直接调用 |
| **响应式 UI 状态**（LiveData、ChangeNotifier）| 框架相关，Core 只提供 callback |
| **通知栏展示** | 系统 API，平台相关 |
| **App 前后台感知** | 生命周期 API 各平台不同 |
| **音视频 UI**（LiveKit SDK 集成）| 各平台有独立的 Call UI 组件 |
| **相机/相册访问**（发图片前） | 系统权限 API |
| **键盘/输入法适配** | UI 层逻辑 |

---

## 数据流

### 发送消息

```
App 调用 sendTextMessage()
  → Core 生成 local_id，写入 DB（send_state=发送中）
  → 更新内存缓存，触发 onConversationUpdated 回调（UI 立即显示）
  → 将消息加入发送队列
  → 队列 worker 从 WebSocket 发送 message.send
  → 收到 message.sent 确认
  → 用服务端 msg_id 更新 DB，send_state=已发送
  → 触发 onMessageSent 回调
```

### 接收消息

```
WebSocket 收到 notification(message.new)
  → 去重检查（查 DB msg_id）
  → 写入 DB messages 表
  → 更新 conversations 表（last_msg、unread_count）
  → 更新内存缓存
  → 触发 onMessageReceived(message, conversation) 回调
  → 检查 seq 是否连续，若有缺口触发消息补齐
```

### 冷启动同步

```
AnyChatClient::create(config)
  → 打开 SQLite（config.db_path）
  → 从 DB metadata 读取 token，恢复登录态
  → HttpClient 注入 Bearer token
  → connect() 建立 WebSocket
  → WebSocket 连接成功后，调用 POST /sync 增量同步
  → 同步完成，触发 onReady 回调
```

---

## 线程模型

```
┌─────────────────────────────────────────────────┐
│              调用方线程（UI/主线程）               │
│  调用 SDK API，注册 callback                      │
└────────────────┬────────────────────────────────┘
                 │ 线程安全的调用（加锁或消息队列）
┌────────────────▼────────────────────────────────┐
│           C++ Core 内部线程                       │
│  ┌──────────────┐  ┌──────────────────────────┐ │
│  │ HTTP Worker  │  │  WebSocket Event Loop     │ │
│  │ (libcurl     │  │  (libwebsockets service)  │ │
│  │  multi loop) │  │                           │ │
│  └──────────────┘  └──────────────────────────┘ │
│  ┌──────────────────────────────────────────┐   │
│  │  SQLite Worker（串行，避免锁竞争）          │   │
│  └──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
                 │ 回调由 Platform Binding 转发
┌────────────────▼────────────────────────────────┐
│          Platform Binding 层                      │
│  回调调度到 UI 线程（Android: Handler，              │
│  iOS: dispatch_async main，Flutter: Dart isolate）│
└─────────────────────────────────────────────────┘
```

**规则：**
- SQLite 操作只在专用的 DB 串行线程执行（避免 SQLITE_BUSY）
- 内存缓存读写通过 `std::shared_mutex`（多读单写）
- SDK 对外的所有 callback 由 binding 层负责调度到正确线程，Core 不假设任何线程上下文

---

## 目录结构

```
core/
├── include/anychat/
│   ├── client.h          # AnyChatClient 入口 + ClientConfig
│   ├── auth.h            # AuthManager 接口
│   ├── message.h         # MessageManager 接口
│   ├── conversation.h    # ConversationManager 接口
│   ├── friend.h          # FriendManager 接口
│   ├── group.h           # GroupManager 接口
│   ├── file.h            # FileManager 接口
│   ├── user.h            # UserManager 接口
│   ├── call.h             # CallManager 接口（音视频通话 + 会议室）
│   ├── network_monitor.h # NetworkMonitor 平台注入接口
│   └── types.h           # 共用数据结构（Message、Conversation、Friend、
│                         #   Group、FileInfo、UserProfile、UserSettings、
│                         #   CallSession、MeetingRoom 等）
├── src/
│   ├── client.cpp                    # AnyChatClientImpl 工厂 + 组装
│   ├── auth_manager.h/cpp            # AuthManagerImpl（Token 全生命周期）
│   ├── connection_manager.h/cpp      # 连接状态机 + 心跳
│   ├── notification_manager.h/cpp    # WS 消息路由（多订阅者 fan-out）
│   ├── outbound_queue.h/cpp          # 消息发送队列（持久化重发）
│   ├── sync_engine.h/cpp             # 增量同步引擎（POST /sync）
│   ├── message_manager.h/cpp         # MessageManagerImpl
│   ├── conversation_manager.h/cpp    # ConversationManagerImpl
│   ├── friend_manager.h/cpp          # FriendManagerImpl
│   ├── group_manager.h/cpp           # GroupManagerImpl
│   ├── file_manager.h/cpp            # FileManagerImpl（三步上传流程）
│   ├── user_manager.h/cpp            # UserManagerImpl（资料 / 设置 / 搜索）
│   ├── call_manager.h/cpp            # CallManagerImpl（通话 + 会议室 + 通知）
│   ├── db/
│   │   ├── database.h/cpp            # SQLite 封装（WAL、单工作线程）
│   │   └── migrations.h/cpp          # Schema 版本管理
│   ├── cache/
│   │   ├── lru_cache.h               # 通用 LRU 缓存
│   │   ├── conversation_cache.h      # 会话列表缓存（pin+时间排序）
│   │   └── message_cache.h           # 消息分桶缓存（每会话最近 100 条）
│   └── network/
│       ├── http_client.h/cpp         # libcurl 异步 HTTP（CURLM multi）
│       ├── websocket_client.h/cpp    # libwebsockets 封装
│       └── iwebsocket_client.h       # WebSocketClient 抽象接口（测试用）
└── tests/
    ├── test_types.cpp
    ├── test_client.cpp
    ├── test_connection_manager.cpp
    ├── test_database.cpp
    ├── test_cache.cpp
    ├── test_auth.cpp
    ├── test_notification_manager.cpp
    ├── test_outbound_queue.cpp
    ├── test_sync_engine.cpp
    ├── test_message_manager.cpp
    ├── test_conversation_manager.cpp
    ├── test_friend_manager.cpp
    ├── test_group_manager.cpp
    ├── test_file_manager.cpp
    ├── test_user_manager.cpp
    └── test_call_manager.cpp
```

---

## 第三方依赖汇总

| 依赖 | 用途 | 引入方式 |
|------|------|---------|
| libcurl | HTTP 异步请求 | CMake / CPM |
| libwebsockets | WebSocket | CMake / CPM |
| nlohmann/json | JSON 解析 | CMake / CPM |
| SQLite3 | 本地持久化 | CPM（amalgamation 单文件） |
| OpenSSL / mbedTLS | TLS（libcurl / lws 依赖） | 系统库 |
