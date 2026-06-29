#include <gtest/gtest.h>

#include "pipeline/Detection.hpp"
#include "pipeline/DetectionProcessor.hpp"

namespace {

dsd::model::Detection makeDetection(int class_id, float confidence) {
    dsd::model::Detection d;
    d.class_id = class_id;
    d.confidence = confidence;
    d.label = "obj";
    d.box = {0.1f, 0.1f, 0.2f, 0.2f};
    return d;
}

TEST(DetectionProcessor, DropsBelowConfidence) {
    dsd::DetectionProcessor::Config cfg;
    cfg.min_confidence = 0.5f;
    const dsd::DetectionProcessor proc(cfg);

    const auto out =
        proc.process({makeDetection(0, 0.9f), makeDetection(0, 0.3f)});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FLOAT_EQ(out[0].confidence, 0.9f);
}

TEST(DetectionProcessor, EmptyAllowlistKeepsAllClasses) {
    const dsd::DetectionProcessor proc;  // default: min_conf 0.5, allow all classes
    const auto out =
        proc.process({makeDetection(0, 0.9f), makeDetection(7, 0.9f)});
    EXPECT_EQ(out.size(), 2u);
}

TEST(DetectionProcessor, AllowlistFiltersClasses) {
    dsd::DetectionProcessor::Config cfg;
    cfg.allowed_classes = {0};  // keep only class 0
    const dsd::DetectionProcessor proc(cfg);

    const auto out =
        proc.process({makeDetection(0, 0.9f), makeDetection(7, 0.9f)});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].class_id, 0);
}

TEST(DetectionProcessor, ClampsBoundingBox) {
    const dsd::DetectionProcessor proc;
    dsd::model::Detection d = makeDetection(0, 0.9f);
    d.box = {-0.5f, 1.2f, 2.0f, -1.0f};  // out-of-range box

    const auto out = proc.process({d});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FLOAT_EQ(out[0].box.x, 0.0f);
    EXPECT_FLOAT_EQ(out[0].box.y, 1.0f);
    EXPECT_FLOAT_EQ(out[0].box.width, 1.0f);
    EXPECT_FLOAT_EQ(out[0].box.height, 0.0f);
}

}  // namespace
