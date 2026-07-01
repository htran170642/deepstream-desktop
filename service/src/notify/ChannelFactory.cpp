#include "notify/ChannelFactory.hpp"

#include "logging/Logger.hpp"

#ifdef DSD_WITH_CURL
#include <cstdlib>
#include <string>
#include <utility>

#include "notify/EmailChannel.hpp"
#include "notify/SlackChannel.hpp"
#include "notify/TelegramChannel.hpp"
#include "notify/WebhookChannel.hpp"
#endif

namespace dsd {

#ifdef DSD_WITH_CURL
namespace {

std::string env(const char* key) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string();
}

int envInt(const char* key, int fallback) {
    const char* v = std::getenv(key);
    if (!v || !*v) return fallback;
    try {
        return std::stoi(v);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

std::vector<std::unique_ptr<NotificationChannel>> buildNotificationChannels() {
    std::vector<std::unique_ptr<NotificationChannel>> channels;

    channels.push_back(std::make_unique<SlackChannel>(env("DSD_SLACK_WEBHOOK")));
    channels.push_back(std::make_unique<WebhookChannel>(env("DSD_WEBHOOK_URL")));
    channels.push_back(std::make_unique<TelegramChannel>(
        env("DSD_TELEGRAM_TOKEN"), env("DSD_TELEGRAM_CHAT")));

    EmailConfig email;
    email.host = env("DSD_SMTP_HOST");
    email.port = envInt("DSD_SMTP_PORT", 587);
    email.user = env("DSD_SMTP_USER");
    email.password = env("DSD_SMTP_PASS");
    email.from = env("DSD_SMTP_FROM");
    email.to = env("DSD_SMTP_TO");
    channels.push_back(std::make_unique<EmailChannel>(std::move(email)));

    return channels;
}
#else
std::vector<std::unique_ptr<NotificationChannel>> buildNotificationChannels() {
    Logger::get()->info(
        "Notifications: built without libcurl (DSD_WITH_CURL off); "
        "no channels available");
    return {};
}
#endif

}  // namespace dsd
