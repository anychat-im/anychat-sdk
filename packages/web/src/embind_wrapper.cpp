/**
 * embind_wrapper.cpp - Emscripten embind bindings for AnyChat C API
 *
 * This file uses Emscripten's embind to wrap the C API into JavaScript-friendly
 * interfaces. The TypeScript layer (AnyChatClient.ts) provides a Promise-based
 * API on top of these bindings.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstring>

#include "anychat_c/anychat_c.h"

using namespace emscripten;

// ============================================================================
// Helper: Convert C structs to JavaScript objects
// ============================================================================

val authTokenToJS(const AnyChatAuthToken_C& token) {
    val obj = val::object();
    obj.set("accessToken", std::string(token.access_token));
    obj.set("refreshToken", std::string(token.refresh_token));
    obj.set("expiresAt", token.expires_at_ms);
    return obj;
}

val userInfoToJS(const AnyChatUserInfo_C& user) {
    val obj = val::object();
    obj.set("userId", std::string(user.user_id));
    obj.set("username", std::string(user.username));
    obj.set("avatarUrl", std::string(user.avatar_url));
    return obj;
}

val messageToJS(const AnyChatMessage_C& msg) {
    val obj = val::object();
    obj.set("messageId", std::string(msg.message_id));
    obj.set("localId", std::string(msg.local_id));
    obj.set("convId", std::string(msg.conv_id));
    obj.set("senderId", std::string(msg.sender_id));
    obj.set("contentType", std::string(msg.content_type));
    obj.set("type", msg.type);
    obj.set("content", msg.content ? std::string(msg.content) : "");
    obj.set("seq", (double)msg.seq);
    obj.set("replyTo", std::string(msg.reply_to));
    obj.set("timestamp", (double)msg.timestamp_ms);
    obj.set("status", msg.status);
    obj.set("sendState", msg.send_state);
    obj.set("isRead", msg.is_read != 0);
    return obj;
}

val conversationToJS(const AnyChatConversation_C& conv) {
    val obj = val::object();
    obj.set("convId", std::string(conv.conv_id));
    obj.set("convType", conv.conv_type);
    obj.set("targetId", std::string(conv.target_id));
    obj.set("lastMsgId", std::string(conv.last_msg_id));
    obj.set("lastMsgText", std::string(conv.last_msg_text));
    obj.set("lastMsgTime", (double)conv.last_msg_time_ms);
    obj.set("unreadCount", conv.unread_count);
    obj.set("isPinned", conv.is_pinned != 0);
    obj.set("isMuted", conv.is_muted != 0);
    obj.set("updatedAt", (double)conv.updated_at_ms);
    return obj;
}

val friendToJS(const AnyChatFriend_C& f) {
    val obj = val::object();
    obj.set("userId", std::string(f.user_id));
    obj.set("remark", std::string(f.remark));
    obj.set("updatedAt", (double)f.updated_at_ms);
    obj.set("isDeleted", f.is_deleted != 0);
    obj.set("userInfo", userInfoToJS(f.user_info));
    return obj;
}

val friendRequestToJS(const AnyChatFriendRequest_C& req) {
    val obj = val::object();
    obj.set("requestId", (double)req.request_id);
    obj.set("fromUserId", std::string(req.from_user_id));
    obj.set("toUserId", std::string(req.to_user_id));
    obj.set("message", std::string(req.message));
    obj.set("status", std::string(req.status));
    obj.set("createdAt", (double)req.created_at_ms);
    obj.set("fromUserInfo", userInfoToJS(req.from_user_info));
    return obj;
}

val groupToJS(const AnyChatGroup_C& g) {
    val obj = val::object();
    obj.set("groupId", std::string(g.group_id));
    obj.set("name", std::string(g.name));
    obj.set("avatarUrl", std::string(g.avatar_url));
    obj.set("ownerId", std::string(g.owner_id));
    obj.set("memberCount", g.member_count);
    obj.set("myRole", g.my_role);
    obj.set("joinVerify", g.join_verify != 0);
    obj.set("updatedAt", (double)g.updated_at_ms);
    return obj;
}

val groupMemberToJS(const AnyChatGroupMember_C& m) {
    val obj = val::object();
    obj.set("userId", std::string(m.user_id));
    obj.set("groupNickname", std::string(m.group_nickname));
    obj.set("role", m.role);
    obj.set("isMuted", m.is_muted != 0);
    obj.set("joinedAt", (double)m.joined_at_ms);
    obj.set("userInfo", userInfoToJS(m.user_info));
    return obj;
}

// ============================================================================
// Callback Storage: JavaScript callbacks passed from TypeScript
// ============================================================================

struct CallbackStore {
    // Connection state callback
    val connectionStateCallback = val::null();

    // Auth callbacks
    std::map<int, val> authCallbacks;
    std::map<int, val> resultCallbacks;
    val authExpiredCallback = val::null();

    // Message callbacks
    std::map<int, val> messageCallbacks;
    std::map<int, val> messageListCallbacks;
    val messageReceivedCallback = val::null();

    // Conversation callbacks
    std::map<int, val> convCallbacks;
    std::map<int, val> convListCallbacks;
    val convUpdatedCallback = val::null();

    // Friend callbacks
    std::map<int, val> friendCallbacks;
    std::map<int, val> friendListCallbacks;
    std::map<int, val> friendRequestListCallbacks;
    val friendRequestCallback = val::null();
    val friendListChangedCallback = val::null();

    // Group callbacks
    std::map<int, val> groupCallbacks;
    std::map<int, val> groupListCallbacks;
    std::map<int, val> groupMemberCallbacks;
    val groupInvitedCallback = val::null();
    val groupUpdatedCallback = val::null();

    int nextCallbackId = 1;
};

static CallbackStore g_callbacks;

// ============================================================================
// C Callback Wrappers
// ============================================================================

void connectionStateCallbackWrapper(void* userdata, int state) {
    if (!g_callbacks.connectionStateCallback.isNull()) {
        g_callbacks.connectionStateCallback(state);
    }
}

void authCallbackWrapper(void* userdata, int success, const AnyChatAuthToken_C* token, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.authCallbacks.find(callbackId);
    if (it != g_callbacks.authCallbacks.end()) {
        val callback = it->second;
        if (success && token) {
            callback(val::null(), authTokenToJS(*token));
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.authCallbacks.erase(it);
    }
}

void resultCallbackWrapper(void* userdata, int success, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.resultCallbacks.find(callbackId);
    if (it != g_callbacks.resultCallbacks.end()) {
        val callback = it->second;
        if (success) {
            callback(val::null());
        } else {
            callback(std::string(error ? error : "Unknown error"));
        }
        g_callbacks.resultCallbacks.erase(it);
    }
}

void authExpiredCallbackWrapper(void* userdata) {
    if (!g_callbacks.authExpiredCallback.isNull()) {
        g_callbacks.authExpiredCallback();
    }
}

void messageCallbackWrapper(void* userdata, int success, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.messageCallbacks.find(callbackId);
    if (it != g_callbacks.messageCallbacks.end()) {
        val callback = it->second;
        if (success) {
            callback(val::null());
        } else {
            callback(std::string(error ? error : "Unknown error"));
        }
        g_callbacks.messageCallbacks.erase(it);
    }
}

void messageListCallbackWrapper(void* userdata, const AnyChatMessageList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.messageListCallbacks.find(callbackId);
    if (it != g_callbacks.messageListCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", messageToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.messageListCallbacks.erase(it);
    }
}

void messageReceivedCallbackWrapper(void* userdata, const AnyChatMessage_C* message) {
    if (!g_callbacks.messageReceivedCallback.isNull() && message) {
        g_callbacks.messageReceivedCallback(messageToJS(*message));
    }
}

void convListCallbackWrapper(void* userdata, const AnyChatConversationList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.convListCallbacks.find(callbackId);
    if (it != g_callbacks.convListCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", conversationToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.convListCallbacks.erase(it);
    }
}

void convCallbackWrapper(void* userdata, int success, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.convCallbacks.find(callbackId);
    if (it != g_callbacks.convCallbacks.end()) {
        val callback = it->second;
        if (success) {
            callback(val::null());
        } else {
            callback(std::string(error ? error : "Unknown error"));
        }
        g_callbacks.convCallbacks.erase(it);
    }
}

void convUpdatedCallbackWrapper(void* userdata, const AnyChatConversation_C* conversation) {
    if (!g_callbacks.convUpdatedCallback.isNull() && conversation) {
        g_callbacks.convUpdatedCallback(conversationToJS(*conversation));
    }
}

void friendListCallbackWrapper(void* userdata, const AnyChatFriendList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.friendListCallbacks.find(callbackId);
    if (it != g_callbacks.friendListCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", friendToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.friendListCallbacks.erase(it);
    }
}

void friendRequestListCallbackWrapper(void* userdata, const AnyChatFriendRequestList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.friendRequestListCallbacks.find(callbackId);
    if (it != g_callbacks.friendRequestListCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", friendRequestToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.friendRequestListCallbacks.erase(it);
    }
}

void friendCallbackWrapper(void* userdata, int success, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.friendCallbacks.find(callbackId);
    if (it != g_callbacks.friendCallbacks.end()) {
        val callback = it->second;
        if (success) {
            callback(val::null());
        } else {
            callback(std::string(error ? error : "Unknown error"));
        }
        g_callbacks.friendCallbacks.erase(it);
    }
}

void friendRequestCallbackWrapper(void* userdata, const AnyChatFriendRequest_C* request) {
    if (!g_callbacks.friendRequestCallback.isNull() && request) {
        g_callbacks.friendRequestCallback(friendRequestToJS(*request));
    }
}

void friendListChangedCallbackWrapper(void* userdata) {
    if (!g_callbacks.friendListChangedCallback.isNull()) {
        g_callbacks.friendListChangedCallback();
    }
}

void groupListCallbackWrapper(void* userdata, const AnyChatGroupList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.groupListCallbacks.find(callbackId);
    if (it != g_callbacks.groupListCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", groupToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.groupListCallbacks.erase(it);
    }
}

void groupCallbackWrapper(void* userdata, int success, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.groupCallbacks.find(callbackId);
    if (it != g_callbacks.groupCallbacks.end()) {
        val callback = it->second;
        if (success) {
            callback(val::null());
        } else {
            callback(std::string(error ? error : "Unknown error"));
        }
        g_callbacks.groupCallbacks.erase(it);
    }
}

void groupMemberCallbackWrapper(void* userdata, const AnyChatGroupMemberList_C* list, const char* error) {
    int callbackId = reinterpret_cast<intptr_t>(userdata);
    auto it = g_callbacks.groupMemberCallbacks.find(callbackId);
    if (it != g_callbacks.groupMemberCallbacks.end()) {
        val callback = it->second;
        if (list) {
            val arr = val::array();
            for (int i = 0; i < list->count; ++i) {
                arr.call<void>("push", groupMemberToJS(list->items[i]));
            }
            callback(val::null(), arr);
        } else {
            callback(std::string(error ? error : "Unknown error"), val::null());
        }
        g_callbacks.groupMemberCallbacks.erase(it);
    }
}

void groupInvitedCallbackWrapper(void* userdata, const AnyChatGroup_C* group, const char* inviter_id) {
    if (!g_callbacks.groupInvitedCallback.isNull() && group) {
        g_callbacks.groupInvitedCallback(groupToJS(*group), std::string(inviter_id ? inviter_id : ""));
    }
}

void groupUpdatedCallbackWrapper(void* userdata, const AnyChatGroup_C* group) {
    if (!g_callbacks.groupUpdatedCallback.isNull() && group) {
        g_callbacks.groupUpdatedCallback(groupToJS(*group));
    }
}

// ============================================================================
// Embind Wrapper Class
// ============================================================================

class AnyChatClientWrapper {
private:
    AnyChatClientHandle handle_;
    AnyChatAuthHandle authHandle_;
    AnyChatMessageHandle messageHandle_;
    AnyChatConvHandle convHandle_;
    AnyChatFriendHandle friendHandle_;
    AnyChatGroupHandle groupHandle_;

public:
    AnyChatClientWrapper(val config) {
        AnyChatClientConfig_C cfg;
        std::memset(&cfg, 0, sizeof(cfg));

        // Extract config from JavaScript object
        std::string gatewayUrl = config["gatewayUrl"].as<std::string>();
        std::string apiBaseUrl = config["apiBaseUrl"].as<std::string>();
        std::string deviceId = config["deviceId"].as<std::string>();
        std::string dbPath = config.hasOwnProperty("dbPath") ? config["dbPath"].as<std::string>() : ":memory:";

        cfg.gateway_url = gatewayUrl.c_str();
        cfg.api_base_url = apiBaseUrl.c_str();
        cfg.device_id = deviceId.c_str();
        cfg.db_path = dbPath.c_str();
        cfg.connect_timeout_ms = config.hasOwnProperty("connectTimeoutMs") ? config["connectTimeoutMs"].as<int>() : 10000;
        cfg.max_reconnect_attempts = config.hasOwnProperty("maxReconnectAttempts") ? config["maxReconnectAttempts"].as<int>() : 5;
        cfg.auto_reconnect = config.hasOwnProperty("autoReconnect") ? (config["autoReconnect"].as<bool>() ? 1 : 0) : 1;

        handle_ = anychat_client_create(&cfg);
        if (!handle_) {
            throw std::runtime_error(std::string("Failed to create client: ") + anychat_get_last_error());
        }

        authHandle_ = anychat_client_get_auth(handle_);
        messageHandle_ = anychat_client_get_message(handle_);
        convHandle_ = anychat_client_get_conversation(handle_);
        friendHandle_ = anychat_client_get_friend(handle_);
        groupHandle_ = anychat_client_get_group(handle_);
    }

    ~AnyChatClientWrapper() {
        if (handle_) {
            anychat_client_destroy(handle_);
        }
    }

    // Client methods
    void connect() {
        anychat_client_connect(handle_);
    }

    void disconnect() {
        anychat_client_disconnect(handle_);
    }

    int getConnectionState() {
        return anychat_client_get_connection_state(handle_);
    }

    void setConnectionCallback(val callback) {
        g_callbacks.connectionStateCallback = callback;
        anychat_client_set_connection_callback(handle_, nullptr, connectionStateCallbackWrapper);
    }

    // Auth methods
    void login(std::string account, std::string password, std::string deviceType, std::string clientVersion, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.authCallbacks[callbackId] = callback;

        int result = anychat_auth_login(
            authHandle_,
            account.c_str(),
            password.c_str(),
            deviceType.c_str(),
            clientVersion.empty() ? nullptr : clientVersion.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            authCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.authCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void register_(std::string phoneOrEmail, std::string password, std::string verifyCode,
                   std::string deviceType, std::string nickname, std::string clientVersion, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.authCallbacks[callbackId] = callback;

        int result = anychat_auth_register(
            authHandle_,
            phoneOrEmail.c_str(),
            password.c_str(),
            verifyCode.c_str(),
            deviceType.c_str(),
            nickname.empty() ? nullptr : nickname.c_str(),
            clientVersion.empty() ? nullptr : clientVersion.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            authCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.authCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void logout(val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.resultCallbacks[callbackId] = callback;

        int result = anychat_auth_logout(
            authHandle_,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            resultCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.resultCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void refreshToken(std::string refreshToken, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.authCallbacks[callbackId] = callback;

        int result = anychat_auth_refresh_token(
            authHandle_,
            refreshToken.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            authCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.authCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    bool isLoggedIn() {
        return anychat_auth_is_logged_in(authHandle_) != 0;
    }

    void setAuthExpiredCallback(val callback) {
        g_callbacks.authExpiredCallback = callback;
        anychat_auth_set_on_expired(authHandle_, nullptr, authExpiredCallbackWrapper);
    }

    // Message methods
    void sendTextMessage(std::string sessionId, std::string content, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.messageCallbacks[callbackId] = callback;

        int result = anychat_message_send_text(
            messageHandle_,
            sessionId.c_str(),
            content.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            messageCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.messageCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void getMessageHistory(std::string sessionId, double beforeTimestamp, int limit, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.messageListCallbacks[callbackId] = callback;

        int result = anychat_message_get_history(
            messageHandle_,
            sessionId.c_str(),
            static_cast<int64_t>(beforeTimestamp),
            limit,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            messageListCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.messageListCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void markMessageRead(std::string sessionId, std::string messageId, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.messageCallbacks[callbackId] = callback;

        int result = anychat_message_mark_read(
            messageHandle_,
            sessionId.c_str(),
            messageId.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            messageCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.messageCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void setMessageReceivedCallback(val callback) {
        g_callbacks.messageReceivedCallback = callback;
        anychat_message_set_received_callback(messageHandle_, nullptr, messageReceivedCallbackWrapper);
    }

    // Conversation methods
    void getConversationList(val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.convListCallbacks[callbackId] = callback;

        int result = anychat_conv_get_list(
            convHandle_,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            convListCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.convListCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void markConversationRead(std::string convId, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.convCallbacks[callbackId] = callback;

        int result = anychat_conv_mark_read(
            convHandle_,
            convId.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            convCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.convCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void setConversationPinned(std::string convId, bool pinned, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.convCallbacks[callbackId] = callback;

        int result = anychat_conv_set_pinned(
            convHandle_,
            convId.c_str(),
            pinned ? 1 : 0,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            convCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.convCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void setConversationMuted(std::string convId, bool muted, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.convCallbacks[callbackId] = callback;

        int result = anychat_conv_set_muted(
            convHandle_,
            convId.c_str(),
            muted ? 1 : 0,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            convCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.convCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void deleteConversation(std::string convId, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.convCallbacks[callbackId] = callback;

        int result = anychat_conv_delete(
            convHandle_,
            convId.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            convCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.convCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void setConversationUpdatedCallback(val callback) {
        g_callbacks.convUpdatedCallback = callback;
        anychat_conv_set_updated_callback(convHandle_, nullptr, convUpdatedCallbackWrapper);
    }

    // Friend methods
    void getFriendList(val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.friendListCallbacks[callbackId] = callback;

        int result = anychat_friend_get_list(
            friendHandle_,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            friendListCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.friendListCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void sendFriendRequest(std::string toUserId, std::string message, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.friendCallbacks[callbackId] = callback;

        int result = anychat_friend_send_request(
            friendHandle_,
            toUserId.c_str(),
            message.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            friendCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.friendCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void handleFriendRequest(double requestId, bool accept, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.friendCallbacks[callbackId] = callback;

        int result = anychat_friend_handle_request(
            friendHandle_,
            static_cast<int64_t>(requestId),
            accept ? 1 : 0,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            friendCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.friendCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void getPendingFriendRequests(val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.friendRequestListCallbacks[callbackId] = callback;

        int result = anychat_friend_get_pending_requests(
            friendHandle_,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            friendRequestListCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.friendRequestListCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void deleteFriend(std::string friendId, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.friendCallbacks[callbackId] = callback;

        int result = anychat_friend_delete(
            friendHandle_,
            friendId.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            friendCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.friendCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void setFriendRequestCallback(val callback) {
        g_callbacks.friendRequestCallback = callback;
        anychat_friend_set_request_callback(friendHandle_, nullptr, friendRequestCallbackWrapper);
    }

    void setFriendListChangedCallback(val callback) {
        g_callbacks.friendListChangedCallback = callback;
        anychat_friend_set_list_changed_callback(friendHandle_, nullptr, friendListChangedCallbackWrapper);
    }

    // Group methods
    void getGroupList(val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupListCallbacks[callbackId] = callback;

        int result = anychat_group_get_list(
            groupHandle_,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupListCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupListCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void createGroup(std::string name, val memberIds, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupCallbacks[callbackId] = callback;

        // Convert JavaScript array to C array
        std::vector<std::string> members;
        std::vector<const char*> memberPtrs;
        int length = memberIds["length"].as<int>();
        for (int i = 0; i < length; ++i) {
            members.push_back(memberIds[i].as<std::string>());
            memberPtrs.push_back(members.back().c_str());
        }
        memberPtrs.push_back(nullptr);

        int result = anychat_group_create(
            groupHandle_,
            name.c_str(),
            memberPtrs.data(),
            length,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void joinGroup(std::string groupId, std::string message, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupCallbacks[callbackId] = callback;

        int result = anychat_group_join(
            groupHandle_,
            groupId.c_str(),
            message.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void inviteToGroup(std::string groupId, val userIds, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupCallbacks[callbackId] = callback;

        std::vector<std::string> users;
        std::vector<const char*> userPtrs;
        int length = userIds["length"].as<int>();
        for (int i = 0; i < length; ++i) {
            users.push_back(userIds[i].as<std::string>());
            userPtrs.push_back(users.back().c_str());
        }
        userPtrs.push_back(nullptr);

        int result = anychat_group_invite(
            groupHandle_,
            groupId.c_str(),
            userPtrs.data(),
            length,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void quitGroup(std::string groupId, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupCallbacks[callbackId] = callback;

        int result = anychat_group_quit(
            groupHandle_,
            groupId.c_str(),
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()));
        }
    }

    void getGroupMembers(std::string groupId, int page, int pageSize, val callback) {
        int callbackId = g_callbacks.nextCallbackId++;
        g_callbacks.groupMemberCallbacks[callbackId] = callback;

        int result = anychat_group_get_members(
            groupHandle_,
            groupId.c_str(),
            page,
            pageSize,
            reinterpret_cast<void*>(static_cast<intptr_t>(callbackId)),
            groupMemberCallbackWrapper
        );

        if (result != ANYCHAT_OK) {
            g_callbacks.groupMemberCallbacks.erase(callbackId);
            callback(std::string(anychat_get_last_error()), val::null());
        }
    }

    void setGroupInvitedCallback(val callback) {
        g_callbacks.groupInvitedCallback = callback;
        anychat_group_set_invited_callback(groupHandle_, nullptr, groupInvitedCallbackWrapper);
    }

    void setGroupUpdatedCallback(val callback) {
        g_callbacks.groupUpdatedCallback = callback;
        anychat_group_set_updated_callback(groupHandle_, nullptr, groupUpdatedCallbackWrapper);
    }
};

// ============================================================================
// Embind Registration
// ============================================================================

EMSCRIPTEN_BINDINGS(anychat) {
    class_<AnyChatClientWrapper>("AnyChatClientWrapper")
        .constructor<val>()
        .function("connect", &AnyChatClientWrapper::connect)
        .function("disconnect", &AnyChatClientWrapper::disconnect)
        .function("getConnectionState", &AnyChatClientWrapper::getConnectionState)
        .function("setConnectionCallback", &AnyChatClientWrapper::setConnectionCallback)
        .function("login", &AnyChatClientWrapper::login)
        .function("register", &AnyChatClientWrapper::register_)
        .function("logout", &AnyChatClientWrapper::logout)
        .function("refreshToken", &AnyChatClientWrapper::refreshToken)
        .function("isLoggedIn", &AnyChatClientWrapper::isLoggedIn)
        .function("setAuthExpiredCallback", &AnyChatClientWrapper::setAuthExpiredCallback)
        .function("sendTextMessage", &AnyChatClientWrapper::sendTextMessage)
        .function("getMessageHistory", &AnyChatClientWrapper::getMessageHistory)
        .function("markMessageRead", &AnyChatClientWrapper::markMessageRead)
        .function("setMessageReceivedCallback", &AnyChatClientWrapper::setMessageReceivedCallback)
        .function("getConversationList", &AnyChatClientWrapper::getConversationList)
        .function("markConversationRead", &AnyChatClientWrapper::markConversationRead)
        .function("setConversationPinned", &AnyChatClientWrapper::setConversationPinned)
        .function("setConversationMuted", &AnyChatClientWrapper::setConversationMuted)
        .function("deleteConversation", &AnyChatClientWrapper::deleteConversation)
        .function("setConversationUpdatedCallback", &AnyChatClientWrapper::setConversationUpdatedCallback)
        .function("getFriendList", &AnyChatClientWrapper::getFriendList)
        .function("sendFriendRequest", &AnyChatClientWrapper::sendFriendRequest)
        .function("handleFriendRequest", &AnyChatClientWrapper::handleFriendRequest)
        .function("getPendingFriendRequests", &AnyChatClientWrapper::getPendingFriendRequests)
        .function("deleteFriend", &AnyChatClientWrapper::deleteFriend)
        .function("setFriendRequestCallback", &AnyChatClientWrapper::setFriendRequestCallback)
        .function("setFriendListChangedCallback", &AnyChatClientWrapper::setFriendListChangedCallback)
        .function("getGroupList", &AnyChatClientWrapper::getGroupList)
        .function("createGroup", &AnyChatClientWrapper::createGroup)
        .function("joinGroup", &AnyChatClientWrapper::joinGroup)
        .function("inviteToGroup", &AnyChatClientWrapper::inviteToGroup)
        .function("quitGroup", &AnyChatClientWrapper::quitGroup)
        .function("getGroupMembers", &AnyChatClientWrapper::getGroupMembers)
        .function("setGroupInvitedCallback", &AnyChatClientWrapper::setGroupInvitedCallback)
        .function("setGroupUpdatedCallback", &AnyChatClientWrapper::setGroupUpdatedCallback)
        ;

    // Constants
    constant("STATE_DISCONNECTED", ANYCHAT_STATE_DISCONNECTED);
    constant("STATE_CONNECTING", ANYCHAT_STATE_CONNECTING);
    constant("STATE_CONNECTED", ANYCHAT_STATE_CONNECTED);
    constant("STATE_RECONNECTING", ANYCHAT_STATE_RECONNECTING);

    constant("MSG_TEXT", ANYCHAT_MSG_TEXT);
    constant("MSG_IMAGE", ANYCHAT_MSG_IMAGE);
    constant("MSG_FILE", ANYCHAT_MSG_FILE);
    constant("MSG_AUDIO", ANYCHAT_MSG_AUDIO);
    constant("MSG_VIDEO", ANYCHAT_MSG_VIDEO);

    constant("CONV_PRIVATE", ANYCHAT_CONV_PRIVATE);
    constant("CONV_GROUP", ANYCHAT_CONV_GROUP);
}
