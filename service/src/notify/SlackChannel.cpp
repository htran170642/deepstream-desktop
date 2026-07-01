#include "notify/SlackChannel.hpp"

#include <utility>

#include "logging/Logger.hpp"
#include "notify/Http.hpp"
#include "notify/Payloads.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {

SlackChannel::SlackChannel(std::string webhook_url)
    : webhook_url_(std::move(webhook_url)) {}

std::string SlackChannel::name() const { return "slack"; }

bool SlackChannel::enabled() const { return !webhook_url_.empty(); }

bool SlackChannel::send(const model::Alert& alert) {
    const notify::HttpResult r =
        notify::postJson(webhook_url_, notify::buildSlackBody(alert));
    if (!r.ok) {
        Logger::get()->warn("Slack notification failed: {}",
                            r.error.empty() ? "unknown error" : r.error);
    }
    return r.ok;
}

}  // namespace dsd
