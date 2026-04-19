#include "anychat/auth.h"

#include "auth_manager.h"
#include "utils_c.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

void tokenToCStruct(const anychat::AuthToken& src, AnyChatAuthToken_C* dst) {
    anychat_strlcpy(dst->access_token, src.access_token.c_str(), sizeof(dst->access_token));
    anychat_strlcpy(dst->refresh_token, src.refresh_token.c_str(), sizeof(dst->refresh_token));
    dst->expires_at_ms = src.expires_at_ms;
}

void verificationCodeToCStruct(const anychat::VerificationCodeResult& src, AnyChatVerificationCodeResult_C* dst) {
    anychat_strlcpy(dst->code_id, src.code_id.c_str(), sizeof(dst->code_id));
    dst->expires_in = src.expires_in;
}

void authDeviceToCStruct(const anychat::AuthDevice& src, AnyChatAuthDevice_C* dst) {
    anychat_strlcpy(dst->device_id, src.device_id.c_str(), sizeof(dst->device_id));
    dst->device_type = src.device_type;
    anychat_strlcpy(dst->client_version, src.client_version.c_str(), sizeof(dst->client_version));
    anychat_strlcpy(dst->last_login_ip, src.last_login_ip.c_str(), sizeof(dst->last_login_ip));
    dst->last_login_at_ms = src.last_login_at_ms;
    dst->is_current = src.is_current ? 1 : 0;
}

template <typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback) {
        return false;
    }
    return true;
}

template <typename CallbackStruct>
CallbackStruct copyCallbackStruct(const CallbackStruct* callback) {
    CallbackStruct callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }
    return callback_copy;
}

template <typename CallbackStruct>
void invokeAuthError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeResultCallback(const AnyChatAuthResultCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeAuthError(callback, code, error);
    };
    return result;
}

class CAuthListener final : public anychat::AuthListener {
public:
    explicit CAuthListener(const AnyChatAuthListener_C& listener)
        : listener_(listener) {}

    void onAuthExpired() override {
        if (listener_.on_auth_expired) {
            listener_.on_auth_expired(listener_.userdata);
        }
    }

private:
    AnyChatAuthListener_C listener_{};
};

} // namespace

