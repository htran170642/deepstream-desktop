#pragma once

#include <cstdint>
#include <vector>

#include "pipeline/Detection.hpp"

namespace dsd::model {

// One processed video frame from a single source: its detections plus the
// (optionally) encoded image. `jpeg` is empty until the DeepStream encode fills
// it in Stage 6b; FakePipeline leaves it empty on the host.
struct Frame {
    std::int64_t camera_id = 0;
    std::int64_t timestamp_ms = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> jpeg;          // encoded frame bytes; empty = no image
    std::vector<Detection> detections;       // objects in this frame
};

}  // namespace dsd::model
