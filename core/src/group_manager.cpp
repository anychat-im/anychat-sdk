#include "group_manager.h"

#include "json_common.h"

#include <cstdlib>
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
using json_common::parseBoolValue;
using json_common::parseInt32Value;
using json_common::parseInt64Value;
using json_common::parseJsonObject;
using json_common::parseTimestampMs;
using json_common::pickList;
using json_common::toLowerCopy;
using json_common::writeJson;

struct CreateGroupBody {
    std::string name{};
    std::vector<std::string> member_ids{};
};

struct JoinGroupBody {
    std::string message{};
};

struct InviteGroupBody {
    std::vector<std::string> user_ids{};
};

struct UpdateGroupBody {
    std::optional<std::string> name{};
    std::optional<std::string> avatar{};
};

struct UpdateMemberRoleBody {
    std::string role{};
};

struct UpdateNicknameBody {
    std::string nickname{};
};

struct TransferOwnershipBody {
    std::string new_owner_id{};
};

struct HandleJoinRequestBody {
    bool accept = false;
};

using IntegerValue = std::variant<int64_t, double, std::string>;
using OptionalIntegerValue = std::optional<IntegerValue>;
using BooleanValue = std::variant<bool, int64_t, double, std::string>;
using OptionalBooleanValue = std::optional<BooleanValue>;
using RoleValue = std::variant<int64_t, double, std::string>;
using OptionalRoleValue = std::optional<RoleValue>;

struct UserInfoPayload {
    std::string user_id{};
    std::string id{};
    std::string username{};
    std::string nickname{};
    std::string name{};
    std::string avatar_url{};
    std::string avatar{};
    std::string signature{};
    std::string bio{};
    OptionalIntegerValue gender{};
    std::string region{};
    OptionalBooleanValue is_friend{};
    OptionalBooleanValue is_blocked{};
};

struct GroupPayload {
    std::string group_id{};
    std::string id{};
    std::string name{};
    std::string group_name{};
    std::string display_name{};
    std::string avatar_url{};
    std::string avatar{};
    std::string group_avatar{};
    std::string announcement{};
    std::string description{};
    std::string group_remark{};
    std::string remark{};
    std::string owner_id{};
    OptionalIntegerValue member_count{};
    OptionalIntegerValue max_members{};
    OptionalRoleValue my_role{};
    OptionalRoleValue role{};
    OptionalBooleanValue join_verify{};
    OptionalBooleanValue need_verify{};
    OptionalBooleanValue is_muted{};
    json_common::OptionalTimestampValue created_at{};
    json_common::OptionalTimestampValue updated_at{};
    json_common::OptionalTimestampValue update_time{};
};

struct GroupInfoDataPayload {
    std::optional<GroupPayload> group{};
};

struct GroupListDataPayload {
    std::optional<std::vector<GroupPayload>> groups{};
    std::optional<std::vector<GroupPayload>> list{};
    std::optional<std::vector<GroupPayload>> items{};
};

using GroupListDataValue = std::variant<std::monostate, GroupListDataPayload, std::vector<GroupPayload>>;

