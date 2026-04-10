#pragma once

#include "types.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace anychat {

class CallListener {
public:
    virtual ~CallListener() = default;

    virtual void onIncomingCall(const CallSession& session) {
        (void) session;
    }

    virtual void onCallStatusChanged(const std::string& call_id, CallStatus status) {
        (void) call_id;
        (void) status;
    }
};

class CallManager {
public:
    using CallCallback = std::function<void(bool ok, const CallSession&, const std::string& err)>;
    using CallListCallback =
        std::function<void(const std::vector<CallSession>& calls, int64_t total, const std::string& err)>;
    using MeetingCallback = std::function<void(bool ok, const MeetingRoom&, const std::string& err)>;
    using MeetingListCallback =
        std::function<void(const std::vector<MeetingRoom>& rooms, int64_t total, const std::string& err)>;
    using ResultCallback = std::function<void(bool ok, const std::string& err)>;

    virtual ~CallManager() = default;

    // ---- One-to-one calls ------------------------------------------------

    // POST /calling/calls
    virtual void initiateCall(const std::string& callee_id, CallType type, CallCallback callback) = 0;

    // POST /calling/calls/{callId}/join
    virtual void joinCall(const std::string& call_id, CallCallback callback) = 0;

    // POST /calling/calls/{callId}/reject
    virtual void rejectCall(const std::string& call_id, ResultCallback callback) = 0;

    // POST /calling/calls/{callId}/end
    virtual void endCall(const std::string& call_id, ResultCallback callback) = 0;

    // GET  /calling/calls/{callId}
    virtual void getCallSession(const std::string& call_id, CallCallback callback) = 0;

    // GET  /calling/calls?page=&pageSize=
    virtual void getCallLogs(int page, int page_size, CallListCallback callback) = 0;

    // ---- Meetings --------------------------------------------------------

    // POST /calling/meetings
    virtual void createMeeting(
        const std::string& title,
        const std::string& password,
        int max_participants,
        MeetingCallback callback
    ) = 0;

    // POST /calling/meetings/{roomId}/join
    virtual void joinMeeting(const std::string& room_id, const std::string& password, MeetingCallback callback) = 0;

    // POST /calling/meetings/{roomId}/end
    virtual void endMeeting(const std::string& room_id, ResultCallback callback) = 0;

    // GET  /calling/meetings/{roomId}
    virtual void getMeeting(const std::string& room_id, MeetingCallback callback) = 0;

    // GET  /calling/meetings?page=&pageSize=
    virtual void listMeetings(int page, int page_size, MeetingListCallback callback) = 0;

    // ---- WebSocket notification handlers ---------------------------------

    // livekit.call_invite / livekit.call_status / livekit.call_rejected.
    virtual void setListener(std::shared_ptr<CallListener> listener) = 0;
};

} // namespace anychat
