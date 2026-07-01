#pragma once

#include <string>

namespace dsd {

namespace model { struct Alert; }  // forward-declare the domain type

// Pure alert -> wire-format builders. No I/O, no config state beyond the args:
// every function is deterministic in its inputs, so tests assert on the exact
// string without touching the network. The channels (8b) only add transport.
namespace notify {

// Escape a string for embedding inside a JSON string literal (", \, control
// chars, newlines). The one primitive the JSON builders below rely on.
std::string jsonEscape(const std::string& s);

// Human-readable one-line summary shared by Slack/Telegram/Email bodies.
std::string formatMessage(const model::Alert& alert);

// Email subject line (short, no snapshot info).
std::string formatSubject(const model::Alert& alert);

// Slack incoming-webhook body: {"text":"<message>"}.
std::string buildSlackBody(const model::Alert& alert);

// Telegram sendMessage body: {"chat_id":"<id>","text":"<message>"}.
// chat_id comes from channel config, not the alert — still deterministic.
std::string buildTelegramBody(const model::Alert& alert,
                              const std::string& chat_id);

// Generic webhook body: the full alert as structured JSON (for integrators).
std::string buildWebhookBody(const model::Alert& alert);

}  // namespace notify
}  // namespace dsd
