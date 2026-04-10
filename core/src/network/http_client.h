#pragma once

#include <functional>
#include <memory>
#include <string>

namespace anychat::network {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string error; // non-empty on transport failure (not HTTP errors)
};

using HttpCallback = std::function<void(HttpResponse)>;

// Async HTTP client backed by libcurl CURLM (multi interface).
// All callbacks are invoked from an internal worker thread.
class HttpClient {
public:
    explicit HttpClient(std::string base_url);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Set/clear the Bearer token added to every request.
    void setAuthToken(const std::string& token);
    void clearAuthToken();

    // Async HTTP methods. |path| is appended to the base_url set at construction.
    // |body| for POST/PUT must be a JSON string.
    void get(const std::string& path, HttpCallback cb);
    void post(const std::string& path, const std::string& body, HttpCallback cb);
    void put(const std::string& path, const std::string& body, HttpCallback cb);
    void patch(const std::string& path, const std::string& body, HttpCallback cb);
    void del(const std::string& path, HttpCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace anychat::network
