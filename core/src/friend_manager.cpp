#include "friend_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

const nlohmann::json* findField(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

int64_t normalizeUnixMs(int64_t raw) {
    return (raw > 0 && raw < 1'000'000'000'000LL) ? raw * 1000LL : raw;
}

int64_t parseInt64(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        return value.get<int64_t>();
    }
    if (value.is_number_unsigned()) {
        return static_cast<int64_t>(value.get<uint64_t>());
    }
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text.empty()) {
            return 0;
        }
        const bool all_digits = std::all_of(text.begin(), text.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
        if (all_digits) {
            try {
                return std::stoll(text);
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

bool parseBool(const nlohmann::json& value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<int64_t>() != 0;
    }
    if (value.is_number_unsigned()) {
        return value.get<uint64_t>() != 0;
    }
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        return text == "1" || text == "true" || text == "TRUE";
    }
    return false;
}

time_t utcTimeFromTm(std::tm* tm_utc) {
#if defined(_WIN32)
    return _mkgmtime(tm_utc);
#else
    return timegm(tm_utc);
#endif
}

int parseTwoDigits(const std::string& text, size_t pos) {
    if (pos + 2 > text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))
        || !std::isdigit(static_cast<unsigned char>(text[pos + 1]))) {
        return -1;
    }
    return (text[pos] - '0') * 10 + (text[pos + 1] - '0');
}

int64_t parseRfc3339Ms(const std::string& text) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6
        && std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }

    std::tm tm_utc{};
    tm_utc.tm_year = year - 1900;
    tm_utc.tm_mon = month - 1;
    tm_utc.tm_mday = day;
    tm_utc.tm_hour = hour;
    tm_utc.tm_min = minute;
    tm_utc.tm_sec = second;

    const time_t epoch_seconds = utcTimeFromTm(&tm_utc);
    if (epoch_seconds == static_cast<time_t>(-1)) {
        return 0;
    }

    int ms = 0;
    const size_t dot_pos = text.find('.', 19);
    if (dot_pos != std::string::npos) {
        int scale = 100;
        for (size_t i = dot_pos + 1; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) && scale > 0;
             ++i) {
            ms += (text[i] - '0') * scale;
            scale /= 10;
        }
    }

    int tz_offset_seconds = 0;
    const size_t tz_pos = text.find_first_of("+-Z", 19);
    if (tz_pos != std::string::npos && text[tz_pos] != 'Z') {
        const int hh = parseTwoDigits(text, tz_pos + 1);
        if (hh >= 0) {
            int mm = 0;
            if (tz_pos + 3 < text.size() && text[tz_pos + 3] == ':') {
                const int parsed = parseTwoDigits(text, tz_pos + 4);
                if (parsed >= 0) {
                    mm = parsed;
                }
            } else if (tz_pos + 3 < text.size() && std::isdigit(static_cast<unsigned char>(text[tz_pos + 3]))) {
                const int parsed = parseTwoDigits(text, tz_pos + 3);
                if (parsed >= 0) {
                    mm = parsed;
                }
            }
            tz_offset_seconds = hh * 3600 + mm * 60;
            if (text[tz_pos] == '-') {
                tz_offset_seconds = -tz_offset_seconds;
            }
        }
    }

    return static_cast<int64_t>(epoch_seconds - tz_offset_seconds) * 1000LL + ms;
}

int64_t parseTimestampMs(const nlohmann::json& value) {
    if (value.is_null()) {
        return 0;
    }

    if (value.is_object()) {
        const auto* seconds = findField(value, { "seconds" });
        if (seconds != nullptr) {
            const int64_t sec = parseInt64(*seconds);
            int64_t ms = sec * 1000LL;
            const auto* nanos = findField(value, { "nanos", "nanoseconds" });
            if (nanos != nullptr) {
                ms += parseInt64(*nanos) / 1'000'000LL;
            }
            return ms;
        }
    }

    if (value.is_number_integer() || value.is_number_unsigned() || value.is_string()) {
        const int64_t raw = parseInt64(value);
        if (raw != 0) {
            return normalizeUnixMs(raw);
        }
        if (value.is_string()) {
            return parseRfc3339Ms(value.get<std::string>());
        }
    }

    return 0;
}

