#include "anychat/version.h"

#include "handles_c.h"
#include "utils_c.h"

#include <cstdlib>
#include <vector>

namespace {

void updateInfoToC(const anychat::VersionUpdateInfo& src, AnyChatVersionUpdateInfo_C* dst) {
    anychat_strlcpy(dst->title, src.title.c_str(), sizeof(dst->title));
    anychat_strlcpy(dst->content, src.content.c_str(), sizeof(dst->content));
    anychat_strlcpy(dst->download_url, src.download_url.c_str(), sizeof(dst->download_url));
    dst->file_size = src.file_size;
    anychat_strlcpy(dst->file_hash, src.file_hash.c_str(), sizeof(dst->file_hash));
}

void checkResultToC(const anychat::VersionCheckResult& src, AnyChatVersionCheckResult_C* dst) {
    dst->has_update = src.has_update ? 1 : 0;
    anychat_strlcpy(dst->latest_version, src.latest_version.c_str(), sizeof(dst->latest_version));
    dst->latest_build_number = src.latest_build_number;
    dst->force_update = src.force_update ? 1 : 0;
    anychat_strlcpy(dst->min_version, src.min_version.c_str(), sizeof(dst->min_version));
    dst->min_build_number = src.min_build_number;
    updateInfoToC(src.update_info, &dst->update_info);
}

void versionInfoToC(const anychat::AppVersionInfo& src, AnyChatVersionInfo_C* dst) {
    dst->id = src.id;
    anychat_strlcpy(dst->platform, src.platform.c_str(), sizeof(dst->platform));
    anychat_strlcpy(dst->version, src.version.c_str(), sizeof(dst->version));
    dst->build_number = src.build_number;
    dst->version_code = src.version_code;
    anychat_strlcpy(dst->min_version, src.min_version.c_str(), sizeof(dst->min_version));
    dst->min_build_number = src.min_build_number;
    dst->force_update = src.force_update ? 1 : 0;
    anychat_strlcpy(dst->release_type, src.release_type.c_str(), sizeof(dst->release_type));
    anychat_strlcpy(dst->title, src.title.c_str(), sizeof(dst->title));
    anychat_strlcpy(dst->content, src.content.c_str(), sizeof(dst->content));
    anychat_strlcpy(dst->download_url, src.download_url.c_str(), sizeof(dst->download_url));
    dst->file_size = src.file_size;
    anychat_strlcpy(dst->file_hash, src.file_hash.c_str(), sizeof(dst->file_hash));
    dst->published_at_ms = src.published_at_ms;
}

} // namespace

extern "C" {

int anychat_version_check(
    AnyChatVersionHandle handle,
    const char* platform,
    const char* version,
    int32_t build_number,
    const AnyChatVersionCheckCallback_C* callback
) {
    if (!handle || !handle->impl || !platform || !version) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatVersionCheckCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionCheckCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->checkVersion(
        platform,
        version,
        build_number,
        anychat::AnyChatValueCallback<anychat::VersionCheckResult>{
            .on_success =
                [callback_copy](const anychat::VersionCheckResult& result) {
                    if (!callback_copy.on_success)
                        return;

                    AnyChatVersionCheckResult_C c_result{};
                    checkResultToC(result, &c_result);
                    callback_copy.on_success(callback_copy.userdata, &c_result);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    if (!callback_copy.on_error)
                        return;
                    callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_version_get_latest(
    AnyChatVersionHandle handle,
    const char* platform,
    const char* release_type,
    const AnyChatVersionInfoCallback_C* callback
) {
    if (!handle || !handle->impl || !platform) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatVersionInfoCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionInfoCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->getLatestVersion(
        platform,
        release_type ? release_type : "",
        anychat::AnyChatValueCallback<anychat::AppVersionInfo>{
            .on_success =
                [callback_copy](const anychat::AppVersionInfo& version) {
                    if (!callback_copy.on_success)
                        return;

                    AnyChatVersionInfo_C c_version{};
                    versionInfoToC(version, &c_version);
                    callback_copy.on_success(callback_copy.userdata, &c_version);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    if (!callback_copy.on_error)
                        return;
                    callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_version_list(
    AnyChatVersionHandle handle,
    const char* platform,
    const char* release_type,
    int page,
    int page_size,
    const AnyChatVersionListCallback_C* callback
) {
    if (!handle || !handle->impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatVersionListCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionListCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->listVersions(
        platform ? platform : "",
        release_type ? release_type : "",
        page,
        page_size,
        anychat::AnyChatValueCallback<anychat::VersionListResult>{
            .on_success =
                [callback_copy](const anychat::VersionListResult& result) {
                    if (!callback_copy.on_success)
                        return;

                    const int count = static_cast<int>(result.versions.size());
                    AnyChatVersionList_C c_list{};
                    c_list.count = count;
                    c_list.total = result.total;
                    c_list.page = result.page;
                    c_list.page_size = result.page_size;
                    c_list.items = count > 0
                                       ? static_cast<AnyChatVersionInfo_C*>(
                                             std::calloc(count, sizeof(AnyChatVersionInfo_C))
                                         )
                                       : nullptr;

                    for (int i = 0; i < count; ++i) {
                        versionInfoToC(result.versions[i], &c_list.items[i]);
                    }

                    callback_copy.on_success(callback_copy.userdata, &c_list);
                    std::free(c_list.items);
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    if (!callback_copy.on_error)
                        return;
                    callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
                },
        }
    );

    return ANYCHAT_OK;
}

int anychat_version_report(
    AnyChatVersionHandle handle,
    const char* platform,
    const char* version,
    int32_t build_number,
    const char* device_id,
    const char* os_version,
    const char* sdk_version,
    const AnyChatVersionCallback_C* callback
) {
    if (!handle || !handle->impl || !platform || !version) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback && callback->struct_size < sizeof(AnyChatVersionCallback_C)) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    handle->impl->reportVersion(
        platform,
        version,
        build_number,
        device_id ? device_id : "",
        os_version ? os_version : "",
        sdk_version ? sdk_version : "",
        anychat::AnyChatCallback{
            .on_success =
                [callback_copy]() {
                    if (callback_copy.on_success) {
                        callback_copy.on_success(callback_copy.userdata);
                    }
                },
            .on_error =
                [callback_copy](int code, const std::string& error) {
                    if (!callback_copy.on_error)
                        return;
                    callback_copy.on_error(callback_copy.userdata, code, error.empty() ? nullptr : error.c_str());
                },
        }
    );

    return ANYCHAT_OK;
}

} // extern "C"
