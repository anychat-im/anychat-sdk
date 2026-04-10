#include "group_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <string>
#include <utility>

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
        if (!all_digits) {
            return 0;
        }
        try {
            return std::stoll(text);
        } catch (...) {
            return 0;
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

    if (value.is_number_integer() || value.is_number_unsigned()) {
        return normalizeUnixMs(parseInt64(value));
    }

    if (value.is_string()) {
        const int64_t as_num = parseInt64(value);
        if (as_num != 0) {
            return normalizeUnixMs(as_num);
        }
        return parseRfc3339Ms(value.get<std::string>());
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
    if (it->is_number_integer() || it->is_number_unsigned() || it->is_string()) {
        return parseInt64(*it);
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

GroupRole parseRoleString(const std::string& role_str) {
    std::string lowered;
    lowered.reserve(role_str.size());
    for (unsigned char ch : role_str) {
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    }

    if (lowered == "owner") {
        return GroupRole::Owner;
    }
    if (lowered == "admin") {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

std::string roleToString(GroupRole role) {
    switch (role) {
    case GroupRole::Owner:
        return "owner";
    case GroupRole::Admin:
        return "admin";
    case GroupRole::Member:
    default:
        return "member";
    }
}

GroupRole parseRole(const nlohmann::json& item, std::initializer_list<const char*> keys, GroupRole default_value) {
    const auto* value = findField(item, keys);
    if (value == nullptr) {
        return default_value;
    }

    if (value->is_string()) {
        return parseRoleString(value->get<std::string>());
    }

    if (value->is_number_integer() || value->is_number_unsigned()) {
        const int64_t role = parseInt64(*value);
        if (role == 0) {
            return GroupRole::Owner;
        }
        if (role == 1) {
            return GroupRole::Admin;
        }
    }

    return GroupRole::Member;
}

UserInfo parseUserInfo(const nlohmann::json& item) {
    UserInfo info;
    info.user_id = jsonStringValue(item, { "userId", "user_id", "id" });
    info.username = jsonStringValue(item, { "nickname", "username", "name" });
    info.avatar_url = jsonStringValue(item, { "avatarUrl", "avatar_url", "avatar" });
    info.signature = jsonStringValue(item, { "signature", "bio" });
    info.gender = static_cast<int32_t>(jsonIntValue(item, { "gender" }, 0));
    info.region = jsonStringValue(item, { "region" });
    info.is_friend = jsonBoolValue(item, { "isFriend", "is_friend" }, false);
    info.is_blocked = jsonBoolValue(item, { "isBlocked", "is_blocked" }, false);
    return info;
}

Group parseGroup(const nlohmann::json& item) {
    Group g;
    if (!item.is_object()) {
        return g;
    }

    g.group_id = jsonStringValue(item, { "groupId", "group_id", "id" });
    g.name = jsonStringValue(item, { "name", "groupName", "group_name", "displayName", "display_name" });
    g.display_name = jsonStringValue(item, { "displayName", "display_name" }, g.name);
    g.avatar_url = jsonStringValue(item, { "avatarUrl", "avatar_url", "avatar", "groupAvatar", "group_avatar" });
    g.announcement = jsonStringValue(item, { "announcement" });
    g.description = jsonStringValue(item, { "description" });
    g.group_remark = jsonStringValue(item, { "groupRemark", "group_remark", "remark" });
    g.owner_id = jsonStringValue(item, { "ownerId", "owner_id" });
    g.member_count = static_cast<int32_t>(jsonIntValue(item, { "memberCount", "member_count" }, 0));
    g.max_members = static_cast<int32_t>(jsonIntValue(item, { "maxMembers", "max_members" }, 0));
    g.my_role = parseRole(item, { "myRole", "my_role", "role" }, GroupRole::Member);
    g.join_verify = jsonBoolValue(item, { "joinVerify", "join_verify", "needVerify", "need_verify" }, false);
    g.is_muted = jsonBoolValue(item, { "isMuted", "is_muted" }, false);

    if (const auto* created = findField(item, { "createdAt", "created_at" }); created != nullptr) {
        g.created_at_ms = parseTimestampMs(*created);
    }
    if (const auto* updated = findField(item, { "updatedAt", "updated_at", "updateTime", "update_time" });
        updated != nullptr) {
        g.updated_at_ms = parseTimestampMs(*updated);
    }

    return g;
}

GroupMember parseGroupMember(const nlohmann::json& item) {
    GroupMember m;
    if (!item.is_object()) {
        return m;
    }

    m.user_id = jsonStringValue(item, { "userId", "user_id", "id" });
    m.group_nickname = jsonStringValue(item, { "groupNickname", "group_nickname", "nickname" });
    m.role = parseRole(item, { "role" }, GroupRole::Member);
    m.is_muted = jsonBoolValue(item, { "isMuted", "is_muted" }, false);

    if (const auto* muted_until = findField(item, { "mutedUntil", "muted_until" }); muted_until != nullptr) {
        m.muted_until_ms = parseTimestampMs(*muted_until);
    }
    if (const auto* joined = findField(item, { "joinedAt", "joined_at", "createdAt", "created_at" });
        joined != nullptr) {
        m.joined_at_ms = parseTimestampMs(*joined);
    }

    const auto* user_info = findField(item, { "userInfo", "user_info" });
    if (user_info != nullptr && user_info->is_object()) {
        m.user_info = parseUserInfo(*user_info);
    } else {
        m.user_info = parseUserInfo(item);
    }

    if (m.user_info.user_id.empty()) {
        m.user_info.user_id = m.user_id;
    }

    return m;
}

GroupJoinRequest parseJoinRequest(const nlohmann::json& item) {
    GroupJoinRequest req;
    if (!item.is_object()) {
        return req;
    }

    req.request_id = jsonIntValue(item, { "requestId", "request_id", "id" }, 0);
    req.group_id = jsonStringValue(item, { "groupId", "group_id" });
    req.user_id = jsonStringValue(item, { "userId", "user_id" });
    req.inviter_id = jsonStringValue(item, { "inviterId", "inviter_id" });
    req.message = jsonStringValue(item, { "message" });
    req.status = jsonStringValue(item, { "status" }, "pending");

    if (const auto* created = findField(item, { "createdAt", "created_at" }); created != nullptr) {
        req.created_at_ms = parseTimestampMs(*created);
    }

    const auto* user_info = findField(item, { "userInfo", "user_info" });
    if (user_info != nullptr && user_info->is_object()) {
        req.user_info = parseUserInfo(*user_info);
    }
    if (req.user_info.user_id.empty()) {
        req.user_info.user_id = req.user_id;
    }

    return req;
}

GroupQRCode parseGroupQRCode(const nlohmann::json& item) {
    GroupQRCode qrcode;
    if (!item.is_object()) {
        return qrcode;
    }

    qrcode.group_id = jsonStringValue(item, { "groupId", "group_id" });
    qrcode.token = jsonStringValue(item, { "token" });
    qrcode.deep_link = jsonStringValue(item, { "deepLink", "deep_link" });
    if (const auto* expire = findField(item, { "expireAt", "expire_at", "expiresAt", "expires_at" });
        expire != nullptr) {
        qrcode.expire_at_ms = parseTimestampMs(*expire);
    }

    return qrcode;
}

void completeBoolRequest(GroupCallback cb, network::HttpResponse resp) {
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

GroupManagerImpl::GroupManagerImpl(
    db::Database* db,
    NotificationManager* notif_mgr,
    std::shared_ptr<network::HttpClient> http
)
    : db_(db)
    , notif_mgr_(notif_mgr)
    , http_(std::move(http)) {
    if (notif_mgr_) {
        notif_mgr_->addNotificationHandler([this](const NotificationEvent& ev) {
            handleGroupNotification(ev);
        });
    }
}

// ---------------------------------------------------------------------------
// getList / getInfo
// ---------------------------------------------------------------------------

void GroupManagerImpl::getList(GroupListCallback cb) {
    http_->get("/groups", [cb = std::move(cb), this](network::HttpResponse resp) {
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

            std::vector<Group> groups;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "groups", "list", "items" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    Group g = parseGroup(item);
                    groups.push_back(g);

                    if (db_) {
                        db_->exec(
                            "INSERT OR REPLACE INTO groups"
                            " (group_id, name, avatar_url, owner_id,"
                            "  member_count, my_role, updated_at)"
                            " VALUES (?, ?, ?, ?, ?, ?, ?)",
                            { g.group_id,
                              g.name,
                              g.avatar_url,
                              g.owner_id,
                              static_cast<int64_t>(g.member_count),
                              roleToString(g.my_role),
                              g.updated_at_ms },
                            nullptr
                        );
                    }
                }
            }

            if (cb) {
                cb(std::move(groups), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

void GroupManagerImpl::getInfo(const std::string& group_id, GroupInfoCallback cb) {
    const std::string path = "/groups/" + group_id;
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

            Group group;
            const auto* data = findField(root, { "data" });
            if (data != nullptr && data->is_object()) {
                group = parseGroup(*data);
            } else {
                group = parseGroup(root);
            }

            if (cb) {
                cb(std::move(group), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

// ---------------------------------------------------------------------------
// create / join / invite / quit / disband
// ---------------------------------------------------------------------------

void GroupManagerImpl::create(const std::string& name, const std::vector<std::string>& member_ids, GroupCallback cb) {
    nlohmann::json body;
    body["name"] = name;
    body["memberIds"] = member_ids;

    http_->post("/groups", body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::join(const std::string& group_id, const std::string& message, GroupCallback cb) {
    nlohmann::json body;
    body["message"] = message;

    const std::string path = "/groups/" + group_id + "/join";
    http_->post(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::invite(const std::string& group_id, const std::vector<std::string>& user_ids, GroupCallback cb) {
    nlohmann::json body;
    body["userIds"] = user_ids;

    const std::string path = "/groups/" + group_id + "/members";
    http_->post(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::quit(const std::string& group_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id + "/quit";
    http_->post(path, "{}", [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::disband(const std::string& group_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// update / members
// ---------------------------------------------------------------------------

void GroupManagerImpl::update(
    const std::string& group_id,
    const std::string& name,
    const std::string& avatar_url,
    GroupCallback cb
) {
    nlohmann::json body;
    if (!name.empty()) {
        body["name"] = name;
    }
    if (!avatar_url.empty()) {
        body["avatar"] = avatar_url;
    }

    const std::string path = "/groups/" + group_id;
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::getMembers(const std::string& group_id, int page, int page_size, GroupMemberCallback cb) {
    const std::string path =
        "/groups/" + group_id + "/members" + "?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

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

            std::vector<GroupMember> members;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "members", "list", "items" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    members.push_back(parseGroupMember(item));
                }
            }

            if (cb) {
                cb(std::move(members), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

void GroupManagerImpl::removeMember(const std::string& group_id, const std::string& user_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id + "/members/" + user_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::updateMemberRole(
    const std::string& group_id,
    const std::string& user_id,
    GroupRole role,
    GroupCallback cb
) {
    nlohmann::json body;
    body["role"] = roleToString(role);

    const std::string path = "/groups/" + group_id + "/members/" + user_id + "/role";
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::updateNickname(const std::string& group_id, const std::string& nickname, GroupCallback cb) {
    nlohmann::json body;
    body["nickname"] = nickname;

    const std::string path = "/groups/" + group_id + "/nickname";
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::transferOwnership(const std::string& group_id, const std::string& new_owner_id, GroupCallback cb) {
    nlohmann::json body;
    body["newOwnerId"] = new_owner_id;

    const std::string path = "/groups/" + group_id + "/transfer";
    http_->post(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

// ---------------------------------------------------------------------------
// join requests / qrcode
// ---------------------------------------------------------------------------

void GroupManagerImpl::getJoinRequests(
    const std::string& group_id,
    const std::string& status,
    GroupJoinRequestListCallback cb
) {
    std::string path = "/groups/" + group_id + "/requests";
    if (!status.empty()) {
        path += "?status=" + status;
    }

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

            std::vector<GroupJoinRequest> requests;
            const auto* data = findField(root, { "data" });
            const auto* arr = data != nullptr ? pickArray(*data, { "requests", "list", "items" }) : nullptr;
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    requests.push_back(parseJoinRequest(item));
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

void GroupManagerImpl::handleJoinRequest(const std::string& group_id, int64_t request_id, bool accept, GroupCallback cb) {
    nlohmann::json body;
    body["accept"] = accept;

    const std::string path = "/groups/" + group_id + "/requests/" + std::to_string(request_id);
    http_->put(path, body.dump(), [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp));
    });
}

void GroupManagerImpl::getQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
    const std::string path = "/groups/" + group_id + "/qrcode";

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

            GroupQRCode qrcode;
            const auto* data = findField(root, { "data" });
            if (data != nullptr && data->is_object()) {
                qrcode = parseGroupQRCode(*data);
            } else {
                qrcode = parseGroupQRCode(root);
            }

            if (cb) {
                cb(std::move(qrcode), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

void GroupManagerImpl::refreshQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
    const std::string path = "/groups/" + group_id + "/qrcode/refresh";

    http_->post(path, "{}", [cb = std::move(cb)](network::HttpResponse resp) {
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

            GroupQRCode qrcode;
            const auto* data = findField(root, { "data" });
            if (data != nullptr && data->is_object()) {
                qrcode = parseGroupQRCode(*data);
            } else {
                qrcode = parseGroupQRCode(root);
            }

            if (cb) {
                cb(std::move(qrcode), "");
            }
        } catch (const std::exception& e) {
            if (cb) {
                cb({}, std::string("parse error: ") + e.what());
            }
        }
    });
}

// ---------------------------------------------------------------------------
// setListener
// ---------------------------------------------------------------------------

void GroupManagerImpl::setListener(std::shared_ptr<GroupListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

// ---------------------------------------------------------------------------
// handleGroupNotification
// ---------------------------------------------------------------------------

void GroupManagerImpl::handleGroupNotification(const NotificationEvent& event) {
    const std::string& type = event.notification_type;
    if (type.rfind("group.", 0) != 0) {
        return;
    }

    const auto parseEventGroup = [&event]() {
        Group g = parseGroup(event.data);
        if (!g.group_id.empty()) {
            return g;
        }
        if (const auto* nested = findField(event.data, { "group", "groupInfo", "group_info" });
            nested != nullptr && nested->is_object()) {
            return parseGroup(*nested);
        }
        return g;
    };

    if (type == "group.invited") {
        std::shared_ptr<GroupListener> listener;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            Group g = parseEventGroup();
            const std::string inviter_id =
                jsonStringValue(event.data, { "inviterId", "inviter_id", "inviterUserId", "inviter_user_id" });
            listener->onGroupInvited(g, inviter_id);
        }
        return;
    }

    if (type == "group.info_updated" || type == "group.member_joined" || type == "group.member_left"
        || type == "group.role_changed" || type == "group.muted" || type == "group.disbanded"
        || type == "group.member_muted" || type == "group.member_unmuted") {
        std::shared_ptr<GroupListener> listener;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            listener->onGroupUpdated(parseEventGroup());
        }
    }
}

} // namespace anychat
