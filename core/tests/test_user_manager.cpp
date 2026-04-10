#include "user_manager.h"
#include "notification_manager.h"

#include "network/http_client.h"

#include <memory>
#include <string>
#include <functional>

#include <gtest/gtest.h>

class UserManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        notif_mgr_ = std::make_unique<anychat::NotificationManager>();
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");
        mgr_ = std::make_unique<anychat::UserManagerImpl>(http_, notif_mgr_.get(), "device-ut-001");
    }

    void TearDown() override {
        mgr_.reset();
        http_.reset();
        notif_mgr_.reset();
    }

    std::unique_ptr<anychat::NotificationManager> notif_mgr_;
    std::shared_ptr<anychat::network::HttpClient> http_;
    std::unique_ptr<anychat::UserManagerImpl> mgr_;
};

class TestUserListener final : public anychat::UserListener {
public:
    std::function<void(const anychat::UserInfo&)> on_profile_updated;
    std::function<void(const anychat::UserInfo&)> on_friend_profile_changed;
    std::function<void(const anychat::UserStatusEvent&)> on_status_changed;

    void onProfileUpdated(const anychat::UserInfo& info) override {
        if (on_profile_updated) {
            on_profile_updated(info);
        }
    }

    void onFriendProfileChanged(const anychat::UserInfo& info) override {
        if (on_friend_profile_changed) {
            on_friend_profile_changed(info);
        }
    }

    void onUserStatusChanged(const anychat::UserStatusEvent& event) override {
        if (on_status_changed) {
            on_status_changed(event);
        }
    }
};

TEST_F(UserManagerTest, GetProfileDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getProfile([](bool /*ok*/, const anychat::UserProfile& /*p*/, const std::string& /*err*/) {})
    );
}

TEST_F(UserManagerTest, UpdateProfileDoesNotCrash) {
    anychat::UserProfile p;
    p.nickname = "TestUser";
    p.birthday_ms = 1'708'329'600'000LL;
    EXPECT_NO_THROW(
        mgr_->updateProfile(p, [](bool /*ok*/, const anychat::UserProfile& /*p*/, const std::string& /*err*/) {})
    );
}

TEST_F(UserManagerTest, GetSettingsDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getSettings([](bool /*ok*/, const anychat::UserSettings& /*s*/, const std::string& /*err*/) {}
    ));
}

TEST_F(UserManagerTest, SearchUsersDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->searchUsers(
        "keyword with spaces",
        1,
        20,
        [](const std::vector<anychat::UserInfo>& /*users*/, int64_t /*total*/, const std::string& /*err*/) {}
    ));
}

TEST_F(UserManagerTest, GetUserInfoDoesNotCrash) {
    EXPECT_NO_THROW(
        mgr_->getUserInfo("user-001", [](bool /*ok*/, const anychat::UserInfo& /*info*/, const std::string& /*err*/) {})
    );
}

TEST_F(UserManagerTest, UpdatePushTokenDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->updatePushToken("push-token-abc", "ios", [](bool /*ok*/, const std::string& /*err*/) {}));
}

TEST_F(UserManagerTest, UpdatePushTokenWithDeviceDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->updatePushToken(
        "push-token-abc",
        "ios",
        "device-explicit-123",
        [](bool /*ok*/, const std::string& /*err*/) {}
    ));
}

TEST_F(UserManagerTest, BindPhoneDoesNotCrash) {
    EXPECT_NO_THROW(
        mgr_->bindPhone("13800138000", "123456", [](bool, const anychat::BindPhoneResult&, const std::string&) {})
    );
}

TEST_F(UserManagerTest, ChangePhoneDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->changePhone(
        "13800138000",
        "13900139000",
        "654321",
        "",
        [](bool, const anychat::ChangePhoneResult&, const std::string&) {}
    ));
}

TEST_F(UserManagerTest, BindEmailDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->bindEmail(
        "test@example.com",
        "123456",
        [](bool, const anychat::BindEmailResult&, const std::string&) {}
    ));
}

TEST_F(UserManagerTest, ChangeEmailDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->changeEmail(
        "old@example.com",
        "new@example.com",
        "654321",
        "",
        [](bool, const anychat::ChangeEmailResult&, const std::string&) {}
    ));
}

