#include "friend_manager.h"

#include "json_common.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat {
namespace {

using json_common::ApiEnvelope;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusSuccessResponse;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::pickList;
using json_common::toLowerCopy;
using json_common::writeJson;

struct SendFriendRequestBody {
    std::string user_id{};
    std::string to_user_id{};
    std::string message{};
    std::string source{};
};

struct HandleFriendRequestBody {
    bool accept = false;
    std::string action{};
};

struct UpdateRemarkBody {
    std::string remark{};
};

struct AddBlacklistBody {
    std::string user_id{};
};

struct NotificationUserInfoPayload {
    std::string user_id{};
    std::string username{};
    std::string nickname{};
    std::string avatar_url{};
    std::string avatar{};
    std::string signature{};
    int32_t gender = 0;
    std::string region{};
    bool is_friend = false;
    bool is_blocked = false;
};

struct NotificationFriendEventPayload {
    std::string user_id{};
    std::string friend_user_id{};
    std::string friend_id{};
    std::string target_user_id{};
    std::string blocked_user_id{};
    std::string remark{};
    bool is_deleted = false;
    bool is_blocked = false;
    int64_t id = 0;
    int64_t request_id = 0;
    std::string from_user_id{};
    std::string to_user_id{};
    std::string message{};
    std::string source{};
    std::string status{};
    std::string action{};
    std::string op{};
    std::string operation{};
    std::string result{};
    json_common::OptionalTimestampValue updated_at{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<NotificationUserInfoPayload> user_info{};
    std::optional<NotificationUserInfoPayload> from_user_info{};
    std::optional<NotificationUserInfoPayload> blocked_user_info{};
};

struct FriendPayload {
    std::string user_id{};
    std::string friend_id{};
    std::string remark{};
    json_common::OptionalTimestampValue updated_at{};
    bool is_deleted = false;
    std::optional<NotificationUserInfoPayload> user_info{};
};

struct FriendListDataPayload {
    std::optional<std::vector<FriendPayload>> friends{};
    std::optional<std::vector<FriendPayload>> list{};
    std::optional<std::vector<FriendPayload>> items{};
};

using FriendListDataValue = std::variant<std::monostate, FriendListDataPayload, std::vector<FriendPayload>>;

struct FriendRequestPayload {
    int64_t request_id = 0;
    int64_t id = 0;
    std::string from_user_id{};
    std::string to_user_id{};
    std::string message{};
    std::string source{};
    std::string status{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<NotificationUserInfoPayload> from_user_info{};
};

struct FriendRequestListDataPayload {
    std::optional<std::vector<FriendRequestPayload>> requests{};
    std::optional<std::vector<FriendRequestPayload>> list{};
    std::optional<std::vector<FriendRequestPayload>> items{};
};

using FriendRequestListDataValue
    = std::variant<std::monostate, FriendRequestListDataPayload, std::vector<FriendRequestPayload>>;

struct BlacklistItemPayload {
    int64_t id = 0;
    std::string user_id{};
    std::string blocked_user_id{};
    std::string target_user_id{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<NotificationUserInfoPayload> blocked_user_info{};
    std::optional<NotificationUserInfoPayload> user_info{};
};

struct BlacklistListDataPayload {
    std::optional<std::vector<BlacklistItemPayload>> items{};
    std::optional<std::vector<BlacklistItemPayload>> blacklist{};
    std::optional<std::vector<BlacklistItemPayload>> list{};
};

using BlacklistListDataValue = std::variant<std::monostate, BlacklistListDataPayload, std::vector<BlacklistItemPayload>>;

UserInfo toUserInfo(const NotificationUserInfoPayload& payload) {
    UserInfo info;
    info.user_id = payload.user_id;
    info.username = payload.username.empty() ? payload.nickname : payload.username;
    info.avatar_url = payload.avatar_url.empty() ? payload.avatar : payload.avatar_url;
    info.signature = payload.signature;
    info.gender = payload.gender;
    info.region = payload.region;
    info.is_friend = payload.is_friend;
    info.is_blocked = payload.is_blocked;
    return info;
}

Friend toFriend(const FriendPayload& payload) {
    Friend f;
    f.user_id = payload.user_id.empty() ? payload.friend_id : payload.user_id;
    f.remark = payload.remark;
    f.updated_at_ms = parseTimestampMs(payload.updated_at);
    f.is_deleted = payload.is_deleted;

    if (payload.user_info.has_value()) {
        f.user_info = toUserInfo(*payload.user_info);
    }
    if (f.user_info.user_id.empty()) {
        f.user_info.user_id = f.user_id;
    }
    return f;
}

FriendRequest toFriendRequest(const FriendRequestPayload& payload) {
    FriendRequest request;
    request.request_id = payload.request_id != 0 ? payload.request_id : payload.id;
    request.from_user_id = payload.from_user_id;
    request.to_user_id = payload.to_user_id;
    request.message = payload.message;
    request.source = payload.source;
    request.status = payload.status.empty() ? "pending" : payload.status;
    request.created_at_ms = parseTimestampMs(payload.created_at);

    if (payload.from_user_info.has_value()) {
        request.from_user_info = toUserInfo(*payload.from_user_info);
    }
    if (request.from_user_info.user_id.empty()) {
        request.from_user_info.user_id = request.from_user_id;
    }
    return request;
}

BlacklistItem toBlacklistItem(const BlacklistItemPayload& payload) {
    BlacklistItem item;
    item.id = payload.id;
    item.user_id = payload.user_id;
    item.blocked_user_id = payload.blocked_user_id.empty() ? payload.target_user_id : payload.blocked_user_id;
    item.created_at_ms = parseTimestampMs(payload.created_at);

    if (payload.blocked_user_info.has_value()) {
        item.blocked_user_info = toUserInfo(*payload.blocked_user_info);
    } else if (payload.user_info.has_value()) {
        item.blocked_user_info = toUserInfo(*payload.user_info);
    }
    if (item.blocked_user_info.user_id.empty()) {
        item.blocked_user_info.user_id = item.blocked_user_id;
    }
    return item;
}

const std::vector<FriendPayload>* toFriendPayloadList(const FriendListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<FriendPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<FriendListDataPayload>(&data); object != nullptr) {
        return pickList(object->friends, object->list, object->items);
    }
    return nullptr;
}

const std::vector<FriendRequestPayload>* toFriendRequestPayloadList(const FriendRequestListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<FriendRequestPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<FriendRequestListDataPayload>(&data); object != nullptr) {
        return pickList(object->requests, object->list, object->items);
    }
    return nullptr;
}

const std::vector<BlacklistItemPayload>* toBlacklistPayloadList(const BlacklistListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<BlacklistItemPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<BlacklistListDataPayload>(&data); object != nullptr) {
        return pickList(object->items, object->blacklist, object->list);
    }
    return nullptr;
}

Friend parseNotificationFriend(const NotificationFriendEventPayload& payload) {
    Friend f;
    if (!payload.user_id.empty()) {
        f.user_id = payload.user_id;
    } else if (!payload.friend_user_id.empty()) {
        f.user_id = payload.friend_user_id;
    } else if (!payload.friend_id.empty()) {
        f.user_id = payload.friend_id;
    } else {
        f.user_id = payload.target_user_id;
    }

    f.remark = payload.remark;
    f.updated_at_ms = parseTimestampMs(payload.updated_at);
    f.is_deleted = payload.is_deleted;

    if (payload.user_info.has_value()) {
        f.user_info = toUserInfo(*payload.user_info);
    }
    if (f.user_info.user_id.empty()) {
        f.user_info.user_id = f.user_id;
    }
    return f;
}

FriendRequest parseNotificationFriendRequest(const NotificationFriendEventPayload& payload) {
    FriendRequest request;
    request.request_id = payload.request_id != 0 ? payload.request_id : payload.id;
    request.from_user_id = payload.from_user_id;
    request.to_user_id = payload.to_user_id;
    request.message = payload.message;
    request.source = payload.source;
    request.status = payload.status.empty() ? "pending" : payload.status;
    request.created_at_ms = parseTimestampMs(payload.created_at);

    if (payload.from_user_info.has_value()) {
        request.from_user_info = toUserInfo(*payload.from_user_info);
    }
    if (request.from_user_info.user_id.empty()) {
        request.from_user_info.user_id = request.from_user_id;
    }
    return request;
}

BlacklistItem parseNotificationBlacklistItem(const NotificationFriendEventPayload& payload) {
    BlacklistItem item;
    item.id = payload.id;
    item.user_id = payload.user_id;
    item.blocked_user_id = payload.blocked_user_id.empty() ? payload.target_user_id : payload.blocked_user_id;
    item.created_at_ms = parseTimestampMs(payload.created_at);

    if (payload.blocked_user_info.has_value()) {
        item.blocked_user_info = toUserInfo(*payload.blocked_user_info);
    } else if (payload.user_info.has_value()) {
        item.blocked_user_info = toUserInfo(*payload.user_info);
    }
    if (item.blocked_user_info.user_id.empty()) {
        item.blocked_user_info.user_id = item.blocked_user_id;
    }
    return item;
}

std::string parseBlacklistAction(const NotificationFriendEventPayload& payload) {
    std::string action = payload.action;
    if (action.empty()) {
        action = payload.op;
    }
    if (action.empty()) {
        action = payload.operation;
    }
    if (action.empty()) {
        action = payload.status;
    }
    if (!action.empty()) {
        return action;
    }
    if (payload.is_blocked) {
        return "added";
    }
    return "";
}

std::string parseRequestStatus(const NotificationFriendEventPayload& payload) {
    std::string status = payload.status;
    if (status.empty()) {
        status = payload.result;
    }
    if (status.empty()) {
        status = payload.action;
    }
    if (status.empty()) {
        status = payload.op;
    }
    return toLowerCopy(status);
}

bool isAddAction(const std::string& action) {
    const std::string lowered = toLowerCopy(action);
    return lowered == "add" || lowered == "added" || lowered == "create" || lowered == "created"
        || lowered == "block" || lowered == "blocked";
}

bool isRemoveAction(const std::string& action) {
    const std::string lowered = toLowerCopy(action);
    return lowered == "remove" || lowered == "removed" || lowered == "delete" || lowered == "deleted"
        || lowered == "unblock" || lowered == "unblocked";
}

void completeBoolRequest(FriendCallback cb, network::HttpResponse resp, const std::string& fallback_error) {
    if (!cb) {
        return;
    }

    std::string err;
    const bool ok = parseApiStatusSuccessResponse(resp, err, fallback_error);
    cb(ok, ok ? "" : err);
}

} // namespace