struct GroupMemberPayload {
    std::string user_id{};
    std::string id{};
    std::string group_nickname{};
    std::string nickname{};
    OptionalRoleValue role{};
    OptionalBooleanValue is_muted{};
    json_common::OptionalTimestampValue muted_until{};
    json_common::OptionalTimestampValue joined_at{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<UserInfoPayload> user_info{};

    std::string username{};
    std::string name{};
    std::string avatar_url{};
    std::string avatar{};
    std::string signature{};
    std::string bio{};
    OptionalIntegerValue gender{};
    std::string region{};
    OptionalBooleanValue is_friend{};
    OptionalBooleanValue is_blocked{};
};

struct GroupMemberListDataPayload {
    std::optional<std::vector<GroupMemberPayload>> members{};
    std::optional<std::vector<GroupMemberPayload>> list{};
    std::optional<std::vector<GroupMemberPayload>> items{};
};

using GroupMemberListDataValue = std::variant<std::monostate, GroupMemberListDataPayload, std::vector<GroupMemberPayload>>;

struct GroupJoinRequestPayload {
    int64_t request_id = 0;
    int64_t id = 0;
    std::string group_id{};
    std::string user_id{};
    std::string inviter_id{};
    std::string message{};
    std::string status{};
    json_common::OptionalTimestampValue created_at{};
    std::optional<UserInfoPayload> user_info{};
};

struct GroupJoinRequestListDataPayload {
    std::optional<std::vector<GroupJoinRequestPayload>> requests{};
    std::optional<std::vector<GroupJoinRequestPayload>> list{};
    std::optional<std::vector<GroupJoinRequestPayload>> items{};
};

using GroupJoinRequestListDataValue
    = std::variant<std::monostate, GroupJoinRequestListDataPayload, std::vector<GroupJoinRequestPayload>>;

struct GroupQRCodePayload {
    std::string group_id{};
    std::string token{};
    std::string deep_link{};
    json_common::OptionalTimestampValue expire_at{};
    json_common::OptionalTimestampValue expires_at{};
};

struct GroupQRCodeDataPayload {
    std::optional<GroupQRCodePayload> qrcode{};
};

struct NotificationGroupPayload {
    std::string group_id{};
    std::string name{};
    std::string group_name{};
    std::string display_name{};
    std::string avatar_url{};
    std::string avatar{};
    std::string group_avatar{};
    std::string announcement{};
    std::string description{};
    std::string group_remark{};
    std::string owner_id{};
    int32_t member_count = 0;
    int32_t max_members = 0;
    std::string my_role{};
    std::string role{};
    bool join_verify = false;
    bool is_muted = false;
    json_common::OptionalTimestampValue created_at{};
    json_common::OptionalTimestampValue updated_at{};
};

struct NotificationGroupEventPayload {
    std::string group_id{};
    std::string name{};
    std::string group_name{};
    std::string display_name{};
    std::string avatar_url{};
    std::string avatar{};
    std::string group_avatar{};
    std::string announcement{};
    std::string description{};
    std::string group_remark{};
    std::string owner_id{};
    int32_t member_count = 0;
    int32_t max_members = 0;
    std::string my_role{};
    std::string role{};
    bool join_verify = false;
    bool is_muted = false;
    json_common::OptionalTimestampValue created_at{};
    json_common::OptionalTimestampValue updated_at{};
    std::optional<NotificationGroupPayload> group{};
    std::string inviter_user_id{};
    std::string inviter_id{};
};

GroupRole parseRoleString(const std::string& role_str) {
    const std::string lowered = toLowerCopy(role_str);
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

GroupRole parseRoleValue(const RoleValue& value, GroupRole default_value = GroupRole::Member) {
    if (const auto* int_value = std::get_if<int64_t>(&value); int_value != nullptr) {
        if (*int_value == 0) {
            return GroupRole::Owner;
        }
        if (*int_value == 1) {
            return GroupRole::Admin;
        }
        return GroupRole::Member;
    }
    if (const auto* dbl_value = std::get_if<double>(&value); dbl_value != nullptr) {
        const int64_t int_value = static_cast<int64_t>(*dbl_value);
        if (int_value == 0) {
            return GroupRole::Owner;
        }
        if (int_value == 1) {
            return GroupRole::Admin;
        }
        return GroupRole::Member;
    }
    if (const auto* str_value = std::get_if<std::string>(&value); str_value != nullptr) {
        if (str_value->empty()) {
            return default_value;
        }

        char* end = nullptr;
        const int64_t parsed = std::strtoll(str_value->c_str(), &end, 10);
        if (end != nullptr && *end == '\0') {
            return parseRoleValue(parsed, default_value);
        }

        return parseRoleString(*str_value);
    }
    return default_value;
}

GroupRole
parseRoleValue(const OptionalRoleValue& primary, const OptionalRoleValue& secondary, GroupRole default_value) {
    if (primary.has_value()) {
        return parseRoleValue(*primary, default_value);
    }
    if (secondary.has_value()) {
        return parseRoleValue(*secondary, default_value);
    }
    return default_value;
}

UserInfo toUserInfo(const UserInfoPayload& payload) {
    UserInfo info;
    info.user_id = payload.user_id.empty() ? payload.id : payload.user_id;
    if (!payload.nickname.empty()) {
        info.username = payload.nickname;
    } else if (!payload.username.empty()) {
        info.username = payload.username;
    } else {
        info.username = payload.name;
    }
    info.avatar_url = payload.avatar_url.empty() ? payload.avatar : payload.avatar_url;
    info.signature = payload.signature.empty() ? payload.bio : payload.signature;
    info.gender = parseInt32Value(payload.gender, 0);
    info.region = payload.region;
    info.is_friend = parseBoolValue(payload.is_friend, false);
    info.is_blocked = parseBoolValue(payload.is_blocked, false);
    return info;
}

UserInfo toUserInfo(const GroupMemberPayload& payload) {
    UserInfoPayload user_info{
        .user_id = payload.user_id,
        .id = payload.id,
        .username = payload.username,
        .nickname = payload.nickname,
        .name = payload.name,
        .avatar_url = payload.avatar_url,
        .avatar = payload.avatar,
        .signature = payload.signature,
        .bio = payload.bio,
        .gender = payload.gender,
        .region = payload.region,
        .is_friend = payload.is_friend,
        .is_blocked = payload.is_blocked,
    };
    return toUserInfo(user_info);
}

Group parseGroup(const GroupPayload& payload) {
    Group g;
    g.group_id = payload.group_id.empty() ? payload.id : payload.group_id;
    if (!payload.name.empty()) {
        g.name = payload.name;
    } else if (!payload.group_name.empty()) {
        g.name = payload.group_name;
    } else {
        g.name = payload.display_name;
    }
    g.display_name = payload.display_name.empty() ? g.name : payload.display_name;
    if (!payload.avatar_url.empty()) {
        g.avatar_url = payload.avatar_url;
    } else if (!payload.avatar.empty()) {
        g.avatar_url = payload.avatar;
    } else {
        g.avatar_url = payload.group_avatar;
    }
    g.announcement = payload.announcement;
    g.description = payload.description;
    g.group_remark = payload.group_remark.empty() ? payload.remark : payload.group_remark;
    g.owner_id = payload.owner_id;
    g.member_count = parseInt32Value(payload.member_count, 0);
    g.max_members = parseInt32Value(payload.max_members, 0);
    g.my_role = parseRoleValue(payload.my_role, payload.role, GroupRole::Member);
    g.join_verify = payload.join_verify.has_value() ? parseBoolValue(payload.join_verify, false)
                                                    : parseBoolValue(payload.need_verify, false);
    g.is_muted = parseBoolValue(payload.is_muted, false);
    g.created_at_ms = parseTimestampMs(payload.created_at);
    g.updated_at_ms = payload.updated_at.has_value() ? parseTimestampMs(payload.updated_at)
                                                     : parseTimestampMs(payload.update_time);
    return g;
}

GroupMember parseGroupMember(const GroupMemberPayload& payload) {
    GroupMember m;
    m.user_id = payload.user_id.empty() ? payload.id : payload.user_id;
    m.group_nickname = payload.group_nickname.empty() ? payload.nickname : payload.group_nickname;
    m.role = parseRoleValue(payload.role, std::nullopt, GroupRole::Member);
    m.is_muted = parseBoolValue(payload.is_muted, false);
    m.muted_until_ms = parseTimestampMs(payload.muted_until);
    m.joined_at_ms = payload.joined_at.has_value() ? parseTimestampMs(payload.joined_at) : parseTimestampMs(payload.created_at);

    m.user_info = payload.user_info.has_value() ? toUserInfo(*payload.user_info) : toUserInfo(payload);

    if (m.user_info.user_id.empty()) {
        m.user_info.user_id = m.user_id;
    }
    return m;
}

GroupJoinRequest parseJoinRequest(const GroupJoinRequestPayload& payload) {
    GroupJoinRequest req;
    req.request_id = payload.request_id != 0 ? payload.request_id : payload.id;
    req.group_id = payload.group_id;
    req.user_id = payload.user_id;
    req.inviter_id = payload.inviter_id;
    req.message = payload.message;
    req.status = payload.status.empty() ? "pending" : payload.status;
    req.created_at_ms = parseTimestampMs(payload.created_at);

    if (payload.user_info.has_value()) {
        req.user_info = toUserInfo(*payload.user_info);
    }
    if (req.user_info.user_id.empty()) {
        req.user_info.user_id = req.user_id;
    }
    return req;
}

GroupQRCode parseGroupQRCode(const GroupQRCodePayload& payload) {
    GroupQRCode qrcode;
    qrcode.group_id = payload.group_id;
    qrcode.token = payload.token;
    qrcode.deep_link = payload.deep_link;
    qrcode.expire_at_ms = payload.expire_at.has_value() ? parseTimestampMs(payload.expire_at)
                                                        : parseTimestampMs(payload.expires_at);
    return qrcode;
}

const std::vector<GroupPayload>* toGroupPayloadList(const GroupListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<GroupPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<GroupListDataPayload>(&data); object != nullptr) {
        return pickList(object->groups, object->list, object->items);
    }
    return nullptr;
}

const std::vector<GroupMemberPayload>* toGroupMemberPayloadList(const GroupMemberListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<GroupMemberPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<GroupMemberListDataPayload>(&data); object != nullptr) {
        return pickList(object->members, object->list, object->items);
    }
    return nullptr;
}