TEST_F(UserManagerTest, RefreshQRCodeDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->refreshQRCode([](bool, const anychat::UserQRCode&, const std::string&) {}));
}

TEST_F(UserManagerTest, GetUserByQRCodeDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getUserByQRCode(
        "anychat://qrcode?token=abc",
        [](bool, const anychat::UserInfo&, const std::string&) {}
    ));
}

TEST_F(UserManagerTest, ProfileUpdatedNotificationFiresHandler) {
    anychat::UserInfo updated{};
    int call_count = 0;
    auto listener = std::make_shared<TestUserListener>();
    listener->on_profile_updated = [&](const anychat::UserInfo& info) {
        updated = info;
        ++call_count;
    };
    mgr_->setListener(listener);

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-user-profile-001",
            "type": "user.profile_updated",
            "timestamp": 1708329600,
            "payload": {
                "user_id": "user-123",
                "nickname": "new-name",
                "avatar_url": "https://cdn.example.com/avatar.png",
                "signature": "new-signature",
                "gender": 1,
                "region": "Shanghai"
            }
        }
    })");

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(updated.user_id, "user-123");
    EXPECT_EQ(updated.username, "new-name");
    EXPECT_EQ(updated.avatar_url, "https://cdn.example.com/avatar.png");
    EXPECT_EQ(updated.signature, "new-signature");
    EXPECT_EQ(updated.gender, 1);
    EXPECT_EQ(updated.region, "Shanghai");
}

TEST_F(UserManagerTest, FriendProfileChangedNotificationFiresHandler) {
    anychat::UserInfo updated{};
    int call_count = 0;
    auto listener = std::make_shared<TestUserListener>();
    listener->on_friend_profile_changed = [&](const anychat::UserInfo& info) {
        updated = info;
        ++call_count;
    };
    mgr_->setListener(listener);

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-user-friend-profile-001",
            "type": "user.friend_profile_changed",
            "timestamp": 1708329600,
            "payload": {
                "friend_user_id": "user-456",
                "nickname": "friend-new-name",
                "avatar_url": "https://cdn.example.com/friend-avatar.png",
                "signature": "friend-signature"
            }
        }
    })");

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(updated.user_id, "user-456");
    EXPECT_EQ(updated.username, "friend-new-name");
    EXPECT_EQ(updated.avatar_url, "https://cdn.example.com/friend-avatar.png");
    EXPECT_EQ(updated.signature, "friend-signature");
}

TEST_F(UserManagerTest, StatusChangedNotificationFiresHandler) {
    anychat::UserStatusEvent status{};
    int call_count = 0;
    auto listener = std::make_shared<TestUserListener>();
    listener->on_status_changed = [&](const anychat::UserStatusEvent& event) {
        status = event;
        ++call_count;
    };
    mgr_->setListener(listener);

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-user-status-001",
            "type": "user.status_changed",
            "timestamp": 1708329600,
            "payload": {
                "user_id": "user-789",
                "status": "online",
                "last_active_at": 1708329555,
                "platform": "iOS"
            }
        }
    })");

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(status.user_id, "user-789");
    EXPECT_EQ(status.status, "online");
    EXPECT_EQ(status.last_active_at_ms, 1708329555000LL);
    EXPECT_EQ(status.platform, "iOS");
}

TEST_F(UserManagerTest, UnrelatedNotificationDoesNotFireUserHandlers) {
    int profile_count = 0;
    int friend_profile_count = 0;
    int status_count = 0;
    auto listener = std::make_shared<TestUserListener>();
    listener->on_profile_updated = [&](const anychat::UserInfo&) {
        ++profile_count;
    };
    listener->on_friend_profile_changed = [&](const anychat::UserInfo&) {
        ++friend_profile_count;
    };
    listener->on_status_changed = [&](const anychat::UserStatusEvent&) {
        ++status_count;
    };
    mgr_->setListener(listener);

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-group-001",
            "type": "group.info_updated",
            "timestamp": 1708329600,
            "payload": {
                "group_id": "group-123"
            }
        }
    })");

    EXPECT_EQ(profile_count, 0);
    EXPECT_EQ(friend_profile_count, 0);
    EXPECT_EQ(status_count, 0);
}
