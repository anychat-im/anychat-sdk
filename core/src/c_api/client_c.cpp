#include "anychat/client.h"

#include "handles_c.h"
#include "utils_c.h"

#include <exception>
#include <mutex>
#include <string>

namespace {

int connectionStateToC(anychat::ConnectionState state) {
    switch (state) {
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

void tokenToC(const anychat::AuthToken& src, AnyChatAuthToken_C* dst) {
    anychat_strlcpy(dst->access_token, src.access_token.c_str(), sizeof(dst->access_token));
    anychat_strlcpy(dst->refresh_token, src.refresh_token.c_str(), sizeof(dst->refresh_token));
    dst->expires_at_ms = src.expires_at_ms;
}

template <typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback && callback->struct_size < sizeof(CallbackStruct)) {
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
void invokeClientError(const CallbackStruct& callback, int code, const std::string& error) {
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
        invokeClientError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

AnyChatClientHandle anychat_client_create(const AnyChatClientConfig_C* config) {
    if (!config) {
        return nullptr;
    }
    if (!config->gateway_url || config->gateway_url[0] == '\0') {
        return nullptr;
    }
    if (!config->api_base_url || config->api_base_url[0] == '\0') {
        return nullptr;
    }
    if (!config->device_id || config->device_id[0] == '\0') {
        return nullptr;
    }

    try {
        anychat::ClientConfig cpp_config;
        cpp_config.gateway_url = config->gateway_url;
        cpp_config.api_base_url = config->api_base_url;
        cpp_config.device_id = config->device_id;
        if (config->db_path) {
            cpp_config.db_path = config->db_path;
        }
        if (config->connect_timeout_ms > 0) {
            cpp_config.connect_timeout_ms = config->connect_timeout_ms;
        }
        if (config->max_reconnect_attempts > 0) {
            cpp_config.max_reconnect_attempts = config->max_reconnect_attempts;
        }
        cpp_config.auto_reconnect = config->auto_reconnect != 0;

        auto* handle = new AnyChatClient_T();
        handle->impl = std::make_unique<anychat::AnyChatClient>(cpp_config);

        handle->auth_handle = { &handle->impl->authMgr() };
        handle->msg_handle = { &handle->impl->messageMgr() };
        handle->conv_handle = { &handle->impl->conversationMgr() };
        handle->friend_handle = { &handle->impl->friendMgr() };
        handle->group_handle = { &handle->impl->groupMgr() };
        handle->file_handle = { &handle->impl->fileMgr() };
        handle->user_handle = { &handle->impl->userMgr() };
        handle->call_handle = { &handle->impl->callMgr() };
        handle->version_handle = { &handle->impl->versionMgr() };

        return handle;
    } catch (const std::exception&) {
        return nullptr;
    }
}

void anychat_client_destroy(AnyChatClientHandle handle) {
    delete handle;
}

int anychat_client_login(
    AnyChatClientHandle handle,
    const char* account,
    const char* password,
    const char* device_type,
    const char* client_version,
    const AnyChatAuthTokenCallback_C* callback
) {
    if (!handle || !account || !password || !device_type) {
        return -1;
    }
    if (!validateCallbackStruct(callback)) {
        return -1;
    }

    try {
        const AnyChatAuthTokenCallback_C callback_copy = copyCallbackStruct(callback);
        handle->impl->login(
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
                        tokenToC(token, &c_token);
                        callback_copy.on_success(callback_copy.userdata, &c_token);
                    },
                .on_error =
                    [callback_copy](int code, const std::string& error) {
                        invokeClientError(callback_copy, code, error);
                    },
            }
        );
        return 0;
    } catch (const std::exception&) {
        return -1;
    }
}

int anychat_client_logout(AnyChatClientHandle handle, const AnyChatAuthResultCallback_C* callback) {
    if (!handle) {
        return -1;
    }
    if (!validateCallbackStruct(callback)) {
        return -1;
    }

    try {
        const AnyChatAuthResultCallback_C callback_copy = copyCallbackStruct(callback);
        handle->impl->logout(makeResultCallback(callback_copy));
        return 0;
    } catch (const std::exception&) {
        return -1;
    }
}

int anychat_client_is_logged_in(AnyChatClientHandle handle) {
    if (!handle) {
        return 0;
    }
    return handle->impl->isLoggedIn() ? 1 : 0;
}

int anychat_client_get_current_token(AnyChatClientHandle handle, AnyChatAuthToken_C* out_token) {
    if (!handle || !out_token) {
        return -1;
    }

    try {
        tokenToC(handle->impl->getCurrentToken(), out_token);
        return 0;
    } catch (const std::exception&) {
        return -1;
    }
}

int anychat_client_get_connection_state(AnyChatClientHandle handle) {
    if (!handle) {
        return ANYCHAT_STATE_DISCONNECTED;
    }
    return connectionStateToC(handle->impl->connectionState());
}

void anychat_client_set_connection_callback(
    AnyChatClientHandle handle,
    void* userdata,
    AnyChatConnectionStateCallback callback
) {
    if (!handle) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(handle->cb_mutex);
        handle->cb_userdata = userdata;
        handle->cb = callback;
    }

    if (callback) {
        handle->impl->setOnConnectionStateChanged([handle](anychat::ConnectionState state) {
            AnyChatConnectionStateCallback cb;
            void* userdata_local = nullptr;
            {
                std::lock_guard<std::mutex> lock(handle->cb_mutex);
                cb = handle->cb;
                userdata_local = handle->cb_userdata;
            }
            if (cb) {
                cb(userdata_local, connectionStateToC(state));
            }
        });
    } else {
        handle->impl->setOnConnectionStateChanged(nullptr);
    }
}

AnyChatAuthHandle anychat_client_get_auth(AnyChatClientHandle handle) {
    return handle ? &handle->auth_handle : nullptr;
}

AnyChatMessageHandle anychat_client_get_message(AnyChatClientHandle handle) {
    return handle ? &handle->msg_handle : nullptr;
}

AnyChatConvHandle anychat_client_get_conversation(AnyChatClientHandle handle) {
    return handle ? &handle->conv_handle : nullptr;
}

AnyChatFriendHandle anychat_client_get_friend(AnyChatClientHandle handle) {
    return handle ? &handle->friend_handle : nullptr;
}

AnyChatGroupHandle anychat_client_get_group(AnyChatClientHandle handle) {
    return handle ? &handle->group_handle : nullptr;
}

AnyChatFileHandle anychat_client_get_file(AnyChatClientHandle handle) {
    return handle ? &handle->file_handle : nullptr;
}

AnyChatUserHandle anychat_client_get_user(AnyChatClientHandle handle) {
    return handle ? &handle->user_handle : nullptr;
}

AnyChatCallHandle anychat_client_get_call(AnyChatClientHandle handle) {
    return handle ? &handle->call_handle : nullptr;
}

AnyChatVersionHandle anychat_client_get_version(AnyChatClientHandle handle) {
    return handle ? &handle->version_handle : nullptr;
}

} // extern "C"