const std::vector<GroupJoinRequestPayload>* toGroupJoinRequestPayloadList(const GroupJoinRequestListDataValue& data) {
    if (const auto* list = std::get_if<std::vector<GroupJoinRequestPayload>>(&data); list != nullptr) {
        return list;
    }
    if (const auto* object = std::get_if<GroupJoinRequestListDataPayload>(&data); object != nullptr) {
        return pickList(object->requests, object->list, object->items);
    }
    return nullptr;
}

NotificationGroupPayload toGroupPayload(const NotificationGroupEventPayload& payload) {
    return NotificationGroupPayload{
        .group_id = payload.group_id,
        .name = payload.name,
        .group_name = payload.group_name,
        .display_name = payload.display_name,
        .avatar_url = payload.avatar_url,
        .avatar = payload.avatar,
        .group_avatar = payload.group_avatar,
        .announcement = payload.announcement,
        .description = payload.description,
        .group_remark = payload.group_remark,
        .owner_id = payload.owner_id,
        .member_count = payload.member_count,
        .max_members = payload.max_members,
        .my_role = payload.my_role,
        .role = payload.role,
        .join_verify = payload.join_verify,
        .is_muted = payload.is_muted,
        .created_at = payload.created_at,
        .updated_at = payload.updated_at,
    };
}

