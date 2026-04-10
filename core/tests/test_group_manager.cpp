#include "group_manager.h"
#include "notification_manager.h"

#include "db/database.h"
#include "network/http_client.h"

#include <memory>
#include <string>
#include <functional>

#include <gtest/gtest.h>

class GroupManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_unique<anychat::db::Database>(":memory:");
        ASSERT_TRUE(db_->open());
        notif_mgr_ = std::make_unique<anychat::NotificationManager>();
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");
        mgr_ = std::make_unique<anychat::GroupManagerImpl>(db_.get(), notif_mgr_.get(), http_);
    }

    void TearDown() override {
        mgr_.reset();
        http_.reset();
        notif_mgr_.reset();
        db_->close();
        db_.reset();
    }

    std::unique_ptr<anychat::db::Database> db_;
    std::unique_ptr<anychat::NotificationManager> notif_mgr_;
    std::shared_ptr<anychat::network::HttpClient> http_;
    std::unique_ptr<anychat::GroupManagerImpl> mgr_;
};

class TestGroupListener final : public anychat::GroupListener {
public:
    std::function<void(const anychat::Group&, const std::string&)> on_invited;
    std::function<void(const anychat::Group&)> on_updated;

    void onGroupInvited(const anychat::Group& group, const std::string& inviter_id) override {
        if (on_invited) {
            on_invited(group, inviter_id);
        }
    }

    void onGroupUpdated(const anychat::Group& group) override {
        if (on_updated) {
            on_updated(group);
        }
    }
};

TEST_F(GroupManagerTest, GroupInvitedNotificationFiresHandler) {
    anychat::Group received{};
    std::string inviter_id;
    int call_count = 0;

    auto listener = std::make_shared<TestGroupListener>();
    listener->on_invited = [&](const anychat::Group& g, const std::string& inviter) {
        received = g;
        inviter_id = inviter;
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-group-invite-001",
            "type": "group.invited",
            "timestamp": 1708329600,
            "payload": {
                "group_id": "grp-001",
                "group_name": "项目讨论组",
                "inviter_user_id": "user-inviter-111"
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(received.group_id, "grp-001");
    EXPECT_EQ(received.name, "项目讨论组");
    EXPECT_EQ(inviter_id, "user-inviter-111");
}

TEST_F(GroupManagerTest, GroupInfoUpdatedNotificationFiresUpdatedHandler) {
    anychat::Group updated{};
    int call_count = 0;
    auto listener = std::make_shared<TestGroupListener>();
    listener->on_updated = [&](const anychat::Group& g) {
        updated = g;
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-group-update-001",
            "type": "group.info_updated",
            "timestamp": 1708329601,
            "payload": {
                "group_id": "grp-002",
                "group_name": "新名称",
                "group_avatar": "https://cdn.example/avatar.png",
                "updated_fields": ["name", "avatar"]
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(updated.group_id, "grp-002");
    EXPECT_EQ(updated.name, "新名称");
    EXPECT_EQ(updated.avatar_url, "https://cdn.example/avatar.png");
}

TEST_F(GroupManagerTest, UnrelatedNotificationDoesNotFireHandlers) {
    int invited_count = 0;
    int updated_count = 0;
    auto listener = std::make_shared<TestGroupListener>();
    listener->on_invited = [&](const anychat::Group&, const std::string&) {
        ++invited_count;
    };
    listener->on_updated = [&](const anychat::Group&) {
        ++updated_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-friend-003",
            "type": "friend.request",
            "timestamp": 1708329602,
            "payload": { "from_user_id": "user-X" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(invited_count, 0);
    EXPECT_EQ(updated_count, 0);
}

TEST_F(GroupManagerTest, GroupMutedNotificationFiresUpdatedHandler) {
    anychat::Group updated{};
    int call_count = 0;

    auto listener = std::make_shared<TestGroupListener>();
    listener->on_updated = [&](const anychat::Group& g) {
        updated = g;
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-group-muted-001",
            "type": "group.muted",
            "timestamp": 1708329603,
            "payload": {
                "group_id": "grp-003",
                "is_muted": true
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(updated.group_id, "grp-003");
    EXPECT_TRUE(updated.is_muted);
}

TEST_F(GroupManagerTest, GetListDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getList([](const std::vector<anychat::Group>&, const std::string&) {}));
}
