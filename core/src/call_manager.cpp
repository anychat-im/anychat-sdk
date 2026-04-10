#include "call_manager.h"

#include <initializer_list>
#include <string>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

const nlohmann::json* findField(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

std::string getString(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    return value != nullptr && value->is_string() ? value->get<std::string>() : "";
}

int64_t getInt64(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    if (value == nullptr) {
        return 0;
    }
    if (value->is_number_integer()) {
        return value->get<int64_t>();
    }
    if (value->is_number_unsigned()) {
        return static_cast<int64_t>(value->get<uint64_t>());
    }
    return 0;
}

int32_t getInt32(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    return static_cast<int32_t>(getInt64(obj, keys));
}

bool getBool(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    return value != nullptr && value->is_boolean() ? value->get<bool>() : false;
}

CallType parseCallTypeValue(const nlohmann::json& obj) {
    const auto* value = findField(obj, { "callType", "call_type" });
    if (value == nullptr) {
        return CallType::Audio;
    }
    if (value->is_number_integer() || value->is_number_unsigned()) {
        return getInt64(obj, { "callType", "call_type" }) == 1 ? CallType::Video : CallType::Audio;
    }
    if (value->is_string()) {
        return value->get<std::string>() == "video" ? CallType::Video : CallType::Audio;
    }
    return CallType::Audio;
}

CallStatus parseCallStatusValue(const nlohmann::json& obj) {
    const auto* value = findField(obj, { "status" });
    if (value == nullptr) {
        return CallStatus::Ringing;
    }

    if (value->is_number_integer() || value->is_number_unsigned()) {
        switch (getInt64(obj, { "status" })) {
            case 1:
                return CallStatus::Connected;
            case 2:
                return CallStatus::Ended;
            case 3:
                return CallStatus::Rejected;
            case 4:
                return CallStatus::Missed;
            case 5:
                return CallStatus::Cancelled;
            default:
                return CallStatus::Ringing;
        }
    }

    if (!value->is_string()) {
        return CallStatus::Ringing;
    }

    const std::string status = value->get<std::string>();
    if (status == "connected")
        return CallStatus::Connected;
    if (status == "ended")
        return CallStatus::Ended;
    if (status == "rejected")
        return CallStatus::Rejected;
    if (status == "missed")
        return CallStatus::Missed;
    if (status == "cancelled")
        return CallStatus::Cancelled;
    return CallStatus::Ringing;
}

bool parseMeetingIsActive(const nlohmann::json& obj) {
    const auto* value = findField(obj, { "status" });
    if (value == nullptr) {
        return true;
    }
    if (value->is_number_integer() || value->is_number_unsigned()) {
        return getInt64(obj, { "status" }) != 1;
    }
    if (value->is_string()) {
        return value->get<std::string>() != "ended";
    }
    return true;
}

MeetingRoom parseMeetingResult(const nlohmann::json& data) {
    const auto* meeting = findField(data, { "meeting" });
    const nlohmann::json& room_data = meeting != nullptr && meeting->is_object() ? *meeting : data;

    MeetingRoom room;
    room.room_id = getString(room_data, { "roomId", "room_id" });
    room.creator_id = getString(room_data, { "creatorId", "creator_id" });
    room.title = getString(room_data, { "title" });
    room.room_name = getString(room_data, { "roomName", "room_name" });
    room.token = getString(data, { "token" });
    if (room.token.empty()) {
        room.token = getString(room_data, { "token" });
    }
    room.has_password = getBool(room_data, { "hasPassword", "has_password" });
    room.max_participants = getInt32(room_data, { "maxParticipants", "max_participants" });
    room.started_at = getInt64(room_data, { "startedAt", "started_at" });
    room.created_at_ms = getInt64(room_data, { "createdAt", "created_at" }) * 1000;
    room.is_active = parseMeetingIsActive(room_data);
    return room;
}

} // namespace

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
    s.call_id = getString(j, { "callId", "call_id" });
    s.caller_id = getString(j, { "callerId", "caller_id" });
    s.callee_id = getString(j, { "calleeId", "callee_id" });
    s.room_name = getString(j, { "roomName", "room_name" });
    s.token = getString(j, { "token" });
    s.started_at = getInt64(j, { "startedAt", "started_at" });
    s.connected_at = getInt64(j, { "connectedAt", "connected_at" });
    s.ended_at = getInt64(j, { "endedAt", "ended_at" });
    s.duration = getInt32(j, { "duration" });
    s.call_type = parseCallTypeValue(j);
    s.status = parseCallStatusValue(j);
    return s;
}