GroupRole parseNotificationRole(const NotificationGroupPayload& payload, GroupRole fallback = GroupRole::Member) {
    if (!payload.my_role.empty()) {
        return parseRoleString(payload.my_role);
    }
    if (!payload.role.empty()) {
        return parseRoleString(payload.role);
    }
    return fallback;
}

Group parseNotificationGroup(const NotificationGroupPayload& payload) {
    Group group;
    group.group_id = payload.group_id;
    group.name = payload.name.empty() ? payload.group_name : payload.name;
    group.display_name = payload.display_name.empty() ? group.name : payload.display_name;
    group.avatar_url = payload.avatar_url;
    if (group.avatar_url.empty()) {
        group.avatar_url = payload.group_avatar;
    }
    if (group.avatar_url.empty()) {
        group.avatar_url = payload.avatar;
    }
    group.announcement = payload.announcement;
    group.description = payload.description;
    group.group_remark = payload.group_remark;
    group.owner_id = payload.owner_id;
    group.member_count = payload.member_count;
    group.max_members = payload.max_members;
    group.my_role = parseNotificationRole(payload);
    group.join_verify = payload.join_verify;
    group.is_muted = payload.is_muted;
    group.created_at_ms = parseTimestampMs(payload.created_at);
    group.updated_at_ms = parseTimestampMs(payload.updated_at);
    return group;
}

