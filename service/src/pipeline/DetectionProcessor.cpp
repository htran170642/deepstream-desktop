#include "pipeline/DetectionProcessor.hpp"

#include <algorithm>
#include <utility>

namespace dsd {
namespace {

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

}  // namespace

DetectionProcessor::DetectionProcessor(Config config)
    : config_(std::move(config)) {}

std::vector<model::Detection> DetectionProcessor::process(
    const std::vector<model::Detection>& detections) const {
    std::vector<model::Detection> result;
    result.reserve(detections.size());

    for (const model::Detection& d : detections) {
        if (d.confidence < config_.min_confidence) {
            continue;  // below the confidence threshold
        }
        if (!config_.allowed_classes.empty() &&
            config_.allowed_classes.count(d.class_id) == 0) {
            continue;  // class not in the allowlist
        }

        model::Detection normalized = d;
        normalized.box.x = clamp01(normalized.box.x);
        normalized.box.y = clamp01(normalized.box.y);
        normalized.box.width = clamp01(normalized.box.width);
        normalized.box.height = clamp01(normalized.box.height);
        result.push_back(std::move(normalized));
    }

    return result;
}

}  // namespace dsd
