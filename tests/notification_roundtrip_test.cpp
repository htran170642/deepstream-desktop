#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GrpcServer.hpp"
#include "NotificationServiceImpl.hpp"
#include "client/NotificationClient.hpp"
#include "notify/NotificationChannel.hpp"
#include "notify/NotificationManager.hpp"
#include "pipeline/Alert.hpp"

namespace {

// A channel with no network: enabled() drives status, send() returns enabled_
// so the "enabled channel accepts the test" path is exercised without a socket.
class FakeChannel : public dsd::NotificationChannel {
public:
    FakeChannel(std::string name, bool enabled)
        : name_(std::move(name)), enabled_(enabled) {}
    std::string name() const override { return name_; }
    bool enabled() const override { return enabled_; }
    bool send(const dsd::model::Alert&) override { return enabled_; }

    std::string name_;
    bool enabled_;
};

std::vector<std::unique_ptr<dsd::NotificationChannel>> makeChannels() {
    std::vector<std::unique_ptr<dsd::NotificationChannel>> v;
    v.push_back(std::make_unique<FakeChannel>("slack", true));
    v.push_back(std::make_unique<FakeChannel>("email", false));
    return v;
}

TEST(NotificationRoundtrip, StatusAndSendTest) {
    dsd::NotificationManager manager(makeChannels());
    dsd::NotificationServiceImpl service(manager);

    dsd::GrpcServer server("localhost:0");
    server.registerService(&service);
    const int port = server.start();
    ASSERT_GT(port, 0);

    dsd::NotificationClient client("localhost:" + std::to_string(port));

    // GetStatus: both channels reported, with correct enabled flags.
    const auto status = client.status();
    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(status->size(), 2u);
    EXPECT_EQ((*status)[0].name, "slack");
    EXPECT_TRUE((*status)[0].enabled);
    EXPECT_EQ((*status)[1].name, "email");
    EXPECT_FALSE((*status)[1].enabled);

    // SendTest: only the enabled channel is attempted, and it reports ok.
    const auto results = client.sendTest();
    ASSERT_TRUE(results.has_value());
    ASSERT_EQ(results->size(), 1u);
    EXPECT_EQ((*results)[0].name, "slack");
    EXPECT_TRUE((*results)[0].ok);
}

}  // namespace
