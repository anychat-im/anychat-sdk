#include "file_manager.h"

#include "json_common.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace anychat::file_manager_detail {

using json_common::ApiEnvelope;
using json_common::parseApiEnvelopeResponse;
using json_common::parseBoolValue;
using json_common::parseInt64Value;
using json_common::parseTimestampMs;
using json_common::writeJson;

using IntegerValue = std::variant<int64_t, double, std::string>;
using OptionalIntegerValue = std::optional<IntegerValue>;
using BooleanValue = std::variant<bool, int64_t, double, std::string>;
using OptionalBooleanValue = std::optional<BooleanValue>;

struct UploadTokenRequest {
    std::string file_name{};
    std::string file_type{};
    int64_t file_size = 0;
    std::string mime_type{};
};

struct UploadClientLogInitRequest {
    std::string file_name{};
    int64_t file_size = 0;
    int32_t expires_hours = 0;
};

struct UploadLogCompleteRequest {
    std::string file_id{};
};

struct UploadTokenPayload {
    std::string file_id{};
    std::string upload_url{};
};

struct DownloadUrlPayload {
    std::string download_url{};
};

struct FileInfoPayload {
    std::string file_id{};
    std::string file_name{};
    std::string file_type{};
    OptionalIntegerValue file_size{};
    std::string mime_type{};
    std::string download_url{};
    json_common::OptionalTimestampValue created_at{};
};

using FileInfoDataValue = std::variant<std::monostate, FileInfoPayload>;

struct FileListDataPayload {
    std::optional<std::vector<FileInfoPayload>> files{};
    OptionalIntegerValue total{};
};

struct DeleteFilePayload {
    OptionalBooleanValue success{};
};

std::string extractFileName(const std::string& local_path) {
    std::string file_name = local_path;
    const auto slash = file_name.find_last_of('/');
    if (slash != std::string::npos) {
        file_name = file_name.substr(slash + 1);
    }
#ifdef _WIN32
    const auto bslash = file_name.find_last_of('\\');
    if (bslash != std::string::npos) {
        file_name = file_name.substr(bslash + 1);
    }
#endif
    return file_name;
}

bool readFileBytes(const std::string& local_path, std::string& file_name, std::string& file_bytes, std::string& err) {
    file_name = extractFileName(local_path);

    std::ifstream ifs(local_path, std::ios::binary);
    if (!ifs.is_open()) {
        err = "cannot open file: " + local_path;
        return false;
    }

    file_bytes.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    if (ifs.bad()) {
        err = "read file failed: " + local_path;
        return false;
    }

    if (file_bytes.empty()) {
        err = "file is empty: " + local_path;
        return false;
    }

    return true;
}

template <typename T>
bool parseTypedDataResponse(const network::HttpResponse& resp, T& out, std::string& err) {
    ApiEnvelope<T> wrapped{};
    if (!parseApiEnvelopeResponse(resp, wrapped, err, "server error", false, true)) {
        return false;
    }
    out = std::move(wrapped.data);
    return true;
}

FileInfo parseFileInfoPayload(const FileInfoPayload& payload) {
    FileInfo info;
    info.file_id = payload.file_id;
    info.file_name = payload.file_name;
    info.file_type = payload.file_type;
    info.file_size_bytes = parseInt64Value(payload.file_size, 0);
    info.mime_type = payload.mime_type;
    info.download_url = payload.download_url;
    info.created_at_ms = parseTimestampMs(payload.created_at);
    return info;
}

FileInfo parseFileInfoData(const FileInfoDataValue& data) {
    if (const auto* direct = std::get_if<FileInfoPayload>(&data); direct != nullptr) {
        return parseFileInfoPayload(*direct);
    }
    return {};
}

const std::vector<FileInfoPayload>* toFileInfoPayloadList(const FileListDataPayload& data) {
    return data.files.has_value() ? &(*data.files) : nullptr;
}

std::string urlEncode(const std::string& input) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(input.size() * 3);

    for (unsigned char ch : input) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back(static_cast<char>(ch));
            continue;
        }

        out.push_back('%');
        out.push_back(hex[(ch >> 4) & 0x0F]);
        out.push_back(hex[ch & 0x0F]);
    }
    return out;
}

} // namespace anychat::file_manager_detail

