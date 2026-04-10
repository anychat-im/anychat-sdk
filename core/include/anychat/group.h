#pragma once

#include "types.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace anychat {

using GroupListCallback = std::function<void(std::vector<Group> list, std::string err)>;
using GroupCallback = std::function<void(bool ok, std::string err)>;
using GroupInfoCallback = std::function<void(Group group, std::string err)>;
using GroupMemberCallback = std::function<void(std::vector<GroupMember> members, std::string err)>;
using GroupJoinRequestListCallback = std::function<void(std::vector<GroupJoinRequest> list, std::string err)>;
using GroupQRCodeCallback = std::function<void(GroupQRCode qrcode, std::string err)>;

class GroupListener {
public:
    virtual ~GroupListener() = default;

    virtual void onGroupInvited(const Group& group, const std::string& inviter_id) {
        (void) group;
        (void) inviter_id;
    }

    virtual void onGroupUpdated(const Group& group) {
        (void) group;
    }
};

class GroupManager {
public:
    virtual ~GroupManager() = default;

    virtual void getList(GroupListCallback cb) = 0;

    virtual void getInfo(const std::string& group_id, GroupInfoCallback cb) {
        if (cb) {
            cb({}, "unsupported operation");
        }
    }

    virtual void create(const std::string& name, const std::vector<std::string>& member_ids, GroupCallback cb) = 0;

    virtual void join(const std::string& group_id, const std::string& message, GroupCallback cb) = 0;

    virtual void invite(const std::string& group_id, const std::vector<std::string>& user_ids, GroupCallback cb) = 0;

    virtual void quit(const std::string& group_id, GroupCallback cb) = 0;

    virtual void disband(const std::string& group_id, GroupCallback cb) {
        (void) group_id;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void
    update(const std::string& group_id, const std::string& name, const std::string& avatar_url, GroupCallback cb) = 0;

    virtual void getMembers(const std::string& group_id, int page, int page_size, GroupMemberCallback cb) = 0;

    virtual void removeMember(const std::string& group_id, const std::string& user_id, GroupCallback cb) {
        (void) group_id;
        (void) user_id;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void
    updateMemberRole(const std::string& group_id, const std::string& user_id, GroupRole role, GroupCallback cb) {
        (void) group_id;
        (void) user_id;
        (void) role;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void updateNickname(const std::string& group_id, const std::string& nickname, GroupCallback cb) {
        (void) group_id;
        (void) nickname;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void transferOwnership(const std::string& group_id, const std::string& new_owner_id, GroupCallback cb) {
        (void) group_id;
        (void) new_owner_id;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void getJoinRequests(const std::string& group_id, const std::string& status, GroupJoinRequestListCallback cb)
    {
        (void) group_id;
        (void) status;
        if (cb) {
            cb({}, "unsupported operation");
        }
    }

    virtual void handleJoinRequest(const std::string& group_id, int64_t request_id, bool accept, GroupCallback cb) {
        (void) group_id;
        (void) request_id;
        (void) accept;
        if (cb) {
            cb(false, "unsupported operation");
        }
    }

    virtual void getQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
        (void) group_id;
        if (cb) {
            cb({}, "unsupported operation");
        }
    }

    virtual void refreshQRCode(const std::string& group_id, GroupQRCodeCallback cb) {
        (void) group_id;
        if (cb) {
            cb({}, "unsupported operation");
        }
    }

    virtual void setListener(std::shared_ptr<GroupListener> listener) = 0;
};

} // namespace anychat
