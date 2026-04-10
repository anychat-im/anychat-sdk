#include "http_client.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

#include <curl/curl.h>

namespace anychat::network {

// ── internal helpers ─────────────────────────────────────────────────────────

namespace {

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

enum class Method
{
    GET,
    POST,
    PUT,
    DEL
};

struct RequestCtx {
    CURL* easy = nullptr;
    curl_slist* headers = nullptr;
    std::string body; // kept alive for the lifetime of the easy handle
    std::string response_body;
    HttpCallback callback;
};

bool isAbsoluteUrl(const std::string& path) {
    return path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
}

} // namespace

// ── Impl ──────────────────────────────────────────────────────────────────────

struct HttpClient::Impl {
    std::string base_url;
    std::string auth_token;
    std::mutex token_mutex;

    CURLM* multi = nullptr;
    std::thread worker;
    std::atomic<bool> running{ false };

    std::mutex queue_mutex;
    std::queue<RequestCtx*> pending;

    explicit Impl(std::string url)
        : base_url(std::move(url)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multi = curl_multi_init();
        running = true;
        worker = std::thread(&Impl::loop, this);
    }

    ~Impl() {
        running = false;
        if (worker.joinable())
            worker.join();
        curl_multi_cleanup(multi);
        curl_global_cleanup();
    }

    void loop() {
        while (running) {
            // Enqueue pending requests
            {
                std::lock_guard<std::mutex> lk(queue_mutex);
                while (!pending.empty()) {
                    curl_multi_add_handle(multi, pending.front()->easy);
                    pending.pop();
                }
            }

            int active = 0;
            curl_multi_perform(multi, &active);
            curl_multi_wait(multi, nullptr, 0, 20 /*ms*/, nullptr);

            // Harvest completed requests
            int msgs_left = 0;
            CURLMsg* msg;
            while ((msg = curl_multi_info_read(multi, &msgs_left))) {
                if (msg->msg != CURLMSG_DONE)
                    continue;

                CURL* easy = msg->easy_handle;
                RequestCtx* ctx = nullptr;
                curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ctx);

                HttpResponse resp;
                long code = 0;
                curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
                resp.status_code = static_cast<int>(code);
                resp.body = std::move(ctx->response_body);
                if (msg->data.result != CURLE_OK)
                    resp.error = curl_easy_strerror(msg->data.result);

                curl_multi_remove_handle(multi, easy);
                if (ctx->headers)
                    curl_slist_free_all(ctx->headers);
                curl_easy_cleanup(easy);

                if (ctx->callback)
                    ctx->callback(std::move(resp));
                delete ctx;
            }
        }
    }

    void enqueue(Method method, const std::string& path, std::string body, HttpCallback cb) {
        auto* ctx = new RequestCtx;
        ctx->body = std::move(body);
        ctx->callback = std::move(cb);
        ctx->easy = curl_easy_init();

        const std::string url = isAbsoluteUrl(path) ? path : (base_url + path);
        curl_easy_setopt(ctx->easy, CURLOPT_URL, url.c_str());
        curl_easy_setopt(ctx->easy, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(ctx->easy, CURLOPT_WRITEDATA, &ctx->response_body);
        curl_easy_setopt(ctx->easy, CURLOPT_PRIVATE, ctx);
        curl_easy_setopt(ctx->easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(ctx->easy, CURLOPT_TIMEOUT_MS, 30000L);

        const bool absolute_url = isAbsoluteUrl(path);

        // Headers
        curl_slist* hdrs = nullptr;
        if (absolute_url) {
            if (method == Method::PUT) {
                hdrs = curl_slist_append(hdrs, "Content-Type: application/octet-stream");
            }
        } else {
            hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
            hdrs = curl_slist_append(hdrs, "Accept: application/json");
            {
                std::lock_guard<std::mutex> lk(token_mutex);
                if (!auth_token.empty()) {
                    std::string auth = "Authorization: Bearer " + auth_token;
                    hdrs = curl_slist_append(hdrs, auth.c_str());
                }
            }
        }
        curl_easy_setopt(ctx->easy, CURLOPT_HTTPHEADER, hdrs);
        ctx->headers = hdrs;

        // Method-specific options
        switch (method) {
            case Method::POST:
                curl_easy_setopt(ctx->easy, CURLOPT_POST, 1L);
                curl_easy_setopt(ctx->easy, CURLOPT_POSTFIELDS, ctx->body.c_str());
                curl_easy_setopt(ctx->easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(ctx->body.size()));
                break;
            case Method::PUT:
                curl_easy_setopt(ctx->easy, CURLOPT_CUSTOMREQUEST, "PUT");
                curl_easy_setopt(ctx->easy, CURLOPT_POSTFIELDS, ctx->body.c_str());
                curl_easy_setopt(ctx->easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(ctx->body.size()));
                break;
            case Method::DEL:
                curl_easy_setopt(ctx->easy, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case Method::GET:
            default:
                break;
        }

        std::lock_guard<std::mutex> lk(queue_mutex);
        pending.push(ctx);
    }
};

// ── HttpClient public API ────────────────────────────────────────────────────

HttpClient::HttpClient(std::string base_url)
    : impl_(std::make_unique<Impl>(std::move(base_url))) {}

HttpClient::~HttpClient() = default;

void HttpClient::setAuthToken(const std::string& token) {
    std::lock_guard<std::mutex> lk(impl_->token_mutex);
    impl_->auth_token = token;
}

void HttpClient::clearAuthToken() {
    std::lock_guard<std::mutex> lk(impl_->token_mutex);
    impl_->auth_token.clear();
}

void HttpClient::get(const std::string& path, HttpCallback cb) {
    impl_->enqueue(Method::GET, path, {}, std::move(cb));
}

void HttpClient::post(const std::string& path, const std::string& body, HttpCallback cb) {
    impl_->enqueue(Method::POST, path, body, std::move(cb));
}

void HttpClient::put(const std::string& path, const std::string& body, HttpCallback cb) {
    impl_->enqueue(Method::PUT, path, body, std::move(cb));
}

void HttpClient::del(const std::string& path, HttpCallback cb) {
    impl_->enqueue(Method::DEL, path, {}, std::move(cb));
}

} // namespace anychat::network