namespace anychat {
using namespace file_manager_detail;


FileManagerImpl::FileManagerImpl(std::shared_ptr<network::HttpClient> http)
    : http_(std::move(http)) {}

void FileManagerImpl::upload(
    const std::string& local_path,
    const std::string& file_type,
    UploadProgressCallback on_progress,
    FileInfoCallback on_done
) {
    std::string file_name;
    auto file_bytes = std::make_shared<std::string>();
    std::string read_err;
    if (!readFileBytes(local_path, file_name, *file_bytes, read_err)) {
        if (on_done) {
            on_done(false, FileInfo{}, read_err);
        }
        return;
    }

    UploadTokenRequest req_body{
        .file_name = file_name,
        .file_type = file_type,
        .file_size = static_cast<int64_t>(file_bytes->size()),
        .mime_type = "application/octet-stream",
    };

    std::string req_json;
    std::string req_err;
    if (!writeJson(req_body, req_json, req_err)) {
        if (on_done) {
            on_done(false, FileInfo{}, req_err);
        }
        return;
    }

    http_->post(
        "/files/upload-token",
        req_json,
        [this, file_bytes, on_progress, on_done](network::HttpResponse resp) {
            UploadTokenPayload token{};
            std::string err;
            if (!parseTypedDataResponse(resp, token, err)) {
                if (on_done) {
                    on_done(false, FileInfo{}, "upload-token failed: " + err);
                }
                return;
            }

            if (token.file_id.empty() || token.upload_url.empty()) {
                if (on_done) {
                    on_done(false, FileInfo{}, "upload-token response missing file id or upload url");
                }
                return;
            }

            if (on_progress) {
                on_progress(0, static_cast<int64_t>(file_bytes->size()));
            }

            http_->put(
                token.upload_url,
                *file_bytes,
                [this, file_id = token.file_id, file_bytes, on_progress, on_done](network::HttpResponse put_resp) {
                    if (!put_resp.error.empty()) {
                        if (on_done) {
                            on_done(false, FileInfo{}, put_resp.error);
                        }
                        return;
                    }

                    if (put_resp.status_code < 200 || put_resp.status_code >= 300) {
                        if (on_done) {
                            on_done(false, FileInfo{}, "upload PUT failed: " + put_resp.body);
                        }
                        return;
                    }

                    if (on_progress) {
                        const auto uploaded = static_cast<int64_t>(file_bytes->size());
                        on_progress(uploaded, uploaded);
                    }

                    http_->post("/files/" + file_id + "/complete", "{}", [file_id, on_done](network::HttpResponse complete_resp) {
                        FileInfoDataValue complete_data{};
                        std::string complete_err;
                        if (!parseTypedDataResponse(complete_resp, complete_data, complete_err)) {
                            if (on_done) {
                                on_done(false, FileInfo{}, "complete failed: " + complete_err);
                            }
                            return;
                        }

                        FileInfo info = parseFileInfoData(complete_data);
                        if (info.file_id.empty()) {
                            info.file_id = file_id;
                        }

                        if (on_done) {
                            on_done(true, info, "");
                        }
                    });
                }
            );
        }
    );
}

void FileManagerImpl::getDownloadUrl(
    const std::string& file_id,
    std::function<void(bool ok, std::string url, std::string err)> cb
) {
    http_->get("/files/" + file_id + "/download", [cb](network::HttpResponse resp) {
        DownloadUrlPayload data{};
        std::string err;
        if (!parseTypedDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, "", "getDownloadUrl failed: " + err);
            }
            return;
        }

        if (data.download_url.empty()) {
            if (cb) {
                cb(false, "", "download url missing in response");
            }
            return;
        }

        if (cb) {
            cb(true, data.download_url, "");
        }
    });
}

void FileManagerImpl::getFileInfo(const std::string& file_id, FileInfoCallback cb) {
    http_->get("/files/" + file_id, [cb, file_id](network::HttpResponse resp) {
        FileInfoDataValue data{};
        std::string err;
        if (!parseTypedDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, FileInfo{}, "getFileInfo failed: " + err);
            }
            return;
        }

        FileInfo info = parseFileInfoData(data);
        if (info.file_id.empty()) {
            info.file_id = file_id;
        }

        if (cb) {
            cb(true, info, "");
        }
    });
}

