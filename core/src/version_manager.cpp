#include "version_manager.h"

#include "json_common.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat {
namespace {
using json_common::ApiEnvelope;
using json_common::parseApiEnvelopeResponse;
using json_common::parseApiStatusSuccessResponse;
using json_common::parseTimestampMs;
using json_common::writeJson;

struct VersionUpdatePayload {
    std::string title{};
    std::string content{};
    std::string download_url{};
    int64_t file_size = 0;
    std::string file_hash{};
};

struct VersionCheckPayload {
    bool has_update = false;
    std::string latest_version{};
    int32_t latest_build_number = 0;
    bool force_update = false;
    std::string min_version{};
    int32_t min_build_number = 0;
    std::optional<VersionUpdatePayload> update_info{};
};

struct VersionInfoPayload {
    int64_t id = 0;
    std::string platform{};
    std::string version{};
    int32_t build_number = 0;
    int32_t version_code = 0;
    std::string min_version{};
    int32_t min_build_number = 0;
    bool force_update = false;
    std::string release_type{};
    std::string title{};
    std::string content{};
    std::string download_url{};
    int64_t file_size = 0;
    std::string file_hash{};
    json_common::OptionalTimestampValue published_at{};
};

struct LatestVersionWrappedPayload {
    std::optional<VersionInfoPayload> version{};
};

struct VersionListDataPayload {
    int64_t total = 0;
    std::optional<std::vector<VersionInfoPayload>> versions{};
    std::optional<std::vector<VersionInfoPayload>> list{};
    std::optional<std::vector<VersionInfoPayload>> items{};
};

using VersionListDataValue = std::variant<std::monostate, VersionListDataPayload, std::vector<VersionInfoPayload>>;

struct ReportVersionRequestPayload {
    std::string platform{};
    std::string version{};
    std::optional<int32_t> build_number{};
    std::optional<std::string> device_id{};
    std::optional<std::string> os_version{};
    std::optional<std::string> sdk_version{};
};

VersionUpdateInfo toUpdateInfo(const VersionUpdatePayload& payload) {
    VersionUpdateInfo info;
    info.title = payload.title;
    info.content = payload.content;
    info.download_url = payload.download_url;
    info.file_size = payload.file_size;
    info.file_hash = payload.file_hash;
    return info;
}

VersionCheckResult toCheckResult(const VersionCheckPayload& payload) {
    VersionCheckResult result;
    result.has_update = payload.has_update;
    result.latest_version = payload.latest_version;
    result.latest_build_number = payload.latest_build_number;
    result.force_update = payload.force_update;
    result.min_version = payload.min_version;
    result.min_build_number = payload.min_build_number;
    if (payload.update_info.has_value()) {
        result.update_info = toUpdateInfo(*payload.update_info);
    }
    return result;
}

AppVersionInfo toVersionInfo(const VersionInfoPayload& payload) {
    AppVersionInfo info;
    info.id = payload.id;
    info.platform = payload.platform;
    info.version = payload.version;
    info.build_number = payload.build_number;
    info.version_code = payload.version_code;
    info.min_version = payload.min_version;
    info.min_build_number = payload.min_build_number;
    info.force_update = payload.force_update;
    info.release_type = payload.release_type;
    info.title = payload.title;
    info.content = payload.content;
    info.download_url = payload.download_url;
    info.file_size = payload.file_size;
    info.file_hash = payload.file_hash;
    info.published_at_ms = parseTimestampMs(payload.published_at);
    return info;
}

const std::vector<VersionInfoPayload>* pickVersionList(const VersionListDataValue& data, int64_t& total) {
    if (const auto* list = std::get_if<std::vector<VersionInfoPayload>>(&data); list != nullptr) {
        total = static_cast<int64_t>(list->size());
        return list;
    }
    if (const auto* object = std::get_if<VersionListDataPayload>(&data); object != nullptr) {
        total = object->total;
        if (object->versions.has_value()) {
            return &(*object->versions);
        }
        if (object->list.has_value()) {
            return &(*object->list);
        }
        if (object->items.has_value()) {
            return &(*object->items);
        }
    }
    return nullptr;
}

} // namespace

