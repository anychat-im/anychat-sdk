#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatFileInfoSuccessCallback)(void* userdata, const AnyChatFileInfo_C* info);
typedef void (*AnyChatDownloadUrlSuccessCallback)(void* userdata, const char* url);
typedef void (*AnyChatFileListSuccessCallback)(void* userdata, const AnyChatFileList_C* list);

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatFileCallback_C;

typedef struct {
    void* userdata;
    AnyChatFileInfoSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatFileInfoCallback_C;

/* Progress during upload: uploaded and total are byte counts. */
typedef void (*AnyChatUploadProgressCallback)(void* userdata, int64_t uploaded, int64_t total);

typedef struct {
    void* userdata;
    AnyChatDownloadUrlSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatDownloadUrlCallback_C;

typedef struct {
    void* userdata;
    AnyChatFileListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatFileListCallback_C;

/* ---- File operations ---- */

/* Upload a local file.
 * file_type: ANYCHAT_FILE_TYPE_*
 * on_progress: may be NULL.
 * on_done: fired when the upload completes (success or failure). */
ANYCHAT_C_API int anychat_file_upload(
    AnyChatFileHandle handle,
    const char* local_path,
    int32_t file_type,
    AnyChatUploadProgressCallback on_progress,
    const AnyChatFileInfoCallback_C* on_done
);

/* Retrieve a presigned download URL for a file. */
ANYCHAT_C_API int anychat_file_get_download_url(
    AnyChatFileHandle handle,
    const char* file_id,
    const AnyChatDownloadUrlCallback_C* callback
);

/* Retrieve metadata for a single file. */
ANYCHAT_C_API int anychat_file_get_info(
    AnyChatFileHandle handle,
    const char* file_id,
    const AnyChatFileInfoCallback_C* callback
);

/* List user files; file_type=ANYCHAT_FILE_TYPE_UNSPECIFIED means all types. */
ANYCHAT_C_API int anychat_file_list(
    AnyChatFileHandle handle,
    int32_t file_type,
    int page,
    int page_size,
    const AnyChatFileListCallback_C* callback
);

/* Upload client log file via /logs/upload + /logs/complete. */
ANYCHAT_C_API int anychat_file_upload_log(
    AnyChatFileHandle handle,
    const char* local_path,
    int32_t expires_hours,
    AnyChatUploadProgressCallback on_progress,
    const AnyChatFileInfoCallback_C* on_done
);

/* Delete a file from the server. */
ANYCHAT_C_API int
anychat_file_delete(AnyChatFileHandle handle, const char* file_id, const AnyChatFileCallback_C* callback);

#ifdef __cplusplus
}
#endif
