#include "notify/WebhookChannel.hpp"

#include <utility>

#include "logging/Logger.hpp"
#include "notify/Http.hpp"
#include "notify/Payloads.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {

WebhookChannel::WebhookChannel(std::string url) : url_(std::move(url)) {}

std::string WebhookChannel::name() const { return "webhook"; }

bool WebhookChannel::enabled() const { return !url_.empty(); }

bool WebhookChannel::send(const model::Alert& alert) {
    const notify::HttpResult r =
        notify::postJson(url_, notify::buildWebhookBody(alert));
    if (!r.ok) {
        Logger::get()->warn("Webhook notification failed: {}",
                            r.error.empty() ? "unknown error" : r.error);
    }
    return r.ok;
}

}  // namespace dsd
