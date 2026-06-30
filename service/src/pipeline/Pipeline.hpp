#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "pipeline/Frame.hpp"

namespace dsd {

// A single multi-source video pipeline (DeepStream-style: all sources batched
// through one nvstreammux + nvinfer). Implementations:
//   - FakePipeline       (host + tests): synthetic frames per source
//   - DeepStreamPipeline (ENABLE_DEEPSTREAM): real GStreamer + nvinfer
class Pipeline {
public:
    // Invoked once per processed source-frame, on the pipeline's worker thread.
    // Callees must be thread-safe.
    using FrameCallback = std::function<void(model::Frame)>;

    virtual ~Pipeline() = default;

    // Start/stop the whole pipeline (streammux, inference, tracker, ...).
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // Add or remove a camera source while the pipeline runs.
    virtual bool addSource(std::int64_t camera_id,
                           const std::string& rtsp_url) = 0;
    virtual void removeSource(std::int64_t camera_id) = 0;
    virtual std::size_t sourceCount() const = 0;

    // Registers the frame sink. Set before start().
    virtual void setFrameCallback(FrameCallback callback) = 0;
};

}  // namespace dsd
