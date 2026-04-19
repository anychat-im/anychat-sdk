#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Callback types ---- */

typedef void (*AnyChatVersionCheckSuccessCallback)(void* userdata, const AnyChatVersionCheckResult_C* result);
typedef void (*AnyChatVersionInfoSuccessCallback)(void* userdata, const AnyChatVersionInfo_C* version);
typedef void (*AnyChatVersionListSuccessCallback)(void* userdata, const AnyChatVersionList_C* list);

typedef struct {
    void* userdata;
    AnyChatVersionCheckSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatVersionCheckCallback_C;

typedef struct {
    void* userdata;
    AnyChatVersionInfoSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatVersionInfoCallback_C;

typedef struct {
    void* userdata;
    AnyChatVersionListSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatVersionListCallback_C;

typedef struct {
    void* userdata;
    AnyChatSuccessCallback on_success;
    AnyChatErrorCallback on_error;
} AnyChatVersionCallback_C;

/* ---- Version operations ---- */

/* GET /versions/check?platform=&version=&buildNumber= */
ANYCHAT_C_API int anychat_version_check(
    AnyChatVersionHandle handle,
    int32_t platform,
    const char* version,
    int32_t build_number,
    const AnyChatVersionCheckCallback_C* callback
);

/* GET /versions/latest?platform=&releaseType= */
ANYCHAT_C_API int anychat_version_get_latest(
    AnyChatVersionHandle handle,
    int32_t platform,
    int32_t release_type,
    const AnyChatVersionInfoCallback_C* callback
);

/* GET /versions/list?platform=&releaseType=&page=&pageSize= */
ANYCHAT_C_API int anychat_version_list(
    AnyChatVersionHandle handle,
    int32_t platform,
    int32_t release_type,
    int page,
    int page_size,
    const AnyChatVersionListCallback_C* callback
);

/* POST /versions/report */
ANYCHAT_C_API int anychat_version_report(
    AnyChatVersionHandle handle,
    int32_t platform,
    const char* version,
    int32_t build_number,
    const char* device_id,
    const char* os_version,
    const char* sdk_version,
    const AnyChatVersionCallback_C* callback
);

#ifdef __cplusplus
}
#endif
