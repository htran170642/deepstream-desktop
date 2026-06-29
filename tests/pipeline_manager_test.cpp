#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "CameraModel.hpp"
#include "pipeline/FakePipeline.hpp"
#include "pipeline/PipelineManager.hpp"

namespace {

// Inject a factory that always builds a FakePipeline, so these tests are
// deterministic regardless of the ENABLE_DEEPSTREAM build flag.
dsd::PipelineFactory fakeFactory() {
    return [] { return std::make_unique<dsd::FakePipeline>(); };
}

dsd::model::Camera makeCamera(std::int64_t id) {
    dsd::model::Camera c;
    c.id = id;
    c.name = "cam";
    c.rtsp_url = "rtsp://example/" + std::to_string(id);
    c.enabled = true;
    return c;
}

TEST(PipelineManager, StartTracksActiveSources) {
    dsd::PipelineManager manager(fakeFactory());
    EXPECT_EQ(manager.runningCount(), 0u);

    EXPECT_TRUE(manager.start(makeCamera(1)));
    EXPECT_TRUE(manager.isRunning(1));
    EXPECT_EQ(manager.runningCount(), 1u);

    manager.start(makeCamera(2));
    EXPECT_EQ(manager.runningCount(), 2u);
}

TEST(PipelineManager, StartIsIdempotentPerCamera) {
    dsd::PipelineManager manager(fakeFactory());
    manager.start(makeCamera(1));
    manager.start(makeCamera(1));  // same camera again
    EXPECT_EQ(manager.runningCount(), 1u);
}

TEST(PipelineManager, StopRemovesSource) {
    dsd::PipelineManager manager(fakeFactory());
    manager.start(makeCamera(1));
    manager.start(makeCamera(2));

    manager.stop(1);
    EXPECT_FALSE(manager.isRunning(1));
    EXPECT_TRUE(manager.isRunning(2));
    EXPECT_EQ(manager.runningCount(), 1u);
}

TEST(PipelineManager, StopAllClears) {
    dsd::PipelineManager manager(fakeFactory());
    manager.start(makeCamera(1));
    manager.start(makeCamera(2));

    manager.stopAll();
    EXPECT_EQ(manager.runningCount(), 0u);
}

TEST(PipelineManager, StopUnknownCameraIsNoop) {
    dsd::PipelineManager manager(fakeFactory());
    manager.stop(999);  // never started
    EXPECT_EQ(manager.runningCount(), 0u);
}

TEST(PipelineManager, StartFailsForInvalidSource) {
    dsd::PipelineManager manager(fakeFactory());
    dsd::model::Camera bad = makeCamera(1);
    bad.rtsp_url.clear();  // invalid -> addSource fails

    EXPECT_FALSE(manager.start(bad));
    EXPECT_FALSE(manager.isRunning(1));
    EXPECT_EQ(manager.runningCount(), 0u);  // empty pipeline torn down
}

}  // namespace
