#include "StreamServiceImpl.hpp"

#include <optional>
#include <algorithm>   // std::remove
#include <chrono>

#include "CameraManager.hpp"
#include "CameraModel.hpp"
#include "logging/Logger.hpp"
#include "pipeline/PipelineManager.hpp"

namespace dsd {

StreamServiceImpl::StreamServiceImpl(CameraManager& cameras,
                                     PipelineManager& pipelines)
    : cameras_(cameras), pipelines_(pipelines) {}

grpc::Status StreamServiceImpl::StartCamera(grpc::ServerContext* /*context*/,
                                            const CameraStreamRequest* request,
                                            CameraStreamResponse* response) {
    const std::int64_t id = request->camera_id();
    try {
        // Look up the camera to get its source URI.
        std::optional<model::Camera> found;
        for (const model::Camera& c : cameras_.list()) {
            if (c.id == id) {
                found = c;
                break;
            }
        }
        if (!found) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "camera not found");
        }

        const bool ok = pipelines_.start(*found);
        response->set_running(ok);
        if (!ok) {
            return grpc::Status(grpc::StatusCode::INTERNAL,
                                "pipeline failed to start");
        }
        Logger::get()->info("StreamService: started camera {}", id);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::get()->error("StartCamera({}) failed: {}", id, e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status StreamServiceImpl::StopCamera(grpc::ServerContext* /*context*/,
                                           const CameraStreamRequest* request,
                                           CameraStreamResponse* response) {
    const std::int64_t id = request->camera_id();
    pipelines_.stop(id);

    // End any Live View streams for this camera: wake their loops to exit.
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto it = subscribers_.find(id);
        if (it != subscribers_.end()) {
            for (const std::shared_ptr<Subscriber>& sub : it->second) {
                std::lock_guard<std::mutex> sub_lock(sub->mutex);
                sub->cancelled = true;
                sub->cv.notify_one();
            }
        }
    }

    response->set_running(false);
    Logger::get()->info("StreamService: stopped camera {}", id);
    return grpc::Status::OK;
}

namespace {
constexpr std::size_t kMaxQueue = 8;  // bounded per-subscriber queue (drop oldest)
}  // namespace

void StreamServiceImpl::broadcast(const std::vector<model::Detection>& detections) {
    if (detections.empty()) {
        return;
    }

    // Group detections by camera_id -> one DetectionFrame each (a batch may
    // span several sources).
    std::unordered_map<std::int64_t, DetectionFrame> frames;
    for (const model::Detection& d : detections) {
        DetectionFrame& frame = frames[d.camera_id];
        frame.set_camera_id(d.camera_id);
        if (frame.timestamp_ms() == 0) {
            frame.set_timestamp_ms(d.timestamp_ms);
        }
        frame.set_codec(FRAME_CODEC_NONE);  // 6a: no encoded video yet

        Detection* pd = frame.add_detections(); // add a detection
        pd->set_class_id(d.class_id);
        pd->set_label(d.label);
        pd->set_confidence(d.confidence);
        pd->set_x(d.box.x);
        pd->set_y(d.box.y);
        pd->set_width(d.box.width);
        pd->set_height(d.box.height);
        pd->set_track_id(d.track_id);
    }

    // Push each camera's frame to its subscribers.
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (auto& [camera_id, frame] : frames) {
        auto it = subscribers_.find(camera_id);
        if (it == subscribers_.end()) {
            continue;  // nobody watching this camera
        }
        for (const std::shared_ptr<Subscriber>& sub : it->second) {
            std::lock_guard<std::mutex> sub_lock(sub->mutex);
            if (sub->queue.size() >= kMaxQueue) {
                sub->queue.pop_front();  // slow consumer -> drop oldest
            }
            sub->queue.push_back(frame);
            sub->cv.notify_one();
        }
    }
}

grpc::Status StreamServiceImpl::StreamDetections(
    grpc::ServerContext* context, const CameraStreamRequest* request,
    grpc::ServerWriter<DetectionFrame>* writer) {
    const std::int64_t camera_id = request->camera_id();

    // Register this client as a subscriber for the camera.
    auto sub = std::make_shared<Subscriber>();
    sub->camera_id = camera_id;
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_[camera_id].push_back(sub);
    }
    Logger::get()->info("StreamService: client subscribed to camera {}", camera_id);

    // Drain frames to the client until it cancels or disconnects.
    while (!context->IsCancelled()) {
        DetectionFrame frame;
        {
            std::unique_lock<std::mutex> lock(sub->mutex);
            // Wait up to 200ms for a frame, then re-check cancellation.
            sub->cv.wait_for(lock, std::chrono::milliseconds(200), [&] {
                return !sub->queue.empty() || sub->cancelled;
            });
            if (sub->cancelled && sub->queue.empty()) {
                break;  // camera stopped -> drain done, end the stream
            }
            if (sub->queue.empty()) {
                continue;  // timeout -> loop and re-check IsCancelled()
            }
            frame = std::move(sub->queue.front());
            sub->queue.pop_front();
        }
        if (!writer->Write(frame)) {
            break;  // client went away
        }
    }

    // Deregister this subscriber.
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto& vec = subscribers_[camera_id];
        vec.erase(std::remove(vec.begin(), vec.end(), sub), vec.end());
        if (vec.empty()) {
            subscribers_.erase(camera_id);
        }
    }
    Logger::get()->info("StreamService: client left camera {}", camera_id);
    return grpc::Status::OK;
}

}  // namespace dsd
