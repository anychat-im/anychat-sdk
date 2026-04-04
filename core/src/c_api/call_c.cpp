#include "anychat_c/call_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace {

void callSessionToC(const anychat::CallSession& src, AnyChatCallSession_C* dst) {
    anychat_strlcpy(dst->call_id, src.call_id.c_str(), sizeof(dst->call_id));
    anychat_strlcpy(dst->caller_id, src.caller_id.c_str(), sizeof(dst->caller_id));
    anychat_strlcpy(dst->callee_id, src.callee_id.c_str(), sizeof(dst->callee_id));
    anychat_strlcpy(dst->room_name, src.room_name.c_str(), sizeof(dst->room_name));
    anychat_strlcpy(dst->token, src.token.c_str(), sizeof(dst->token));
    dst->call_type = static_cast<int>(src.call_type);
    dst->status = static_cast<int>(src.status);
    dst->started_at = src.started_at;
    dst->connected_at = src.connected_at;
    dst->ended_at = src.ended_at;
    dst->duration = src.duration;
}

void meetingRoomToC(const anychat::MeetingRoom& src, AnyChatMeetingRoom_C* dst) {
    anychat_strlcpy(dst->room_id, src.room_id.c_str(), sizeof(dst->room_id));
    anychat_strlcpy(dst->creator_id, src.creator_id.c_str(), sizeof(dst->creator_id));
    anychat_strlcpy(dst->title, src.title.c_str(), sizeof(dst->title));
    anychat_strlcpy(dst->room_name, src.room_name.c_str(), sizeof(dst->room_name));
    anychat_strlcpy(dst->token, src.token.c_str(), sizeof(dst->token));
    dst->has_password = src.has_password ? 1 : 0;
    dst->max_participants = src.max_participants;
    dst->is_active = src.is_active ? 1 : 0;
    dst->started_at = src.started_at;
    dst->created_at_ms = src.created_at_ms;
}

anychat::CallType callTypeFromC(int t) {
    return (t == ANYCHAT_CALL_VIDEO) ? anychat::CallType::Video : anychat::CallType::Audio;
}

struct CallCbState {
    std::mutex incoming_mutex;
    void* incoming_userdata = nullptr;
    AnyChatIncomingCallCallback incoming_cb = nullptr;

    std::mutex status_mutex;
    void* status_userdata = nullptr;
    AnyChatCallStatusChangedCallback status_cb = nullptr;
};

} // namespace

static std::mutex g_call_cb_map_mutex;
static std::unordered_map<anychat::CallManager*, CallCbState*> g_call_cb_map;

static CallCbState* getOrCreateCallState(anychat::CallManager* impl) {
    std::lock_guard<std::mutex> lock(g_call_cb_map_mutex);
    auto it = g_call_cb_map.find(impl);
    if (it != g_call_cb_map.end())
        return it->second;
    auto* s = new CallCbState();
    g_call_cb_map[impl] = s;
    return s;
}

