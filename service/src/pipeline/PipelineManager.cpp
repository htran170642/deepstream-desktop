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

void PipelineManager::setFrameSink(FrameSink sink) {
    // Contract: call before start(). sink_ is read on the pipeline worker
    // thread (onFrame) without a lock, so it must not change while running.
    sink_ = std::move(sink);
}

bool PipelineManager::start(const model::Camera& camera) {
    std::lock_guard<std::mutex> lock(mutex_);

    // A previous pipeline that ended on its own (EOS) is no longer running.
    if (pipeline_ && !pipeline_->isRunning()) {
        pipeline_->stop();        // clean teardown of the parked pipeline
        pipeline_.reset();
        active_cameras_.clear();
    }
    
    if (!pipeline_) {
        // First source: create and start the pipeline.
        pipeline_ = factory_();
        pipeline_->setFrameCallback(
            [this](model::Frame frame) { onFrame(std::move(frame)); });


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
        if (active_cameras_.empty()) {
            pipeline_->stop();   // no sources left -> release the pipeline
            pipeline_.reset();
        } else {
            pipeline_->removeSource(camera_id);
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

void PipelineManager::onFrame(model::Frame frame) {
    frame.detections = processor_.process(frame.detections);  // filter in place
    if (sink_) {
        sink_(std::move(frame));   // move frame (incl. jpeg) onward — no copy
    } else {
        Logger::get()->debug("Processed {} detection(s) (no sink)",
                             frame.detections.size());
    }
}

}  // namespace dsd
