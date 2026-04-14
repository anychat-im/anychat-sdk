#include "anychat/call.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <memory>
#include <string>

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

class CCallListener final : public anychat::CallListener {
public:
    explicit CCallListener(const AnyChatCallListener_C& listener)
        : listener_(listener) {}

    void onIncomingCall(const anychat::CallSession& session) override {
        if (!listener_.on_incoming_call) {
            return;
        }
        AnyChatCallSession_C c_s{};
        callSessionToC(session, &c_s);
        listener_.on_incoming_call(listener_.userdata, &c_s);
    }

    void onCallStatusChanged(const std::string& call_id, anychat::CallStatus status) override {
        if (!listener_.on_call_status_changed) {
            return;
        }
        listener_.on_call_status_changed(listener_.userdata, call_id.c_str(), static_cast<int>(status));
    }

private:
    AnyChatCallListener_C listener_{};
};

template<typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback && callback->struct_size < sizeof(CallbackStruct)) {
        return false;
    }
    return true;
}

template<typename CallbackStruct>
CallbackStruct copyCallbackStruct(const CallbackStruct* callback) {
    CallbackStruct callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }
    return callback_copy;
}

template<typename CallbackStruct>
void invokeCallError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeCallCallback(const AnyChatCallCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeCallError(callback, code, error);
    };
    return result;
}

template<typename CallbackStruct, typename Value, typename CValue, typename ConvertFn>
anychat::AnyChatValueCallback<Value> makeCallPtrValueCallback(const CallbackStruct& callback, ConvertFn convert) {
    anychat::AnyChatValueCallback<Value> result{};
    result.on_success = [callback, convert](const Value& value) {
        if (!callback.on_success) {
            return;
        }
        CValue converted{};
        convert(value, &converted);
        callback.on_success(callback.userdata, &converted);
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeCallError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

int anychat_call_initiate_call(
    AnyChatCallHandle handle,
    const char* callee_id,
    int call_type,
    const AnyChatCallSessionCallback_C* callback
) {
    if (!handle || !handle->impl || !callee_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallSessionCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->initiateCall(
        callee_id,
        callTypeFromC(call_type),
        makeCallPtrValueCallback<AnyChatCallSessionCallback_C, anychat::CallSession, AnyChatCallSession_C>(
            callback_copy,
            callSessionToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_join_call(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallSessionCallback_C* callback
) {
    if (!handle || !handle->impl || !call_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallSessionCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->joinCall(
        call_id,
        makeCallPtrValueCallback<AnyChatCallSessionCallback_C, anychat::CallSession, AnyChatCallSession_C>(
            callback_copy,
            callSessionToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_reject_call(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallCallback_C* callback
) {
    if (!handle || !handle->impl || !call_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->rejectCall(call_id, makeCallCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_call_end_call(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallCallback_C* callback
) {
    if (!handle || !handle->impl || !call_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->endCall(call_id, makeCallCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_call_get_call_session(
    AnyChatCallHandle handle,
    const char* call_id,
    const AnyChatCallSessionCallback_C* callback
) {
    if (!handle || !handle->impl || !call_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallSessionCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->getCallSession(
        call_id,
        makeCallPtrValueCallback<AnyChatCallSessionCallback_C, anychat::CallSession, AnyChatCallSession_C>(
            callback_copy,
            callSessionToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_get_call_logs(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    const AnyChatCallListCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallListCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->getCallLogs(
        page,
        page_size,
        anychat::AnyChatValueCallback<anychat::CallLogResult>{
            .on_success =
                [callback_copy](const anychat::CallLogResult& result) {
                    if (!callback_copy.on_success)
                        return;
                    const int count = static_cast<int>(result.calls.size());
                    AnyChatCallList_C c_list{};
                    c_list.count = count;
                    c_list.total = result.total;
                    c_list.items = count > 0
                                       ? static_cast<AnyChatCallSession_C*>(std::calloc(count, sizeof(AnyChatCallSession_C)))
                                       : nullptr;
                    for (int i = 0; i < count; ++i)
                        callSessionToC(result.calls[static_cast<size_t>(i)], &c_list.items[i]);
                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeCallError(callback_copy, code, error);
                },
        }
    );
    return ANYCHAT_OK;
}

int anychat_call_create_meeting(
    AnyChatCallHandle handle,
    const char* title,
    const char* password,
    int max_participants,
    const AnyChatMeetingCallback_C* callback
) {
    if (!handle || !handle->impl || !title) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatMeetingCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->createMeeting(
        title,
        password ? password : "",
        max_participants,
        makeCallPtrValueCallback<AnyChatMeetingCallback_C, anychat::MeetingRoom, AnyChatMeetingRoom_C>(
            callback_copy,
            meetingRoomToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_join_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const char* password,
    const AnyChatMeetingCallback_C* callback
) {
    if (!handle || !handle->impl || !room_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatMeetingCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->joinMeeting(
        room_id,
        password ? password : "",
        makeCallPtrValueCallback<AnyChatMeetingCallback_C, anychat::MeetingRoom, AnyChatMeetingRoom_C>(
            callback_copy,
            meetingRoomToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_end_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const AnyChatCallCallback_C* callback
) {
    if (!handle || !handle->impl || !room_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatCallCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->endMeeting(room_id, makeCallCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_call_get_meeting(
    AnyChatCallHandle handle,
    const char* room_id,
    const AnyChatMeetingCallback_C* callback
) {
    if (!handle || !handle->impl || !room_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatMeetingCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->getMeeting(
        room_id,
        makeCallPtrValueCallback<AnyChatMeetingCallback_C, anychat::MeetingRoom, AnyChatMeetingRoom_C>(
            callback_copy,
            meetingRoomToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_call_list_meetings(
    AnyChatCallHandle handle,
    int page,
    int page_size,
    const AnyChatMeetingListCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatMeetingListCallback_C callback_copy = copyCallbackStruct(callback);

    handle->impl->listMeetings(
        page,
        page_size,
        anychat::AnyChatValueCallback<anychat::MeetingListResult>{
            .on_success =
                [callback_copy](const anychat::MeetingListResult& result) {
                    if (!callback_copy.on_success)
                        return;
                    const int count = static_cast<int>(result.rooms.size());
                    AnyChatMeetingList_C c_list{};
                    c_list.count = count;
                    c_list.total = result.total;
                    c_list.items = count > 0
                                       ? static_cast<AnyChatMeetingRoom_C*>(std::calloc(count, sizeof(AnyChatMeetingRoom_C)))
                                       : nullptr;
                    for (int i = 0; i < count; ++i)
                        meetingRoomToC(result.rooms[static_cast<size_t>(i)], &c_list.items[i]);
                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeCallError(callback_copy, code, error);
                },
        }
    );
    return ANYCHAT_OK;
}

int anychat_call_set_listener(AnyChatCallHandle handle, const AnyChatCallListener_C* listener) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatCallListener_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatCallListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CCallListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