extern "C" {

int anychat_call_initiate_call(
    AnyChatCallHandle handle,
    const char* callee_id,
    int call_type,
    void* userdata,
    AnyChatCallCallback callback
) {
    if (!handle || !handle->impl || !callee_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->initiateCall(
        callee_id,
        callTypeFromC(call_type),
        [userdata, callback](bool ok, const anychat::CallSession& s, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatCallSession_C c_s{};
                callSessionToC(s, &c_s);
                callback(userdata, 1, &c_s, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_join_call(AnyChatCallHandle handle, const char* call_id, void* userdata, AnyChatCallCallback callback) {
    if (!handle || !handle->impl || !call_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->joinCall(
        call_id,
        [userdata, callback](bool ok, const anychat::CallSession& s, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatCallSession_C c_s{};
                callSessionToC(s, &c_s);
                callback(userdata, 1, &c_s, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_reject_call(
    AnyChatCallHandle handle,
    const char* call_id,
    void* userdata,
    AnyChatCallResultCallback callback
) {
    if (!handle || !handle->impl || !call_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->rejectCall(call_id, [userdata, callback](bool ok, const std::string& err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_end_call(
    AnyChatCallHandle handle,
    const char* call_id,
    void* userdata,
    AnyChatCallResultCallback callback
) {
    if (!handle || !handle->impl || !call_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->endCall(call_id, [userdata, callback](bool ok, const std::string& err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_get_call_session(
    AnyChatCallHandle handle,
    const char* call_id,
    void* userdata,
    AnyChatCallCallback callback
) {
    if (!handle || !handle->impl || !call_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getCallSession(
        call_id,
        [userdata, callback](bool ok, const anychat::CallSession& s, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatCallSession_C c_s{};
                callSessionToC(s, &c_s);
                callback(userdata, 1, &c_s, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_get_call_logs(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    void* userdata,
    AnyChatCallListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getCallLogs(
        page,
        page_size,
        [userdata, callback](const std::vector<anychat::CallSession>& calls, int64_t total, const std::string& err) {
            if (!callback)
                return;
            int count = static_cast<int>(calls.size());
            AnyChatCallList_C c_list{};
            c_list.count = count;
            c_list.total = total;
            c_list.items = count > 0
                               ? static_cast<AnyChatCallSession_C*>(std::calloc(count, sizeof(AnyChatCallSession_C)))
                               : nullptr;
            for (int i = 0; i < count; ++i)
                callSessionToC(calls[i], &c_list.items[i]);
            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_create_meeting(
    AnyChatCallHandle handle,
    const char* title,
    const char* password,
    int max_participants,
    void* userdata,
    AnyChatMeetingCallback callback
) {
    if (!handle || !handle->impl || !title) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->createMeeting(
        title,
        password ? password : "",
        max_participants,
        [userdata, callback](bool ok, const anychat::MeetingRoom& r, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatMeetingRoom_C c_r{};
                meetingRoomToC(r, &c_r);
                callback(userdata, 1, &c_r, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_join_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const char* password,
    void* userdata,
    AnyChatMeetingCallback callback
) {
    if (!handle || !handle->impl || !room_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->joinMeeting(
        room_id,
        password ? password : "",
        [userdata, callback](bool ok, const anychat::MeetingRoom& r, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatMeetingRoom_C c_r{};
                meetingRoomToC(r, &c_r);
                callback(userdata, 1, &c_r, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_end_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    void* userdata,
    AnyChatCallResultCallback callback
) {
    if (!handle || !handle->impl || !room_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->endMeeting(room_id, [userdata, callback](bool ok, const std::string& err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_get_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    void* userdata,
    AnyChatMeetingCallback callback
) {
    if (!handle || !handle->impl || !room_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getMeeting(
        room_id,
        [userdata, callback](bool ok, const anychat::MeetingRoom& r, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatMeetingRoom_C c_r{};
                meetingRoomToC(r, &c_r);
                callback(userdata, 1, &c_r, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_call_list_meetings(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    void* userdata,
    AnyChatMeetingListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->listMeetings(
        page,
        page_size,
        [userdata, callback](const std::vector<anychat::MeetingRoom>& rooms, int64_t total, const std::string& err) {
            if (!callback)
                return;
            int count = static_cast<int>(rooms.size());
            AnyChatMeetingList_C c_list{};
            c_list.count = count;
            c_list.total = total;
            c_list.items = count > 0
                               ? static_cast<AnyChatMeetingRoom_C*>(std::calloc(count, sizeof(AnyChatMeetingRoom_C)))
                               : nullptr;
            for (int i = 0; i < count; ++i)
                meetingRoomToC(rooms[i], &c_list.items[i]);
            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

void anychat_call_set_incoming_call_callback(
    AnyChatCallHandle handle,
    void* userdata,
    AnyChatIncomingCallCallback callback
) {
    if (!handle || !handle->impl)
        return;
    CallCbState* state = getOrCreateCallState(handle->impl);
    {
        std::lock_guard<std::mutex> lock(state->incoming_mutex);
        state->incoming_userdata = userdata;
        state->incoming_cb = callback;
    }
    if (callback) {
        handle->impl->setOnIncomingCall([state](const anychat::CallSession& session) {
            AnyChatIncomingCallCallback cb;
            void* ud;
            {
                std::lock_guard<std::mutex> lock(state->incoming_mutex);
                cb = state->incoming_cb;
                ud = state->incoming_userdata;
            }
            if (!cb)
                return;
            AnyChatCallSession_C c_s{};
            callSessionToC(session, &c_s);
            cb(ud, &c_s);
        });
    } else {
        handle->impl->setOnIncomingCall(nullptr);
    }
}

void anychat_call_set_call_status_changed_callback(
    AnyChatCallHandle handle,
    void* userdata,
    AnyChatCallStatusChangedCallback callback
) {
    if (!handle || !handle->impl)
        return;
    CallCbState* state = getOrCreateCallState(handle->impl);
    {
        std::lock_guard<std::mutex> lock(state->status_mutex);
        state->status_userdata = userdata;
        state->status_cb = callback;
    }
    if (callback) {
        handle->impl->setOnCallStatusChanged([state](const std::string& call_id, anychat::CallStatus status) {
            AnyChatCallStatusChangedCallback cb;
            void* ud;
            {
                std::lock_guard<std::mutex> lock(state->status_mutex);
                cb = state->status_cb;
                ud = state->status_userdata;
            }
            if (cb)
                cb(ud, call_id.c_str(), static_cast<int>(status));
        });
    } else {
        handle->impl->setOnCallStatusChanged(nullptr);
    }
}

} // extern "C"
