#include "message_manager.h"
#include "notification_manager.h"
#include "outbound_queue.h"

#include "cache/message_cache.h"
#include "db/database.h"
#include "network/http_client.h"

#include <atomic>
#include <memory>
#include <string>

#include <gtest/gtest.h>

class MessageManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_unique<anychat::db::Database>(":memory:");
        ASSERT_TRUE(db_->open()) << "Failed to open in-memory DB";

        msg_cache_ = std::make_unique<anychat::cache::MessageCache>();
        notif_mgr_ = std::make_unique<anychat::NotificationManager>();
        outbound_q_ = std::make_unique<anychat::OutboundQueue>(db_.get());
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");

        mgr_ = std::make_unique<anychat::MessageManagerImpl>(
            db_.get(),
            msg_cache_.get(),
            outbound_q_.get(),
            notif_mgr_.get(),
            http_,
            "user-test-001"
        );
    }

    void TearDown() override {
        mgr_.reset();
        outbound_q_.reset();
        notif_mgr_.reset();
        msg_cache_.reset();
        http_.reset();
        db_->close();
        db_.reset();
    }

    void drainDb() {
        db_->querySync("SELECT 1");
    }

    int outboundRowCount() {
        return static_cast<int>(db_->querySync("SELECT local_id FROM outbound_queue").size());
    }

    std::unique_ptr<anychat::db::Database> db_;
    std::unique_ptr<anychat::cache::MessageCache> msg_cache_;
    std::unique_ptr<anychat::NotificationManager> notif_mgr_;
    std::unique_ptr<anychat::OutboundQueue> outbound_q_;
    std::shared_ptr<anychat::network::HttpClient> http_;
    std::unique_ptr<anychat::MessageManagerImpl> mgr_;
};

TEST_F(MessageManagerTest, SendTextMessageEnqueues) {
    bool cb_called = false;

    mgr_->sendTextMessage("conv-1", "hello", [&cb_called](bool /*success*/, const std::string& /*err*/) {
        cb_called = true;
    });

    drainDb();

    EXPECT_EQ(outboundRowCount(), 1) << "One row should be present in outbound_queue";
    EXPECT_FALSE(cb_called) << "Callback should not be called before the server ack";
}

TEST_F(MessageManagerTest, IncomingMessageFiresHandler) {
    anychat::Message received_msg{};
    int call_count = 0;

    mgr_->setOnMessageReceived([&](const anychat::Message& msg) {
        received_msg = msg;
        ++call_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-msg-001",
            "type": "message.new",
            "timestamp": 1708329600,
            "payload": {
                "message_id": "msg-incoming-001",
                "conversation_id": "conv-1",
                "from_user_id": "user-sender-999",
                "content_type": "text",
                "content": "你好吗？",
                "sent_at": 1708329500,
                "seq": 7
            }
        }
    })";

    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1) << "OnMessageReceived handler should be called once";
    EXPECT_EQ(received_msg.message_id, "msg-incoming-001");
    EXPECT_EQ(received_msg.conv_id, "conv-1");
    EXPECT_EQ(received_msg.sender_id, "user-sender-999");
    EXPECT_EQ(received_msg.content, "你好吗？");
    EXPECT_EQ(received_msg.seq, 7);
    EXPECT_EQ(received_msg.timestamp_ms, 1708329500LL * 1000LL);
}

TEST_F(MessageManagerTest, IncomingMessageCacheDedup) {
    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-msg-dup",
            "type": "message.new",
            "timestamp": 1708329600,
            "payload": {
                "message_id": "msg-dup-001",
                "conversation_id": "conv-dedup",
                "from_user_id": "user-A",
                "content_type": "text",
                "content": "duplicate",
                "sent_at": 1708329501,
                "seq": 1
            }
        }
    })";

    notif_mgr_->handleRaw(frame);
    notif_mgr_->handleRaw(frame);

    auto cached = msg_cache_->get("conv-dedup");
    EXPECT_EQ(cached.size(), 1u) << "Message cache should store only one copy of a duplicate message_id";
    EXPECT_EQ(cached[0].message_id, "msg-dup-001");
}

TEST_F(MessageManagerTest, NonMessageNewNotificationDoesNotFireHandler) {
    int call_count = 0;
    mgr_->setOnMessageReceived([&](const anychat::Message&) {
        ++call_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-friend-001",
            "type": "friend.request",
            "timestamp": 1708329601,
            "payload": {
                "request_id": 999,
                "from_user_id": "user-X"
            }
        }
    })";

    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(call_count, 0) << "OnMessageReceived should not be called for non-message.new notifications";
}

TEST_F(MessageManagerTest, SendTextMessageMultiple) {
    mgr_->sendTextMessage("conv-m", "first", nullptr);
    mgr_->sendTextMessage("conv-m", "second", nullptr);
    mgr_->sendTextMessage("conv-m", "third", nullptr);

    drainDb();

    EXPECT_EQ(outboundRowCount(), 3) << "Each sendTextMessage call should create one outbound_queue row";
}

