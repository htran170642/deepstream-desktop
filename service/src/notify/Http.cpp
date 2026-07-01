#include "notify/Http.hpp"

#include <mutex>
#include <string>

#include <curl/curl.h>

namespace dsd::notify {
namespace {

// We don't care about the response body; swallow it so libcurl doesn't dump it
// to stdout.
std::size_t discardBody(char*, std::size_t size, std::size_t nmemb, void*) {
    return size * nmemb;
}

}  // namespace

// libcurl requires one global init before any easy handle. Thread-safe, once.
void ensureCurlGlobalInit() {
    static std::once_flag flag;
    std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

HttpResult postJson(const std::string& url, const std::string& body,
                    long timeout_sec) {
    ensureCurlGlobalInit();

    HttpResult result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }

    curl_slist* headers =
        curl_slist_append(nullptr, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body.c_str());  // copies body
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardBody);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);  // safe timeouts off-main-thread

    const CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        result.ok = (result.status >= 200 && result.status < 300);
        if (!result.ok) {
            result.error = "HTTP status " + std::to_string(result.status);
        }
    } else {
        result.error = curl_easy_strerror(rc);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

}  // namespace dsd::notify
