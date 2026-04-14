#include "anychat/user.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

void userInfoToC(const anychat::UserInfo& src, AnyChatUserInfo_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->username, src.username.c_str(), sizeof(dst->username));
    anychat_strlcpy(dst->avatar_url, src.avatar_url.c_str(), sizeof(dst->avatar_url));
    anychat_strlcpy(dst->signature, src.signature.c_str(), sizeof(dst->signature));
    dst->gender = src.gender;
    anychat_strlcpy(dst->region, src.region.c_str(), sizeof(dst->region));
    dst->is_friend = src.is_friend ? 1 : 0;
    dst->is_blocked = src.is_blocked ? 1 : 0;
}

void profileToC(const anychat::UserProfile& src, AnyChatUserProfile_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->nickname, src.nickname.c_str(), sizeof(dst->nickname));
    anychat_strlcpy(dst->avatar_url, src.avatar_url.c_str(), sizeof(dst->avatar_url));
    anychat_strlcpy(dst->phone, src.phone.c_str(), sizeof(dst->phone));
    anychat_strlcpy(dst->email, src.email.c_str(), sizeof(dst->email));
    anychat_strlcpy(dst->signature, src.signature.c_str(), sizeof(dst->signature));
    anychat_strlcpy(dst->region, src.region.c_str(), sizeof(dst->region));
    dst->gender = src.gender;
    dst->birthday_ms = src.birthday_ms;
    anychat_strlcpy(dst->qrcode_url, src.qrcode_url.c_str(), sizeof(dst->qrcode_url));
    dst->created_at_ms = src.created_at_ms;
}

void profileFromC(const AnyChatUserProfile_C* src, anychat::UserProfile& dst) {
    dst.user_id = src->user_id;
    dst.nickname = src->nickname;
    dst.avatar_url = src->avatar_url;
    dst.phone = src->phone;
    dst.email = src->email;
    dst.signature = src->signature;
    dst.region = src->region;
    dst.gender = src->gender;
    dst.birthday_ms = src->birthday_ms;
    dst.qrcode_url = src->qrcode_url;
}

void settingsToC(const anychat::UserSettings& src, AnyChatUserSettings_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    dst->notification_enabled = src.notification_enabled ? 1 : 0;
    dst->sound_enabled = src.sound_enabled ? 1 : 0;
    dst->vibration_enabled = src.vibration_enabled ? 1 : 0;
    dst->message_preview_enabled = src.message_preview_enabled ? 1 : 0;
    dst->friend_verify_required = src.friend_verify_required ? 1 : 0;
    dst->search_by_phone = src.search_by_phone ? 1 : 0;
    dst->search_by_id = src.search_by_id ? 1 : 0;
    anychat_strlcpy(dst->language, src.language.c_str(), sizeof(dst->language));
}

void settingsFromC(const AnyChatUserSettings_C* src, anychat::UserSettings& dst) {
    dst.user_id = src->user_id;
    dst.notification_enabled = src->notification_enabled != 0;
    dst.sound_enabled = src->sound_enabled != 0;
    dst.vibration_enabled = src->vibration_enabled != 0;
    dst.message_preview_enabled = src->message_preview_enabled != 0;
    dst.friend_verify_required = src->friend_verify_required != 0;
    dst.search_by_phone = src->search_by_phone != 0;
    dst.search_by_id = src->search_by_id != 0;
    dst.language = src->language;
}

void qrcodeToC(const anychat::UserQRCode& src, AnyChatUserQRCode_C* dst) {
    anychat_strlcpy(dst->qrcode_url, src.qrcode_url.c_str(), sizeof(dst->qrcode_url));
    dst->expires_at_ms = src.expires_at_ms;
}

void bindPhoneToC(const anychat::BindPhoneResult& src, AnyChatBindPhoneResult_C* dst) {
    anychat_strlcpy(dst->phone_number, src.phone_number.c_str(), sizeof(dst->phone_number));
    dst->is_primary = src.is_primary ? 1 : 0;
}

void changePhoneToC(const anychat::ChangePhoneResult& src, AnyChatChangePhoneResult_C* dst) {
    anychat_strlcpy(dst->old_phone_number, src.old_phone_number.c_str(), sizeof(dst->old_phone_number));
    anychat_strlcpy(dst->new_phone_number, src.new_phone_number.c_str(), sizeof(dst->new_phone_number));
}

void bindEmailToC(const anychat::BindEmailResult& src, AnyChatBindEmailResult_C* dst) {
    anychat_strlcpy(dst->email, src.email.c_str(), sizeof(dst->email));
    dst->is_primary = src.is_primary ? 1 : 0;
}

void changeEmailToC(const anychat::ChangeEmailResult& src, AnyChatChangeEmailResult_C* dst) {
    anychat_strlcpy(dst->old_email, src.old_email.c_str(), sizeof(dst->old_email));
    anychat_strlcpy(dst->new_email, src.new_email.c_str(), sizeof(dst->new_email));
}