int64_t jsonIntValue(
    const nlohmann::json& item,
    std::initializer_list<const char*> keys,
    int64_t default_value = 0
) {
    const auto* it = findField(item, keys);
    if (it == nullptr) {
        return default_value;
    }
    if (it->is_number_integer() || it->is_number_unsigned()) {
        return parseInt64(*it);
    }
    if (it->is_string()) {
        const std::string text = it->get<std::string>();
        if (text.empty()) {
            return default_value;
        }
        const bool all_digits = std::all_of(text.begin(), text.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
        if (!all_digits) {
            return default_value;
        }
        try {
            return std::stoll(text);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

bool jsonBoolValue(const nlohmann::json& item, std::initializer_list<const char*> keys, bool default_value = false) {
    const auto* it = findField(item, keys);
    if (it == nullptr) {
        return default_value;
    }
    return parseBool(*it);
}

std::string jsonStringValue(
    const nlohmann::json& item,
    std::initializer_list<const char*> keys,
    const std::string& default_value = ""
) {
    const auto* it = findField(item, keys);
    if (it == nullptr) {
        return default_value;
    }
    if (it->is_string()) {
        return it->get<std::string>();
    }
    if (it->is_number_integer()) {
        return std::to_string(it->get<int64_t>());
    }
    if (it->is_number_unsigned()) {
        return std::to_string(it->get<uint64_t>());
    }
    return default_value;
}

const nlohmann::json* pickArray(const nlohmann::json& data, std::initializer_list<const char*> keys) {
    if (data.is_array()) {
        return &data;
    }
    if (!data.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        auto it = data.find(key);
        if (it != data.end() && it->is_array()) {
            return &(*it);
        }
    }
    return nullptr;
}

UserInfo parseUserInfo(const nlohmann::json& item) {
    UserInfo info;
    info.user_id = jsonStringValue(item, { "userId", "user_id" });
    info.username = jsonStringValue(item, { "nickname", "username" });
    info.avatar_url = jsonStringValue(item, { "avatarUrl", "avatar_url", "avatar" });
    info.signature = jsonStringValue(item, { "signature" });
    info.gender = static_cast<int32_t>(jsonIntValue(item, { "gender" }, 0));
    info.region = jsonStringValue(item, { "region" });
    info.is_friend = jsonBoolValue(item, { "isFriend", "is_friend" }, false);
    info.is_blocked = jsonBoolValue(item, { "isBlocked", "is_blocked" }, false);
    return info;
}

Friend parseFriend(const nlohmann::json& item) {
    Friend f;
    f.user_id = jsonStringValue(item, { "userId", "user_id", "friendId", "friend_id" });
    f.remark = jsonStringValue(item, { "remark" });
    const auto* updated_at = findField(item, { "updatedAt", "updated_at" });
    if (updated_at != nullptr) {
        f.updated_at_ms = parseTimestampMs(*updated_at);
    }
    f.is_deleted = jsonBoolValue(item, { "isDeleted", "is_deleted" }, false);

    const auto* user_info = findField(item, { "userInfo", "user_info" });
    if (user_info != nullptr && user_info->is_object()) {
        f.user_info = parseUserInfo(*user_info);
    }
    return f;
}

FriendRequest parseFriendRequest(const nlohmann::json& item) {
    FriendRequest r;
    r.request_id = jsonIntValue(item, { "requestId", "request_id", "id" });
    r.from_user_id = jsonStringValue(item, { "fromUserId", "from_user_id" });
    r.to_user_id = jsonStringValue(item, { "toUserId", "to_user_id" });
    r.message = jsonStringValue(item, { "message" });
    r.source = jsonStringValue(item, { "source" });
    r.status = jsonStringValue(item, { "status" }, "pending");
    const auto* created_at = findField(item, { "createdAt", "created_at" });
    if (created_at != nullptr) {
        r.created_at_ms = parseTimestampMs(*created_at);
    }
    const auto* from_user_info = findField(item, { "fromUserInfo", "from_user_info" });
    if (from_user_info != nullptr && from_user_info->is_object()) {
        r.from_user_info = parseUserInfo(*from_user_info);
    }
    return r;
}

BlacklistItem parseBlacklistItem(const nlohmann::json& item) {
    BlacklistItem b;
    b.id = jsonIntValue(item, { "id" });
    b.user_id = jsonStringValue(item, { "userId", "user_id" });
    b.blocked_user_id =
        jsonStringValue(item, { "blockedUserId", "blocked_user_id", "targetUserId", "target_user_id" });

    const auto* created_at = findField(item, { "createdAt", "created_at" });
    if (created_at != nullptr) {
        b.created_at_ms = parseTimestampMs(*created_at);
    }

    const auto* blocked_user_info = findField(item, { "blockedUserInfo", "blocked_user_info", "userInfo", "user_info" });
    if (blocked_user_info != nullptr && blocked_user_info->is_object()) {
        b.blocked_user_info = parseUserInfo(*blocked_user_info);
    }

    return b;
}

std::string toLowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
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

std::string parseBlacklistAction(const nlohmann::json& data) {
    std::string action = jsonStringValue(data, { "action", "op", "operation", "status" });
    if (!action.empty()) {
        return action;
    }
    if (jsonBoolValue(data, { "isBlocked", "is_blocked" }, false)) {
        return "added";
    }
    return "";
}

std::string parseRequestStatus(const nlohmann::json& data) {
    return toLowerCopy(jsonStringValue(data, { "status", "result", "action", "op" }));
}

void completeBoolRequest(FriendCallback cb, network::HttpResponse resp) {
    if (!cb) {
        return;
    }

    if (!resp.error.empty()) {
        cb(false, resp.error);
        return;
    }

    try {
        const auto root = nlohmann::json::parse(resp.body);
        if (jsonIntValue(root, { "code" }, -1) == 0) {
            cb(true, "");
        } else {
            cb(false, jsonStringValue(root, { "message" }, "server error"));
        }
    } catch (const std::exception& e) {
        cb(false, std::string("parse error: ") + e.what());
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// getList
// ---------------------------------------------------------------------------

void FriendManagerImpl::getList(FriendListCallback cb) {
    http_->get("/friends", [cb = std::move(cb), this](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            if (cb) {
                cb({}, resp.error);
            }
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (jsonIntValue(root, { "code" }, -1) != 0) {
                if (cb) {
                    cb({}, jsonStringValue(root, { "message" }, "server error"));
                }
                return;
            }

            std::vector<Friend> friends;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "friends", "list", "items" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    Friend f = parseFriend(item);
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
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

// ---------------------------------------------------------------------------
// sendRequest
// ---------------------------------------------------------------------------

void FriendManagerImpl::sendRequest(const std::string& to_user_id, const std::string& message, FriendCallback cb) {
    sendRequest(to_user_id, message, "search", std::move(cb));
}

void FriendManagerImpl::sendRequest(
    const std::string& to_user_id,
    const std::string& message,
    const std::string& source,
    FriendCallback cb
) {
    nlohmann::json body;
    body["userId"] = to_user_id;
    body["toUserId"] = to_user_id;
    body["message"] = message;
    body["source"] = source.empty() ? "search" : source;

    http_->post("/friends/requests", body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// handleRequest
// ---------------------------------------------------------------------------

void FriendManagerImpl::handleRequest(int64_t request_id, bool accept, FriendCallback cb) {
    nlohmann::json body;
    body["accept"] = accept;
    body["action"] = accept ? "accept" : "reject";

    const std::string path = "/friends/requests/" + std::to_string(request_id);
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// getPendingRequests / getRequests
// ---------------------------------------------------------------------------

void FriendManagerImpl::getPendingRequests(FriendRequestListCallback cb) {
    getRequests("received", std::move(cb));
}

void FriendManagerImpl::getRequests(const std::string& request_type, FriendRequestListCallback cb) {
    const std::string type = request_type.empty() ? "received" : request_type;
    const std::string path = "/friends/requests?type=" + type;

    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            if (cb) {
                cb({}, resp.error);
            }
            return;
        }
        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (jsonIntValue(root, { "code" }, -1) != 0) {
                if (cb) {
                    cb({}, jsonStringValue(root, { "message" }, "server error"));
                }
                return;
            }

            std::vector<FriendRequest> requests;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "requests", "list", "items" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    requests.push_back(parseFriendRequest(item));
                }
            }

            if (cb) {
                cb(std::move(requests), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

// ---------------------------------------------------------------------------
// deleteFriend
// ---------------------------------------------------------------------------

void FriendManagerImpl::deleteFriend(const std::string& friend_id, FriendCallback cb) {
    const std::string path = "/friends/" + friend_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// updateRemark
// ---------------------------------------------------------------------------

void FriendManagerImpl::updateRemark(const std::string& friend_id, const std::string& remark, FriendCallback cb) {
    nlohmann::json body;
    body["remark"] = remark;

    const std::string path = "/friends/" + friend_id + "/remark";
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// getBlacklist / addToBlacklist / removeFromBlacklist
// ---------------------------------------------------------------------------

void FriendManagerImpl::getBlacklist(BlacklistListCallback cb) {
    http_->get("/friends/blacklist", [cb = std::move(cb)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            if (cb) {
                cb({}, resp.error);
            }
            return;
        }

        try {
            const auto root = nlohmann::json::parse(resp.body);
            if (jsonIntValue(root, { "code" }, -1) != 0) {
                if (cb) {
                    cb({}, jsonStringValue(root, { "message" }, "server error"));
                }
                return;
            }

            std::vector<BlacklistItem> list;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "items", "blacklist", "list" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    list.push_back(parseBlacklistItem(item));
                }
            }

            if (cb) {
                cb(std::move(list), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

void FriendManagerImpl::addToBlacklist(const std::string& user_id, FriendCallback cb) {
    nlohmann::json body;
    body["userId"] = user_id;

    http_->post("/friends/blacklist", body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void FriendManagerImpl::removeFromBlacklist(const std::string& user_id, FriendCallback cb) {
    const std::string path = "/friends/blacklist/" + user_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// setListener
// ---------------------------------------------------------------------------

void FriendManagerImpl::setListener(std::shared_ptr<FriendListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

// ---------------------------------------------------------------------------
// handleFriendNotification
// ---------------------------------------------------------------------------

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
        if (type == "friend.added") {
            listener->onFriendAdded(parseFriend(event.data));
            return;
        }

        if (type == "friend.deleted") {
            std::string user_id = jsonStringValue(
                event.data,
                { "userId", "user_id", "friendId", "friend_id", "targetUserId", "target_user_id" }
            );
            if (user_id.empty()) {
                user_id = parseFriend(event.data).user_id;
            }
            listener->onFriendDeleted(user_id);
            return;
        }

        if (type == "friend.remark_updated" || type == "friend.info_updated") {
            listener->onFriendInfoChanged(parseFriend(event.data));
            return;
        }

        if (type == "friend.blacklist_added") {
            listener->onBlacklistAdded(parseBlacklistItem(event.data));
            return;
        }

        if (type == "friend.blacklist_removed") {
            std::string blocked_user_id =
                jsonStringValue(event.data, { "blockedUserId", "blocked_user_id", "targetUserId", "target_user_id" });
            if (blocked_user_id.empty()) {
                blocked_user_id = parseBlacklistItem(event.data).blocked_user_id;
            }
            listener->onBlacklistRemoved(blocked_user_id);
            return;
        }

        if (type == "friend.blacklist_changed") {
            const std::string action = parseBlacklistAction(event.data);
            if (isAddAction(action)) {
                listener->onBlacklistAdded(parseBlacklistItem(event.data));
            } else if (isRemoveAction(action)) {
                std::string blocked_user_id = jsonStringValue(
                    event.data,
                    { "blockedUserId", "blocked_user_id", "targetUserId", "target_user_id" }
                );
                if (blocked_user_id.empty()) {
                    blocked_user_id = parseBlacklistItem(event.data).blocked_user_id;
                }
                listener->onBlacklistRemoved(blocked_user_id);
            }
            return;
        }

        if (type == "friend.request") {
            listener->onFriendRequestReceived(parseFriendRequest(event.data));
            return;
        }

        if (type == "friend.request_deleted") {
            listener->onFriendRequestDeleted(parseFriendRequest(event.data));
            return;
        }

        if (type == "friend.request_accepted") {
            listener->onFriendRequestAccepted(parseFriendRequest(event.data));
            return;
        }

        if (type == "friend.request_rejected") {
            listener->onFriendRequestRejected(parseFriendRequest(event.data));
            return;
        }

        if (type == "friend.request_handled") {
            FriendRequest req = parseFriendRequest(event.data);
            const std::string status = parseRequestStatus(event.data);
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
        // Ignore malformed notification payloads.
    }
}

} // namespace anychat
