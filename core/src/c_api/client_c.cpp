#include "anychat_c/client_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstring>

static int connectionStateToC(anychat::ConnectionState s) {
    switch (s) {
        case anychat::ConnectionState::Disconnected:
            return ANYCHAT_STATE_DISCONNECTED;
        case anychat::ConnectionState::Connecting:
            return ANYCHAT_STATE_CONNECTING;
        case anychat::ConnectionState::Connected:
            return ANYCHAT_STATE_CONNECTED;
        case anychat::ConnectionState::Reconnecting:
            return ANYCHAT_STATE_RECONNECTING;
    }
    return ANYCHAT_STATE_DISCONNECTED;
}

extern "C" {

AnyChatClientHandle anychat_client_create(const AnyChatClientConfig_C* config) {
    if (!config) {
        anychat_set_last_error("config must not be NULL");
        return nullptr;
    }
    if (!config->gateway_url || config->gateway_url[0] == '\0') {
        anychat_set_last_error("gateway_url must not be empty");
        return nullptr;
    }
    if (!config->api_base_url || config->api_base_url[0] == '\0') {
        anychat_set_last_error("api_base_url must not be empty");
        return nullptr;
    }
    if (!config->device_id || config->device_id[0] == '\0') {
        anychat_set_last_error("device_id must not be empty");
        return nullptr;
    }

    try {
        anychat::ClientConfig cpp_config;
        cpp_config.gateway_url = config->gateway_url;
        cpp_config.api_base_url = config->api_base_url;
        cpp_config.device_id = config->device_id;
        if (config->db_path)
            cpp_config.db_path = config->db_path;
        if (config->connect_timeout_ms > 0)
            cpp_config.connect_timeout_ms = config->connect_timeout_ms;
        if (config->max_reconnect_attempts > 0)
            cpp_config.max_reconnect_attempts = config->max_reconnect_attempts;
        cpp_config.auto_reconnect = (config->auto_reconnect != 0);

        auto cpp_client = anychat::AnyChatClient::create(cpp_config);

        auto* handle = new AnyChatClient_T();
        handle->impl = std::move(cpp_client);

        handle->auth_handle = { &handle->impl->authMgr(), handle };
        handle->msg_handle = { &handle->impl->messageMgr(), handle };
        handle->conv_handle = { &handle->impl->conversationMgr(), handle };
        handle->friend_handle = { &handle->impl->friendMgr(), handle };
        handle->group_handle = { &handle->impl->groupMgr(), handle };
        handle->file_handle = { &handle->impl->fileMgr(), handle };
        handle->user_handle = { &handle->impl->userMgr(), handle };
        handle->rtc_handle = { &handle->impl->rtcMgr(), handle };

        anychat_clear_last_error();
        return handle;
    } catch (const std::exception& e) {
        anychat_set_last_error(e.what());
        return nullptr;
    }
}

void anychat_client_destroy(AnyChatClientHandle handle) {
    delete handle;
}

// ---- Authentication & Connection ------------------------------------------

int anychat_client_login(
    AnyChatClientHandle handle,
    const char* account,
    const char* password,
    const char* device_type,
    const char* client_version,
    void* userdata,
    AnyChatAuthCallback callback
) {
    if (!handle || !account || !password || !device_type) {
        anychat_set_last_error("Invalid parameters");
        return -1;
    }

    try {
        handle->impl->login(
            account,
            password,
            device_type,
            client_version ? client_version : "",
            [handle, callback, userdata](bool success, const anychat::AuthToken& token, const std::string& error) {
                if (!callback)
                    return;

                AnyChatAuthToken_C* c_token_ptr = nullptr;
                const char* error_ptr = nullptr;

                if (success) {
                    // Store token in handle's buffer so it persists across async boundary
                    std::lock_guard<std::mutex> lock(handle->auth_token_mutex);
                    strncpy(
                        handle->auth_token_buffer.access_token,
                        token.access_token.c_str(),
                        sizeof(handle->auth_token_buffer.access_token) - 1
                    );
                    strncpy(
                        handle->auth_token_buffer.refresh_token,
                        token.refresh_token.c_str(),
                        sizeof(handle->auth_token_buffer.refresh_token) - 1
                    );
                    handle->auth_token_buffer.expires_at_ms = token.expires_at_ms;
                    c_token_ptr = &handle->auth_token_buffer;
                } else {
                    error_ptr = ANYCHAT_STORE_ERROR(handle, client_error, error);
                }

                callback(userdata, success ? 1 : 0, c_token_ptr, error_ptr);
            }
        );
        anychat_clear_last_error();
        return 0;
    } catch (const std::exception& e) {
        anychat_set_last_error(e.what());
        return -1;
    }
}

int anychat_client_logout(AnyChatClientHandle handle, void* userdata, AnyChatResultCallback callback) {
    if (!handle) {
        anychat_set_last_error("Invalid handle");
        return -1;
    }

    try {
        handle->impl->logout([handle, callback, userdata](bool success, const std::string& error) {
            if (!callback)
                return;

            const char* error_ptr = success ? nullptr : ANYCHAT_STORE_ERROR(handle, client_error, error);
            callback(userdata, success ? 1 : 0, error_ptr);
        });
        anychat_clear_last_error();
        return 0;
    } catch (const std::exception& e) {
        anychat_set_last_error(e.what());
        return -1;
    }
}

int anychat_client_is_logged_in(AnyChatClientHandle handle) {
    if (!handle)
        return 0;
    return handle->impl->isLoggedIn() ? 1 : 0;
}

int anychat_client_get_current_token(AnyChatClientHandle handle, AnyChatAuthToken_C* out_token) {
    if (!handle || !out_token) {
        anychat_set_last_error("Invalid parameters");
        return -1;
    }

    try {
        auto token = handle->impl->getCurrentToken();
        strncpy(out_token->access_token, token.access_token.c_str(), sizeof(out_token->access_token) - 1);
        strncpy(out_token->refresh_token, token.refresh_token.c_str(), sizeof(out_token->refresh_token) - 1);
        out_token->expires_at_ms = token.expires_at_ms;
        anychat_clear_last_error();
        return 0;
    } catch (const std::exception& e) {
        anychat_set_last_error(e.what());
        return -1;
    }
}

int anychat_client_get_connection_state(AnyChatClientHandle handle) {
    if (!handle)
        return ANYCHAT_STATE_DISCONNECTED;
    return connectionStateToC(handle->impl->connectionState());
}

void anychat_client_set_connection_callback(
    AnyChatClientHandle handle,
    void* userdata,
    AnyChatConnectionStateCallback callback
) {
    if (!handle)
        return;

    {
        std::lock_guard<std::mutex> lock(handle->cb_mutex);
        handle->cb_userdata = userdata;
        handle->cb = callback;
    }

    if (callback) {
        handle->impl->setOnConnectionStateChanged([handle](anychat::ConnectionState state) {
            AnyChatConnectionStateCallback cb;
            void* ud;
            {
                std::lock_guard<std::mutex> lock(handle->cb_mutex);
                cb = handle->cb;
                ud = handle->cb_userdata;
            }
            if (cb)
                cb(ud, connectionStateToC(state));
        });
    } else {
        handle->impl->setOnConnectionStateChanged(nullptr);
    }
}

AnyChatAuthHandle anychat_client_get_auth(AnyChatClientHandle h) {
    return h ? &h->auth_handle : nullptr;
}
AnyChatMessageHandle anychat_client_get_message(AnyChatClientHandle h) {
    return h ? &h->msg_handle : nullptr;
}
AnyChatConvHandle anychat_client_get_conversation(AnyChatClientHandle h) {
    return h ? &h->conv_handle : nullptr;
}
AnyChatFriendHandle anychat_client_get_friend(AnyChatClientHandle h) {
    return h ? &h->friend_handle : nullptr;
}
AnyChatGroupHandle anychat_client_get_group(AnyChatClientHandle h) {
    return h ? &h->group_handle : nullptr;
}
AnyChatFileHandle anychat_client_get_file(AnyChatClientHandle h) {
    return h ? &h->file_handle : nullptr;
}
AnyChatUserHandle anychat_client_get_user(AnyChatClientHandle h) {
    return h ? &h->user_handle : nullptr;
}
AnyChatRtcHandle anychat_client_get_rtc(AnyChatClientHandle h) {
    return h ? &h->rtc_handle : nullptr;
}

} // extern "C"
