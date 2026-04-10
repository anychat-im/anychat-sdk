#include "anychat_c/group_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
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

void groupToC(const anychat::Group& src, AnyChatGroup_C* dst) {
    anychat_strlcpy(dst->group_id, src.group_id.c_str(), sizeof(dst->group_id));
    anychat_strlcpy(dst->name, src.name.c_str(), sizeof(dst->name));
    anychat_strlcpy(dst->avatar_url, src.avatar_url.c_str(), sizeof(dst->avatar_url));
    anychat_strlcpy(dst->owner_id, src.owner_id.c_str(), sizeof(dst->owner_id));
    dst->member_count = src.member_count;
    dst->my_role = static_cast<int>(src.my_role);
    dst->join_verify = src.join_verify ? 1 : 0;
    dst->updated_at_ms = src.updated_at_ms;

    anychat_strlcpy(dst->display_name, src.display_name.c_str(), sizeof(dst->display_name));
    anychat_strlcpy(dst->announcement, src.announcement.c_str(), sizeof(dst->announcement));
    anychat_strlcpy(dst->description, src.description.c_str(), sizeof(dst->description));
    anychat_strlcpy(dst->group_remark, src.group_remark.c_str(), sizeof(dst->group_remark));
    dst->max_members = src.max_members;
    dst->is_muted = src.is_muted ? 1 : 0;
    dst->created_at_ms = src.created_at_ms;
}

void memberToC(const anychat::GroupMember& src, AnyChatGroupMember_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->group_nickname, src.group_nickname.c_str(), sizeof(dst->group_nickname));
    dst->role = static_cast<int>(src.role);
    dst->is_muted = src.is_muted ? 1 : 0;
    dst->muted_until_ms = src.muted_until_ms;
    dst->joined_at_ms = src.joined_at_ms;
    userInfoToC(src.user_info, &dst->user_info);
}

void joinRequestToC(const anychat::GroupJoinRequest& src, AnyChatGroupJoinRequest_C* dst) {
    dst->request_id = src.request_id;
    anychat_strlcpy(dst->group_id, src.group_id.c_str(), sizeof(dst->group_id));
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->inviter_id, src.inviter_id.c_str(), sizeof(dst->inviter_id));
    anychat_strlcpy(dst->message, src.message.c_str(), sizeof(dst->message));
    anychat_strlcpy(dst->status, src.status.c_str(), sizeof(dst->status));
    dst->created_at_ms = src.created_at_ms;
    userInfoToC(src.user_info, &dst->user_info);
}

void qrcodeToC(const anychat::GroupQRCode& src, AnyChatGroupQRCode_C* dst) {
    anychat_strlcpy(dst->group_id, src.group_id.c_str(), sizeof(dst->group_id));
    anychat_strlcpy(dst->token, src.token.c_str(), sizeof(dst->token));
    anychat_strlcpy(dst->deep_link, src.deep_link.c_str(), sizeof(dst->deep_link));
    dst->expire_at_ms = src.expire_at_ms;
}

anychat::GroupRole parseRoleArg(const char* role) {
    if (!role) {
        return anychat::GroupRole::Member;
    }

    std::string role_str(role);
    for (char& ch : role_str) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    if (role_str == "owner") {
        return anychat::GroupRole::Owner;
    }
    if (role_str == "admin") {
        return anychat::GroupRole::Admin;
    }
    return anychat::GroupRole::Member;
}

class CGroupListener final : public anychat::GroupListener {
public:
    explicit CGroupListener(const AnyChatGroupListener_C& listener)
        : listener_(listener) {}

    void onGroupInvited(const anychat::Group& group, const std::string& inviter_id) override {
        if (!listener_.on_group_invited) {
            return;
        }
        AnyChatGroup_C c_group{};
        groupToC(group, &c_group);
        listener_.on_group_invited(listener_.userdata, &c_group, inviter_id.c_str());
    }

    void onGroupUpdated(const anychat::Group& group) override {
        if (!listener_.on_group_updated) {
            return;
        }
        AnyChatGroup_C c_group{};
        groupToC(group, &c_group);
        listener_.on_group_updated(listener_.userdata, &c_group);
    }

private:
    AnyChatGroupListener_C listener_{};
};

} // namespace

