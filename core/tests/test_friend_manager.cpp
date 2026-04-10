#include "friend_manager.h"
#include "notification_manager.h"

#include "db/database.h"
#include "network/http_client.h"

#include <memory>
#include <string>
#include <functional>

#include <gtest/gtest.h>

class FriendManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_unique<anychat::db::Database>(":memory:");
        ASSERT_TRUE(db_->open());
        notif_mgr_ = std::make_unique<anychat::NotificationManager>();
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");
        mgr_ = std::make_unique<anychat::FriendManagerImpl>(db_.get(), notif_mgr_.get(), http_);
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
    std::unique_ptr<anychat::FriendManagerImpl> mgr_;
};

class TestFriendListener final : public anychat::FriendListener {
public:
    std::function<void(const anychat::FriendRequest&)> on_request_received;
    std::function<void(const anychat::Friend&)> on_friend_added;
    std::function<void(const std::string&)> on_friend_deleted;

    void onFriendRequestReceived(const anychat::FriendRequest& req) override {
        if (on_request_received) {
            on_request_received(req);
        }
    }

    void onFriendAdded(const anychat::Friend& friend_info) override {
        if (on_friend_added) {
            on_friend_added(friend_info);
        }
    }

    void onFriendDeleted(const std::string& user_id) override {
        if (on_friend_deleted) {
            on_friend_deleted(user_id);
        }
    }
};

TEST_F(FriendManagerTest, FriendRequestNotificationFiresHandler) {
    anychat::FriendRequest received{};
    int call_count = 0;

    auto listener = std::make_shared<TestFriendListener>();
    listener->on_request_received = [&](const anychat::FriendRequest& req) {
        received = req;
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-friend-req-001",
            "type": "friend.request",
            "timestamp": 1708329600,
            "payload": {
                "request_id": 1001,
                "from_user_id": "user-sender-111",
                "message": "你好，加个好友",
                "created_at": 1708329500
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(received.request_id, 1001);
    EXPECT_EQ(received.from_user_id, "user-sender-111");
    EXPECT_EQ(received.message, "你好，加个好友");
}

TEST_F(FriendManagerTest, FriendDeletedNotificationFiresListChangedHandler) {
    int call_count = 0;
    auto listener = std::make_shared<TestFriendListener>();
    listener->on_friend_deleted = [&](const std::string&) {
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-friend-del-001",
            "type": "friend.deleted",
            "timestamp": 1708329601,
            "payload": { "friend_user_id": "user-deleted-222" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(call_count, 1);
}

TEST_F(FriendManagerTest, FriendAddedNotificationFiresListChangedHandler) {
    int call_count = 0;
    auto listener = std::make_shared<TestFriendListener>();
    listener->on_friend_added = [&](const anychat::Friend&) {
        ++call_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-friend-added-001",
            "type": "friend.added",
            "timestamp": 1708329603,
            "payload": { "added_by_user_id": "user-added-by-333" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(call_count, 1);
}

TEST_F(FriendManagerTest, UnrelatedNotificationDoesNotFireHandlers) {
    int req_count = 0;
    int added_count = 0;
    auto listener = std::make_shared<TestFriendListener>();
    listener->on_request_received = [&](const anychat::FriendRequest&) {
        ++req_count;
    };
    listener->on_friend_added = [&](const anychat::Friend&) {
        ++added_count;
    };
    mgr_->setListener(listener);

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-group-001",
            "type": "group.info_updated",
            "timestamp": 1708329602,
            "payload": { "group_id": "grp-999" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(req_count, 0);
    EXPECT_EQ(added_count, 0);
}

TEST_F(FriendManagerTest, GetListDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getList([](const std::vector<anychat::Friend>&, const std::string&) {}));
}

TEST_F(FriendManagerTest, SendRequestWithSourceDoesNotCrash) {
    EXPECT_NO_THROW(
        mgr_->sendRequest("user-target-001", "hi", "search", [](bool /*ok*/, const std::string& /*err*/) {})
    );
}

TEST_F(FriendManagerTest, GetRequestsDoesNotCrash) {
    EXPECT_NO_THROW(
        mgr_->getRequests("received", [](const std::vector<anychat::FriendRequest>&, const std::string&) {})
    );
}

TEST_F(FriendManagerTest, GetBlacklistDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getBlacklist([](const std::vector<anychat::BlacklistItem>&, const std::string&) {}));
}