TEST_F(MessageManagerTest, SetCurrentUserId) {
    EXPECT_NO_THROW(mgr_->setCurrentUserId("new-user-id"));

    bool handler_called = false;
    mgr_->setOnMessageReceived([&](const anychat::Message&) {
        handler_called = true;
    });

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "notification_id": "notif-msg-002",
            "type": "message.new",
            "timestamp": 1708329700,
            "payload": {
                "message_id": "msg-after-uid-change",
                "conversation_id": "conv-x",
                "from_user_id": "other-user",
                "content_type": "text",
                "content": "hello",
                "sent_at": 1708329650,
                "seq": 1
            }
        }
    })");

    EXPECT_TRUE(handler_called) << "Handler should still fire after setCurrentUserId";
}

TEST_F(MessageManagerTest, MessageRecalledNotificationUpdatesCacheAndCallback) {
    anychat::Message seed{};
    seed.message_id = "msg-recall-1";
    seed.conv_id = "conv-recall";
    seed.content = "original";
    seed.status = 0;
    msg_cache_->insert(seed);

    anychat::Message recalled{};
    int callback_count = 0;
    mgr_->setOnMessageRecalled([&](const anychat::Message& msg) {
        recalled = msg;
        ++callback_count;
    });

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "type": "message.recalled",
            "timestamp": 1708329800,
            "payload": {
                "message_id": "msg-recall-1",
                "conversation_id": "conv-recall",
                "recalled_at": 1708329800
            }
        }
    })");

    ASSERT_EQ(callback_count, 1);
    EXPECT_EQ(recalled.message_id, "msg-recall-1");
    EXPECT_EQ(recalled.status, 1);

    auto cached = msg_cache_->get("conv-recall");
    ASSERT_EQ(cached.size(), 1u);
    EXPECT_EQ(cached[0].status, 1);
}

TEST_F(MessageManagerTest, MessageEditedNotificationUpdatesCacheAndCallback) {
    anychat::Message seed{};
    seed.message_id = "msg-edit-1";
    seed.conv_id = "conv-edit";
    seed.content = "before";
    seed.status = 0;
    msg_cache_->insert(seed);

    anychat::Message edited{};
    int callback_count = 0;
    mgr_->setOnMessageEdited([&](const anychat::Message& msg) {
        edited = msg;
        ++callback_count;
    });

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "type": "message.edited",
            "timestamp": 1708329850,
            "payload": {
                "message_id": "msg-edit-1",
                "conversation_id": "conv-edit",
                "content": "after"
            }
        }
    })");

    ASSERT_EQ(callback_count, 1);
    EXPECT_EQ(edited.message_id, "msg-edit-1");
    EXPECT_EQ(edited.content, "after");

    auto cached = msg_cache_->get("conv-edit");
    ASSERT_EQ(cached.size(), 1u);
    EXPECT_EQ(cached[0].content, "after");
}

TEST_F(MessageManagerTest, MessageTypingNotificationCallback) {
    anychat::MessageTypingEvent typing{};
    int callback_count = 0;
    mgr_->setOnMessageTyping([&](const anychat::MessageTypingEvent& event) {
        typing = event;
        ++callback_count;
    });

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "type": "message.typing",
            "timestamp": 1708329900,
            "payload": {
                "conversation_id": "conv-typing",
                "from_user_id": "user-a",
                "typing": true,
                "expire_at": 1708329905
            }
        }
    })");

    ASSERT_EQ(callback_count, 1);
    EXPECT_EQ(typing.conversation_id, "conv-typing");
    EXPECT_EQ(typing.from_user_id, "user-a");
    EXPECT_TRUE(typing.typing);
}

TEST_F(MessageManagerTest, MessageReadReceiptNotificationCallback) {
    anychat::MessageReadReceiptEvent receipt{};
    int callback_count = 0;
    mgr_->setOnMessageReadReceipt([&](const anychat::MessageReadReceiptEvent& event) {
        receipt = event;
        ++callback_count;
    });

    notif_mgr_->handleRaw(R"({
        "type": "notification",
        "payload": {
            "type": "message.read_receipt",
            "timestamp": 1708330000,
            "payload": {
                "conversation_id": "conv-r",
                "from_user_id": "user-b",
                "message_id": "msg-r-1",
                "last_read_seq": 88,
                "read_at": 1708330000
            }
        }
    })");

    ASSERT_EQ(callback_count, 1);
    EXPECT_EQ(receipt.conversation_id, "conv-r");
    EXPECT_EQ(receipt.from_user_id, "user-b");
    EXPECT_EQ(receipt.message_id, "msg-r-1");
    EXPECT_EQ(receipt.last_read_seq, 88);
}

TEST_F(MessageManagerTest, SendTypingWithoutWebSocketFails) {
    bool cb_called = false;
    bool cb_success = true;
    std::string cb_error;

    mgr_->sendTyping("conv-typing", true, 5, [&](bool success, const std::string& error) {
        cb_called = true;
        cb_success = success;
        cb_error = error;
    });

    EXPECT_TRUE(cb_called);
    EXPECT_FALSE(cb_success);
    EXPECT_FALSE(cb_error.empty());
}
