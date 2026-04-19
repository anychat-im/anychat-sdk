#include "file_manager.h"
#include "utils_c.h"

#include "anychat/file.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>


namespace {

void fileInfoToC(const anychat::FileInfo& src, AnyChatFileInfo_C* dst) {
    anychat_strlcpy(dst->file_id, src.file_id.c_str(), sizeof(dst->file_id));
    anychat_strlcpy(dst->file_name, src.file_name.c_str(), sizeof(dst->file_name));
    dst->file_type = src.file_type;
    anychat_strlcpy(dst->mime_type, src.mime_type.c_str(), sizeof(dst->mime_type));
    anychat_strlcpy(dst->download_url, src.download_url.c_str(), sizeof(dst->download_url));
    dst->file_size_bytes = src.file_size_bytes;
    dst->created_at_ms = src.created_at_ms;
}

template<typename CallbackStruct>
bool validateCallbackStruct(const CallbackStruct* callback) {
    if (callback) {
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
void invokeFileError(const CallbackStruct& callback, int code, const std::string& error) {
    if (!callback.on_error) {
        return;
    }
    callback.on_error(callback.userdata, code, error.empty() ? nullptr : error.c_str());
}

anychat::AnyChatCallback makeFileCallback(const AnyChatFileCallback_C& callback) {
    anychat::AnyChatCallback result{};
    result.on_success = [callback]() {
        if (callback.on_success) {
            callback.on_success(callback.userdata);
        }
    };
    result.on_error = [callback](int code, const std::string& error) {
        invokeFileError(callback, code, error);
    };
    return result;
}

} // namespace

extern "C" {

int anychat_file_upload(
    AnyChatFileHandle handle,
    const char* local_path,
    int32_t file_type,
    AnyChatUploadProgressCallback on_progress,
    const AnyChatFileInfoCallback_C* on_done
) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl || !local_path) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(on_done)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatFileInfoCallback_C callback_copy = copyCallbackStruct(on_done);

    impl->upload(
        local_path,
        file_type,
        [userdata = callback_copy.userdata, on_progress](int64_t uploaded, int64_t total) {
            if (on_progress)
                on_progress(userdata, uploaded, total);
        },
        anychat::AnyChatValueCallback<anychat::FileInfo>{
            .on_success =
                [callback_copy](const anychat::FileInfo& info) {
                    if (!callback_copy.on_success) {
                        return;
                    }
                    AnyChatFileInfo_C c_info{};
                    fileInfoToC(info, &c_info);
                    callback_copy.on_success(callback_copy.userdata, &c_info);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeFileError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_file_get_download_url(
    AnyChatFileHandle handle,
    const char* file_id,
    const AnyChatDownloadUrlCallback_C* callback
) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl || !file_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatDownloadUrlCallback_C callback_copy = copyCallbackStruct(callback);
    impl->getDownloadUrl(
        file_id,
        anychat::AnyChatValueCallback<std::string>{
            .on_success =
                [callback_copy](const std::string& url) {
                    if (callback_copy.on_success) {
                        callback_copy.on_success(callback_copy.userdata, url.c_str());
                    }
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeFileError(callback_copy, code, error);
                },
        }
    );
    return ANYCHAT_OK;
}

int anychat_file_get_info(AnyChatFileHandle handle, const char* file_id, const AnyChatFileInfoCallback_C* callback) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl || !file_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatFileInfoCallback_C callback_copy = copyCallbackStruct(callback);
    impl->getFileInfo(
        file_id,
        anychat::AnyChatValueCallback<anychat::FileInfo>{
            .on_success =
                [callback_copy](const anychat::FileInfo& info) {
                    if (!callback_copy.on_success)
                        return;
                    AnyChatFileInfo_C c_info{};
                    fileInfoToC(info, &c_info);
                    callback_copy.on_success(callback_copy.userdata, &c_info);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeFileError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_file_list(
    AnyChatFileHandle handle,
    int32_t file_type,
    int page,
    int page_size,
    const AnyChatFileListCallback_C* callback
) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const int safe_page = page > 0 ? page : 1;
    const int safe_page_size = page_size > 0 ? page_size : 20;
    const AnyChatFileListCallback_C callback_copy = copyCallbackStruct(callback);
    impl->listFiles(
        file_type,
        safe_page,
        safe_page_size,
        anychat::AnyChatValueCallback<anychat::FileListResult>{
            .on_success =
                [callback_copy, safe_page, safe_page_size](const anychat::FileListResult& result) {
                    if (!callback_copy.on_success)
                        return;

                    AnyChatFileList_C c_list{};
                    c_list.count = static_cast<int>(result.files.size());
                    c_list.total = result.total;
                    c_list.page = result.page > 0 ? result.page : safe_page;
                    c_list.page_size = result.page_size > 0 ? result.page_size : safe_page_size;
                    c_list.items =
                        c_list.count > 0
                            ? static_cast<AnyChatFileInfo_C*>(std::calloc(c_list.count, sizeof(AnyChatFileInfo_C)))
                            : nullptr;

                    for (int i = 0; i < c_list.count; ++i) {
                        fileInfoToC(result.files[static_cast<size_t>(i)], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeFileError(callback_copy, code, error);
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_file_upload_log(
    AnyChatFileHandle handle,
    const char* local_path,
    int32_t expires_hours,
    AnyChatUploadProgressCallback on_progress,
    const AnyChatFileInfoCallback_C* on_done
) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl || !local_path) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(on_done)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const AnyChatFileInfoCallback_C callback_copy = copyCallbackStruct(on_done);

    impl->uploadClientLog(
        local_path,
        [userdata = callback_copy.userdata, on_progress](int64_t uploaded, int64_t total) {
            if (on_progress)
                on_progress(userdata, uploaded, total);
        },
        anychat::AnyChatValueCallback<anychat::FileInfo>{
            .on_success =
                [callback_copy](const anychat::FileInfo& info) {
                    if (!callback_copy.on_success)
                        return;
                    AnyChatFileInfo_C c_info{};
                    fileInfoToC(info, &c_info);
                    callback_copy.on_success(callback_copy.userdata, &c_info);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    invokeFileError(callback_copy, code, error);
                },
        },
        expires_hours
    );

    return ANYCHAT_OK;
}

int anychat_file_delete(AnyChatFileHandle handle, const char* file_id, const AnyChatFileCallback_C* callback) {
    auto* impl = static_cast<anychat::FileManagerImpl*>(handle);
    if (!impl || !file_id) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (!validateCallbackStruct(callback)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    const AnyChatFileCallback_C callback_copy = copyCallbackStruct(callback);
    impl->deleteFile(file_id, makeFileCallback(callback_copy));
    return ANYCHAT_OK;
}

} // extern "C"
