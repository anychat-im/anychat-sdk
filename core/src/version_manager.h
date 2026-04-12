#pragma once

#include "anychat/version.h"

#include "network/http_client.h"

#include <memory>
#include <string>

namespace anychat {

class VersionManagerImpl : public VersionManager {
public:
    explicit VersionManagerImpl(std::shared_ptr<network::HttpClient> http);

    void checkVersion(
        const std::string& platform,
        const std::string& version,
        int32_t build_number,
        CheckVersionCallback callback
    ) override;

    void getLatestVersion(
        const std::string& platform,
        const std::string& release_type,
        LatestVersionCallback callback
    ) override;

    void listVersions(
        const std::string& platform,
        const std::string& release_type,
        int page,
        int page_size,
        VersionListCallback callback
    ) override;

    void reportVersion(
        const std::string& platform,
        const std::string& version,
        int32_t build_number,
        const std::string& device_id,
        const std::string& os_version,
        const std::string& sdk_version,
        ResultCallback callback
    ) override;

private:
    static std::string urlEncode(const std::string& input);

    std::shared_ptr<network::HttpClient> http_;
};

} // namespace anychat
