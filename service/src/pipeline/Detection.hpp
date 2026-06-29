#pragma once

#include <cstdint>
#include <string>

namespace dsd::model {

// Bounding box in normalized coordinates: all values in [0, 1] relative to the
// frame size, so detections are resolution-independent. DetectionProcessor is
// responsible for producing normalized boxes.
struct BoundingBox {
    float x = 0.0f;       // left edge
    float y = 0.0f;       // top edge
    float width = 0.0f;
    float height = 0.0f;
};

// A single detected object in one frame.
struct Detection {
    std::int64_t camera_id = 0;      // which camera produced this detection
    int class_id = 0;                // detector class index (e.g. 0 = person)
    std::string label;               // human-readable class name
    float confidence = 0.0f;         // detection score in [0, 1]
    BoundingBox box;
    std::int64_t track_id = -1;      // tracker id across frames; -1 = untracked
    std::int64_t timestamp_ms = 0;   // capture time, ms since epoch
};

}  // namespace dsd::model