void statusEventToC(const anychat::UserStatusEvent& src, AnyChatUserStatusEvent_C* dst) {
    anychat_strlcpy(dst->user_id, src.user_id.c_str(), sizeof(dst->user_id));
    anychat_strlcpy(dst->status, src.status.c_str(), sizeof(dst->status));
    dst->last_active_at_ms = src.last_active_at_ms;
    anychat_strlcpy(dst->platform, src.platform.c_str(), sizeof(dst->platform));
}

class CUserListener final : public anychat::UserListener {
public:
    explicit CUserListener(const AnyChatUserListener_C& listener)
        : listener_(listener) {}

    void onProfileUpdated(const anychat::UserInfo& info) override {
        if (!listener_.on_profile_updated) {
            return;
        }
        AnyChatUserInfo_C c_info{};
        userInfoToC(info, &c_info);
        listener_.on_profile_updated(listener_.userdata, &c_info);
    }

    void onFriendProfileChanged(const anychat::UserInfo& info) override {
        if (!listener_.on_friend_profile_changed) {
            return;
        }
        AnyChatUserInfo_C c_info{};
        userInfoToC(info, &c_info);
        listener_.on_friend_profile_changed(listener_.userdata, &c_info);
    }

    void onUserStatusChanged(const anychat::UserStatusEvent& event) override {
        if (!listener_.on_status_changed) {
            return;
        }
        AnyChatUserStatusEvent_C c_event{};
        statusEventToC(event, &c_event);
        listener_.on_status_changed(listener_.userdata, &c_event);
    }

private:
    AnyChatUserListener_C listener_{};
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
void invokeUserError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

template<typename CallbackStruct>
anychat::AnyChatCallback makeUserCallback(const CallbackStruct& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeUserError(callback, code, error);
    };
    return result;
}

