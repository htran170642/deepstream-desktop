#pragma once

#include <string>

#include "notify/NotificationChannel.hpp"

namespace dsd {

// Posts alerts to a Slack incoming webhook. Enabled only when a webhook URL is
// configured (the URL itself encodes the destination channel).
class SlackChannel : public NotificationChannel {
public:
    explicit SlackChannel(std::string webhook_url);

    std::string name() const override;
    bool enabled() const override;
    bool send(const model::Alert& alert) override;

private:
    std::string webhook_url_;
};

}  // namespace dsd
