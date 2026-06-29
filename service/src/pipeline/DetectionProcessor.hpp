#pragma once

#include <unordered_set>
#include <vector>

#include "pipeline/Detection.hpp"

namespace dsd {

// Filters and normalizes raw detections coming out of a pipeline. Stateless:
// configured once, then process() is a pure function of its input — easy to
// unit-test and safe to call from the pipeline's worker thread.
class DetectionProcessor {
public:
    struct Config {
        float min_confidence = 0.5f;              // drop detections below this
        std::unordered_set<int> allowed_classes;  // empty = allow all classes
    };

    DetectionProcessor() = default;
    explicit DetectionProcessor(Config config);


    // Returns the detections that pass the filters, with bounding boxes
    // clamped to [0, 1].
    std::vector<model::Detection> process(
        const std::vector<model::Detection>& detections) const;

private:
    Config config_;
};

}  // namespace dsd

