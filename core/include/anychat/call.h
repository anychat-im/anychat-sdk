#pragma once

#include "types.h"

#include <functional>
#include <string>
#include <vector>

namespace anychat {

class CallManager {
public:
    using CallCallback = std::function<void(bool ok, const CallSession&, const std::string& err)>;
    using CallListCallback =
        std::function<void(const std::vector<CallSession>& calls, int64_t total, const std::string& err)>;
    using MeetingCallback = std::function<void(bool ok, const MeetingRoom&, const std::string& err)>;
    using MeetingListCallback =
        std::function<void(const std::vector<MeetingRoom>& rooms, int64_t total, const std::string& err)>;
    using ResultCallback = std::function<void(bool ok, const std::string& err)>;

    // Notification handlers for incoming WebSocket events.
    using OnIncomingCall = std::function<void(const CallSession&)>;
    using OnCallStatusChanged = std::function<void(const std::string& call_id, CallStatus status)>;

    virtual ~CallManager() = default;

    // ---- One-to-one calls ------------------------------------------------

    // POST /rtc/calls
    virtual void initiateCall(const std::string& callee_id, CallType type, CallCallback callback) = 0;

    // POST /rtc/calls/{callId}/join
    virtual void joinCall(const std::string& call_id, CallCallback callback) = 0;

    // POST /rtc/calls/{callId}/reject
    virtual void rejectCall(const std::string& call_id, ResultCallback callback) = 0;

    // POST /rtc/calls/{callId}/end
    virtual void endCall(const std::string& call_id, ResultCallback callback) = 0;

    // GET  /rtc/calls/{callId}
    virtual void getCallSession(const std::string& call_id, CallCallback callback) = 0;

    // GET  /rtc/calls?page=&pageSize=
    virtual void getCallLogs(int page, int page_size, CallListCallback callback) = 0;

    // ---- Meetings --------------------------------------------------------

    // POST /rtc/meetings
    virtual void createMeeting(
        const std::string& title,
        const std::string& password,
        int max_participants,
        MeetingCallback callback
    ) = 0;

    // POST /rtc/meetings/{roomId}/join
    virtual void joinMeeting(const std::string& room_id, const std::string& password, MeetingCallback callback) = 0;

    // POST /rtc/meetings/{roomId}/end
    virtual void endMeeting(const std::string& room_id, ResultCallback callback) = 0;

    // GET  /rtc/meetings/{roomId}
    virtual void getMeeting(const std::string& room_id, MeetingCallback callback) = 0;

    // GET  /rtc/meetings?page=&pageSize=
    virtual void listMeetings(int page, int page_size, MeetingListCallback callback) = 0;

    // ---- WebSocket notification handlers ---------------------------------

    // livekit.call_invite — incoming call from another user.
    virtual void setOnIncomingCall(OnIncomingCall handler) = 0;

    // livekit.call_status / livekit.call_rejected — call state changes.
    virtual void setOnCallStatusChanged(OnCallStatusChanged handler) = 0;
};

} // namespace anychat
