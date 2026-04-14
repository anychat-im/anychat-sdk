#include "anychat/group.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cctype>
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

template <typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback && callback->struct_size < sizeof(CallbackStruct)) {
        return false;
    }
    return true;
}

template <typename CallbackStruct>
CallbackStruct copyCallbackStruct(const CallbackStruct* callback) {
    CallbackStruct callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }
    return callback_copy;
}

template <typename CallbackStruct>
void invokeGroupError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeGroupCallback(const AnyChatGroupCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeGroupError(callback, code, error);
    };
    return result;
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

int anychat_group_get_list(AnyChatGroupHandle handle, const AnyChatGroupListCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupListCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getGroupList(
        anychat::AnyChatValueCallback<std::vector<anychat::Group>>{
            .on_success =
                [callback_copy](const std::vector<anychat::Group>& list) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    const int count = static_cast<int>(list.size());
                    AnyChatGroupList_C c_list{};
                    c_list.count = count;
                    c_list.items =
                        count > 0 ? static_cast<AnyChatGroup_C*>(std::calloc(count, sizeof(AnyChatGroup_C))) : nullptr;

                    for (int i = 0; i < count; ++i) {
                        groupToC(list[static_cast<size_t>(i)], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_get_info(
    AnyChatGroupHandle handle,
    const char* group_id,
    const AnyChatGroupInfoCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupInfoCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getInfo(
        group_id,
        anychat::AnyChatValueCallback<anychat::Group>{
            .on_success =
                [callback_copy](const anychat::Group& group) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatGroup_C c_group{};
                    groupToC(group, &c_group);
                    callback_copy.on_success(callback_copy.userdata, &c_group);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_create(
    AnyChatGroupHandle handle,
    const char* name,
    const char* const* member_ids,
    int member_count,
    const AnyChatGroupInfoCallback_C* callback
) {
    if (!handle || !handle->impl || !name) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    std::vector<std::string> ids;
    if (member_ids) {
        for (int i = 0; i < member_count; ++i) {
            if (member_ids[i]) {
                ids.emplace_back(member_ids[i]);
            }
        }
    }

    const AnyChatGroupInfoCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->create(
        name,
        ids,
        anychat::AnyChatValueCallback<anychat::Group>{
            .on_success =
                [callback_copy](const anychat::Group& group) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatGroup_C c_group{};
                    groupToC(group, &c_group);
                    callback_copy.on_success(callback_copy.userdata, &c_group);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_join(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* message,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->join(group_id, message ? message : "", makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_invite(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* const* user_ids,
    int user_count,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    std::vector<std::string> ids;
    if (user_ids) {
        for (int i = 0; i < user_count; ++i) {
            if (user_ids[i]) {
                ids.emplace_back(user_ids[i]);
            }
        }
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->invite(group_id, ids, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_quit(AnyChatGroupHandle handle, const char* group_id, const AnyChatGroupCallback_C* callback) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->quit(group_id, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_disband(AnyChatGroupHandle handle, const char* group_id, const AnyChatGroupCallback_C* callback) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->disband(group_id, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_update(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* name,
    const char* avatar_url,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->update(group_id, name ? name : "", avatar_url ? avatar_url : "", makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_get_members(
    AnyChatGroupHandle handle,
    const char* group_id,
    int page,
    int page_size,
    const AnyChatGroupMemberListCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupMemberListCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getMembers(
        group_id,
        page,
        page_size,
        anychat::AnyChatValueCallback<std::vector<anychat::GroupMember>>{
            .on_success =
                [callback_copy](const std::vector<anychat::GroupMember>& members) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    const int count = static_cast<int>(members.size());
                    AnyChatGroupMemberList_C c_list{};
                    c_list.count = count;
                    c_list.items = count > 0
                        ? static_cast<AnyChatGroupMember_C*>(std::calloc(count, sizeof(AnyChatGroupMember_C)))
                        : nullptr;

                    for (int i = 0; i < count; ++i) {
                        memberToC(members[static_cast<size_t>(i)], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_remove_member(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id || !user_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->removeMember(group_id, user_id, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_update_member_role(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* user_id,
    const char* role,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id || !user_id || !role) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->updateMemberRole(group_id, user_id, parseRoleArg(role), makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_update_nickname(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* nickname,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id || !nickname) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->updateNickname(group_id, nickname, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_transfer_ownership(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* new_owner_id,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id || !new_owner_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->transferOwnership(group_id, new_owner_id, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_get_join_requests(
    AnyChatGroupHandle handle,
    const char* group_id,
    const char* status,
    const AnyChatGroupJoinRequestListCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const std::string status_arg = (status && status[0] != '\0') ? status : "";
    const AnyChatGroupJoinRequestListCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getJoinRequests(
        group_id,
        status_arg,
        anychat::AnyChatValueCallback<std::vector<anychat::GroupJoinRequest>>{
            .on_success =
                [callback_copy](const std::vector<anychat::GroupJoinRequest>& list) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    const int count = static_cast<int>(list.size());
                    AnyChatGroupJoinRequestList_C c_list{};
                    c_list.count = count;
                    c_list.items = count > 0
                        ? static_cast<AnyChatGroupJoinRequest_C*>(std::calloc(count, sizeof(AnyChatGroupJoinRequest_C)))
                        : nullptr;

                    for (int i = 0; i < count; ++i) {
                        joinRequestToC(list[static_cast<size_t>(i)], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_handle_join_request(
    AnyChatGroupHandle handle,
    const char* group_id,
    int64_t request_id,
    int accept,
    const AnyChatGroupCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->handleJoinRequest(group_id, request_id, accept != 0, makeGroupCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_group_get_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    const AnyChatGroupQRCodeCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupQRCodeCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getQRCode(
        group_id,
        anychat::AnyChatValueCallback<anychat::GroupQRCode>{
            .on_success =
                [callback_copy](const anychat::GroupQRCode& qrcode) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatGroupQRCode_C c_qrcode{};
                    qrcodeToC(qrcode, &c_qrcode);
                    callback_copy.on_success(callback_copy.userdata, &c_qrcode);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_refresh_qrcode(
    AnyChatGroupHandle handle,
    const char* group_id,
    const AnyChatGroupQRCodeCallback_C* callback
) {
    if (!handle || !handle->impl || !group_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatGroupQRCodeCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->refreshQRCode(
        group_id,
        anychat::AnyChatValueCallback<anychat::GroupQRCode>{
            .on_success =
                [callback_copy](const anychat::GroupQRCode& qrcode) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatGroupQRCode_C c_qrcode{};
                    qrcodeToC(qrcode, &c_qrcode);
                    callback_copy.on_success(callback_copy.userdata, &c_qrcode);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeGroupError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_group_set_listener(AnyChatGroupHandle handle, const AnyChatGroupListener_C* listener) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatGroupListener_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatGroupListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CGroupListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
