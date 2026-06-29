#include "client/CameraClient.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include "logging/Logger.hpp"

namespace dsd {
namespace {

// UI struct -> proto message.
Camera toProto(const CameraInfo& c) {
    Camera p;
    p.set_id(c.id);
    p.set_name(c.name);
    p.set_rtsp_url(c.rtspUrl);
    p.set_enabled(c.enabled);
    return p;
}

// Proto message -> UI struct.
CameraInfo fromProto(const Camera& p) {
    CameraInfo c;
    c.id = p.id();
    c.name = p.name();
    c.rtspUrl = p.rtsp_url();
    c.enabled = p.enabled();
    return c;
}

std::chrono::system_clock::time_point deadlineFrom(
    std::chrono::milliseconds timeout) {
    return std::chrono::system_clock::now() + timeout;
}

}  // namespace

CameraClient::CameraClient(const std::string& address)
    : channel_(grpc::CreateChannel(address, grpc::InsecureChannelCredentials())),
      stub_(CameraService::NewStub(channel_)) {}

std::optional<std::vector<CameraInfo>> CameraClient::list(
    std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    ListCamerasRequest request;
    ListCamerasResponse response;
    const grpc::Status status = stub_->ListCameras(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("ListCameras failed: {}", status.error_message());
        return std::nullopt;
    }

    std::vector<CameraInfo> cameras;
    cameras.reserve(response.cameras_size());
    for (const Camera& c : response.cameras()) {
        cameras.push_back(fromProto(c));
    }
    return cameras;
}

std::optional<CameraInfo> CameraClient::add(const CameraInfo& camera,
                                            std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    AddCameraRequest request;
    request.set_name(camera.name);
    request.set_rtsp_url(camera.rtspUrl);
    request.set_enabled(camera.enabled);

    CameraResponse response;
    const grpc::Status status = stub_->AddCamera(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("AddCamera failed: {}", status.error_message());
        return std::nullopt;
    }
    return fromProto(response.camera());
}

std::optional<CameraInfo> CameraClient::update(const CameraInfo& camera,
                                               std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    UpdateCameraRequest request;
    *request.mutable_camera() = toProto(camera);

    CameraResponse response;
    const grpc::Status status = stub_->UpdateCamera(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("UpdateCamera failed: {}", status.error_message());
        return std::nullopt;
    }
    return fromProto(response.camera());
}

bool CameraClient::remove(std::int64_t id, std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    DeleteCameraRequest request;
    request.set_id(id);

    DeleteCameraResponse response;
    const grpc::Status status = stub_->DeleteCamera(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("DeleteCamera failed: {}", status.error_message());
        return false;
    }
    return true;
}

}  // namespace dsd
