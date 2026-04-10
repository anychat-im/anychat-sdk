#include "anychat_c/auth_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>
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
    anychat_strlcpy(dst->device_type, src.device_type.c_str(), sizeof(dst->device_type));
    anychat_strlcpy(dst->client_version, src.client_version.c_str(), sizeof(dst->client_version));
    anychat_strlcpy(dst->last_login_ip, src.last_login_ip.c_str(), sizeof(dst->last_login_ip));
    dst->last_login_at_ms = src.last_login_at_ms;
    dst->is_current = src.is_current ? 1 : 0;
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
    const char* device_type,
    const char* client_version,
    void* userdata,
    AnyChatAuthCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!account || !password) {
        anychat_set_last_error("account and password must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->login(
        account,
        password,
        device_type ? device_type : "",
        client_version ? client_version : "",
        [parent, userdata, callback](bool success, const anychat::AuthToken& token, const std::string& error) {
            if (!callback)
                return;
            if (success) {
                // Store token in parent handle's buffer so it persists across async boundary
                AnyChatAuthToken_C* c_token_ptr = nullptr;
                if (parent) {
                    std::lock_guard<std::mutex> lock(parent->auth_token_mutex);
                    tokenToCStruct(token, &parent->auth_token_buffer);
                    c_token_ptr = &parent->auth_token_buffer;
                }
                callback(userdata, 1, c_token_ptr, "");
            } else {
                callback(userdata, 0, nullptr, ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_register(
    AnyChatAuthHandle handle,
    const char* phone_or_email,
    const char* password,
    const char* verify_code,
    const char* device_type,
    const char* nickname,
    const char* client_version,
    void* userdata,
    AnyChatAuthCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!phone_or_email || !password || !verify_code) {
        anychat_set_last_error("phone_or_email, password, and verify_code must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->registerUser(
        phone_or_email,
        password,
        verify_code,
        device_type ? device_type : "",
        nickname ? nickname : "",
        client_version ? client_version : "",
        [parent, userdata, callback](bool success, const anychat::AuthToken& token, const std::string& error) {
            if (!callback)
                return;
            if (success) {
                // Store token in parent handle's buffer so it persists across async boundary
                AnyChatAuthToken_C* c_token_ptr = nullptr;
                if (parent) {
                    std::lock_guard<std::mutex> lock(parent->auth_token_mutex);
                    tokenToCStruct(token, &parent->auth_token_buffer);
                    c_token_ptr = &parent->auth_token_buffer;
                }
                callback(userdata, 1, c_token_ptr, "");
            } else {
                callback(userdata, 0, nullptr, ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_logout(AnyChatAuthHandle handle, void* userdata, AnyChatResultCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->logout([parent, userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, success ? "" : ANYCHAT_STORE_ERROR(parent, auth_error, error));
        }
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_send_code(
    AnyChatAuthHandle handle,
    const char* target,
    const char* target_type,
    const char* purpose,
    void* userdata,
    AnyChatSendCodeCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!target || !target_type || !purpose) {
        anychat_set_last_error("target, target_type and purpose must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->sendVerificationCode(
        target,
        target_type,
        purpose,
        [parent, userdata, callback](bool success, const anychat::VerificationCodeResult& result, const std::string& error
        ) {
            if (!callback)
                return;
            if (success) {
                AnyChatVerificationCodeResult_C c_result{};
                verificationCodeToCStruct(result, &c_result);
                callback(userdata, 1, &c_result, "");
            } else {
                callback(userdata, 0, nullptr, ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_refresh_token(
    AnyChatAuthHandle handle,
    const char* refresh_token,
    void* userdata,
    AnyChatAuthCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!refresh_token) {
        anychat_set_last_error("refresh_token must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->refreshToken(
        refresh_token,
        [parent, userdata, callback](bool success, const anychat::AuthToken& token, const std::string& error) {
            if (!callback)
                return;
            if (success) {
                // Store token in parent handle's buffer so it persists across async boundary
                AnyChatAuthToken_C* c_token_ptr = nullptr;
                if (parent) {
                    std::lock_guard<std::mutex> lock(parent->auth_token_mutex);
                    tokenToCStruct(token, &parent->auth_token_buffer);
                    c_token_ptr = &parent->auth_token_buffer;
                }
                callback(userdata, 1, c_token_ptr, "");
            } else {
                callback(userdata, 0, nullptr, ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_change_password(
    AnyChatAuthHandle handle,
    const char* old_password,
    const char* new_password,
    void* userdata,
    AnyChatResultCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!old_password || !new_password) {
        anychat_set_last_error("passwords must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->changePassword(
        old_password,
        new_password,
        [parent, userdata, callback](bool success, const std::string& error) {
            if (callback) {
                callback(userdata, success ? 1 : 0, success ? "" : ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_reset_password(
    AnyChatAuthHandle handle,
    const char* account,
    const char* verify_code,
    const char* new_password,
    void* userdata,
    AnyChatResultCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!account || !verify_code || !new_password) {
        anychat_set_last_error("account, verify_code and new_password must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->resetPassword(
        account,
        verify_code,
        new_password,
        [parent, userdata, callback](bool success, const std::string& error) {
            if (callback) {
                callback(userdata, success ? 1 : 0, success ? "" : ANYCHAT_STORE_ERROR(parent, auth_error, error));
            }
        }
    );
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_get_device_list(AnyChatAuthHandle handle, void* userdata, AnyChatAuthDeviceListCallback callback) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->getDeviceList(
        [parent, userdata, callback](bool success, const std::vector<anychat::AuthDevice>& devices, const std::string& error) {
            if (!callback)
                return;

            if (!success) {
                callback(userdata, nullptr, ANYCHAT_STORE_ERROR(parent, auth_error, error));
                return;
            }

            AnyChatAuthDeviceList_C c_list{};
            c_list.count = static_cast<int>(devices.size());
            c_list.items =
                c_list.count > 0 ? static_cast<AnyChatAuthDevice_C*>(std::calloc(c_list.count, sizeof(AnyChatAuthDevice_C)))
                                 : nullptr;

            for (int i = 0; i < c_list.count; ++i) {
                authDeviceToCStruct(devices[static_cast<size_t>(i)], &c_list.items[i]);
            }

            callback(userdata, &c_list, nullptr);
            std::free(c_list.items);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_logout_device(
    AnyChatAuthHandle handle,
    const char* device_id,
    void* userdata,
    AnyChatResultCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!device_id) {
        anychat_set_last_error("device_id must not be NULL");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    auto* parent = handle->parent;
    handle->impl->logoutDevice(device_id, [parent, userdata, callback](bool success, const std::string& error) {
        if (callback) {
            callback(userdata, success ? 1 : 0, success ? "" : ANYCHAT_STORE_ERROR(parent, auth_error, error));
        }
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_is_logged_in(AnyChatAuthHandle handle) {
    if (!handle || !handle->impl)
        return 0;
    return handle->impl->isLoggedIn() ? 1 : 0;
}

int anychat_auth_get_current_token(AnyChatAuthHandle handle, AnyChatAuthToken_C* out_token) {
    if (!handle || !handle->impl || !out_token) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!handle->impl->isLoggedIn()) {
        anychat_set_last_error("not logged in");
        return ANYCHAT_ERROR_NOT_LOGGED_IN;
    }
    tokenToCStruct(handle->impl->currentToken(), out_token);
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_auth_set_listener(AnyChatAuthHandle handle, const AnyChatAuthListener_C* listener) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!listener) {
        handle->impl->setListener(nullptr);
        anychat_clear_last_error();
        return ANYCHAT_OK;
    }
    if (listener->struct_size < sizeof(AnyChatAuthListener_C)) {
        anychat_set_last_error("listener struct_size is too small");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatAuthListener_C copied = *listener;
    handle->impl->setListener(std::make_shared<CAuthListener>(copied));
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
