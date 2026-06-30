#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CameraManager.hpp"
#include "CameraModel.hpp"
#include "GrpcServer.hpp"
#include "StreamServiceImpl.hpp"
#include "client/StreamClient.hpp"
#include "pipeline/Detection.hpp"
#include "pipeline/FakePipeline.hpp"
#include "pipeline/PipelineManager.hpp"
#include "pipeline/Frame.hpp"

namespace {

dsd::PipelineFactory fakeFactory() {
    return [] { return std::make_unique<dsd::FakePipeline>(); };
}

TEST(StreamRoundtrip, StartSubscribeReceivesDetections) {
    // --- Service side: camera DB + (fake) pipeline + stream service ---
    dsd::CameraManager cameras(":memory:");
    dsd::model::Camera cam;
    cam.name = "cam";
    cam.rtsp_url = "file:///x";
    cam.enabled = true;
    const std::int64_t id = cameras.add(cam).id;

    dsd::PipelineManager pipelines(fakeFactory());
    dsd::StreamServiceImpl stream_service(cameras, pipelines);
    pipelines.setFrameSink(
        [&stream_service](dsd::model::Frame f) {
            stream_service.broadcast(std::move(f));
        });

    dsd::GrpcServer server("localhost:0");
    server.registerService(&stream_service);
    const int port = server.start();
    ASSERT_GT(port, 0);

    // --- Client side: start + subscribe + wait for a frame ---
    dsd::StreamClient client("localhost:" + std::to_string(port));
    ASSERT_TRUE(client.startCamera(id));

    std::mutex m;
    std::condition_variable cv;
    int frames = 0;
    dsd::FrameUpdate last;
    client.subscribe(id, [&](std::shared_ptr<const dsd::FrameUpdate> f) {
        std::lock_guard<std::mutex> lock(m);
        last = *f;
        ++frames;
        cv.notify_one();
    });

    {
        std::unique_lock<std::mutex> lock(m);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3),
                                [&] { return frames > 0; }))
            << "no detection frame received within 3s";
    }

    // --- Teardown (stop pipeline while stream_service is still alive) ---
    client.unsubscribe();
    client.stopCamera(id);
    pipelines.stopAll();

    EXPECT_EQ(last.camera_id, id);
    EXPECT_FALSE(last.detections.empty());  // FakePipeline emits a "person"
}

}  // namespace