void completeBoolRequest(GroupCallback cb, network::HttpResponse resp, const std::string& fallback_error) {
    if (!cb) {
        return;
    }

    std::string err;
    const bool ok = parseApiStatusSuccessResponse(resp, err, fallback_error);
    cb(ok, ok ? "" : err);
}

} // namespace

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

void GroupManagerImpl::getList(GroupListCallback cb) {
    http_->get("/groups", [cb = std::move(cb), this](network::HttpResponse resp) {
        ApiEnvelope<GroupListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<Group> groups;
        const auto* payloads = toGroupPayloadList(root.data);
        if (payloads != nullptr) {
            groups.reserve(payloads->size());
            for (const auto& item : *payloads) {
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
    });
}

void GroupManagerImpl::getInfo(const std::string& group_id, GroupInfoCallback cb) {
    const std::string path = "/groups/" + group_id;
    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<GroupInfoDataPayload> wrapped{};
        std::string err;
        if (parseApiEnvelopeResponse(resp, wrapped, err) && wrapped.data.group.has_value()) {
            if (cb) {
                cb(parseGroup(*wrapped.data.group), "");
            }
            return;
        }

        ApiEnvelope<GroupPayload> root{};
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        if (cb) {
            cb(parseGroup(root.data), "");
        }
    });
}

void GroupManagerImpl::create(const std::string& name, const std::vector<std::string>& member_ids, GroupCallback cb) {
    const CreateGroupBody body{.name = name, .member_ids = member_ids};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    http_->post("/groups", body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "create group failed");
    });
}

void GroupManagerImpl::join(const std::string& group_id, const std::string& message, GroupCallback cb) {
    const JoinGroupBody body{.message = message};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/join";
    http_->post(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "join group failed");
    });
}

void GroupManagerImpl::invite(const std::string& group_id, const std::vector<std::string>& user_ids, GroupCallback cb) {
    const InviteGroupBody body{.user_ids = user_ids};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/members";
    http_->post(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "invite group member failed");
    });
}

void GroupManagerImpl::quit(const std::string& group_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id + "/quit";
    http_->post(path, "{}", [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "quit group failed");
    });
}

void GroupManagerImpl::disband(const std::string& group_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "disband group failed");
    });
}

void GroupManagerImpl::update(
    const std::string& group_id,
    const std::string& name,
    const std::string& avatar_url,
    GroupCallback cb
) {
    UpdateGroupBody body{};
    if (!name.empty()) {
        body.name = name;
    }
    if (!avatar_url.empty()) {
        body.avatar = avatar_url;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id;
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "update group failed");
    });
}

void GroupManagerImpl::getMembers(const std::string& group_id, int page, int page_size, GroupMemberCallback cb) {
    const std::string path =
        "/groups/" + group_id + "/members" + "?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<GroupMemberListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<GroupMember> members;
        const auto* payloads = toGroupMemberPayloadList(root.data);
        if (payloads != nullptr) {
            members.reserve(payloads->size());
            for (const auto& item : *payloads) {
                members.push_back(parseGroupMember(item));
            }
        }

        if (cb) {
            cb(std::move(members), "");
        }
    });
}

void GroupManagerImpl::removeMember(const std::string& group_id, const std::string& user_id, GroupCallback cb) {
    const std::string path = "/groups/" + group_id + "/members/" + user_id;
    http_->del(path, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "remove group member failed");
    });
}