template<typename CallbackStruct, typename Value, typename CValue, typename ConvertFn>
anychat::AnyChatValueCallback<Value> makeUserValueCallback(const CallbackStruct& callback, ConvertFn convert) {
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
        invokeUserError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

int anychat_user_get_profile(AnyChatUserHandle handle, const AnyChatUserProfileCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserProfileCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getProfile(
        makeUserValueCallback<AnyChatUserProfileCallback_C, anychat::UserProfile, AnyChatUserProfile_C>(
            callback_copy,
            profileToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_update_profile(
    AnyChatUserHandle handle,
    const AnyChatUserProfile_C* profile,
    const AnyChatUserProfileCallback_C* callback
) {
    if (!handle || !handle->impl || !profile) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserProfileCallback_C callback_copy = copyCallbackStruct(callback);
    anychat::UserProfile cpp_profile;
    profileFromC(profile, cpp_profile);
    handle->impl->updateProfile(
        cpp_profile,
        makeUserValueCallback<AnyChatUserProfileCallback_C, anychat::UserProfile, AnyChatUserProfile_C>(
            callback_copy,
            profileToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_get_settings(AnyChatUserHandle handle, const AnyChatUserSettingsCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserSettingsCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getSettings(
        makeUserValueCallback<AnyChatUserSettingsCallback_C, anychat::UserSettings, AnyChatUserSettings_C>(
            callback_copy,
            settingsToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_update_settings(
    AnyChatUserHandle handle,
    const AnyChatUserSettings_C* settings,
    const AnyChatUserSettingsCallback_C* callback
) {
    if (!handle || !handle->impl || !settings) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserSettingsCallback_C callback_copy = copyCallbackStruct(callback);
    anychat::UserSettings cpp_settings;
    settingsFromC(settings, cpp_settings);
    handle->impl->updateSettings(
        cpp_settings,
        makeUserValueCallback<AnyChatUserSettingsCallback_C, anychat::UserSettings, AnyChatUserSettings_C>(
            callback_copy,
            settingsToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_update_push_token(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    const AnyChatUserCallback_C* callback
) {
    if (!handle || !handle->impl || !push_token || !platform) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->updatePushToken(push_token, platform, makeUserCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_user_update_push_token_with_device(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    const char* device_id,
    const AnyChatUserCallback_C* callback
) {
    if (!handle || !handle->impl || !push_token || !platform || !device_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->updatePushToken(
        push_token,
        platform,
        device_id,
        makeUserCallback(callback_copy)
    );
    return ANYCHAT_OK;
}

int anychat_user_search(
    AnyChatUserHandle handle,
    const char* keyword,
    int page,
    int page_size,
    const AnyChatUserListCallback_C* callback
) {
    if (!handle || !handle->impl || !keyword) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserListCallback_C callback_copy = copyCallbackStruct(callback);
    anychat::AnyChatValueCallback<anychat::UserSearchResult> cb{};
    cb.on_success = [callback_copy](const anychat::UserSearchResult& result) {
        if (!callback_copy.on_success) {
            return;
        }
        const int count = static_cast<int>(result.users.size());
        AnyChatUserList_C c_list{};
        c_list.count = count;
        c_list.total = result.total;
        c_list.items =
            count > 0 ? static_cast<AnyChatUserInfo_C*>(std::calloc(count, sizeof(AnyChatUserInfo_C))) : nullptr;
        for (int i = 0; i < count; ++i) {
            userInfoToC(result.users[i], &c_list.items[i]);
        }
        callback_copy.on_success(callback_copy.userdata, &c_list);
        std::free(c_list.items);
    };
    cb.on_error = [callback_copy](int code, const std::string& error) {
        invokeUserError(callback_copy, code, error);
    };
    handle->impl->searchUsers(keyword, page, page_size, std::move(cb));
    return ANYCHAT_OK;
}

int anychat_user_get_info(
    AnyChatUserHandle handle,
    const char* user_id,
    const AnyChatUserInfoCallback_C* callback
) {
    if (!handle || !handle->impl || !user_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserInfoCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getUserInfo(
        user_id,
        makeUserValueCallback<AnyChatUserInfoCallback_C, anychat::UserInfo, AnyChatUserInfo_C>(callback_copy, userInfoToC)
    );
    return ANYCHAT_OK;
}

int anychat_user_bind_phone(
    AnyChatUserHandle handle,
    const char* phone_number,
    const char* verify_code,
    const AnyChatBindPhoneCallback_C* callback
) {
    if (!handle || !handle->impl || !phone_number || !verify_code) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatBindPhoneCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->bindPhone(
        phone_number,
        verify_code,
        makeUserValueCallback<AnyChatBindPhoneCallback_C, anychat::BindPhoneResult, AnyChatBindPhoneResult_C>(
            callback_copy,
            bindPhoneToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_change_phone(
    AnyChatUserHandle handle,
    const char* old_phone_number,
    const char* new_phone_number,
    const char* new_verify_code,
    const char* old_verify_code,
    const AnyChatChangePhoneCallback_C* callback
) {
    if (!handle || !handle->impl || !old_phone_number || !new_phone_number || !new_verify_code) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatChangePhoneCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->changePhone(
        old_phone_number,
        new_phone_number,
        new_verify_code,
        old_verify_code ? old_verify_code : "",
        makeUserValueCallback<AnyChatChangePhoneCallback_C, anychat::ChangePhoneResult, AnyChatChangePhoneResult_C>(
            callback_copy,
            changePhoneToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_bind_email(
    AnyChatUserHandle handle,
    const char* email,
    const char* verify_code,
    const AnyChatBindEmailCallback_C* callback
) {
    if (!handle || !handle->impl || !email || !verify_code) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatBindEmailCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->bindEmail(
        email,
        verify_code,
        makeUserValueCallback<AnyChatBindEmailCallback_C, anychat::BindEmailResult, AnyChatBindEmailResult_C>(
            callback_copy,
            bindEmailToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_change_email(
    AnyChatUserHandle handle,
    const char* old_email,
    const char* new_email,
    const char* new_verify_code,
    const char* old_verify_code,
    const AnyChatChangeEmailCallback_C* callback
) {
    if (!handle || !handle->impl || !old_email || !new_email || !new_verify_code) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatChangeEmailCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->changeEmail(
        old_email,
        new_email,
        new_verify_code,
        old_verify_code ? old_verify_code : "",
        makeUserValueCallback<AnyChatChangeEmailCallback_C, anychat::ChangeEmailResult, AnyChatChangeEmailResult_C>(
            callback_copy,
            changeEmailToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_refresh_qrcode(AnyChatUserHandle handle, const AnyChatUserQRCodeCallback_C* callback) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserQRCodeCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->refreshQRCode(
        makeUserValueCallback<AnyChatUserQRCodeCallback_C, anychat::UserQRCode, AnyChatUserQRCode_C>(
            callback_copy,
            qrcodeToC
        )
    );
    return ANYCHAT_OK;
}

int anychat_user_get_by_qrcode(
    AnyChatUserHandle handle,
    const char* qrcode,
    const AnyChatUserInfoCallback_C* callback
) {
    if (!handle || !handle->impl || !qrcode) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatUserInfoCallback_C callback_copy = copyCallbackStruct(callback);
    handle->impl->getUserByQRCode(
        qrcode,
        makeUserValueCallback<AnyChatUserInfoCallback_C, anychat::UserInfo, AnyChatUserInfo_C>(callback_copy, userInfoToC)
    );
    return ANYCHAT_OK;
}

int anychat_user_set_listener(AnyChatUserHandle handle, const AnyChatUserListener_C* listener) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatUserListener_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatUserListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CUserListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
