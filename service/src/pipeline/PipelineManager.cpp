#include "pipeline/PipelineManager.hpp"

#include <utility>

#include "logging/Logger.hpp"

namespace dsd {

PipelineManager::PipelineManager(PipelineFactory factory,
                                 DetectionProcessor::Config processor_config)
    : factory_(std::move(factory)),
      processor_(std::move(processor_config)) {}

PipelineManager::~PipelineManager() {
    stopAll();
}

void PipelineManager::setDetectionSink(DetectionSink sink) {
    // Contract: call before start(). sink_ is read on the pipeline worker
    // thread (onDetections) without a lock, so it must not change while running.
    sink_ = std::move(sink);
}


bool PipelineManager::start(const model::Camera& camera) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!pipeline_) {
        // First source: create and start the pipeline.
        pipeline_ = factory_();
        pipeline_->setDetectionCallback(
            [this](const std::vector<model::Detection>& dets) {
                onDetections(dets);
            });
        if (!pipeline_->start()) {
            Logger::get()->error("Pipeline failed to start");
            pipeline_.reset();
            return false;
        }
    }

    if (!pipeline_->addSource(camera.id, camera.rtsp_url)) {
        // A duplicate source is benign — the camera is already active.
        if (active_cameras_.count(camera.id) > 0) {
            return true;
        }
        // A genuine failure (bad URI, unreachable): do not mark it active.
        Logger::get()->error("Failed to add camera {} as a source", camera.id);
        if (active_cameras_.empty()) {
            pipeline_->stop();  // we just started an empty pipeline — tear it down
            pipeline_.reset();
        }
        return false;
    }
    active_cameras_.insert(camera.id);
    Logger::get()->info("Camera {} started ({} active)", camera.id,
                        active_cameras_.size());
    return true;

}

void PipelineManager::stop(std::int64_t camera_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_cameras_.erase(camera_id) == 0) {
        return;  // not an active source
    }
    if (pipeline_) {
        pipeline_->removeSource(camera_id);
        if (active_cameras_.empty()) {
            pipeline_->stop();   // no sources left -> release the pipeline
            pipeline_.reset();
        }
    }
    Logger::get()->info("Camera {} stopped ({} active)", camera_id,
                        active_cameras_.size());
}

void PipelineManager::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }
    active_cameras_.clear();
}

bool PipelineManager::isRunning(std::int64_t camera_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_cameras_.count(camera_id) > 0;
}

std::size_t PipelineManager::runningCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_cameras_.size();
}

void PipelineManager::onDetections(
    const std::vector<model::Detection>& detections) {
    std::vector<model::Detection> filtered = processor_.process(detections);
    if (filtered.empty()) {
        return;
    }
    if (sink_) {
        sink_(filtered);  // forward to the stream service (or any observer)
    } else {
        Logger::get()->debug("Processed {} detection(s) from {} raw (no sink)",
                             filtered.size(), detections.size());
    }
}

}  // namespace dsd