VersionManagerImpl::VersionManagerImpl(std::shared_ptr<network::HttpClient> http)
    : http_(std::move(http)) {}

/*static*/ std::string VersionManagerImpl::urlEncode(const std::string& input) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : input) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

void VersionManagerImpl::checkVersion(
    const std::string& platform,
    const std::string& version,
    int32_t build_number,
    CheckVersionCallback callback
) {
    if (platform.empty() || version.empty()) {
        callback(false, {}, "platform and version are required");
        return;
    }

    std::string path = "/versions/check?platform=" + urlEncode(platform) + "&version=" + urlEncode(version);
    if (build_number > 0) {
        path += "&buildNumber=" + std::to_string(build_number);
    }

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<VersionCheckPayload> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toCheckResult(root.data), "");
    });
}

void VersionManagerImpl::getLatestVersion(
    const std::string& platform,
    const std::string& release_type,
    LatestVersionCallback callback
) {
    if (platform.empty()) {
        callback(false, {}, "platform is required");
        return;
    }

    std::string path = "/versions/latest?platform=" + urlEncode(platform);
    if (!release_type.empty()) {
        path += "&releaseType=" + urlEncode(release_type);
    }

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<LatestVersionWrappedPayload> wrapped{};
        std::string err;
        if (parseApiEnvelopeResponse(resp, wrapped, err) && wrapped.data.version.has_value()) {
            cb(true, toVersionInfo(*wrapped.data.version), "");
            return;
        }

        ApiEnvelope<VersionInfoPayload> flat{};
        if (!parseApiEnvelopeResponse(resp, flat, err)) {
            cb(false, {}, err);
            return;
        }

        cb(true, toVersionInfo(flat.data), "");
    });
}

void VersionManagerImpl::listVersions(
    const std::string& platform,
    const std::string& release_type,
    int page,
    int page_size,
    VersionListCallback callback
) {
    if (page < 1) {
        page = 1;
    }
    if (page_size < 1) {
        page_size = 20;
    }

    std::string path = "/versions/list?page=" + std::to_string(page) + "&pageSize=" + std::to_string(page_size);
    if (!platform.empty()) {
        path += "&platform=" + urlEncode(platform);
    }
    if (!release_type.empty()) {
        path += "&releaseType=" + urlEncode(release_type);
    }

    http_->get(path, [cb = std::move(callback)](network::HttpResponse resp) {
        ApiEnvelope<VersionListDataValue> root{};
        std::string err;
        if (!parseApiEnvelopeResponse(resp, root, err)) {
            cb({}, 0, err);
            return;
        }

        int64_t total = 0;
        const auto* payloads = pickVersionList(root.data, total);
        std::vector<AppVersionInfo> versions;
        if (payloads != nullptr) {
            versions.reserve(payloads->size());
            for (const auto& item : *payloads) {
                versions.push_back(toVersionInfo(item));
            }
        }

        cb(versions, total, "");
    });
}

void VersionManagerImpl::reportVersion(
    const std::string& platform,
    const std::string& version,
    int32_t build_number,
    const std::string& device_id,
    const std::string& os_version,
    const std::string& sdk_version,
    ResultCallback callback
) {
    if (platform.empty() || version.empty()) {
        callback(false, "platform and version are required");
        return;
    }

    ReportVersionRequestPayload body{.platform = platform, .version = version};
    if (build_number > 0) {
        body.build_number = build_number;
    }
    if (!device_id.empty()) {
        body.device_id = device_id;
    }
    if (!os_version.empty()) {
        body.os_version = os_version;
    }
    if (!sdk_version.empty()) {
        body.sdk_version = sdk_version;
    }

    std::string body_json;
    std::string err;
    if (!writeJson(body, body_json, err)) {
        callback(false, err);
        return;
    }

    http_->post("/versions/report", body_json, [cb = std::move(callback)](network::HttpResponse resp) {
        std::string err;
        const bool ok = parseApiStatusSuccessResponse(resp, err);
        cb(ok, ok ? "" : err);
    });
}

} // namespace anychat
