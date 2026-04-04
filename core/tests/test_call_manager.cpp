#include "call_manager.h"

#include "anychat/types.h"

#include "network/http_client.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

// ===========================================================================
// Fixture
// ===========================================================================
class CallManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        notif_mgr_ = std::make_unique<anychat::NotificationManager>();
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");
        mgr_ = std::make_unique<anychat::CallManagerImpl>(http_, notif_mgr_.get());
    }

    void TearDown() override {
        mgr_.reset();
        http_.reset();
        notif_mgr_.reset();
    }

    std::unique_ptr<anychat::NotificationManager> notif_mgr_;
    std::shared_ptr<anychat::network::HttpClient> http_;
    std::unique_ptr<anychat::CallManagerImpl> mgr_;
};

// ---------------------------------------------------------------------------
// 1. IncomingCallNotificationFiresHandler
//    A livekit.call_invite WebSocket notification should invoke the
//    OnIncomingCall handler with the correct CallSession fields.
// ---------------------------------------------------------------------------
TEST_F(CallManagerTest, IncomingCallNotificationFiresHandler) {
    anychat::CallSession received{};
    int call_count = 0;

    mgr_->setOnIncomingCall([&](const anychat::CallSession& s) {
        received = s;
        ++call_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notificationType": "livekit.call_invite",
            "timestamp": 1708329600,
            "data": {
                "callId":   "call-001",
                "callerId": "user-caller-111",
                "calleeId": "user-callee-222",
                "callType": "video",
                "status":   "ringing",
                "roomName": "room-abc",
                "token":    "rtc-jwt-xxx"
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1) << "OnIncomingCall must be called exactly once";
    EXPECT_EQ(received.call_id, "call-001");
    EXPECT_EQ(received.caller_id, "user-caller-111");
    EXPECT_EQ(received.call_type, anychat::CallType::Video);
    EXPECT_EQ(received.status, anychat::CallStatus::Ringing);
    EXPECT_EQ(received.room_name, "room-abc");
}

// ---------------------------------------------------------------------------
// 2. CallStatusChangedNotificationFiresHandler
//    A livekit.call_status notification should invoke OnCallStatusChanged
//    with the correct call_id and parsed status.
// ---------------------------------------------------------------------------
TEST_F(CallManagerTest, CallStatusChangedNotificationFiresHandler) {
    std::string received_id;
    anychat::CallStatus received_status = anychat::CallStatus::Ringing;
    int call_count = 0;

    mgr_->setOnCallStatusChanged([&](const std::string& id, anychat::CallStatus st) {
        received_id = id;
        received_status = st;
        ++call_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notificationType": "livekit.call_status",
            "timestamp": 1708329601,
            "data": {
                "callId": "call-002",
                "status": "connected"
            }
        }
    })";
    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(received_id, "call-002");
    EXPECT_EQ(received_status, anychat::CallStatus::Connected);
}

// ---------------------------------------------------------------------------
// 3. CallRejectedNotificationFiresHandler
//    A livekit.call_rejected notification should invoke OnCallStatusChanged
//    with status = Rejected.
// ---------------------------------------------------------------------------
TEST_F(CallManagerTest, CallRejectedNotificationFiresHandler) {
    anychat::CallStatus received_status = anychat::CallStatus::Ringing;
    int call_count = 0;

    mgr_->setOnCallStatusChanged([&](const std::string& /*id*/, anychat::CallStatus st) {
        received_status = st;
        ++call_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notificationType": "livekit.call_rejected",
            "timestamp": 1708329602,
            "data": { "callId": "call-003" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    ASSERT_EQ(call_count, 1);
    EXPECT_EQ(received_status, anychat::CallStatus::Rejected);
}

// ---------------------------------------------------------------------------
// 4. UnrelatedNotificationDoesNotFireCallHandlers
// ---------------------------------------------------------------------------
TEST_F(CallManagerTest, UnrelatedNotificationDoesNotFireCallHandlers) {
    int incoming_count = 0;
    int status_count = 0;
    mgr_->setOnIncomingCall([&](const anychat::CallSession&) {
        ++incoming_count;
    });
    mgr_->setOnCallStatusChanged([&](const std::string&, anychat::CallStatus) {
        ++status_count;
    });

    const std::string frame = R"({
        "type": "notification",
        "payload": {
            "notificationType": "message.new",
            "timestamp": 1708329603,
            "data": { "messageId": "msg-999" }
        }
    })";
    notif_mgr_->handleRaw(frame);

    EXPECT_EQ(incoming_count, 0);
    EXPECT_EQ(status_count, 0);
}

// ---------------------------------------------------------------------------
// 5. InitiateCallDoesNotCrash
// ---------------------------------------------------------------------------
TEST_F(CallManagerTest, InitiateCallDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->initiateCall(
        "callee-999",
        anychat::CallType::Audio,
        [](bool /*ok*/, const anychat::CallSession& /*s*/, const std::string& /*err*/) {}
    ));
}