/*static*/ MeetingRoom CallManagerImpl::parseMeetingRoom(const nlohmann::json& j) {
    MeetingRoom m;
    m.room_id = getString(j, { "roomId", "room_id" });
    m.creator_id = getString(j, { "creatorId", "creator_id" });
    m.title = getString(j, { "title" });
    m.room_name = getString(j, { "roomName", "room_name" });
    m.token = getString(j, { "token" });
    m.has_password = getBool(j, { "hasPassword", "has_password" });
    m.max_participants = getInt32(j, { "maxParticipants", "max_participants" });
    m.started_at = getInt64(j, { "startedAt", "started_at" });
    m.created_at_ms = getInt64(j, { "createdAt", "created_at" }) * 1000;
    m.is_active = parseMeetingIsActive(j);
    return m;
}

// ---------------------------------------------------------------------------
// One-to-one calls
// ---------------------------------------------------------------------------

void CallManagerImpl::initiateCall(const std::string& callee_id, CallType type, CallCallback callback) {
    nlohmann::json body;
    body["calleeId"] = callee_id;
    body["callType"] = (type == CallType::Video) ? "video" : "audio";

    http_->post("/calling/calls", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
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
    http_->post("/calling/calls/" + call_id + "/join", "", [cb = std::move(callback), call_id](network::HttpResponse resp) {
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
            CallSession session = parseCallSession(root["data"]);
            if (session.call_id.empty()) {
                session.call_id = call_id;
            }
            cb(true, session, "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::rejectCall(const std::string& call_id, ResultCallback callback) {
    http_->post("/calling/calls/" + call_id + "/reject", "", [cb = std::move(callback)](network::HttpResponse resp) {
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
    http_->post("/calling/calls/" + call_id + "/end", "", [cb = std::move(callback)](network::HttpResponse resp) {
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
    http_->get("/calling/calls/" + call_id, [cb = std::move(callback)](network::HttpResponse resp) {
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
    std::string path = "/calling/calls?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

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
            int64_t total = getInt64(data, { "total" });
            std::vector<CallSession> calls;
            const auto* sessions = findField(data, { "sessions", "list" });
            if (sessions != nullptr && sessions->is_array()) {
                for (const auto& item : *sessions) {
                    calls.push_back(parseCallSession(item));
                }
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

    http_->post("/calling/meetings", body.dump(), [cb = std::move(callback)](network::HttpResponse resp) {
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
            cb(true, parseMeetingResult(root["data"]), "");
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
        "/calling/meetings/" + room_id + "/join",
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
                cb(true, parseMeetingResult(root["data"]), "");
            } catch (const std::exception& e) {
                cb(false, {}, std::string("parse error: ") + e.what());
            }
        }
    );
}

void CallManagerImpl::endMeeting(const std::string& room_id, ResultCallback callback) {
    http_->post("/calling/meetings/" + room_id + "/end", "", [cb = std::move(callback)](network::HttpResponse resp) {
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
    http_->get("/calling/meetings/" + room_id, [cb = std::move(callback)](network::HttpResponse resp) {
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
            cb(true, parseMeetingResult(root["data"]), "");
        } catch (const std::exception& e) {
            cb(false, {}, std::string("parse error: ") + e.what());
        }
    });
}

void CallManagerImpl::listMeetings(int page, int page_size, MeetingListCallback callback) {
    std::string path = "/calling/meetings?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);

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
            int64_t total = getInt64(data, { "total" });
            std::vector<MeetingRoom> rooms;
            const auto* meetings = findField(data, { "meetings", "list" });
            if (meetings != nullptr && meetings->is_array()) {
                for (const auto& item : *meetings) {
                    rooms.push_back(parseMeetingRoom(item));
                }
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

void CallManagerImpl::setListener(std::shared_ptr<CallListener> listener) {
    std::lock_guard<std::mutex> lk(handler_mutex_);
    listener_ = std::move(listener);
}

void CallManagerImpl::handleCallNotification(const NotificationEvent& event) {
    const auto& nt = event.notification_type;

    if (nt == "livekit.call_invite") {
        std::shared_ptr<CallListener> listener;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            try {
                listener->onIncomingCall(parseCallSession(event.data));
            } catch (...) {
            }
        }
        return;
    }

    if (nt == "livekit.call_status" || nt == "livekit.call_rejected") {
        std::shared_ptr<CallListener> listener;
        {
            std::lock_guard<std::mutex> lk(handler_mutex_);
            listener = listener_;
        }
        if (listener) {
            try {
                std::string call_id = getString(event.data, { "callId", "call_id" });
                CallStatus status = (nt == "livekit.call_rejected")
                                        ? CallStatus::Rejected
                                        : parseCallStatusValue(event.data);
                listener->onCallStatusChanged(call_id, status);
            } catch (...) {
            }
        }
        return;
    }
}

} // namespace anychat
