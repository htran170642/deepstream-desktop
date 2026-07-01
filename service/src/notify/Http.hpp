#pragma once

#include <string>

namespace dsd::notify {

// Result of an HTTP request. `ok` is true only on a 2xx response; `status` is
// the HTTP code (0 if the request never completed); `error` describes a
// transport/HTTP failure and is empty on success.
struct HttpResult {
    bool ok = false;
    long status = 0;
    std::string error;
};

// Run libcurl's global init exactly once, thread-safely. Shared by every
// channel (HTTP and SMTP) so curl_global_init — which is not thread-safe — is
// never invoked twice or concurrently.
void ensureCurlGlobalInit();

// POST `body` as application/json to `url`. Blocking — called on the
// notification worker thread, never the frame/alert path. `timeout_sec` caps
// the whole request so a dead endpoint can't wedge the worker.
HttpResult postJson(const std::string& url, const std::string& body,
                    long timeout_sec = 5);

}  // namespace dsd::notify
