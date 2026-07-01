#include <string>

#include <gtest/gtest.h>

#include "notify/Payloads.hpp"
#include "pipeline/Alert.hpp"

namespace {

dsd::model::Alert makeAlert() {
    dsd::model::Alert a;
    a.id = 7;
    a.camera_id = 2;
    a.label = "person";
    a.confidence = 0.9375f;  // exact in float -> %.4f == "0.9375"
    a.timestamp_ms = 0;      // epoch -> 1970-01-01 00:00:00 UTC (deterministic)
    a.has_snapshot = true;
    return a;
}

TEST(Payloads, JsonEscapeHandlesSpecials) {
    using dsd::notify::jsonEscape;
    EXPECT_EQ(jsonEscape("a\"b"), "a\\\"b");   // double quote
    EXPECT_EQ(jsonEscape("a\\b"), "a\\\\b");   // backslash
    EXPECT_EQ(jsonEscape("a\nb"), "a\\nb");    // newline
    EXPECT_EQ(jsonEscape("plain"), "plain");   // untouched
}

TEST(Payloads, WebhookBodyIsExactStructuredJson) {
    const std::string body = dsd::notify::buildWebhookBody(makeAlert());
    EXPECT_EQ(body,
              "{\"id\":7,\"camera_id\":2,\"label\":\"person\","
              "\"confidence\":0.9375,\"timestamp_ms\":0,"
              "\"has_snapshot\":true}");
}

TEST(Payloads, SlackBodyWrapsMessageText) {
    const std::string body = dsd::notify::buildSlackBody(makeAlert());
    // Shape: {"text":"..."} with the human message inside.
    EXPECT_EQ(body.rfind("{\"text\":\"", 0), 0u);  // starts with
    EXPECT_NE(body.find("person detected on camera 2"), std::string::npos);
    EXPECT_NE(body.find("0.94"), std::string::npos);       // %.2f rounding
    EXPECT_NE(body.find("1970-01-01 00:00:00 UTC"), std::string::npos);
}

TEST(Payloads, TelegramBodyCarriesChatId) {
    const std::string body =
        dsd::notify::buildTelegramBody(makeAlert(), "12345");
    EXPECT_NE(body.find("\"chat_id\":\"12345\""), std::string::npos);
    EXPECT_NE(body.find("\"text\":\""), std::string::npos);
}

TEST(Payloads, LabelWithQuoteStaysValidJson) {
    dsd::model::Alert a = makeAlert();
    a.label = "gun\"drop";  // a quote in the label must not break the JSON
    const std::string body = dsd::notify::buildWebhookBody(a);
    EXPECT_NE(body.find("\"label\":\"gun\\\"drop\""), std::string::npos);
}

TEST(Payloads, SubjectIsShortAndLabelled) {
    const std::string subj = dsd::notify::formatSubject(makeAlert());
    EXPECT_EQ(subj, "[DeepStream] person on camera 2");
}

}  // namespace
