#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "CameraManager.hpp"
#include "CameraServiceImpl.hpp"
#include "GrpcServer.hpp"
#include "client/CameraClient.hpp"

namespace {

// A real in-process gRPC server backed by an in-memory database, plus a real
// client connected to it — exercises the full CameraClient -> service path.
class CameraRoundtripTest : public ::testing::Test {
protected:
    CameraRoundtripTest()
        : manager_(":memory:"), service_(manager_), server_("localhost:0") {
        server_.registerService(&service_);
        const int port = server_.start();
        EXPECT_GT(port, 0) << "server failed to start";
        client_ = std::make_unique<dsd::CameraClient>(
            "localhost:" + std::to_string(port));
    }

    dsd::CameraManager manager_;
    dsd::CameraServiceImpl service_;
    dsd::GrpcServer server_;
    std::unique_ptr<dsd::CameraClient> client_;
};

TEST_F(CameraRoundtripTest, AddListUpdateDelete) {
    // Starts empty.
    auto initial = client_->list();
    ASSERT_TRUE(initial.has_value());
    EXPECT_TRUE(initial->empty());

    // Add.
    dsd::CameraInfo info;
    info.name = "front";
    info.rtspUrl = "rtsp://cam/front";
    info.enabled = true;
    const auto added = client_->add(info);
    ASSERT_TRUE(added.has_value());
    EXPECT_GT(added->id, 0);
    EXPECT_EQ(added->name, "front");

    // List shows it.
    auto listed = client_->list();
    ASSERT_TRUE(listed.has_value());
    ASSERT_EQ(listed->size(), 1u);
    EXPECT_EQ((*listed)[0].id, added->id);

    // Update.
    dsd::CameraInfo edit = *added;
    edit.name = "renamed";
    edit.enabled = false;
    const auto updated = client_->update(edit);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->name, "renamed");
    EXPECT_FALSE(updated->enabled);

    // Delete.
    EXPECT_TRUE(client_->remove(added->id));
    auto after_delete = client_->list();
    ASSERT_TRUE(after_delete.has_value());
    EXPECT_TRUE(after_delete->empty());
}

TEST_F(CameraRoundtripTest, AddInvalidIsRejected) {
    dsd::CameraInfo info;  // empty name + url
    // Server validation -> INVALID_ARGUMENT -> client maps to nullopt.
    EXPECT_FALSE(client_->add(info).has_value());
}

}  // namespace
