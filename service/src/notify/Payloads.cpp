#include "notify/Payloads.hpp"

#include <cstdio>
#include <ctime>

#include "pipeline/Alert.hpp"

namespace dsd::notify {
namespace {

// Fixed-precision helpers: snprintf keeps the locale-independent, exact-width
// formatting JSON/humans want (std::to_string can't control precision).
std::string confidence2(float conf) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2f", conf);
    return buf;
}

std::string confidenceJson(float conf) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.4f", conf);
    return buf;
}

// Epoch millis -> "YYYY-MM-DD HH:MM:SS UTC". UTC keeps it deterministic (no
// host-timezone dependence), which also makes the tests stable.
std::string formatTimestamp(std::int64_t ms) {
    const std::time_t secs = static_cast<std::time_t>(ms / 1000);
    std::tm tm{};
    gmtime_r(&secs, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return buf;
}

}  // namespace

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Other control chars -> \u00XX.
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string formatMessage(const model::Alert& alert) {
    return "\xF0\x9F\x9A\xA8 " + alert.label + " detected on camera " +
           std::to_string(alert.camera_id) + " (confidence " +
           confidence2(alert.confidence) + ") at " +
           formatTimestamp(alert.timestamp_ms);
}

std::string formatSubject(const model::Alert& alert) {
    return "[DeepStream] " + alert.label + " on camera " +
           std::to_string(alert.camera_id);
}

std::string buildSlackBody(const model::Alert& alert) {
    return "{\"text\":\"" + jsonEscape(formatMessage(alert)) + "\"}";
}

std::string buildTelegramBody(const model::Alert& alert,
                              const std::string& chat_id) {
    return "{\"chat_id\":\"" + jsonEscape(chat_id) + "\",\"text\":\"" +
           jsonEscape(formatMessage(alert)) + "\"}";
}

std::string buildWebhookBody(const model::Alert& alert) {
    return "{\"id\":" + std::to_string(alert.id) +
           ",\"camera_id\":" + std::to_string(alert.camera_id) +
           ",\"label\":\"" + jsonEscape(alert.label) +
           "\",\"confidence\":" + confidenceJson(alert.confidence) +
           ",\"timestamp_ms\":" + std::to_string(alert.timestamp_ms) +
           ",\"has_snapshot\":" + (alert.has_snapshot ? "true" : "false") + "}";
}

}  // namespace dsd::notify
