#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "GrpcServer.hpp"
#include "client/HealthClient.hpp"

namespace {

// Full round-trip: a real server on an ephemeral port, a real client over gRPC.
TEST(HealthRoundtrip, ServingWhenServerRunning) {
    dsd::GrpcServer server("localhost:0");  // port 0 -> OS picks a free port
    const int port = server.start();
    ASSERT_GT(port, 0) << "server failed to start";

    dsd::HealthClient client("localhost:" + std::to_string(port));
    EXPECT_EQ(client.check(), dsd::ServiceHealth::Serving);
}

// No server listening -> the client reports Unreachable (not a crash/hang).
TEST(HealthRoundtrip, UnreachableWhenNoServer) {
    dsd::HealthClient client("127.0.0.1:65111");  // nothing listens here
    EXPECT_EQ(client.check(std::chrono::milliseconds(1000)),
              dsd::ServiceHealth::Unreachable);
}

}  // namespace
