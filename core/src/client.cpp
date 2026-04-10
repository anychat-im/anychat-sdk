#include "anychat/client.h"

#include "auth_manager.h"
#include "connection_manager.h"
#include "conversation_manager.h"
#include "file_manager.h"
#include "friend_manager.h"
#include "group_manager.h"
#include "message_manager.h"
#include "notification_manager.h"
#include "outbound_queue.h"
#include "call_manager.h"
#include "sync_engine.h"
#include "user_manager.h"

#include "anychat/auth.h"
#include "anychat/message.h"
#include "anychat/types.h"

#include "cache/conversation_cache.h"
#include "cache/message_cache.h"
#include "db/database.h"
#include "network/http_client.h"
#include "network/websocket_client.h"

#include <mutex>
#include <stdexcept>
#include <string>

namespace anychat {

// ---------------------------------------------------------------------------
// AnyChatClientImpl
// ---------------------------------------------------------------------------

class AnyChatClientImpl : public AnyChatClient {
public:
    explicit AnyChatClientImpl(const ClientConfig& config)
        : http_(std::make_shared<network::HttpClient>(config.api_base_url))
        , gateway_url_(config.gateway_url)
        , config_(config) {
        // 1. Open DB
        db_ = std::make_unique<db::Database>(config.db_path);
        if (!config.db_path.empty())
            db_->open();

        // 2. Initialize caches
        conv_cache_ = std::make_unique<cache::ConversationCache>();
        msg_cache_ = std::make_unique<cache::MessageCache>();

        // 3. Create NotificationManager
        notif_mgr_ = std::make_unique<NotificationManager>();

        // 4. Create auth manager with DB for token persistence
        auth_mgr_ = createAuthManager(http_, config.device_id, db_.get(), notif_mgr_.get());

        // 5. Create OutboundQueue
        outbound_q_ = std::make_unique<OutboundQueue>(db_.get());

        // 6. Create SyncEngine
        sync_engine_ = std::make_unique<SyncEngine>(db_.get(), conv_cache_.get(), msg_cache_.get(), http_);

        // 7. Wire NotificationManager → OutboundQueue
        notif_mgr_->setOnMessageSent([this](const MsgSentAck& ack) {
            outbound_q_->onMessageSentAck(ack);
        });

        // 8. Create Phase 4 business managers
        msg_mgr_ = std::make_unique<MessageManagerImpl>(
            db_.get(),
            msg_cache_.get(),
            outbound_q_.get(),
            notif_mgr_.get(),
            http_,
            ""
        );
        conv_mgr_ = std::make_unique<ConversationManagerImpl>(db_.get(), conv_cache_.get(), notif_mgr_.get(), http_);
        friend_mgr_ = std::make_unique<FriendManagerImpl>(db_.get(), notif_mgr_.get(), http_);
        group_mgr_ = std::make_unique<GroupManagerImpl>(db_.get(), notif_mgr_.get(), http_);
        file_mgr_ = std::make_unique<FileManagerImpl>(http_);
        user_mgr_ = std::make_unique<UserManagerImpl>(http_);
        call_mgr_ = std::make_unique<CallManagerImpl>(http_, notif_mgr_.get());

        // Note: WebSocket and ConnectionManager will be created after login
    }

    ~AnyChatClientImpl() override {
        // Ensure clean shutdown: stop reconnects and close WebSocket.
        if (conn_mgr_)
            conn_mgr_->disconnect();
    }

    // ---- 认证与连接管理 ------------------------------------------------------

    void login(
        const std::string& account,
        const std::string& password,
        const std::string& device_type,
        const std::string& client_version,
        AuthCallback callback
    ) override {
        // 调用HTTP登录
        auth_mgr_->login(
            account,
            password,
            device_type,
            client_version,
            [this, cb = std::move(callback)](bool success, const anychat::AuthToken& token, const std::string& error) {
                if (success) {
                    // 登录成功：使用access_token创建WebSocket连接
                    initializeWebSocket(token.access_token);
                    // 自动建立连接
                    conn_mgr_->connect();
                }
                // 将结果回调给上层
                if (cb)
                    cb(success, token, error);
            }
        );
    }

    void logout(ResultCallback callback) override {
        // 先断开WebSocket连接
        if (conn_mgr_) {
            conn_mgr_->disconnect();
            outbound_q_->onDisconnected();
        }

        // 然后调用HTTP登出
        auth_mgr_->logout(std::move(callback));
    }

    bool isLoggedIn() const override {
        return auth_mgr_->isLoggedIn();
    }

    AuthToken getCurrentToken() const override {
        return auth_mgr_->currentToken();
    }

