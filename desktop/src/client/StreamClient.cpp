#include "client/StreamClient.hpp"

#include <utility>
#include <chrono>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include "logging/Logger.hpp"

namespace dsd {
namespace {

// proto DetectionFrame -> UI FrameUpdate.
FrameUpdate fromProto(const DetectionFrame& f) {
    FrameUpdate u;
    u.camera_id = f.camera_id();
    u.timestamp_ms = f.timestamp_ms();
    u.frame_width = f.frame_width();
    u.frame_height = f.frame_height();
    u.detections.reserve(f.detections_size());
    for (const Detection& d : f.detections()) {
        DetectionBox b;
        b.class_id = d.class_id();
        b.label = d.label();
        b.confidence = d.confidence();
        b.x = d.x();
        b.y = d.y();
        b.width = d.width();
        b.height = d.height();
        b.track_id = d.track_id();
        u.detections.push_back(std::move(b));
    }
    return u;
}

}  // namespace

StreamClient::StreamClient(const std::string& address)
    : channel_(grpc::CreateChannel(address, grpc::InsecureChannelCredentials())),
      stub_(StreamService::NewStub(channel_)) {}

StreamClient::~StreamClient() {
    unsubscribe();  // RAII: cancel + join the worker
}

bool StreamClient::startCamera(std::int64_t camera_id) {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(3));
    CameraStreamRequest request;
    request.set_camera_id(camera_id);
    CameraStreamResponse response;
    const grpc::Status status = stub_->StartCamera(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("StartCamera failed: {}", status.error_message());
        return false;
    }
    return response.running();
}

bool StreamClient::stopCamera(std::int64_t camera_id) {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(3));
    CameraStreamRequest request;
    request.set_camera_id(camera_id);
    CameraStreamResponse response;
    const grpc::Status status = stub_->StopCamera(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("StopCamera failed: {}", status.error_message());
        return false;
    }
    return true;
}

void StreamClient::subscribe(std::int64_t camera_id, FrameCallback on_frame) {
    unsubscribe();  // replace any active stream

    // Create the context BEFORE the worker so unsubscribe() can always cancel
    // (avoids a race where the worker hasn't set it yet).
    context_ = std::make_unique<grpc::ClientContext>();
    streaming_.store(true);
    worker_ = std::thread(&StreamClient::runStream, this, camera_id,
                          std::move(on_frame));
}

void StreamClient::unsubscribe() {
    streaming_.store(false);
    if (context_) {
        context_->TryCancel();  // break the blocking Read()
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    context_.reset();
}

void StreamClient::runStream(std::int64_t camera_id, FrameCallback on_frame) {
    CameraStreamRequest request;
    request.set_camera_id(camera_id);

    std::unique_ptr<grpc::ClientReader<DetectionFrame>> reader(
        stub_->StreamDetections(context_.get(), request));

    DetectionFrame frame;
    while (streaming_.load() && reader->Read(&frame)) {
        if (on_frame) {
            on_frame(fromProto(frame));  // called on this worker thread
        }
        frame.Clear();
    }
    reader->Finish();  // collect final status (ignored; stream ended/cancelled)
}

} // end dsd