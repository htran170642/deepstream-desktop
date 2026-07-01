#pragma once

#include <string>

#include "notify/NotificationChannel.hpp"

namespace dsd {

// Sends alerts via the Telegram Bot API (sendMessage). Needs both a bot token
// (in the URL) and a target chat id (in the body); enabled only when both are
// present.
class TelegramChannel : public NotificationChannel {
public:
    TelegramChannel(std::string bot_token, std::string chat_id);

    std::string name() const override;
    bool enabled() const override;
    bool send(const model::Alert& alert) override;

private:
    std::string bot_token_;
    std::string chat_id_;
};

}  // namespace dsd
