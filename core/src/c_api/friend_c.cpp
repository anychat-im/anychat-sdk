#include "anychat/friend.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

anychat::AnyChatCallback makeFriendCallback(const AnyChatFriendCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        if (!callback.on_error) {
            return;
        }
        callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
    };
    return result;
}

} // namespace

extern "C" {

int anychat_friend_get_list(AnyChatFriendHandle handle, const AnyChatFriendListCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendListCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendListCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    anychat::AnyChatValueCallback<std::vector<anychat::Friend>> cb{};
    cb.on_success = [callback_copy](const std::vector<anychat::Friend>& list) {
        if (!callback_copy.on_success) {
            return;
        }
        const int count = static_cast<int>(list.size());
        AnyChatFriendList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0 ? static_cast<AnyChatFriend_C*>(std::calloc(count, sizeof(AnyChatFriend_C))) : nullptr;
        for (int i = 0; i < count; ++i) {
            friendToC(list[i], &c_list.items[i]);
        }
        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        if (!callback_copy.on_error) {
            return;
        }
        callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
    };
    handle->impl->getFriendList(std::move(cb));

    return ANYCHAT_OK;
}

int anychat_friend_add(
    AnyChatFriendHandle handle,
    const char* to_user_id,
    const char* message,
    const char* source,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl || !to_user_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->addFriend(to_user_id, message ? message : "", source ? source : "", makeFriendCallback(callback_copy));

    return ANYCHAT_OK;
}

int anychat_friend_delete(
    AnyChatFriendHandle handle,
    const char* friend_id,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl || !friend_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->deleteFriend(friend_id, makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_update_remark(
    AnyChatFriendHandle handle,
    const char* friend_id,
    const char* remark,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl || !friend_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->updateRemark(friend_id, remark ? remark : "", makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_get_requests(
    AnyChatFriendHandle handle,
    const char* request_type,
    const AnyChatFriendRequestListCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendRequestListCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendRequestListCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    const std::string type = (request_type && request_type[0] != '\0') ? request_type : "received";
    anychat::AnyChatValueCallback<std::vector<anychat::FriendRequest>> cb{};
    cb.on_success = [callback_copy](const std::vector<anychat::FriendRequest>& list) {
        if (!callback_copy.on_success) {
            return;
        }
        const int count = static_cast<int>(list.size());
        AnyChatFriendRequestList_C c_list{};
        c_list.count = count;
        c_list.items =
            count > 0 ? static_cast<AnyChatFriendRequest_C*>(std::calloc(count, sizeof(AnyChatFriendRequest_C)))
                      : nullptr;
        for (int i = 0; i < count; ++i) {
            friendRequestToC(list[i], &c_list.items[i]);
        }
        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        if (!callback_copy.on_error) {
            return;
        }
        callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
    };
    handle->impl->getFriendRequests(type, std::move(cb));
    return ANYCHAT_OK;
}

int anychat_friend_accept_request(
    AnyChatFriendHandle handle,
    int64_t request_id,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->acceptFriendRequest(request_id, makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_reject_request(
    AnyChatFriendHandle handle,
    int64_t request_id,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->rejectFriendRequest(request_id, makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_add_to_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl || !user_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->addToBlacklist(user_id, makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_remove_from_blacklist(
    AnyChatFriendHandle handle,
    const char* user_id,
    const AnyChatFriendCallback_C* callback
) {
    if (!handle || !handle->impl || !user_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatFriendCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->removeFromBlacklist(user_id, makeFriendCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_friend_get_blacklist(AnyChatFriendHandle handle, const AnyChatBlacklistListCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatBlacklistListCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatBlacklistListCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    anychat::AnyChatValueCallback<std::vector<anychat::BlacklistItem>> cb{};
    cb.on_success = [callback_copy](const std::vector<anychat::BlacklistItem>& list) {
        if (!callback_copy.on_success) {
            return;
        }
        const int count = static_cast<int>(list.size());
        AnyChatBlacklistList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0
                           ? static_cast<AnyChatBlacklistItem_C*>(std::calloc(count, sizeof(AnyChatBlacklistItem_C)))
                           : nullptr;
        for (int i = 0; i < count; ++i) {
            blacklistItemToC(list[i], &c_list.items[i]);
        }
        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        if (!callback_copy.on_error) {
            return;
        }
        callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
    };
    handle->impl->getBlacklist(std::move(cb));
    return ANYCHAT_OK;
}

int anychat_friend_set_listener(AnyChatFriendHandle handle, const AnyChatFriendListener_C* listener) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    if (!listener) {
        handle->impl->setListener(nullptr);
        return ANYCHAT_OK;
    }

    if (listener->struct_size < sizeof(AnyChatFriendListener_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatFriendListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CFriendListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
