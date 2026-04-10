#pragma once

#include "types.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace anychat {

using FriendListCallback = std::function<void(std::vector<Friend> list, std::string err)>;
using FriendRequestListCallback = std::function<void(std::vector<FriendRequest> list, std::string err)>;
using BlacklistListCallback = std::function<void(std::vector<BlacklistItem> list, std::string err)>;
using FriendCallback = std::function<void(bool ok, std::string err)>;

class FriendListener {
public:
    virtual ~FriendListener() = default;

    virtual void onFriendAdded(const Friend& friend_info) {
        (void) friend_info;
    }

    virtual void onFriendDeleted(const std::string& user_id) {
        (void) user_id;
    }

    virtual void onFriendInfoChanged(const Friend& friend_info) {
        (void) friend_info;
    }

    virtual void onBlacklistAdded(const BlacklistItem& item) {
        (void) item;
    }

    virtual void onBlacklistRemoved(const std::string& blocked_user_id) {
        (void) blocked_user_id;
    }

    virtual void onFriendRequestReceived(const FriendRequest& req) {
        (void) req;
    }

    virtual void onFriendRequestDeleted(const FriendRequest& req) {
        (void) req;
    }

    virtual void onFriendRequestAccepted(const FriendRequest& req) {
        (void) req;
    }

    virtual void onFriendRequestRejected(const FriendRequest& req) {
        (void) req;
    }
};

class FriendManager {
public:
    virtual ~FriendManager() = default;

    // Fetches friend list. Uses DB cache; incremental sync via lastUpdateTime stored in metadata.
    virtual void getList(FriendListCallback cb) = 0;

    // Friend requests
    virtual void sendRequest(const std::string& to_user_id, const std::string& message, FriendCallback cb) = 0;
    virtual void sendRequest(
        const std::string& to_user_id,
        const std::string& message,
        const std::string& source,
        FriendCallback cb
    ) {
        (void) source;
        sendRequest(to_user_id, message, std::move(cb));
    }

    virtual void handleRequest(int64_t request_id, bool accept, FriendCallback cb) = 0;

    virtual void getPendingRequests(FriendRequestListCallback cb) = 0;
    virtual void getRequests(const std::string& request_type, FriendRequestListCallback cb) {
        if (request_type.empty() || request_type == "received") {
            getPendingRequests(std::move(cb));
            return;
        }
        if (cb) {
            cb({}, "unsupported request type");
        }
    }

    // Friendship management
    virtual void deleteFriend(const std::string& friend_id, FriendCallback cb) = 0;

    virtual void updateRemark(const std::string& friend_id, const std::string& remark, FriendCallback cb) = 0;

    // Blacklist
    virtual void getBlacklist(BlacklistListCallback cb) = 0;
    virtual void addToBlacklist(const std::string& user_id, FriendCallback cb) = 0;
    virtual void removeFromBlacklist(const std::string& user_id, FriendCallback cb) = 0;

    // Listener (fired on incoming WS notifications)
    virtual void setListener(std::shared_ptr<FriendListener> listener) = 0;
};

} // namespace anychat
