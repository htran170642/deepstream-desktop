#include "notify/EmailChannel.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>

#include <curl/curl.h>

#include "logging/Logger.hpp"
#include "notify/Http.hpp"
#include "notify/Payloads.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {
namespace {

// Streams the message body to libcurl in chunks as it uploads.
struct Upload {
    const std::string* data;
    std::size_t offset = 0;
};

std::size_t readPayload(char* buffer, std::size_t size, std::size_t nmemb,
                        void* userp) {
    auto* up = static_cast<Upload*>(userp);
    const std::size_t room = size * nmemb;
    const std::size_t remaining = up->data->size() - up->offset;
    const std::size_t n = std::min(room, remaining);
    if (n > 0) {
        std::memcpy(buffer, up->data->data() + up->offset, n);
        up->offset += n;
    }
    return n;  // 0 signals end-of-message
}

// RFC 2822 date in the C locale (English day/month names, +0000 UTC).
std::string rfc2822Now() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tm);
    return buf;
}

std::string buildMessage(const EmailConfig& cfg, const model::Alert& alert) {
    // Minimal RFC 5322 message, CRLF line endings as SMTP expects.
    std::string msg;
    msg += "Date: " + rfc2822Now() + "\r\n";
    msg += "From: " + cfg.from + "\r\n";
    msg += "To: " + cfg.to + "\r\n";
    msg += "Subject: " + notify::formatSubject(alert) + "\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "\r\n";
    msg += notify::formatMessage(alert) + "\r\n";
    return msg;
}

}  // namespace

EmailChannel::EmailChannel(EmailConfig config) : config_(std::move(config)) {}

std::string EmailChannel::name() const { return "email"; }

bool EmailChannel::enabled() const {
    return !config_.host.empty() && !config_.from.empty() &&
           !config_.to.empty();
}

bool EmailChannel::send(const model::Alert& alert) {
    notify::ensureCurlGlobalInit();

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::get()->warn("Email notification failed: curl_easy_init");
        return false;
    }

    const std::string message = buildMessage(config_, alert);
    Upload upload{&message, 0};
    curl_slist* recipients = curl_slist_append(nullptr, config_.to.c_str());
    const std::string url =
        "smtp://" + config_.host + ":" + std::to_string(config_.port);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (!config_.user.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, config_.user.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, config_.password.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, config_.from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readPayload);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        Logger::get()->warn("Email notification failed: {}",
                            curl_easy_strerror(rc));
        return false;
    }
    return true;
}

}  // namespace dsd
