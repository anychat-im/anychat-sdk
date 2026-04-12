#pragma once

#include "notification_manager.h"

#include "anychat/call.h"

#include "network/http_client.h"

#include <memory>
#include <mutex>
#include <string>

namespace anychat {

class CallManagerImpl : public CallManager {
public:
    CallManagerImpl(std::shared_ptr<network::HttpClient> http, NotificationManager* notif_mgr);

    void initiateCall(const std::string& callee_id, CallType type, CallCallback callback) override;
    void joinCall(const std::string& call_id, CallCallback callback) override;
    void rejectCall(const std::string& call_id, ResultCallback callback) override;
    void endCall(const std::string& call_id, ResultCallback callback) override;
    void getCallSession(const std::string& call_id, CallCallback callback) override;
    void getCallLogs(int page, int page_size, CallListCallback callback) override;

    void
    createMeeting(const std::string& title, const std::string& password, int max_participants, MeetingCallback callback)
        override;
    void joinMeeting(const std::string& room_id, const std::string& password, MeetingCallback callback) override;
    void endMeeting(const std::string& room_id, ResultCallback callback) override;
    void getMeeting(const std::string& room_id, MeetingCallback callback) override;
    void listMeetings(int page, int page_size, MeetingListCallback callback) override;

    void setListener(std::shared_ptr<CallListener> listener) override;

private:
    void handleCallNotification(const NotificationEvent& event);

    std::shared_ptr<network::HttpClient> http_;

    std::mutex handler_mutex_;
    std::shared_ptr<CallListener> listener_;
};

} // namespace anychat