FriendManagerImpl::FriendManagerImpl(
    db::Database* db,
    NotificationManager* notif_mgr,
    std::shared_ptr<network::HttpClient> http
)
    : db_(db)
    , notif_mgr_(notif_mgr)
    , http_(std::move(http)) {
    if (notif_mgr_) {
        notif_mgr_->addNotificationHandler([this](const NotificationEvent& ev) {
            handleFriendNotification(ev);
        });
    }
}

void FriendManagerImpl::getList(FriendListCallback cb) {
    http_->get("/friends", [cb = std::move(cb), this](network::HttpResponse resp) {
        ApiEnvelope<FriendListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<Friend> friends;
        const auto* payloads = toFriendPayloadList(root.data);
        if (payloads != nullptr) {
            friends.reserve(payloads->size());
            for (const auto& item : *payloads) {
                Friend f = toFriend(item);
                friends.push_back(f);

                if (db_) {
                    db_->exec(
                        "INSERT OR REPLACE INTO friends"
                        " (user_id, remark, updated_at_ms, is_deleted,"
                        "  friend_nickname, friend_avatar)"
                        " VALUES (?, ?, ?, ?, ?, ?)",
                        { f.user_id,
                          f.remark,
                          f.updated_at_ms,
                          f.is_deleted ? int64_t{ 1 } : int64_t{ 0 },
                          f.user_info.username,
                          f.user_info.avatar_url },
                        nullptr
                    );
                }
            }
        }

        if (cb) {
            cb(std::move(friends), "");
        }
    });
}