extern "C" {

int anychat_auth_login(
    AnyChatAuthHandle handle,
    const char* account,
    const char* password,
    int32_t device_type,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!account || !password) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthTokenCallback_C callback_copy = copyCallbackStruct(callback);
    impl->login(
        account,
        password,
        device_type,
        client_version ? client_version : "",
        anychat::AnyChatValueCallback<anychat::AuthToken>{
            .on_success =
                [callback_copy](const anychat::AuthToken& token) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatAuthToken_C c_token{};
                    tokenToCStruct(token, &c_token);
                    callback_copy.on_success(callback_copy.userdata, &c_token);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeAuthError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_auth_register(
    AnyChatAuthHandle handle,
    const char* phone_or_email,
    const char* password,
    const char* verify_code,
    int32_t device_type,
    const char* nickname,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!phone_or_email || !password || !verify_code) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthTokenCallback_C callback_copy = copyCallbackStruct(callback);
    impl->registerUser(
        phone_or_email,
        password,
        verify_code,
        device_type,
        nickname ? nickname : "",
        client_version ? client_version : "",
        anychat::AnyChatValueCallback<anychat::AuthToken>{
            .on_success =
                [callback_copy](const anychat::AuthToken& token) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatAuthToken_C c_token{};
                    tokenToCStruct(token, &c_token);
                    callback_copy.on_success(callback_copy.userdata, &c_token);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeAuthError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_auth_logout(AnyChatAuthHandle handle, const AnyChatAuthResultCallback_C* callback) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthResultCallback_C callback_copy = copyCallbackStruct(callback);
    impl->logout(makeResultCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_auth_send_code(
    AnyChatAuthHandle handle,
    const char* target,
    int32_t target_type,
    int32_t purpose,
    const AnyChatVerificationCodeCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!target) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatVerificationCodeCallback_C callback_copy = copyCallbackStruct(callback);
    impl->sendVerificationCode(
        target,
        target_type,
        purpose,
        anychat::AnyChatValueCallback<anychat::VerificationCodeResult>{
            .on_success =
                [callback_copy](const anychat::VerificationCodeResult& result) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatVerificationCodeResult_C c_result{};
                    verificationCodeToCStruct(result, &c_result);
                    callback_copy.on_success(callback_copy.userdata, &c_result);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeAuthError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_auth_refresh_token(
    AnyChatAuthHandle handle,
    const char* refresh_token,
    const AnyChatAuthTokenCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!refresh_token) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthTokenCallback_C callback_copy = copyCallbackStruct(callback);
    impl->refreshToken(
        refresh_token,
        anychat::AnyChatValueCallback<anychat::AuthToken>{
            .on_success =
                [callback_copy](const anychat::AuthToken& token) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatAuthToken_C c_token{};
                    tokenToCStruct(token, &c_token);
                    callback_copy.on_success(callback_copy.userdata, &c_token);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeAuthError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_auth_change_password(
    AnyChatAuthHandle handle,
    const char* old_password,
    const char* new_password,
    const AnyChatAuthResultCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!old_password || !new_password) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthResultCallback_C callback_copy = copyCallbackStruct(callback);
    impl->changePassword(old_password, new_password, makeResultCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_auth_reset_password(
    AnyChatAuthHandle handle,
    const char* account,
    const char* verify_code,
    const char* new_password,
    const AnyChatAuthResultCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!account || !verify_code || !new_password) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthResultCallback_C callback_copy = copyCallbackStruct(callback);
    impl->resetPassword(account, verify_code, new_password, makeResultCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_auth_get_device_list(AnyChatAuthHandle handle, const AnyChatAuthDeviceListCallback_C* callback) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthDeviceListCallback_C callback_copy = copyCallbackStruct(callback);
    impl->getDeviceList(
        anychat::AnyChatValueCallback<std::vector<anychat::AuthDevice>>{
            .on_success =
                [callback_copy](const std::vector<anychat::AuthDevice>& devices) {
                    if (!callback_copy.on_success) {
                        return;
                    }

                    AnyChatAuthDeviceList_C c_list{};
                    c_list.count = static_cast<int>(devices.size());
                    c_list.items = c_list.count > 0
                        ? static_cast<AnyChatAuthDevice_C*>(std::calloc(c_list.count, sizeof(AnyChatAuthDevice_C)))
                        : nullptr;

                    for (int i = 0; i < c_list.count; ++i) {
                        authDeviceToCStruct(devices[static_cast<size_t>(i)], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeAuthError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_auth_logout_device(
    AnyChatAuthHandle handle,
    const char* device_id,
    const AnyChatAuthResultCallback_C* callback
) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!device_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatAuthResultCallback_C callback_copy = copyCallbackStruct(callback);
    impl->logoutDevice(device_id, makeResultCallback(callback_copy));
    return ANYCHAT_OK;
}

int anychat_auth_is_logged_in(AnyChatAuthHandle handle) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return 0;
    }
    return impl->isLoggedIn() ? 1 : 0;
}

int anychat_auth_get_current_token(AnyChatAuthHandle handle, AnyChatAuthToken_C* out_token) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl || !out_token) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!impl->isLoggedIn()) {
        return ANYCHAT_ERROR_NOT_LOGGED_IN;
    }
    tokenToCStruct(impl->currentToken(), out_token);
    return ANYCHAT_OK;
}

int anychat_auth_set_listener(AnyChatAuthHandle handle, const AnyChatAuthListener_C* listener) {
    auto* impl = static_cast<anychat::AuthManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        impl->setListener(nullptr);
        return ANYCHAT_OK;
    }

    AnyChatAuthListener_C copied = *listener;
    impl->setListener(std::make_shared<CAuthListener>(copied));
    return ANYCHAT_OK;
}

} // extern "C"
