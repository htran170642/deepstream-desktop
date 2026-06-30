#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "stream.grpc.pb.h"

namespace dsd {

// Plain, UI-facing detection (normalized bbox), decoupled from proto.
struct DetectionBox {
    int class_id = 0;
    std::string label;
    float confidence = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::int64_t track_id = -1;
};

// One frame's worth of overlay data for the desktop.
struct FrameUpdate {
    std::int64_t camera_id = 0;
    std::int64_t timestamp_ms = 0;
    int frame_width = 0;
    int frame_height = 0;
    std::vector<DetectionBox> detections;
    std::vector<std::uint8_t> jpeg;  // encoded frame; empty in 6a / no-video
};

// Qt-free gRPC client for the Live View stream. start()/stop() control the
// camera's pipeline; subscribe() opens the server-stream on a worker thread and
// delivers each FrameUpdate via the callback (called on that worker thread).
class StreamClient {
public:
    // Frames are passed as shared_ptr<const ...> to avoid copying the JPEG
    // bytes across the worker -> UI thread boundary.
    using FrameCallback =
        std::function<void(std::shared_ptr<const FrameUpdate>)>;

    explicit StreamClient(const std::string& address = "localhost:50051");
    ~StreamClient();

    StreamClient(const StreamClient&) = delete;
    StreamClient& operator=(const StreamClient&) = delete;

    bool startCamera(std::int64_t camera_id);
    bool stopCamera(std::int64_t camera_id);

    // Opens the detection stream on a worker thread. Replaces any active stream.
    void subscribe(std::int64_t camera_id, FrameCallback on_frame);
    // Cancels the active stream and joins the worker.
    void unsubscribe();

private:
    void runStream(std::int64_t camera_id, FrameCallback on_frame);

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<StreamService::Stub> stub_;

    std::thread worker_;
    std::unique_ptr<grpc::ClientContext> context_;  // for cancelling the stream
    std::atomic<bool> streaming_{false};
};

}  // namespace dsd
