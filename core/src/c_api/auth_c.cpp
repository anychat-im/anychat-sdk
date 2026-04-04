#include "anychat_c/auth_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstring>

namespace {

void tokenToCStruct(const anychat::AuthToken& src, AnyChatAuthToken_C* dst) {
    anychat_strlcpy(dst->access_token, src.access_token.c_str(), sizeof(dst->access_token));
    anychat_strlcpy(dst->refresh_token, src.refresh_token.c_str(), sizeof(dst->refresh_token));
    dst->expires_at_ms = src.expires_at_ms;
}

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

void anychat_auth_set_on_expired(AnyChatAuthHandle handle, void* userdata, AnyChatAuthExpiredCallback callback) {
    if (!handle || !handle->impl)
        return;
    if (callback) {
        handle->impl->setOnAuthExpired([userdata, callback]() {
            callback(userdata);
        });
    } else {
        handle->impl->setOnAuthExpired(nullptr);
    }
}

} // extern "C"
