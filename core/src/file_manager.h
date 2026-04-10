#pragma once
#include "anychat/file.h"

#include "network/http_client.h"

#include <memory>
#include <mutex>

namespace anychat {

class FileManagerImpl : public FileManager {
public:
    explicit FileManagerImpl(std::shared_ptr<network::HttpClient> http);

    // FileManager interface
    void upload(
        const std::string& local_path,
        const std::string& file_type,
        UploadProgressCallback on_progress,
        FileInfoCallback on_done
    ) override;

    void getDownloadUrl(const std::string& file_id, std::function<void(bool ok, std::string url, std::string err)> cb)
        override;

    void getFileInfo(const std::string& file_id, FileInfoCallback cb) override;

    void listFiles(const std::string& file_type, int page, int page_size, FileListCallback cb) override;

    void uploadClientLog(
        const std::string& local_path,
        UploadProgressCallback on_progress,
        FileInfoCallback on_done,
        int32_t expires_hours
    ) override;

    void deleteFile(const std::string& file_id, FileCallback cb) override;

private:
    std::shared_ptr<network::HttpClient> http_;
};

} // namespace anychat
