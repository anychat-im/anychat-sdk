#pragma once

/* Internal handle struct definitions shared across all c_api implementation files.
 * Not part of the public API. */

#include "anychat_c/types_c.h"

#include "anychat/auth.h"
#include "anychat/client.h"
#include "anychat/conversation.h"
#include "anychat/file.h"
#include "anychat/friend.h"
#include "anychat/group.h"
#include "anychat/message.h"
#include "anychat/call.h"
#include "anychat/user.h"

#include <memory>
#include <mutex>

/* Forward declare C callback types to avoid circular includes */
typedef void (*AnyChatConnectionStateCallback)(void* userdata, int state);

/* Forward declare to allow circular reference */
struct AnyChatClient_T;

/* Sub-module handle wrappers (non-owning pointers into the client). */
struct AnyChatAuthManager_T {
    anychat::AuthManager* impl;
    AnyChatClient_T* parent; // Pointer to parent handle for error buffer access
};
struct AnyChatMessage_T {
    anychat::MessageManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatConversation_T {
    anychat::ConversationManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatFriend_T {
    anychat::FriendManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatGroup_T {
    anychat::GroupManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatFile_T {
    anychat::FileManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatUser_T {
    anychat::UserManager* impl;
    AnyChatClient_T* parent;
};
struct AnyChatCall_T {
    anychat::CallManager* impl;
    AnyChatClient_T* parent;
};

/* Main client handle. */
struct AnyChatClient_T {
    std::shared_ptr<anychat::AnyChatClient> impl;

    AnyChatAuthManager_T auth_handle;
    AnyChatMessage_T msg_handle;
    AnyChatConversation_T conv_handle;
    AnyChatFriend_T friend_handle;
    AnyChatGroup_T group_handle;
    AnyChatFile_T file_handle;
    AnyChatUser_T user_handle;
    AnyChatCall_T call_handle;

    std::mutex cb_mutex;
    void* cb_userdata = nullptr;
    AnyChatConnectionStateCallback cb = nullptr;

    // Error message buffers for async callbacks
    // Used to store error messages that need to persist across async callback boundaries
    // Each buffer is protected by its own mutex to avoid contention
    std::mutex auth_error_mutex;
    std::string auth_error_buffer;

    std::mutex msg_error_mutex;
    std::string msg_error_buffer;

    std::mutex conv_error_mutex;
    std::string conv_error_buffer;

    std::mutex friend_error_mutex;
    std::string friend_error_buffer;

    std::mutex group_error_mutex;
    std::string group_error_buffer;

    std::mutex client_error_mutex;
    std::string client_error_buffer;

    // Token buffer for async auth callbacks
    // Must persist until Dart callback completes
    std::mutex auth_token_mutex;
    AnyChatAuthToken_C auth_token_buffer;
};
