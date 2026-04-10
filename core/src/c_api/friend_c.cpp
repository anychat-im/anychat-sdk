#include "anychat_c/friend_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace {

void userInfoToC(const anychat::UserInfo& src, AnyChatUserInfo_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->username, src.username.c_str(), sizeof(dst->username));
    anychat_strlcpy(dst->avatar_url, src.avatar_url.c_str(), sizeof(dst->avatar_url));
    anychat_strlcpy(dst->signature, src.signature.c_str(), sizeof(dst->signature));
    dst->gender = src.gender;
    anychat_strlcpy(dst->region, src.region.c_str(), sizeof(dst->region));
    dst->is_friend = src.is_friend ? 1 : 0;
    dst->is_blocked = src.is_blocked ? 1 : 0;
}

void friendToC(const anychat::Friend& src, AnyChatFriend_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->remark, src.remark.c_str(), sizeof(dst->remark));
    dst->updated_at_ms = src.updated_at_ms;
    dst->is_deleted = src.is_deleted ? 1 : 0;
    userInfoToC(src.user_info, &dst->user_info);
}

void friendRequestToC(const anychat::FriendRequest& src, AnyChatFriendRequest_C* dst) {
    dst->request_id = src.request_id;
    anychat_strlcpy(dst->from_user_id, src.from_user_id.c_str(), sizeof(dst->from_user_id));
    anychat_strlcpy(dst->to_user_id, src.to_user_id.c_str(), sizeof(dst->to_user_id));
    anychat_strlcpy(dst->message, src.message.c_str(), sizeof(dst->message));
    anychat_strlcpy(dst->source, src.source.c_str(), sizeof(dst->source));
    anychat_strlcpy(dst->status, src.status.c_str(), sizeof(dst->status));
    dst->created_at_ms = src.created_at_ms;
    userInfoToC(src.from_user_info, &dst->from_user_info);
}

void blacklistItemToC(const anychat::BlacklistItem& src, AnyChatBlacklistItem_C* dst) {
    dst->id = src.id;
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->blocked_user_id, src.blocked_user_id.c_str(), sizeof(dst->blocked_user_id));
    dst->created_at_ms = src.created_at_ms;
    userInfoToC(src.blocked_user_info, &dst->blocked_user_info);
}

class CFriendListener final : public anychat::FriendListener {
public:
    explicit CFriendListener(const AnyChatFriendListener_C& listener)
        : listener_(listener) {}

    void onFriendAdded(const anychat::Friend& friend_info) override {
        if (!listener_.on_friend_added) {
            return;
        }
        AnyChatFriend_C c_friend{};
        friendToC(friend_info, &c_friend);
        listener_.on_friend_added(listener_.userdata, &c_friend);
    }

    void onFriendDeleted(const std::string& user_id) override {
        if (!listener_.on_friend_deleted) {
            return;
        }
        listener_.on_friend_deleted(listener_.userdata, user_id.c_str());
    }

    void onFriendInfoChanged(const anychat::Friend& friend_info) override {
        if (!listener_.on_friend_info_changed) {
            return;
        }
        AnyChatFriend_C c_friend{};
        friendToC(friend_info, &c_friend);
        listener_.on_friend_info_changed(listener_.userdata, &c_friend);
    }

    void onBlacklistAdded(const anychat::BlacklistItem& item) override {
        if (!listener_.on_blacklist_added) {
            return;
        }
        AnyChatBlacklistItem_C c_item{};
        blacklistItemToC(item, &c_item);
        listener_.on_blacklist_added(listener_.userdata, &c_item);
    }

    void onBlacklistRemoved(const std::string& blocked_user_id) override {
        if (!listener_.on_blacklist_removed) {
            return;
        }
        listener_.on_blacklist_removed(listener_.userdata, blocked_user_id.c_str());
    }

    void onFriendRequestReceived(const anychat::FriendRequest& req) override {
        if (!listener_.on_friend_request_received) {
            return;
        }
        AnyChatFriendRequest_C c_req{};
        friendRequestToC(req, &c_req);
        listener_.on_friend_request_received(listener_.userdata, &c_req);
    }

    void onFriendRequestDeleted(const anychat::FriendRequest& req) override {
        if (!listener_.on_friend_request_deleted) {
            return;
        }
        AnyChatFriendRequest_C c_req{};
        friendRequestToC(req, &c_req);
        listener_.on_friend_request_deleted(listener_.userdata, &c_req);
    }

    void onFriendRequestAccepted(const anychat::FriendRequest& req) override {
        if (!listener_.on_friend_request_accepted) {
            return;
        }
        AnyChatFriendRequest_C c_req{};
        friendRequestToC(req, &c_req);
        listener_.on_friend_request_accepted(listener_.userdata, &c_req);
    }