void FriendManagerImpl::sendRequest(const std::string& to_user_id, const std::string& message, FriendCallback cb) {
    sendRequest(to_user_id, message, "search", std::move(cb));
}

void FriendManagerImpl::sendRequest(
    const std::string& to_user_id,
    const std::string& message,
    const std::string& source,
    FriendCallback cb
) {
    const SendFriendRequestBody body{
        .user_id = to_user_id,
        .to_user_id = to_user_id,
        .message = message,
        .source = source.empty() ? "search" : source,
    };

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    http_->post("/friends/requests", body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "send request failed");
    });
}

void FriendManagerImpl::handleRequest(int64_t request_id, bool accept, FriendCallback cb) {
    const HandleFriendRequestBody body{.accept = accept, .action = accept ? "accept" : "reject"};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/friends/requests/" + std::to_string(request_id);
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "handle request failed");
    });
}

void FriendManagerImpl::getPendingRequests(FriendRequestListCallback cb) {
    getRequests("received", std::move(cb));
}

void FriendManagerImpl::getRequests(const std::string& request_type, FriendRequestListCallback cb) {
    const std::string type = request_type.empty() ? "received" : request_type;
    const std::string path = "/friends/requests?type=" + type;

    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<FriendRequestListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<FriendRequest> requests;
        const auto* payloads = toFriendRequestPayloadList(root.data);
        if (payloads != nullptr) {
            requests.reserve(payloads->size());
            for (const auto& item : *payloads) {
                requests.push_back(toFriendRequest(item));
            }
        }

        if (cb) {
            cb(std::move(requests), "");
        }
    });
}

void FriendManagerImpl::deleteFriend(const std::string& friend_id, FriendCallback cb) {
    const std::string path = "/friends/" + friend_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "delete friend failed");
    });
}

void FriendManagerImpl::updateRemark(const std::string& friend_id, const std::string& remark, FriendCallback cb) {
    const UpdateRemarkBody body{.remark = remark};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/friends/" + friend_id + "/remark";
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "update remark failed");
    });
}

