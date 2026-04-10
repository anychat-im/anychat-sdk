#include "notification_manager.h"
#include "outbound_queue.h"

#include "db/database.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class OutboundQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_unique<anychat::db::Database>(":memory:");
        ASSERT_TRUE(db_->open()) << "Failed to open in-memory DB";
        queue_ = std::make_unique<anychat::OutboundQueue>(db_.get());
    }

    void TearDown() override {
        queue_.reset();
        db_->close();
        db_.reset();
    }

    // Wait for async DB operations to settle by issuing a synchronous query.
    // Because the DB uses a single-threaded task queue (FIFO), this ensures
    // all previously submitted exec() calls have already been processed.
    void drainDb() {
        db_->querySync("SELECT 1");
    }

    // Count rows in outbound_queue for a given local_id.
    int rowCount(const std::string& local_id) {
        auto rows = db_->querySync("SELECT local_id FROM outbound_queue WHERE local_id = ?", { local_id });
        return static_cast<int>(rows.size());
    }

    // Total row count in outbound_queue.
    int totalRows() {
        auto rows = db_->querySync("SELECT local_id FROM outbound_queue");
        return static_cast<int>(rows.size());
    }

    std::unique_ptr<anychat::db::Database> db_;
    std::unique_ptr<anychat::OutboundQueue> queue_;
};

// ---------------------------------------------------------------------------
// 1. EnqueuePersists
//    Enqueueing while disconnected should write a row to outbound_queue.
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, EnqueuePersists) {
    const std::string local_id = "local-001";

    queue_->enqueue("conv-1", "private", "text", "Hello world", local_id, nullptr);

    // drainDb() ensures the async INSERT has been committed.
    drainDb();

    EXPECT_EQ(rowCount(local_id), 1) << "A row for the enqueued message should exist in outbound_queue";
}

// ---------------------------------------------------------------------------
// 2. FlushOnConnect
//    Enqueue while disconnected. Call onConnected — send_fn should be called
//    with a JSON payload that contains the correct type and conversationId.
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, FlushOnConnect) {
    const std::string local_id = "local-002";
    queue_->enqueue("conv-flush", "private", "text", "Test content", local_id, nullptr);
    drainDb();

    std::vector<std::string> sent_payloads;
    queue_->onConnected([&sent_payloads](const std::string& payload) {
        sent_payloads.push_back(payload);
    });

    ASSERT_EQ(sent_payloads.size(), 1u) << "send_fn should have been called once for the queued message";

    // Verify the JSON structure of the sent payload.
    nlohmann::json frame = nlohmann::json::parse(sent_payloads[0]);
    EXPECT_EQ(frame.value("type", ""), "message.send");

    const auto& payload = frame.at("payload");
    EXPECT_EQ(payload.value("conversationId", ""), "conv-flush");
    EXPECT_EQ(payload.value("localId", ""), local_id);
}

// ---------------------------------------------------------------------------
// 3. AckRemovesRow
//    Enqueue, connect, receive ack — the DB row should be deleted and the
//    MessageCallback should be invoked with success=true.
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, AckRemovesRow) {
    const std::string local_id = "local-003";

    bool cb_called = false;
    bool cb_success = false;

    queue_->enqueue("conv-ack", "private", "text", "Ack test", local_id, [&](bool success, const std::string& /*err*/) {
        cb_called = true;
        cb_success = success;
    });
    drainDb();

    // Connect (flush any pending rows).
    queue_->onConnected([](const std::string&) { /* discard */ });

    // Simulate server ack.
    anychat::MsgSentAck ack;
    ack.local_id = local_id;
    ack.message_id = "server-msg-001";
    ack.sequence = 1;
    ack.timestamp = 1708329600;
    queue_->onMessageSentAck(ack);

    // Drain to let the async DELETE settle.
    drainDb();

    EXPECT_EQ(rowCount(local_id), 0) << "Row should be removed from outbound_queue after ack";
    EXPECT_TRUE(cb_called) << "MessageCallback should have been invoked";
    EXPECT_TRUE(cb_success) << "MessageCallback should indicate success";
}

// ---------------------------------------------------------------------------
// 4. RetryOnReconnect
//    Enqueue while disconnected. After the first onConnected / onDisconnected
//    cycle, the row remains in the DB. A second onConnected should resend it.
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, RetryOnReconnect) {
    const std::string local_id = "local-004";

    queue_->enqueue("conv-retry", "private", "text", "Retry me", local_id, nullptr);
    drainDb();

    // First connection — row is sent, but NOT acknowledged (no ack received).
    int first_send_count = 0;
    queue_->onConnected([&first_send_count](const std::string&) {
        ++first_send_count;
    });
    EXPECT_EQ(first_send_count, 1);

    // Simulate disconnection.
    queue_->onDisconnected();

    // Row should still be in the DB because no ack was received.
    drainDb();
    EXPECT_EQ(rowCount(local_id), 1) << "Unacknowledged row must remain in outbound_queue after disconnect";

    // Second connection — row should be re-sent.
    int second_send_count = 0;
    queue_->onConnected([&second_send_count](const std::string&) {
        ++second_send_count;
    });
    EXPECT_EQ(second_send_count, 1) << "Unacknowledged message should be re-sent on reconnect";
}

// ---------------------------------------------------------------------------
// 5. DuplicateLocalIdIgnored (idempotent enqueue)
//    Calling enqueue twice with the same local_id should result in only one
//    row (INSERT OR IGNORE semantics).
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, DuplicateLocalIdIgnored) {
    const std::string local_id = "local-dup";
    queue_->enqueue("conv-1", "private", "text", "first", local_id, nullptr);
    queue_->enqueue("conv-1", "private", "text", "second", local_id, nullptr);
    drainDb();

    EXPECT_EQ(rowCount(local_id), 1) << "Duplicate local_id should not create a second row";
}

// ---------------------------------------------------------------------------
// 6. MultipleEnqueueFlushesAll
//    Enqueue several messages, then call onConnected — each should be sent.
// ---------------------------------------------------------------------------
TEST_F(OutboundQueueTest, MultipleEnqueueFlushesAll) {
    queue_->enqueue("conv-m", "private", "text", "msg-a", "local-a", nullptr);
    queue_->enqueue("conv-m", "private", "text", "msg-b", "local-b", nullptr);
    queue_->enqueue("conv-m", "private", "text", "msg-c", "local-c", nullptr);
    drainDb();

    EXPECT_EQ(totalRows(), 3);

    int send_count = 0;
    queue_->onConnected([&send_count](const std::string&) {
        ++send_count;
    });

    EXPECT_EQ(send_count, 3) << "All three queued messages should be sent on connect";
}

TEST_F(OutboundQueueTest, SendTransientRequiresConnection) {
    EXPECT_FALSE(queue_->sendTransient("{\"type\":\"message.typing\"}"));

    int send_count = 0;
    queue_->onConnected([&send_count](const std::string&) {
        ++send_count;
    });

    EXPECT_TRUE(queue_->sendTransient("{\"type\":\"message.typing\"}"));
    EXPECT_EQ(send_count, 1);
}
