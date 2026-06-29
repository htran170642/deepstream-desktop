#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "pipeline/Detection.hpp"

namespace dsd {

// A single multi-source video pipeline. This matches DeepStream's design: all
// camera sources are batched through one nvstreammux + nvinfer for efficient
// GPU inference. Implementations:
//   - FakePipeline       (host + tests): synthetic detections per source
//   - DeepStreamPipeline (ENABLE_DEEPSTREAM): real GStreamer + nvinfer
class Pipeline {
public:
    // Invoked on the pipeline's worker thread for each batch of detections.
    // Each detection carries the camera_id of the source it came from.
    using DetectionCallback =
        std::function<void(const std::vector<model::Detection>&)>;

    virtual ~Pipeline() = default;

    // Start/stop the whole pipeline (streammux, inference, tracker, ...).
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // Add or remove a camera source while the pipeline runs. Returns false if
    // the source could not be added (bad URI, duplicate id, ...).
    virtual bool addSource(std::int64_t camera_id,
                           const std::string& rtsp_url) = 0;
    virtual void removeSource(std::int64_t camera_id) = 0;
    virtual std::size_t sourceCount() const = 0;

    // Registers the detection sink. Set before start().
    virtual void setDetectionCallback(DetectionCallback callback) = 0;
};

}  // namespace dsd
