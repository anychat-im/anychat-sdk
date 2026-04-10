#include "file_manager.h"

#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace anychat {
namespace {

constexpr int64_t kUnixMsThreshold = 1'000'000'000'000LL;

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

int64_t normalizeUnixEpochMs(int64_t raw) {
    return (raw > 0 && raw < kUnixMsThreshold) ? raw * 1000LL : raw;
}

const nlohmann::json* findField(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return nullptr;
    }

    for (const char* key : keys) {
        const auto it = obj.find(key);
        if (it != obj.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

std::string jsonString(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    if (value == nullptr) {
        return "";
    }
    if (value->is_string()) {
        return value->get<std::string>();
    }
    return "";
}

int64_t jsonInt64(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    const auto* value = findField(obj, keys);
    if (value == nullptr) {
        return 0;
    }
    if (value->is_number_integer()) {
        return value->get<int64_t>();
    }
    if (value->is_number_unsigned()) {
        return static_cast<int64_t>(value->get<uint64_t>());
    }
    if (value->is_string()) {
        try {
            return std::stoll(value->get<std::string>());
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

bool jsonBool(const nlohmann::json& obj, std::initializer_list<const char*> keys, bool default_value = false) {
    const auto* value = findField(obj, keys);
    if (value == nullptr || !value->is_boolean()) {
        return default_value;
    }
    return value->get<bool>();
}

bool parseApiDataResponse(const network::HttpResponse& resp, nlohmann::json& data, std::string& err) {
    if (!resp.error.empty()) {
        err = resp.error;
        return false;
    }

    if (resp.status_code < 200 || resp.status_code >= 300) {
        err = !resp.body.empty() ? resp.body : ("http status " + std::to_string(resp.status_code));
        return false;
    }

    try {
        auto root = nlohmann::json::parse(resp.body);
        if (!root.is_object()) {
            data = std::move(root);
            return true;
        }

        const auto* code_field = findField(root, { "code" });
        if (code_field != nullptr) {
            int64_t code = 0;
            if (code_field->is_number_integer()) {
                code = code_field->get<int64_t>();
            } else if (code_field->is_number_unsigned()) {
                code = static_cast<int64_t>(code_field->get<uint64_t>());
            }

            if (code != 0) {
                err = jsonString(root, { "message", "msg", "error" });
                if (err.empty()) {
                    err = "server error";
                }
                return false;
            }
        }

        const auto* data_field = findField(root, { "data" });
        data = data_field != nullptr ? *data_field : root;
        return true;
    } catch (const std::exception& e) {
        err = std::string("parse error: ") + e.what();
        return false;
    }
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

FileInfo parseFileInfo(const nlohmann::json& data) {
    FileInfo info;
    info.file_id = jsonString(data, { "fileId", "file_id" });
    info.file_name = jsonString(data, { "fileName", "file_name" });
    info.file_type = jsonString(data, { "fileType", "file_type" });
    info.file_size_bytes = jsonInt64(data, { "fileSize", "file_size" });
    info.mime_type = jsonString(data, { "mimeType", "mime_type" });
    info.download_url = jsonString(data, { "downloadUrl", "download_url" });
    info.created_at_ms = normalizeUnixEpochMs(jsonInt64(data, { "createdAt", "created_at" }));
    return info;
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

} // namespace

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

    nlohmann::json req_body = {
        { "fileName", file_name },
        { "fileType", file_type },
        { "fileSize", static_cast<int64_t>(file_bytes->size()) },
        { "mimeType", "application/octet-stream" },
    };

    http_->post(
        "/files/upload-token",
        req_body.dump(),
        [this, file_bytes, on_progress, on_done](network::HttpResponse resp) {
            nlohmann::json data;
            std::string err;
            if (!parseApiDataResponse(resp, data, err)) {
                if (on_done) {
                    on_done(false, FileInfo{}, "upload-token failed: " + err);
                }
                return;
            }

            const std::string file_id = jsonString(data, { "fileId", "file_id" });
            const std::string upload_url = jsonString(data, { "uploadUrl", "upload_url" });
            if (file_id.empty() || upload_url.empty()) {
                if (on_done) {
                    on_done(false, FileInfo{}, "upload-token response missing file id or upload url");
                }
                return;
            }

            if (on_progress) {
                on_progress(0, static_cast<int64_t>(file_bytes->size()));
            }

            http_->put(
                upload_url,
                *file_bytes,
                [this, file_id, file_bytes, on_progress, on_done](network::HttpResponse put_resp) {
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
                        nlohmann::json complete_data;
                        std::string complete_err;
                        if (!parseApiDataResponse(complete_resp, complete_data, complete_err)) {
                            if (on_done) {
                                on_done(false, FileInfo{}, "complete failed: " + complete_err);
                            }
                            return;
                        }

                        FileInfo info = parseFileInfo(complete_data);
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
        nlohmann::json data;
        std::string err;
        if (!parseApiDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, "", "getDownloadUrl failed: " + err);
            }
            return;
        }

        const std::string url = jsonString(data, { "downloadUrl", "download_url" });
        if (url.empty()) {
            if (cb) {
                cb(false, "", "download url missing in response");
            }
            return;
        }

        if (cb) {
            cb(true, url, "");
        }
    });
}

void FileManagerImpl::getFileInfo(const std::string& file_id, FileInfoCallback cb) {
    http_->get("/files/" + file_id, [cb](network::HttpResponse resp) {
        nlohmann::json data;
        std::string err;
        if (!parseApiDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, FileInfo{}, "getFileInfo failed: " + err);
            }
            return;
        }

        if (cb) {
            cb(true, parseFileInfo(data), "");
        }
    });
}

void FileManagerImpl::listFiles(const std::string& file_type, int page, int page_size, FileListCallback cb) {
    const int safe_page = page > 0 ? page : 1;
    const int safe_page_size = page_size > 0 ? page_size : 20;

    std::string path =
        "/files?page=" + std::to_string(safe_page) + "&pageSize=" + std::to_string(safe_page_size);
    if (!file_type.empty()) {
        path += "&fileType=" + urlEncode(file_type);
    }

    http_->get(path, [cb](network::HttpResponse resp) {
        nlohmann::json data;
        std::string err;
        if (!parseApiDataResponse(resp, data, err)) {
            if (cb) {
                cb({}, 0, "listFiles failed: " + err);
            }
            return;
        }

        std::vector<FileInfo> files;
        const auto* files_field = findField(data, { "files", "list", "items" });
        if (files_field != nullptr && files_field->is_array()) {
            files.reserve(files_field->size());
            for (const auto& item : *files_field) {
                if (item.is_object()) {
                    files.push_back(parseFileInfo(item));
                }
            }
        }

        int64_t total = jsonInt64(data, { "total" });
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

    nlohmann::json req_body = {
        { "fileName", file_name },
        { "fileSize", static_cast<int64_t>(file_bytes->size()) },
        { "expiresHours", expires_hours < 0 ? 0 : expires_hours },
    };

    http_->post(
        "/logs/upload",
        req_body.dump(),
        [this, file_bytes, on_progress, on_done](network::HttpResponse resp) {
            nlohmann::json data;
            std::string err;
            if (!parseApiDataResponse(resp, data, err)) {
                if (on_done) {
                    on_done(false, FileInfo{}, "log upload init failed: " + err);
                }
                return;
            }

            const std::string file_id = jsonString(data, { "fileId", "file_id" });
            const std::string upload_url = jsonString(data, { "uploadUrl", "upload_url" });
            if (file_id.empty() || upload_url.empty()) {
                if (on_done) {
                    on_done(false, FileInfo{}, "log upload init response missing file id or upload url");
                }
                return;
            }

            if (on_progress) {
                on_progress(0, static_cast<int64_t>(file_bytes->size()));
            }

            http_->put(
                upload_url,
                *file_bytes,
                [this, file_id, file_bytes, on_progress, on_done](network::HttpResponse put_resp) {
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

                    nlohmann::json complete_body = { { "file_id", file_id } };
                    http_->post("/logs/complete", complete_body.dump(), [file_id, on_done](network::HttpResponse complete_resp) {
                        nlohmann::json complete_data;
                        std::string complete_err;
                        if (!parseApiDataResponse(complete_resp, complete_data, complete_err)) {
                            if (on_done) {
                                on_done(false, FileInfo{}, "log upload complete failed: " + complete_err);
                            }
                            return;
                        }

                        FileInfo info = parseFileInfo(complete_data);
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
        nlohmann::json data;
        std::string err;
        if (!parseApiDataResponse(resp, data, err)) {
            if (cb) {
                cb(false, "deleteFile failed: " + err);
            }
            return;
        }

        if (data.is_object() && !jsonBool(data, { "success" }, true)) {
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
