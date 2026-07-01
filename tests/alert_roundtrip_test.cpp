#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include "AlertManager.hpp"
#include "AlertRepository.hpp"
#include "AlertServiceImpl.hpp"
#include "GrpcServer.hpp"
#include "alert.grpc.pb.h"
#include "pipeline/Alert.hpp"
#include "pipeline/Frame.hpp"

namespace {

dsd::model::Frame personFrame(std::int64_t camera_id, std::int64_t ts,
                              std::vector<std::uint8_t> jpeg = {}) {
    dsd::model::Frame f;
    f.camera_id = camera_id;
    f.timestamp_ms = ts;
    f.width = 1920;
    f.height = 1080;
    f.jpeg = std::move(jpeg);
    dsd::model::Detection d;
    d.camera_id = camera_id;
    d.label = "person";
    d.confidence = 0.9f;
    d.timestamp_ms = ts;
    f.detections.push_back(d);
    return f;
}

std::unique_ptr<dsd::AlertService::Stub> makeStub(int port) {
    auto channel = grpc::CreateChannel("localhost:" + std::to_string(port),
                                       grpc::InsecureChannelCredentials());
    return dsd::AlertService::NewStub(channel);
}

TEST(AlertRoundtrip, ListAndGetSnapshot) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager manager(repo);
    dsd::AlertServiceImpl service(repo);

    dsd::GrpcServer server("localhost:0");
    server.registerService(&service);
    const int port = server.start();
    ASSERT_GT(port, 0);

    // Fire one alert (directly, no pipeline) with a snapshot.
    manager.onFrame(personFrame(1, 1000, {1, 2, 3}));
    manager.flush();  // persistence is async; wait for the worker to drain

    auto stub = makeStub(port);

    // ListAlerts
    {
        grpc::ClientContext ctx;
        dsd::AlertFilter req;
        dsd::AlertList resp;
        ASSERT_TRUE(stub->ListAlerts(&ctx, req, &resp).ok());
        ASSERT_EQ(resp.alerts_size(), 1);
        EXPECT_EQ(resp.alerts(0).label(), "person");
        EXPECT_TRUE(resp.alerts(0).has_snapshot());
    }

    // GetSnapshot
    {
        const std::int64_t id = repo.list().front().id;
        grpc::ClientContext ctx;
        dsd::SnapshotRequest req;
        req.set_alert_id(id);
        dsd::Snapshot resp;
        ASSERT_TRUE(stub->GetSnapshot(&ctx, req, &resp).ok());
        EXPECT_EQ(resp.jpeg(), std::string({1, 2, 3}));
    }
}

TEST(AlertRoundtrip, StreamDeliversNewAlert) {
    dsd::AlertRepository repo(":memory:");
    // `service` before `manager`: ~AlertManager drains its worker (which calls
    // the sink -> service), so service must outlive the manager.
    dsd::AlertServiceImpl service(repo);
    dsd::AlertManager::Config cfg;
    cfg.cooldown_ms = 0;  // every frame fires, so the stream always has data
    dsd::AlertManager manager(repo, cfg);
    manager.setAlertSink(
        [&service](std::shared_ptr<const dsd::model::Alert> a) {
            service.broadcastAlert(std::move(a));
        });

    dsd::GrpcServer server("localhost:0");
    server.registerService(&service);
    const int port = server.start();
    ASSERT_GT(port, 0);

    auto stub = makeStub(port);
    grpc::ClientContext ctx;
    dsd::AlertStreamRequest req;
    auto reader = stub->StreamAlerts(&ctx, req);

    // Keep firing until the reader gets one (avoids the subscribe/fire race).
    std::atomic<bool> stop{false};
    std::thread feeder([&] {
        std::int64_t ts = 0;
        while (!stop.load()) {
            manager.onFrame(personFrame(1, ts += 1000, {1, 2, 3}));  // with image
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    dsd::Alert got;
    ASSERT_TRUE(reader->Read(&got));  // blocks until an alert arrives
    EXPECT_EQ(got.label(), "person");
    // The stream carries metadata only, but must still advertise that a snapshot
    // exists (fetchable via GetSnapshot) — regression guard for the has_snapshot
    // flag being dropped when the JPEG bytes are cleared before broadcast.
    EXPECT_TRUE(got.has_snapshot());

    stop.store(true);
    feeder.join();
    ctx.TryCancel();
    reader->Finish();
}

}  // namespace
