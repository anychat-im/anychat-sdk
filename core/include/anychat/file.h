#pragma once

#include "types.h"

#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace anychat {

using FileCallback = std::function<void(bool ok, std::string err)>;
using FileInfoCallback = std::function<void(bool ok, FileInfo info, std::string err)>;
using UploadProgressCallback = std::function<void(int64_t uploaded, int64_t total)>;
using FileListCallback = std::function<void(std::vector<FileInfo> files, int64_t total, std::string err)>;

class FileManager {
public:
    virtual ~FileManager() = default;

    // Three-step upload: get-token → PUT → complete
    // local_path: absolute path to the file to upload
    // on_progress: called periodically with bytes uploaded / total
    // on_done: called with the file_id and download_url on success
    virtual void upload(
        const std::string& local_path,
        const std::string& file_type, // "image" | "video" | "audio" | "file"
        UploadProgressCallback on_progress,
        FileInfoCallback on_done
    ) = 0;

    // GET /files/{fileId}/download → presigned URL
    virtual void
    getDownloadUrl(const std::string& file_id, std::function<void(bool ok, std::string url, std::string err)> cb) = 0;

    // GET /files/{fileId}
    virtual void getFileInfo(const std::string& file_id, FileInfoCallback cb) = 0;

    // GET /files?fileType=&page=&pageSize=
    // file_type为空时表示不过滤类型。
    virtual void listFiles(const std::string& file_type, int page, int page_size, FileListCallback cb) = 0;

    // Three-step client log upload: POST /logs/upload -> PUT upload_url -> POST /logs/complete
    virtual void uploadClientLog(
        const std::string& local_path,
        UploadProgressCallback on_progress,
        FileInfoCallback on_done,
        int32_t expires_hours = 0
    ) = 0;

    // DELETE /files/{fileId}
    virtual void deleteFile(const std::string& file_id, FileCallback cb) = 0;
};

} // namespace anychat
