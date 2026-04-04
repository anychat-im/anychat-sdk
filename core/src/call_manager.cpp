#include "call_manager.h"

#include <string>

#include <nlohmann/json.hpp>

namespace anychat {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CallManagerImpl::CallManagerImpl(std::shared_ptr<network::HttpClient> http, NotificationManager* notif_mgr)
    : http_(std::move(http)) {
    if (notif_mgr) {
        notif_mgr->addNotificationHandler([this](const NotificationEvent& ev) {
            handleCallNotification(ev);
        });
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/*static*/ CallStatus CallManagerImpl::parseCallStatus(const std::string& s) {
    if (s == "connected")
        return CallStatus::Connected;
    if (s == "ended")
        return CallStatus::Ended;
    if (s == "rejected")
        return CallStatus::Rejected;
    if (s == "missed")
        return CallStatus::Missed;
    if (s == "cancelled")
        return CallStatus::Cancelled;
    return CallStatus::Ringing;
}

/*static*/ CallSession CallManagerImpl::parseCallSession(const nlohmann::json& j) {
    CallSession s;
    s.call_id = j.value("callId", "");
    s.caller_id = j.value("callerId", "");
    s.callee_id = j.value("calleeId", "");
    s.room_name = j.value("roomName", "");
    s.token = j.value("token", "");
    s.started_at = j.value("startedAt", int64_t{ 0 });
    s.ended_at = j.value("endedAt", int64_t{ 0 });
    s.duration = j.value("duration", 0);
    s.call_type = (j.value("callType", "audio") == "video") ? CallType::Video : CallType::Audio;
    s.status = parseCallStatus(j.value("status", "ringing"));
    return s;
}

/*static*/ MeetingRoom CallManagerImpl::parseMeetingRoom(const nlohmann::json& j) {
    MeetingRoom m;
    m.room_id = j.value("roomId", "");
    m.creator_id = j.value("creatorId", "");
    m.title = j.value("title", "");
    m.room_name = j.value("roomName", "");
    m.token = j.value("token", "");
    m.has_password = j.value("hasPassword", false);
    m.max_participants = j.value("maxParticipants", 0);
    m.started_at = j.value("startedAt", int64_t{ 0 });
    const std::string status = j.value("status", "active");
    m.is_active = (status != "ended");
    return m;
}

// ---------------------------------------------------------------------------
// One-to-one calls
// ---------------------------------------------------------------------------

void CallManagerImpl::initiateCall(const std::string& callee_id, CallType type, CallCallback callback) {
    nlohmann::json body;
    body["calleeId"] = callee_id;
    body["callType"] = (type == CallType::Video) ? "video" : "audio";

    http_->post("/rtc/calls", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseCallSession(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::joinCall(const std::string& call_id, CallCallback callback) {
    http_->post("/rtc/calls/" + call_id + "/join", "", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseCallSession(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::rejectCall(const std::string& call_id, ResultCallback callback) {
    http_->post("/rtc/calls/" + call_id + "/reject", "", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            bool ok = (root.value("code", -1) == 0);
            cb(ok, ok ? "" : root.value("message", "server error"));
        } catch (const std::exception& e) {
            cb(false, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::endCall(const std::string& call_id, ResultCallback callback) {
    http_->post("/rtc/calls/" + call_id + "/end", "", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            bool ok = (root.value("code", -1) == 0);
            cb(ok, ok ? "" : root.value("message", "server error"));
        } catch (const std::exception& e) {
            cb(false, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::getCallSession(const std::string& call_id, CallCallback callback) {
    http_->get("/rtc/calls/" + call_id, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseCallSession(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::getCallLogs(int page, int page_size, CallListCallback callback) {
    std::string path = "/rtc/calls?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb({}, 0, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb({}, 0, root.value("message", "server error"));
                return;
            }
            const auto& data = root["data"];
            int64_t total = data.value("total", int64_t{ 0 });
            std::vector<CallSession> calls;
            for (const auto& item : data["list"]) {
                calls.push_back(parseCallSession(item));
            }
            cb(calls, total, "");
        } catch (const std::exception& e) {
            cb({}, 0, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// Meetings
// ---------------------------------------------------------------------------

void CallManagerImpl::createMeeting(
    const std::string& title,
    const std::string& password,
    int max_participants,
    MeetingCallback callback
) {
    nlohmann::json body;
    body["title"] = title;
    if (!password.empty())
        body["password"] = password;
    if (max_participants > 0)
        body["maxParticipants"] = max_participants;

    http_->post("/rtc/meetings", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseMeetingRoom(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::joinMeeting(const std::string& room_id, const std::string& password, MeetingCallback callback) {
    nlohmann::json body;
    if (!password.empty())
        body["password"] = password;

    http_->post(
        "/rtc/meetings/" + room_id + "/join",
        body.dump(),
        [cb = std::move(callback)](network::HttpResponse resp) {
            if (!resp.error.empty()) {
                cb(false, {}, resp.error);
                return;
            }
            try {
                auto root = nlohmann::json::parse(resp.body);
                if (root.value("code", -1) != 0) {
                    cb(false, {}, root.value("message", "server error"));
                    return;
                }
                cb(true, parseMeetingRoom(root["data"]), "");
            } catch (const std::exception& e) {
                cb(false, {}, std::string("parse error: ") + e.what());
            }
        }
    );
}

void CallManagerImpl::endMeeting(const std::string& room_id, ResultCallback callback) {
    http_->post("/rtc/meetings/" + room_id + "/end", "", [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            bool ok = (root.value("code", -1) == 0);
            cb(ok, ok ? "" : root.value("message", "server error"));
        } catch (const std::exception& e) {
            cb(false, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::getMeeting(const std::string& room_id, MeetingCallback callback) {
    http_->get("/rtc/meetings/" + room_id, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb(false, {}, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb(false, {}, root.value("message", "server error"));
                return;
            }
            cb(true, parseMeetingRoom(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::listMeetings(int page, int page_size, MeetingListCallback callback) {
    std::string path = "/rtc/meetings?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        if (!resp.error.empty()) {
            cb({}, 0, resp.error);
            return;
        }
        try {
            auto root = nlohmann::json::parse(resp.body);
            if (root.value("code", -1) != 0) {
                cb({}, 0, root.value("message", "server error"));
                return;
            }
            const auto& data = root["data"];
            int64_t total = data.value("total", int64_t{ 0 });
            std::vector<MeetingRoom> rooms;
            for (const auto& item : data["list"]) {
                rooms.push_back(parseMeetingRoom(item));
            }
            cb(rooms, total, "");
        } catch (const std::exception& e) {
            cb({}, 0, std::string("parse error: ") + e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// Notification handlers
// ---------------------------------------------------------------------------

void CallManagerImpl::setOnIncomingCall(OnIncomingCall handler) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    on_incoming_call_ = std::move(handler);
}

void CallManagerImpl::setOnCallStatusChanged(OnCallStatusChanged handler) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    on_call_status_changed_ = std::move(handler);
}

void CallManagerImpl::handleCallNotification(const NotificationEvent& event) {
    const auto& nt = event.notification_type;

    if (nt == "livekit.call_invite") {
        OnIncomingCall handler;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            handler = on_incoming_call_;
        }
        if (handler) {
            try {
                handler(parseCallSession(event.data));
            } catch (...) {
            }
        }
        return;
    }

    if (nt == "livekit.call_status" || nt == "livekit.call_rejected") {
        OnCallStatusChanged handler;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            handler = on_call_status_changed_;
        }
        if (handler) {
            try {
                std::string call_id = event.data.value("callId", "");
                CallStatus status = (nt == "livekit.call_rejected")
                                        ? CallStatus::Rejected
                                        : parseCallStatus(event.data.value("status", "ended"));
                handler(call_id, status);
            } catch (...) {
            }
        }
        return;
    }
}

} // namespace anychat
