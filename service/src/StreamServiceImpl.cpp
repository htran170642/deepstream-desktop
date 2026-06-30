#include "StreamServiceImpl.hpp"

#include <optional>
#include <algorithm>   // std::remove
#include <chrono>

#include "CameraManager.hpp"
#include "CameraModel.hpp"
#include "logging/Logger.hpp"
#include "pipeline/PipelineManager.hpp"
#include "pipeline/Frame.hpp"   // model::Frame (+ model::Detection)

namespace dsd {

StreamServiceImpl::StreamServiceImpl(CameraManager& cameras,
                                     PipelineManager& pipelines)
    : cameras_(cameras), pipelines_(pipelines) {}

grpc::Status StreamServiceImpl::StartCamera(grpc::ServerContext* /*context*/,
                                            const CameraStreamRequest* request,
                                            CameraStreamResponse* response) {
    const std::int64_t id = request->camera_id();
    Logger::get()->info("Calling StreamService(StartCamera) for camera {}", id);

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
        Logger::get()->info("StreamService(StartCamera): started camera {}", id);
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
    Logger::get()->info("StreamService(StopCamera): stopped camera {}", id);
    return grpc::Status::OK;
}

namespace {
constexpr std::size_t kMaxQueue = 8;  // bounded per-subscriber queue (drop oldest)
}  // namespace

void StreamServiceImpl::broadcast(model::Frame frame) {
    // Build the wire message for this single source-frame.
    DetectionFrame out;
    out.set_camera_id(frame.camera_id);
    out.set_timestamp_ms(frame.timestamp_ms);
    out.set_frame_width(frame.width);
    out.set_frame_height(frame.height);
    if (frame.jpeg.empty()) {
        out.set_codec(FRAME_CODEC_NONE);       // 6a/host: detections only
    } else {
        out.set_codec(FRAME_CODEC_JPEG);       // 6b: encoded frame
        out.set_frame(frame.jpeg.data(), frame.jpeg.size());
    }
    for (const model::Detection& d : frame.detections) {
        Detection* pd = out.add_detections();
        pd->set_class_id(d.class_id);
        pd->set_label(d.label);
        pd->set_confidence(d.confidence);
        pd->set_x(d.box.x);
        pd->set_y(d.box.y);
        pd->set_width(d.box.width);
        pd->set_height(d.box.height);
        pd->set_track_id(d.track_id);
    }

    auto shared = std::make_shared<const DetectionFrame>(std::move(out));

    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto it = subscribers_.find(frame.camera_id);
    if (it == subscribers_.end()) {
        return;
    }
    for (const std::shared_ptr<Subscriber>& sub : it->second) {
        std::lock_guard<std::mutex> sub_lock(sub->mutex);
        if (sub->queue.size() >= kMaxQueue) {
            sub->queue.pop_front();
        }
        sub->queue.push_back(shared);   // shared_ptr copy (pointer, không copy bytes)
        sub->cv.notify_one();
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
        std::shared_ptr<const DetectionFrame> frame;
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
        if (!writer->Write(*frame)) {
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
    Logger::get()->info("StreamService(StreamDetections): client left camera {}", camera_id);
    return grpc::Status::OK;
}

}  // namespace dsd