void FriendManagerImpl::getBlacklist(BlacklistListCallback cb) {
    http_->get("/friends/blacklist", [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<BlacklistListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<BlacklistItem> list;
        const auto* payloads = toBlacklistPayloadList(root.data);
        if (payloads != nullptr) {
            list.reserve(payloads->size());
            for (const auto& item : *payloads) {
                list.push_back(toBlacklistItem(item));
            }
        }

        if (cb) {
            cb(std::move(list), "");
        }
    });
}

void FriendManagerImpl::addToBlacklist(const std::string& user_id, FriendCallback cb) {
    const AddBlacklistBody body{.user_id = user_id};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    http_->post("/friends/blacklist", body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "add blacklist failed");
    });
}

void FriendManagerImpl::removeFromBlacklist(const std::string& user_id, FriendCallback cb) {
    const std::string path = "/friends/blacklist/" + user_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "remove blacklist failed");
    });
}

void FriendManagerImpl::setListener(std::shared_ptr<FriendListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

void FriendManagerImpl::handleFriendNotification(const NotificationEvent& event) {
    const std::string& type = event.notification_type;
    if (type.rfind("friend.", 0) != 0) {
        return;
    }

    std::shared_ptr<FriendListener> listener;
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        listener = listener_;
    }
    if (!listener) {
        return;
    }

    try {
        NotificationFriendEventPayload payload{};
        std::string err;
        if (!parseJsonObject(event.data, payload, err)) {
            return;
        }

        if (type == "friend.added") {
            listener->onFriendAdded(parseNotificationFriend(payload));
            return;
        }

        if (type == "friend.deleted") {
            std::string user_id = payload.user_id;
            if (user_id.empty()) {
                user_id = payload.friend_user_id;
            }
            if (user_id.empty()) {
                user_id = payload.friend_id;
            }
            if (user_id.empty()) {
                user_id = payload.target_user_id;
            }
            if (user_id.empty()) {
                user_id = parseNotificationFriend(payload).user_id;
            }
            listener->onFriendDeleted(user_id);
            return;
        }

        if (type == "friend.remark_updated" || type == "friend.info_updated") {
            listener->onFriendInfoChanged(parseNotificationFriend(payload));
            return;
        }

        if (type == "friend.blacklist_added") {
            listener->onBlacklistAdded(parseNotificationBlacklistItem(payload));
            return;
        }

        if (type == "friend.blacklist_removed") {
            std::string blocked_user_id = payload.blocked_user_id;
            if (blocked_user_id.empty()) {
                blocked_user_id = payload.target_user_id;
            }
            if (blocked_user_id.empty()) {
                blocked_user_id = parseNotificationBlacklistItem(payload).blocked_user_id;
            }
            listener->onBlacklistRemoved(blocked_user_id);
            return;
        }

        if (type == "friend.blacklist_changed") {
            const std::string action = parseBlacklistAction(payload);
            if (isAddAction(action)) {
                listener->onBlacklistAdded(parseNotificationBlacklistItem(payload));
            } else if (isRemoveAction(action)) {
                std::string blocked_user_id = payload.blocked_user_id;
                if (blocked_user_id.empty()) {
                    blocked_user_id = payload.target_user_id;
                }
                if (blocked_user_id.empty()) {
                    blocked_user_id = parseNotificationBlacklistItem(payload).blocked_user_id;
                }
                listener->onBlacklistRemoved(blocked_user_id);
            }
            return;
        }

        if (type == "friend.request") {
            listener->onFriendRequestReceived(parseNotificationFriendRequest(payload));
            return;
        }

        if (type == "friend.request_deleted") {
            listener->onFriendRequestDeleted(parseNotificationFriendRequest(payload));
            return;
        }

        if (type == "friend.request_accepted") {
            listener->onFriendRequestAccepted(parseNotificationFriendRequest(payload));
            return;
        }

        if (type == "friend.request_rejected") {
            listener->onFriendRequestRejected(parseNotificationFriendRequest(payload));
            return;
        }

        if (type == "friend.request_handled") {
            FriendRequest req = parseNotificationFriendRequest(payload);
            const std::string status = parseRequestStatus(payload);
            if (status == "accepted" || status == "accept" || status == "approved" || status == "approve") {
                listener->onFriendRequestAccepted(req);
            } else if (status == "rejected" || status == "reject" || status == "declined" || status == "decline") {
                listener->onFriendRequestRejected(req);
            } else if (status == "deleted" || status == "delete" || status == "canceled" || status == "cancelled"
                       || status == "cancel") {
                listener->onFriendRequestDeleted(req);
            }
            return;
        }
    } catch (...) {
    }
}

} // namespace anychat
