#include "anychat/version.h"

#include "version_manager.h"
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
    dst->platform = src.platform;
    anychat_strlcpy(dst->version, src.version.c_str(), sizeof(dst->version));
    dst->build_number = src.build_number;
    dst->version_code = src.version_code;
    anychat_strlcpy(dst->min_version, src.min_version.c_str(), sizeof(dst->min_version));
    dst->min_build_number = src.min_build_number;
    dst->force_update = src.force_update ? 1 : 0;
    dst->release_type = src.release_type;
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
    int32_t platform,
    const char* version,
    int32_t build_number,
    const AnyChatVersionCheckCallback_C* callback
) {
    auto* impl = static_cast<anychat::VersionManagerImpl*>(handle);
    if (!impl || !version) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (platform < ANYCHAT_VERSION_PLATFORM_IOS || platform > ANYCHAT_VERSION_PLATFORM_H5) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionCheckCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    impl->checkVersion(
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
    int32_t platform,
    int32_t release_type,
    const AnyChatVersionInfoCallback_C* callback
) {
    auto* impl = static_cast<anychat::VersionManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (platform < ANYCHAT_VERSION_PLATFORM_IOS || platform > ANYCHAT_VERSION_PLATFORM_H5) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (release_type < ANYCHAT_VERSION_RELEASE_TYPE_STABLE || release_type > ANYCHAT_VERSION_RELEASE_TYPE_ALPHA) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionInfoCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    impl->getLatestVersion(
        platform,
        release_type,
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
    int32_t platform,
    int32_t release_type,
    int page,
    int page_size,
    const AnyChatVersionListCallback_C* callback
) {
    auto* impl = static_cast<anychat::VersionManagerImpl*>(handle);
    if (!impl) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (platform < ANYCHAT_VERSION_PLATFORM_IOS || platform > ANYCHAT_VERSION_PLATFORM_H5) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (release_type < ANYCHAT_VERSION_RELEASE_TYPE_UNSPECIFIED || release_type > ANYCHAT_VERSION_RELEASE_TYPE_ALPHA) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionListCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    impl->listVersions(
        platform,
        release_type,
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
    int32_t platform,
    const char* version,
    int32_t build_number,
    const char* device_id,
    const char* os_version,
    const char* sdk_version,
    const AnyChatVersionCallback_C* callback
) {
    auto* impl = static_cast<anychat::VersionManagerImpl*>(handle);
    if (!impl || !version) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (platform < ANYCHAT_VERSION_PLATFORM_IOS || platform > ANYCHAT_VERSION_PLATFORM_H5) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }
    if (callback) {
        return ANYCHAT_ERROR_INVALID_PARAM;
    }

    AnyChatVersionCallback_C callback_copy{};
    if (callback) {
        callback_copy = *callback;
    }

    impl->reportVersion(
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
