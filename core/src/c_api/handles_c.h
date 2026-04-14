#pragma once

/* Internal handle struct definitions shared across all c_api implementation files.
 * Not part of the public API. */

#include "anychat/types.h"

#include "internal/auth.h"
#include "internal/client.h"
#include "internal/conversation.h"
#include "internal/file.h"
#include "internal/friend.h"
#include "internal/group.h"
#include "internal/message.h"
#include "internal/call.h"
#include "internal/user.h"
#include "internal/version.h"

#include <memory>
#include <mutex>

/* Forward declare C callback types to avoid circular includes */
typedef void (*AnyChatConnectionStateCallback)(void* userdata, int state);

/* Sub-module handle wrappers (non-owning pointers into the client). */
struct AnyChatAuthManager_T {
    anychat::AuthManager* impl = nullptr;
};
struct AnyChatMessage_T {
    anychat::MessageManager* impl = nullptr;
};
struct AnyChatConversation_T {
    anychat::ConversationManager* impl = nullptr;
};
struct AnyChatFriend_T {
    anychat::FriendManager* impl = nullptr;
};
struct AnyChatGroup_T {
    anychat::GroupManager* impl = nullptr;
};
struct AnyChatFile_T {
    anychat::FileManager* impl = nullptr;
};
struct AnyChatUser_T {
    anychat::UserManager* impl = nullptr;
};
struct AnyChatCall_T {
    anychat::CallManager* impl = nullptr;
};
struct AnyChatVersion_T {
    anychat::VersionManager* impl = nullptr;
};

/* Main client handle. */
struct AnyChatClient_T {
    std::unique_ptr<anychat::AnyChatClient> impl;

    AnyChatAuthManager_T auth_handle;
    AnyChatMessage_T msg_handle;
    AnyChatConversation_T conv_handle;
    AnyChatFriend_T friend_handle;
    AnyChatGroup_T group_handle;
    AnyChatFile_T file_handle;
    AnyChatUser_T user_handle;
    AnyChatCall_T call_handle;
    AnyChatVersion_T version_handle;

    std::mutex cb_mutex;
    void* cb_userdata = nullptr;
    AnyChatConnectionStateCallback cb = nullptr;
};