void GroupManagerImpl::updateMemberRole(
    const std::string& group_id,
    const std::string& user_id,
    GroupRole role,
    GroupCallback cb
) {
    const UpdateMemberRoleBody body{.role = roleToString(role)};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/members/" + user_id + "/role";
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "update group member role failed");
    });
}

void GroupManagerImpl::updateNickname(const std::string& group_id, const std::string& nickname, GroupCallback cb) {
    const UpdateNicknameBody body{.nickname = nickname};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/nickname";
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "update group nickname failed");
    });
}

void GroupManagerImpl::transferOwnership(const std::string& group_id, const std::string& new_owner_id, GroupCallback cb) {
    const TransferOwnershipBody body{.new_owner_id = new_owner_id};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/transfer";
    http_->post(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "transfer ownership failed");
    });
}

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
        ApiEnvelope<GroupJoinRequestListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        std::vector<GroupJoinRequest> requests;
        const auto* payloads = toGroupJoinRequestPayloadList(root.data);
        if (payloads != nullptr) {
            requests.reserve(payloads->size());
            for (const auto& item : *payloads) {
                requests.push_back(parseJoinRequest(item));
            }
        }

        if (cb) {
            cb(std::move(requests), "");
        }
    });
}

void GroupManagerImpl::handleJoinRequest(const std::string& group_id, int64_t request_id, bool accept, GroupCallback cb) {
    const HandleJoinRequestBody body{.accept = accept};

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        if (cb) {
            cb(false, err);
        }
        return;
    }

    const std::string path = "/groups/" + group_id + "/requests/" + std::to_string(request_id);
    http_->put(path, body_json, [cb = std::move(cb)](network::HttpResponse resp) {
        completeBoolRequest(std::move(cb), std::move(resp), "handle join request failed");
    });
}

void GroupManagerImpl::getQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
    const std::string path = "/groups/" + group_id + "/qrcode";

    http_->get(path, [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<GroupQRCodeDataPayload> wrapped{};
        std::string err;
        if (parseApiEnvelopeResponse(resp, wrapped, err) && wrapped.data.qrcode.has_value()) {
            if (cb) {
                cb(parseGroupQRCode(*wrapped.data.qrcode), "");
            }
            return;
        }

        ApiEnvelope<GroupQRCodePayload> root{};
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        if (cb) {
            cb(parseGroupQRCode(root.data), "");
        }
    });
}

void GroupManagerImpl::refreshQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
    const std::string path = "/groups/" + group_id + "/qrcode/refresh";

    http_->post(path, "{}", [cb = std::move(cb)](network::HttpResponse resp) {
        ApiEnvelope<GroupQRCodeDataPayload> wrapped{};
        std::string err;
        if (parseApiEnvelopeResponse(resp, wrapped, err) && wrapped.data.qrcode.has_value()) {
            if (cb) {
                cb(parseGroupQRCode(*wrapped.data.qrcode), "");
            }
            return;
        }

        ApiEnvelope<GroupQRCodePayload> root{};
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            if (cb) {
                cb({}, err);
            }
            return;
        }

        if (cb) {
            cb(parseGroupQRCode(root.data), "");
        }
    });
}

void GroupManagerImpl::setListener(std::shared_ptr<GroupListener> listener) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    listener_ = std::move(listener);
}

void GroupManagerImpl::handleGroupNotification(const NotificationEvent& event) {
    const std::string& type = event.notification_type;
    if (type.rfind("group.", 0) != 0) {
        return;
    }

    NotificationGroupEventPayload payload{};
    std::string err;
    if (!parseJsonObject(event.data, payload, err)) {
        return;
    }

    const auto parseEventGroup = [&payload]() {
        Group g = parseNotificationGroup(toGroupPayload(payload));
        if (!g.group_id.empty()) {
            return g;
        }
        if (payload.group.has_value()) {
            return parseNotificationGroup(*payload.group);
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
            const std::string inviter_id = payload.inviter_user_id.empty() ? payload.inviter_id : payload.inviter_user_id;
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
