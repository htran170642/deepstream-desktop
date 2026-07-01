#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "CameraManager.hpp"
#include "GrpcServer.hpp"
#include "SystemServiceImpl.hpp"
#include "client/SystemClient.hpp"
#include "pipeline/PipelineManager.hpp"
#include "system/SystemMonitor.hpp"

namespace {

TEST(SystemRoundtrip, GetStatsReportsCpuMemAndCameras) {
    dsd::CameraManager camera_manager(":memory:");
    dsd::model::Camera cam;
    cam.name = "cam-a";
    cam.rtsp_url = "rtsp://x";
    cam.enabled = true;
    camera_manager.add(cam);

    dsd::PipelineManager pipeline_manager;  // FakePipeline factory on host
    dsd::SystemMonitor monitor;
    dsd::SystemServiceImpl service(monitor, camera_manager, pipeline_manager);

    dsd::GrpcServer server("localhost:0");
    server.registerService(&service);
    const int port = server.start();
    ASSERT_GT(port, 0);

    dsd::SystemClient client("localhost:" + std::to_string(port));
    const auto stats = client.stats();
    ASSERT_TRUE(stats.has_value());
    EXPECT_GT(stats->mem_total_bytes, 0);   // parsed from /proc/meminfo
    EXPECT_FALSE(stats->gpu_available);     // host build, no NVML
    EXPECT_FALSE(stats->pipeline_running);  // pipeline not started

    ASSERT_EQ(stats->cameras.size(), 1u);
    EXPECT_EQ(stats->cameras[0].name, "cam-a");
    EXPECT_TRUE(stats->cameras[0].enabled);
    EXPECT_FALSE(stats->cameras[0].running);  // enabled record, but not started
}

}  // namespace
