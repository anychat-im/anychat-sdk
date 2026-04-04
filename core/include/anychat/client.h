#pragma once

#include "auth.h"
#include "conversation.h"
#include "file.h"
#include "friend.h"
#include "group.h"
#include "message.h"
#include "network_monitor.h"
#include "call.h"
#include "types.h"
#include "user.h"

#include <functional>
#include <memory>
#include <string>

namespace anychat {

struct ClientConfig {
    // ---- 网络 ----------------------------------------------------------------
    std::string gateway_url; // WebSocket 网关，e.g. "wss://api.anychat.io"
    std::string api_base_url; // HTTP API 根路径，e.g. "https://api.anychat.io/api/v1"

    // ---- 设备 ----------------------------------------------------------------
    std::string device_id; // 唯一设备标识，由平台 binding 生成并持久化
        // Android: Settings.Secure.ANDROID_ID 或 UUID
        // iOS: UIDevice.identifierForVendor
        // Web: localStorage 中存储的 UUID

    // ---- 持久化 --------------------------------------------------------------
    std::string db_path; // SQLite 数据库文件完整路径，由平台 binding 注入
        // Android: Context.getDatabasePath("anychat.db").absolutePath
        // iOS: <ApplicationSupport>/anychat.db
        // Web: 留空（Web SDK 使用 IndexedDB，不经过 C++ Core）

    // ---- 网络监控 ------------------------------------------------------------
    // 可选。注入平台实现的 NetworkMonitor；为 nullptr 时 SDK 始终视网络为可用。
    std::shared_ptr<NetworkMonitor> network_monitor;

    // ---- 连接参数 ------------------------------------------------------------
    int connect_timeout_ms = 10'000;
    int max_reconnect_attempts = 5; // WebSocket 内层最大重试次数
    bool auto_reconnect = true; // 断线后是否自动重连
};

using ConnectionStateCallback = std::function<void(ConnectionState state)>;

class AnyChatClient {
public:
    static std::shared_ptr<AnyChatClient> create(const ClientConfig& config);

    virtual ~AnyChatClient() = default;

    // ---- 认证与连接管理 ------------------------------------------------------
    // login(): HTTP认证 + 自动建立WebSocket连接
    // logout(): 断开WebSocket + HTTP登出
    // 注意: WebSocket自动重连由SDK内部管理，无需手动调用connect
    // client_version: 客户端版本号 (e.g. "1.0.0")
    virtual void login(
        const std::string& account,
        const std::string& password,
        const std::string& device_type,
        const std::string& client_version,
        AuthCallback callback
    ) = 0;

    virtual void logout(ResultCallback callback) = 0;

    virtual bool isLoggedIn() const = 0;
    virtual AuthToken getCurrentToken() const = 0;

    virtual ConnectionState connectionState() const = 0;
    virtual void setOnConnectionStateChanged(ConnectionStateCallback callback) = 0;

    // ---- 子模块 --------------------------------------------------------------
    virtual AuthManager& authMgr() = 0;
    virtual MessageManager& messageMgr() = 0;
    virtual ConversationManager& conversationMgr() = 0;
    virtual FriendManager& friendMgr() = 0;
    virtual GroupManager& groupMgr() = 0;
    virtual FileManager& fileMgr() = 0;
    virtual UserManager& userMgr() = 0;
    virtual CallManager& callMgr() = 0;
};

} // namespace anychat
