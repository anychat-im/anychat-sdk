#include "anychat_c/file_c.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void fileInfoToC(const anychat::FileInfo& src, AnyChatFileInfo_C* dst) {
    anychat_strlcpy(dst->file_id, src.file_id.c_str(), sizeof(dst->file_id));
    anychat_strlcpy(dst->file_name, src.file_name.c_str(), sizeof(dst->file_name));
    anychat_strlcpy(dst->file_type, src.file_type.c_str(), sizeof(dst->file_type));
    anychat_strlcpy(dst->mime_type, src.mime_type.c_str(), sizeof(dst->mime_type));
    anychat_strlcpy(dst->download_url, src.download_url.c_str(), sizeof(dst->download_url));
    dst->file_size_bytes = src.file_size_bytes;
    dst->created_at_ms = src.created_at_ms;
}

} // namespace

extern "C" {

int anychat_file_upload(
    AnyChatFileHandle handle,
    const char* local_path,
    const char* file_type,
    void* userdata,
    AnyChatUploadProgressCallback on_progress,
    AnyChatFileInfoCallback on_done
) {
    if (!handle || !handle->impl || !local_path || !file_type) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->upload(
        local_path,
        file_type,
        [userdata, on_progress](int64_t uploaded, int64_t total) {
            if (on_progress)
                on_progress(userdata, uploaded, total);
        },
        [userdata, on_done](bool ok, anychat::FileInfo info, std::string err) {
            if (!on_done)
                return;
            if (ok) {
                AnyChatFileInfo_C c_info{};
                fileInfoToC(info, &c_info);
                on_done(userdata, 1, &c_info, "");
            } else {
                on_done(userdata, 0, nullptr, err.c_str());
            }
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_file_get_download_url(
    AnyChatFileHandle handle,
    const char* file_id,
    void* userdata,
    AnyChatDownloadUrlCallback callback
) {
    if (!handle || !handle->impl || !file_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->getDownloadUrl(file_id, [userdata, callback](bool ok, std::string url, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, ok ? url.c_str() : nullptr, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_file_get_info(
    AnyChatFileHandle handle,
    const char* file_id,
    void* userdata,
    AnyChatFileInfoCallback callback
) {
    if (!handle || !handle->impl || !file_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->getFileInfo(file_id, [userdata, callback](bool ok, anychat::FileInfo info, std::string err) {
        if (!callback)
            return;

        if (!ok) {
            callback(userdata, 0, nullptr, err.c_str());
            return;
        }

        AnyChatFileInfo_C c_info{};
        fileInfoToC(info, &c_info);
        callback(userdata, 1, &c_info, "");
    });

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_file_list(
    AnyChatFileHandle handle,
    const char* file_type,
    int page,
    int page_size,
    void* userdata,
    AnyChatFileListCallback callback
) {
    if (!handle || !handle->impl) {
        anychat_set_last_error("invalid handle");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    const int safe_page = page > 0 ? page : 1;
    const int safe_page_size = page_size > 0 ? page_size : 20;
    handle->impl->listFiles(
        file_type ? file_type : "",
        safe_page,
        safe_page_size,
        [userdata, callback, safe_page, safe_page_size](std::vector<anychat::FileInfo> files, int64_t total, std::string err) {
            if (!callback)
                return;

            AnyChatFileList_C c_list{};
            c_list.count = static_cast<int>(files.size());
            c_list.total = total;
            c_list.page = safe_page;
            c_list.page_size = safe_page_size;
            c_list.items =
                c_list.count > 0 ? static_cast<AnyChatFileInfo_C*>(std::calloc(c_list.count, sizeof(AnyChatFileInfo_C)))
                                 : nullptr;

            for (int i = 0; i < c_list.count; ++i) {
                fileInfoToC(files[static_cast<size_t>(i)], &c_list.items[i]);
            }

            callback(userdata, &c_list, err.empty() ? nullptr : err.c_str());
            std::free(c_list.items);
        }
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_file_upload_log(
    AnyChatFileHandle handle,
    const char* local_path,
    int32_t expires_hours,
    void* userdata,
    AnyChatUploadProgressCallback on_progress,
    AnyChatFileInfoCallback on_done
) {
    if (!handle || !handle->impl || !local_path) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    handle->impl->uploadClientLog(
        local_path,
        [userdata, on_progress](int64_t uploaded, int64_t total) {
            if (on_progress)
                on_progress(userdata, uploaded, total);
        },
        [userdata, on_done](bool ok, anychat::FileInfo info, std::string err) {
            if (!on_done)
                return;

            if (!ok) {
                on_done(userdata, 0, nullptr, err.c_str());
                return;
            }

            AnyChatFileInfo_C c_info{};
            fileInfoToC(info, &c_info);
            on_done(userdata, 1, &c_info, "");
        },
        expires_hours
    );

    anychat_clear_last_error();
    return ANYCHAT_OK;
}

int anychat_file_delete(AnyChatFileHandle handle, const char* file_id, void* userdata, AnyChatFileCallback callback) {
    if (!handle || !handle->impl || !file_id) {
        anychat_set_last_error("invalid arguments");
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    handle->impl->deleteFile(file_id, [userdata, callback](bool ok, std::string err) {
        if (callback)
            callback(userdata, ok ? 1 : 0, err.c_str());
    });
    anychat_clear_last_error();
    return ANYCHAT_OK;
}

} // extern "C"