extern "C" {

int anychat_group_get_list(AnyChatGroupHandle handle, void* userdata, AnyChatGroupListCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getList([userdata, callback](std::vector<anychat::Group> list, std::string err) {
        if (!callback)
            return;
        int count = static_cast<int>(list.size());
        AnyChatGroupList_C c_list{};
        c_list.count = count;
        c_list.items = count > 0 ? static_cast<AnyChatGroup_C*>(std::calloc(count, sizeof(AnyChatGroup_C))) : nullptr;
        for (int i = 0; i < count; ++i)
            groupToC(list[i], &c_list.items[i]);
        callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
        std::free(c_list.items);
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_get_info(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupInfoCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getInfo(group_id, [userdata, callback](anychat::Group group, std::string err) {
        if (!callback)
            return;
        AnyChatGroup_C c_group{};
        groupToC(group, &c_group);
        callback(userdata, &c_group, err.empty() ? nullptr : err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_create(
    AnyChatGroupHandle handle,
    const char* name,
    const char* const* member_ids,
    int member_count,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !name) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    std::vector<std::string> ids;
    if (member_ids) {
        for (int i = 0; i < member_count; ++i)
            if (member_ids[i])
                ids.emplace_back(member_ids[i]);
    }
    handle->impl->create(name, ids, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_join(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* message,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->join(group_id, message ? message : "", [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_invite(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* const* user_ids,
    int user_count,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    std::vector<std::string> ids;
    if (user_ids) {
        for (int i = 0; i < user_count; ++i)
            if (user_ids[i])
                ids.emplace_back(user_ids[i]);
    }
    handle->impl->invite(group_id, ids, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_quit(AnyChatGroupHandle handle, const char* group_id, void* userdata, AnyChatGroupCallback callback) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->quit(group_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_disband(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->disband(group_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_update(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* name,
    const char* avatar_url,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->update(
        group_id,
        name ? name : "",
        avatar_url ? avatar_url : "",
        [userdata, callback](bool ok, std::string err) {
            if (callback)
                callback(userdata, ok ? 1 : 0, err.c_str());
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_get_members(
    AnyChatGroupHandle handle,
    const char* group_id,
    int page,
    int page_size,
    void* userdata,
    AnyChatGroupMemberCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getMembers(
        group_id,
        page,
        page_size,
        [userdata, callback](std::vector<anychat::GroupMember> members, std::string err) {
            if (!callback)
                return;
            int count = static_cast<int>(members.size());
            AnyChatGroupMemberList_C c_list{};
            c_list.count = count;
            c_list.items = count > 0
                               ? static_cast<AnyChatGroupMember_C*>(std::calloc(count, sizeof(AnyChatGroupMember_C)))
                               : nullptr;
            for (int i = 0; i < count; ++i)
                memberToC(members[i], &c_list.items[i]);
            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_remove_member(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id || !user_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->removeMember(group_id, user_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_update_member_role(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    const char* role,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id || !user_id || !role) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->updateMemberRole(
        group_id,
        user_id,
        parseRoleArg(role),
        [userdata, callback](bool ok, std::string err) {
            if (callback)
                callback(userdata, ok ? 1 : 0, err.c_str());
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_update_nickname(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* nickname,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id || !nickname) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->updateNickname(group_id, nickname, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_transfer_ownership(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* new_owner_id,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id || !new_owner_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->transferOwnership(group_id, new_owner_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_get_join_requests(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* status,
    void* userdata,
    AnyChatGroupJoinRequestListCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const std::string status_arg = (status && status[0] != '\0') ? status : "";
    handle->impl->getJoinRequests(
        group_id,
        status_arg,
        [userdata, callback](std::vector<anychat::GroupJoinRequest> list, std::string err) {
            if (!callback)
                return;
            int count = static_cast<int>(list.size());
            AnyChatGroupJoinRequestList_C c_list{};
            c_list.count = count;
            c_list.items = count > 0
                               ? static_cast<AnyChatGroupJoinRequest_C*>(
                                     std::calloc(count, sizeof(AnyChatGroupJoinRequest_C))
                                 )
                               : nullptr;
            for (int i = 0; i < count; ++i)
                joinRequestToC(list[i], &c_list.items[i]);
            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_handle_join_request(
    AnyChatGroupHandle handle,
    const char* group_id,
    int64_t request_id,
    int accept,
    void* userdata,
    AnyChatGroupCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->handleJoinRequest(group_id, request_id, accept != 0, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_get_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupQRCodeCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getQRCode(group_id, [userdata, callback](anychat::GroupQRCode qrcode, std::string err) {
        if (!callback)
            return;
        AnyChatGroupQRCode_C c_qrcode{};
        qrcodeToC(qrcode, &c_qrcode);
        callback(userdata, &c_qrcode, err.empty() ? nullptr : err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_refresh_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    void* userdata,
    AnyChatGroupQRCodeCallback callback
) {
    if (!handle || !handle->impl || !group_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->refreshQRCode(group_id, [userdata, callback](anychat::GroupQRCode qrcode, std::string err) {
        if (!callback)
            return;
        AnyChatGroupQRCode_C c_qrcode{};
        qrcodeToC(qrcode, &c_qrcode);
        callback(userdata, &c_qrcode, err.empty() ? nullptr : err.c_str());
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_group_set_listener(AnyChatGroupHandle handle, const AnyChatGroupListener_C* listener) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        anychat_clear_last_error();
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatGroupListener_C)) {
        anychat_set_last_error("listener struct_size is too small");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatGroupListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CGroupListener>(copied));
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
