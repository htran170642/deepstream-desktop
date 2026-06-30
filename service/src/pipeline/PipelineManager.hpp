#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <functional>

#include "CameraModel.hpp"
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

    // Sink for filtered detections (e.g. the gRPC stream service). Set before
    // start(); invoked on a pipeline worker thread, so it must be thread-safe.
    using DetectionSink =
        std::function<void(const std::vector<model::Detection>&)>;
    void setDetectionSink(DetectionSink sink);

    // Removes all sources and stops the pipeline.
    void stopAll();

    bool isRunning(std::int64_t camera_id) const;  // is this camera an active source?
    std::size_t runningCount() const;              // number of active sources

private:
    // Runs on the pipeline's worker thread: filter + log a detection batch.
    void onDetections(const std::vector<model::Detection>& detections);

    PipelineFactory factory_;
    
    DetectionProcessor processor_;
    DetectionSink sink_;

    mutable std::mutex mutex_;
    std::unordered_set<std::int64_t> active_cameras_;
    // Declared last so it is destroyed first: the pipeline joins its worker
    // thread before processor_/factory_ are torn down (no callback UAF).
    std::unique_ptr<Pipeline> pipeline_;
};

}  // namespace dsd

