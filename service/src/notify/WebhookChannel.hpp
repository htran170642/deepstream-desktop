#pragma once

#include <string>

#include "notify/NotificationChannel.hpp"

namespace dsd {

// Posts alerts as structured JSON to a generic HTTP endpoint. Enabled only when
// a URL is configured.
class WebhookChannel : public NotificationChannel {
public:
    explicit WebhookChannel(std::string url);

    std::string name() const override;
    bool enabled() const override;
    bool send(const model::Alert& alert) override;

private:
    std::string url_;
};

}  // namespace dsd