    ConnectionState connectionState() const override {
        if (!conn_mgr_)
            return ConnectionState::Disconnected;
        return conn_mgr_->state();
    }

    void setOnConnectionStateChanged(ConnectionStateCallback callback) override {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        state_cb_ = std::move(callback);
    }

    // ---- 子模块 ---------------------------------------------------------

    AuthManager& authMgr() override {
        return *auth_mgr_;
    }

    MessageManager& messageMgr() override {
        return *msg_mgr_;
    }

    ConversationManager& conversationMgr() override {
        return *conv_mgr_;
    }

    FriendManager& friendMgr() override {
        return *friend_mgr_;
    }

    GroupManager& groupMgr() override {
        return *group_mgr_;
    }

    FileManager& fileMgr() override {
        return *file_mgr_;
    }

    UserManager& userMgr() override {
        return *user_mgr_;
    }

    CallManager& callMgr() override {
        return *call_mgr_;
    }

private:
    // ---- Helpers -------------------------------------------------------------

    static std::string buildWsUrl(const std::string& gateway_url, const std::string& token) {
        // Build WebSocket URL with token query parameter
        // Format: ws://host:port/api/v1/ws?token=TOKEN
        std::string url = gateway_url;
        if (!url.empty() && url.back() != '/')
            url += '/';
        url += "api/v1/ws";
        if (!token.empty()) {
            url += "?token=" + token;
        }
        return url;
    }

    void initializeWebSocket(const std::string& access_token) {
        // Create WebSocket with token-appended URL
        std::string ws_url = buildWsUrl(gateway_url_, access_token);

        ws_ = std::make_shared<network::WebSocketClient>(ws_url);

        // Wire WebSocket message handler to NotificationManager
        ws_->setOnMessage([this](const std::string& raw) {
            notif_mgr_->handleRaw(raw);
        });

        // Create ConnectionManager with the new WebSocket client
        conn_mgr_ = std::make_unique<ConnectionManager>(
            ws_url,
            config_.network_monitor,
            ws_,
            [this](ConnectionState s) {
                onStateChanged(s);
            },
            [this]() {
                onReady();
            }
        );

        // Wire NotificationManager → ConnectionManager heartbeat (pong)
        notif_mgr_->setOnPong([this]() {
            if (conn_mgr_)
                conn_mgr_->onPongReceived();
        });
    }

    void onStateChanged(ConnectionState s) {
        ConnectionStateCallback cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = state_cb_;
        }
        if (cb)
            cb(s);
    }

    void onReady() {
        // WebSocket connected: flush outbound queue and trigger incremental sync.
        if (ws_) {
            outbound_q_->onConnected([this](const std::string& json) {
                ws_->send(json);
            });
        }
        sync_engine_->sync();
    }

    // ---- Members -------------------------------------------------------------

    std::shared_ptr<network::HttpClient> http_;
    std::shared_ptr<network::WebSocketClient> ws_; // Created after login

    std::string gateway_url_; // Base WebSocket gateway URL
    ClientConfig config_; // Original config for ConnectionManager

    std::unique_ptr<db::Database> db_;
    std::unique_ptr<cache::ConversationCache> conv_cache_;
    std::unique_ptr<cache::MessageCache> msg_cache_;

    std::unique_ptr<AuthManager> auth_mgr_;
    std::unique_ptr<MessageManager> msg_mgr_;
    std::unique_ptr<ConnectionManager> conn_mgr_; // Created after login

    std::unique_ptr<NotificationManager> notif_mgr_;
    std::unique_ptr<OutboundQueue> outbound_q_;
    std::unique_ptr<SyncEngine> sync_engine_;

    std::unique_ptr<ConversationManager> conv_mgr_;
    std::unique_ptr<FriendManager> friend_mgr_;
    std::unique_ptr<GroupManager> group_mgr_;
    std::unique_ptr<FileManager> file_mgr_;
    std::unique_ptr<UserManager> user_mgr_;
    std::unique_ptr<CallManager> call_mgr_;

    mutable std::mutex cb_mutex_;
    ConnectionStateCallback state_cb_;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::shared_ptr<AnyChatClient> AnyChatClient::create(const ClientConfig& config) {
    if (config.gateway_url.empty())
        throw std::invalid_argument("ClientConfig::gateway_url must not be empty");
    if (config.api_base_url.empty())
        throw std::invalid_argument("ClientConfig::api_base_url must not be empty");
    if (config.device_id.empty())
        throw std::invalid_argument("ClientConfig::device_id must not be empty");

    return std::make_shared<AnyChatClientImpl>(config);
}

} // namespace anychat