    void onFriendRequestRejected(const anychat::FriendRequest& req) override {
        if (!listener_.on_friend_request_rejected) {
            return;
        }
        AnyChatFriendRequest_C c_req{};
        friendRequestToC(req, &c_req);
        listener_.on_friend_request_rejected(listener_.userdata, &c_req);
    }

private:
    AnyChatFriendListener_C listener_{};
};

} // namespace

extern "C" {

int anychat_friend_get_list(AnyChatFriendHandle handle, void* userdata, AnyChatFriendListCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getList([userdata, callback](std::vector<anychat::Friend> list, std::string err) {
        if (!callback)
            return;
        int count = static_cast<int>(list.size());
        AnyChatFriendList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0 ? static_cast<AnyChatFriend_C*>(std::calloc(count, sizeof(AnyChatFriend_C))) : nullptr;
        for (int i = 0; i < count; ++i)
            friendToC(list[i], &c_list.items[i]);
        callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
        std::free(c_list.items);
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_send_request(
    AnyChatFriendHandle handle,
    const char* to_user_id,
    const char* message,
    void* userdata,
    AnyChatFriendCallback callback
) {
    return anychat_friend_send_request_with_source(handle, to_user_id, message, nullptr, userdata, callback);
}

int anychat_friend_send_request_with_source(
    AnyChatFriendHandle handle,
    const char* to_user_id,
    const char* message,
    const char* source,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl || !to_user_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (source && source[0] != '\0') {
        handle->impl->sendRequest(
            to_user_id,
            message ? message : "",
            source,
            [userdata, callback](bool ok, std::string err) {
                if (callback)
                    callback(userdata, ok ? 1 : 0, err.c_str());
            }
        );
    } else {
        handle->impl->sendRequest(to_user_id, message ? message : "", [userdata, callback](bool ok, std::string err) {
            if (callback)
                callback(userdata, ok ? 1 : 0, err.c_str());
        });
    }
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_handle_request(
    AnyChatFriendHandle handle,
    int64_t request_id,
    int accept,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->handleRequest(request_id, accept != 0, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_get_pending_requests(
    AnyChatFriendHandle handle,
    void* userdata,
    AnyChatFriendRequestListCallback callback
) {
    return anychat_friend_get_requests(handle, "received", userdata, callback);
}

int anychat_friend_get_requests(
    AnyChatFriendHandle handle,
    const char* request_type,
    void* userdata,
    AnyChatFriendRequestListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const std::string type = (request_type && request_type[0] != '\0') ? request_type : "received";
    handle->impl->getRequests(type, [userdata, callback](std::vector<anychat::FriendRequest> list, std::string err) {
        if (!callback)
            return;
        int count = static_cast<int>(list.size());
        AnyChatFriendRequestList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0
                           ? static_cast<AnyChatFriendRequest_C*>(std::calloc(count, sizeof(AnyChatFriendRequest_C)))
                           : nullptr;
        for (int i = 0; i < count; ++i)
            friendRequestToC(list[i], &c_list.items[i]);
        callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
        std::free(c_list.items);
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_delete(
    AnyChatFriendHandle handle,
    const char* friend_id,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl || !friend_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->deleteFriend(friend_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_update_remark(
    AnyChatFriendHandle handle,
    const char* friend_id,
    const char* remark,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl || !friend_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->updateRemark(friend_id, remark ? remark : "", [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_add_to_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl || !user_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->addToBlacklist(user_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_remove_from_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    void* userdata,
    AnyChatFriendCallback callback
) {
    if (!handle || !handle->impl || !user_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->removeFromBlacklist(user_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_get_blacklist(
    AnyChatFriendHandle handle,
    void* userdata,
    AnyChatBlacklistListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getBlacklist([userdata, callback](std::vector<anychat::BlacklistItem> list, std::string err) {
        if (!callback)
            return;
        int count = static_cast<int>(list.size());
        AnyChatBlacklistList_C c_list{};
        c_list.count = count;
        c_list.items =
            count > 0 ? static_cast<AnyChatBlacklistItem_C*>(std::calloc(count, sizeof(AnyChatBlacklistItem_C)))
                      : nullptr;
        for (int i = 0; i < count; ++i)
            blacklistItemToC(list[i], &c_list.items[i]);
        callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
        std::free(c_list.items);
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_friend_set_listener(AnyChatFriendHandle handle, const AnyChatFriendListener_C* listener) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    if (!listener) {
        handle->impl->setListener(nullptr);
        anychat_clear_last_error();
        return ANYCHAT_OK;
    }

    if (listener->struct_size < sizeof(AnyChatFriendListener_C)) {
        anychat_set_last_error("listener struct_size is too small");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CFriendListener>(copied));
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
