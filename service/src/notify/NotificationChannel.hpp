#pragma once

#include <string>

namespace dsd {

namespace model { struct Alert; }  // forward-declare the domain type

// One outbound notification transport (Email, Slack, Telegram, Webhook).
// Implementations are built from environment config and own everything they
// need to deliver a message — the NotificationManager only fans alerts out to
// them, it knows nothing about HTTP or SMTP.
class NotificationChannel {
public:
    virtual ~NotificationChannel() = default;

    // Short identifier for logging (e.g. "slack"). Never include secrets.
    virtual std::string name() const = 0;

    // True only if this channel is fully configured. An unconfigured channel is
    // constructed but skipped, so main can build the full list unconditionally.
    virtual bool enabled() const = 0;

    // Deliver one alert. Returns true on success. May throw on transport error;
    // the manager catches per channel so one failure can't drop the others.
    virtual bool send(const model::Alert& alert) = 0;
};

}  // namespace dsd