void FileManagerImpl::listFiles(const std::string& file_type, int page, int page_size, FileListCallback cb) {
    const int safe_page = page > 0 ? page : 1;
    const int safe_page_size = page_size > 0 ? page_size : 20;

    std::string path =
        "/files?page=" + std::to_string(safe_page) + "&page_size=" + std::to_string(safe_page_size);
    if (!file_type.empty()) {
        path += "&file_type=" + urlEncode(file_type);
    }

    http_->get(path, [cb](network::HttpResponse resp) {
        FileListDataPayload data{};
        std::string err;
        if (!parseTypedDataResponse(resp, data, err)) {
            if (cb) {
                cb({}, 0, "listFiles failed: " + err);
            }
            return;
        }

        std::vector<FileInfo> files;
        const auto* payloads = toFileInfoPayloadList(data);
        if (payloads != nullptr) {
            files.reserve(payloads->size());
            for (const auto& item : *payloads) {
                files.push_back(parseFileInfoPayload(item));
            }
        }

        int64_t total = parseInt64Value(data.total, 0);
        if (total == 0 && !files.empty()) {
            total = static_cast<int64_t>(files.size());
        }

        if (cb) {
            cb(std::move(files), total, "");
        }
    });
}

void FileManagerImpl::uploadClientLog(
    const std::string& local_path,
    UploadProgressCallback on_progress,
    FileInfoCallback on_done,
    int32_t expires_hours
) {
    std::string file_name;
    auto file_bytes = std::make_shared<std::string>();
    std::string read_err;
    if (!readFileBytes(local_path, file_name, *file_bytes, read_err)) {
        if (on_done) {
            on_done(false, FileInfo{}, read_err);
        }
        return;
    }

    UploadClientLogInitRequest req_body{
        .file_name = file_name,
        .file_size = static_cast<int64_t>(file_bytes->size()),
        .expires_hours = expires_hours < 0 ? 0 : expires_hours,
    };

    std::string req_json;
    std::string req_err;
    if (!writeJson(req_body, req_json, req_err)) {
        if (on_done) {
            on_done(false, FileInfo{}, req_err);
        }
        return;
    }

    http_->post(
        "/logs/upload",
        req_json,
        [this, file_bytes, on_progress, on_done](network::HttpResponse resp) {
            UploadTokenPayload init{};
            std::string err;
            if (!parseTypedDataResponse(resp, init, err)) {
                if (on_done) {
                    on_done(false, FileInfo{}, "log upload init failed: " + err);
                }
                return;
            }

            if (init.file_id.empty() || init.upload_url.empty()) {
                if (on_done) {
                    on_done(false, FileInfo{}, "log upload init response missing file id or upload url");
                }
                return;
            }

            if (on_progress) {
                on_progress(0, static_cast<int64_t>(file_bytes->size()));
            }

            http_->put(
                init.upload_url,
                *file_bytes,
                [this, file_id = init.file_id, file_bytes, on_progress, on_done](network::HttpResponse put_resp) {
                    if (!put_resp.error.empty()) {
                        if (on_done) {
                            on_done(false, FileInfo{}, put_resp.error);
                        }
                        return;
                    }

                    if (put_resp.status_code < 200 || put_resp.status_code >= 300) {
                        if (on_done) {
                            on_done(false, FileInfo{}, "log upload PUT failed: " + put_resp.body);
                        }
                        return;
                    }

                    if (on_progress) {
                        const auto uploaded = static_cast<int64_t>(file_bytes->size());
                        on_progress(uploaded, uploaded);
                    }

                    const UploadLogCompleteRequest complete_body{.file_id = file_id};
                    std::string complete_body_json;
                    std::string complete_body_err;
                    if (!writeJson(complete_body, complete_body_json, complete_body_err)) {
                        if (on_done) {
                            on_done(false, FileInfo{}, complete_body_err);
                        }
                        return;
                    }

                    http_->post("/logs/complete", complete_body_json, [file_id, on_done](network::HttpResponse complete_resp) {
                        FileInfoDataValue complete_data{};
                        std::string complete_err;
                        if (!parseTypedDataResponse(complete_resp, complete_data, complete_err)) {
                            if (on_done) {
                                on_done(false, FileInfo{}, "log upload complete failed: " + complete_err);
                            }
                            return;
                        }

                        FileInfo info = parseFileInfoData(complete_data);
                        if (info.file_id.empty()) {
                            info.file_id = file_id;
                        }
                        if (info.file_type.empty()) {
                            info.file_type = "log";
                        }

                        if (on_done) {
                            on_done(true, info, "");
                        }
                    });
                }
            );
        }
    );
}

void FileManagerImpl::deleteFile(const std::string& file_id, FileCallback cb) {
    http_->del("/files/" + file_id, [cb](network::HttpResponse resp) {
        DeleteFilePayload data{};
        std::string err;
        if (!parseTypedDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, "deleteFile failed: " + err);
            }
            return;
        }

        if (data.success.has_value() && !parseBoolValue(data.success, true)) {
            if (cb) {
                cb(false, "deleteFile failed");
            }
            return;
        }

        if (cb) {
            cb(true, "");
        }
    });
}

} // namespace anychat
