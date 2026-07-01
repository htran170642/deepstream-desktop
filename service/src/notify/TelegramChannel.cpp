#include "notify/TelegramChannel.hpp"

#include <utility>

#include "logging/Logger.hpp"
#include "notify/Http.hpp"
#include "notify/Payloads.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {

TelegramChannel::TelegramChannel(std::string bot_token, std::string chat_id)
    : bot_token_(std::move(bot_token)), chat_id_(std::move(chat_id)) {}

std::string TelegramChannel::name() const { return "telegram"; }

bool TelegramChannel::enabled() const {
    return !bot_token_.empty() && !chat_id_.empty();
}

bool TelegramChannel::send(const model::Alert& alert) {
    // The bot token is a URL path segment; it never appears in a log line.
    const std::string url =
        "https://api.telegram.org/bot" + bot_token_ + "/sendMessage";
    const notify::HttpResult r =
        notify::postJson(url, notify::buildTelegramBody(alert, chat_id_));
    if (!r.ok) {
        Logger::get()->warn("Telegram notification failed: {}",
                            r.error.empty() ? "unknown error" : r.error);
    }
    return r.ok;
}

}  // namespace dsd
