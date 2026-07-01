#pragma once

#include <string>

#include "notify/NotificationChannel.hpp"

namespace dsd {

// SMTP connection + envelope settings for EmailChannel. Built from env in the
// ChannelFactory; user/password may be empty for an unauthenticated relay.
struct EmailConfig {
    std::string host;
    int port = 587;        // submission port (STARTTLS)
    std::string user;
    std::string password;
    std::string from;      // envelope + From: header
    std::string to;        // envelope + To: header
};

// Sends alerts as plain-text email over SMTP (libcurl). Enabled only when host,
// from, and to are all set.
class EmailChannel : public NotificationChannel {
public:
    explicit EmailChannel(EmailConfig config);

    std::string name() const override;
    bool enabled() const override;
    bool send(const model::Alert& alert) override;

private:
    EmailConfig config_;
};

}  // namespace dsd
