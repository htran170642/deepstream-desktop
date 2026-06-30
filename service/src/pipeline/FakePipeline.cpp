#include "pipeline/FakePipeline.hpp"

#include <chrono>
#include <utility>
#include <vector>

#include "logging/Logger.hpp"

namespace dsd {
namespace {

constexpr auto kEmitInterval = std::chrono::milliseconds(100);

std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

model::Detection makeDetection(std::int64_t camera_id) {
    model::Detection d;
    d.camera_id = camera_id;
    d.class_id = 0;
    d.label = "person";
    d.confidence = 0.9f;
    d.box = {0.4f, 0.4f, 0.2f, 0.3f};
    d.track_id = 1;
    d.timestamp_ms = nowMs();
    return d;
}

}  // namespace

FakePipeline::FakePipeline() = default;

FakePipeline::~FakePipeline() {
    stop();  // RAII: join the worker thread
}

bool FakePipeline::start() {
    if (running_.exchange(true)) {
        return true;  // already running
    }
    worker_ = std::thread(&FakePipeline::run, this);
    Logger::get()->info("FakePipeline started");
    return true;
}

void FakePipeline::stop() {
    if (!running_.exchange(false)) {
        return;  // was not running
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    Logger::get()->info("FakePipeline stopped");
}

bool FakePipeline::isRunning() const {
    return running_.load();
}

bool FakePipeline::addSource(std::int64_t camera_id, const std::string& rtsp_url) {
    if (rtsp_url.empty()) {
        return false;  // invalid source (mirrors a bad URI failing in 5b)
    }
    std::lock_guard<std::mutex> lock(sources_mutex_);
    return sources_.emplace(camera_id, rtsp_url).second;
}

void FakePipeline::removeSource(std::int64_t camera_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    sources_.erase(camera_id);
}

std::size_t FakePipeline::sourceCount() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    return sources_.size();
}

void FakePipeline::setFrameCallback(FrameCallback callback) {
    callback_ = std::move(callback);
}

void FakePipeline::run() {
    while (running_.load()) {
        std::vector<model::Frame> frames;
        {
            // Snapshot active sources, then build one frame per source.
            std::lock_guard<std::mutex> lock(sources_mutex_);
            frames.reserve(sources_.size());
            for (const auto& [camera_id, rtsp_url] : sources_) {
                (void)rtsp_url;
                model::Frame frame;
                frame.camera_id = camera_id;
                frame.timestamp_ms = nowMs();
                frame.width = 1920;
                frame.height = 1080;
                frame.detections.push_back(makeDetection(camera_id));
                // jpeg stays empty on the host (no encode without DeepStream)
                frames.push_back(std::move(frame));
            }
        }
        // Emit OUTSIDE the lock, one callback per source-frame.
        if (callback_) {
            for (const model::Frame& f : frames) {
                callback_(std::move(f));
            }
        }
        std::this_thread::sleep_for(kEmitInterval);
    }
}


}  // namespace dsd
