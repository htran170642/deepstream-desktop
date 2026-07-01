#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <functional>
#include <atomic> 

#include "CameraModel.hpp"
#include "pipeline/Frame.hpp"
#include "pipeline/Detection.hpp"
#include "pipeline/DetectionProcessor.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/PipelineFactory.hpp"

namespace dsd {

// Supervises the single multi-source Pipeline (DeepStream-style): starts it
// lazily on first use, adds/removes camera sources, and filters detections
// through a shared DetectionProcessor. Thread-safe: start/stop are mutex-guarded.
class PipelineManager {
public:
    explicit PipelineManager(PipelineFactory factory = createPipeline,
                             DetectionProcessor::Config processor_config = {});
    ~PipelineManager();

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    // Adds the camera as a source, starting the pipeline on first use.
    // Returns true if the camera is now an active source.
    bool start(const model::Camera& camera);

    // Removes the camera source; stops the pipeline once the last source goes.
    void stop(std::int64_t camera_id);

    // Sink for processed frames (e.g. the gRPC stream service). Set before
    // start(); invoked on a pipeline worker thread, so it must be thread-safe.
    using FrameSink = std::function<void(model::Frame)>;
    void setFrameSink(FrameSink sink);

    // Removes all sources and stops the pipeline.
    void stopAll();

    bool isRunning(std::int64_t camera_id) const;  // is this camera an active source?
    std::size_t runningCount() const;              // number of active sources
    
    std::uint64_t framesProcessed() const;  // cumulative frames through the sink

private:
    // Runs on a pipeline's worker thread: filter the frame's detections + sink it.
    void onFrame(model::Frame frame);

    PipelineFactory factory_;
    
    DetectionProcessor processor_;
    FrameSink sink_;

    mutable std::mutex mutex_;
    std::unordered_set<std::int64_t> active_cameras_;
    std::atomic<std::uint64_t> frames_processed_{0};

    // Declared last so it is destroyed first: the pipeline joins its worker
    // thread before processor_/factory_ are torn down (no callback UAF).
    std::unique_ptr<Pipeline> pipeline_;
};

}  // namespace dsd

