#include "anychat_c/user_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>

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

} // namespace

extern "C" {

int anychat_user_get_profile(AnyChatUserHandle handle, void* userdata, AnyChatUserProfileCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getProfile([userdata, callback](bool ok, const anychat::UserProfile& p, const std::string& err) {
        if (!callback)
            return;
        if (ok) {
            AnyChatUserProfile_C c_p{};
            profileToC(p, &c_p);
            callback(userdata, 1, &c_p, "");
        } else {
            callback(userdata, 0, nullptr, err.c_str());
        }
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_update_profile(
    AnyChatUserHandle handle,
    const AnyChatUserProfile_C* profile,
    void* userdata,
    AnyChatUserProfileCallback callback
) {
    if (!handle || !handle->impl || !profile) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    anychat::UserProfile cpp_profile;
    profileFromC(profile, cpp_profile);
    handle->impl->updateProfile(
        cpp_profile,
        [userdata, callback](bool ok, const anychat::UserProfile& p, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatUserProfile_C c_p{};
                profileToC(p, &c_p);
                callback(userdata, 1, &c_p, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_get_settings(AnyChatUserHandle handle, void* userdata, AnyChatUserSettingsCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getSettings([userdata, callback](bool ok, const anychat::UserSettings& s, const std::string& err) {
        if (!callback)
            return;
        if (ok) {
            AnyChatUserSettings_C c_s{};
            settingsToC(s, &c_s);
            callback(userdata, 1, &c_s, "");
        } else {
            callback(userdata, 0, nullptr, err.c_str());
        }
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_update_settings(
    AnyChatUserHandle handle,
    const AnyChatUserSettings_C* settings,
    void* userdata,
    AnyChatUserSettingsCallback callback
) {
    if (!handle || !handle->impl || !settings) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    anychat::UserSettings cpp_settings;
    settingsFromC(settings, cpp_settings);
    handle->impl->updateSettings(
        cpp_settings,
        [userdata, callback](bool ok, const anychat::UserSettings& s, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatUserSettings_C c_s{};
                settingsToC(s, &c_s);
                callback(userdata, 1, &c_s, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_update_push_token(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    void* userdata,
    AnyChatUserResultCallback callback
) {
    if (!handle || !handle->impl || !push_token || !platform) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->updatePushToken(push_token, platform, [userdata, callback](bool ok, const std::string& err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_update_push_token_with_device(
    AnyChatUserHandle handle,
    const char* push_token,
    const char* platform,
    const char* device_id,
    void* userdata,
    AnyChatUserResultCallback callback
) {
    if (!handle || !handle->impl || !push_token || !platform || !device_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->updatePushToken(
        push_token,
        platform,
        device_id,
        [userdata, callback](bool ok, const std::string& err) {
            if (callback)
                callback(userdata, ok ? 1 : 0, err.c_str());
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_search(
    AnyChatUserHandle handle,
    const char* keyword,
    int page,
    int page_size,
    void* userdata,
    AnyChatUserListCallback callback
) {
    if (!handle || !handle->impl || !keyword) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->searchUsers(
        keyword,
        page,
        page_size,
        [userdata, callback](const std::vector<anychat::UserInfo>& users, int64_t total, const std::string& err) {
            if (!callback)
                return;
            const int count = static_cast<int>(users.size());
            AnyChatUserList_C c_list{};
            c_list.count = count;
            c_list.total = total;
            c_list.items =
                count > 0 ? static_cast<AnyChatUserInfo_C*>(std::calloc(count, sizeof(AnyChatUserInfo_C))) : nullptr;
            for (int i = 0; i < count; ++i)
                userInfoToC(users[i], &c_list.items[i]);
            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_get_info(
    AnyChatUserHandle handle,
    const char* user_id,
    void* userdata,
    AnyChatUserInfoCallback callback
) {
    if (!handle || !handle->impl || !user_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getUserInfo(
        user_id,
        [userdata, callback](bool ok, const anychat::UserInfo& info, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatUserInfo_C c_info{};
                userInfoToC(info, &c_info);
                callback(userdata, 1, &c_info, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_bind_phone(
    AnyChatUserHandle handle,
    const char* phone_number,
    const char* verify_code,
    void* userdata,
    AnyChatBindPhoneCallback callback
) {
    if (!handle || !handle->impl || !phone_number || !verify_code) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->bindPhone(
        phone_number,
        verify_code,
        [userdata, callback](bool ok, const anychat::BindPhoneResult& result, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatBindPhoneResult_C c_result{};
                bindPhoneToC(result, &c_result);
                callback(userdata, 1, &c_result, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_change_phone(
    AnyChatUserHandle handle,
    const char* old_phone_number,
    const char* new_phone_number,
    const char* new_verify_code,
    const char* old_verify_code,
    void* userdata,
    AnyChatChangePhoneCallback callback
) {
    if (!handle || !handle->impl || !old_phone_number || !new_phone_number || !new_verify_code) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->changePhone(
        old_phone_number,
        new_phone_number,
        new_verify_code,
        old_verify_code ? old_verify_code : "",
        [userdata, callback](bool ok, const anychat::ChangePhoneResult& result, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatChangePhoneResult_C c_result{};
                changePhoneToC(result, &c_result);
                callback(userdata, 1, &c_result, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_bind_email(
    AnyChatUserHandle handle,
    const char* email,
    const char* verify_code,
    void* userdata,
    AnyChatBindEmailCallback callback
) {
    if (!handle || !handle->impl || !email || !verify_code) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->bindEmail(
        email,
        verify_code,
        [userdata, callback](bool ok, const anychat::BindEmailResult& result, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatBindEmailResult_C c_result{};
                bindEmailToC(result, &c_result);
                callback(userdata, 1, &c_result, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_change_email(
    AnyChatUserHandle handle,
    const char* old_email,
    const char* new_email,
    const char* new_verify_code,
    const char* old_verify_code,
    void* userdata,
    AnyChatChangeEmailCallback callback
) {
    if (!handle || !handle->impl || !old_email || !new_email || !new_verify_code) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->changeEmail(
        old_email,
        new_email,
        new_verify_code,
        old_verify_code ? old_verify_code : "",
        [userdata, callback](bool ok, const anychat::ChangeEmailResult& result, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatChangeEmailResult_C c_result{};
                changeEmailToC(result, &c_result);
                callback(userdata, 1, &c_result, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_refresh_qrcode(AnyChatUserHandle handle, void* userdata, AnyChatUserQRCodeCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->refreshQRCode([userdata, callback](bool ok, const anychat::UserQRCode& qrcode, const std::string& err) {
        if (!callback)
            return;
        if (ok) {
            AnyChatUserQRCode_C c_qrcode{};
            qrcodeToC(qrcode, &c_qrcode);
            callback(userdata, 1, &c_qrcode, "");
        } else {
            callback(userdata, 0, nullptr, err.c_str());
        }
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_get_by_qrcode(
    AnyChatUserHandle handle,
    const char* qrcode,
    void* userdata,
    AnyChatUserInfoCallback callback
) {
    if (!handle || !handle->impl || !qrcode) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getUserByQRCode(
        qrcode,
        [userdata, callback](bool ok, const anychat::UserInfo& info, const std::string& err) {
            if (!callback)
                return;
            if (ok) {
                AnyChatUserInfo_C c_info{};
                userInfoToC(info, &c_info);
                callback(userdata, 1, &c_info, "");
            } else {
                callback(userdata, 0, nullptr, err.c_str());
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_user_set_listener(AnyChatUserHandle handle, const AnyChatUserListener_C* listener) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        anychat_clear_last_error();
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatUserListener_C)) {
        anychat_set_last_error("listener struct_size is too small");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatUserListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CUserListener>(copied));
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
