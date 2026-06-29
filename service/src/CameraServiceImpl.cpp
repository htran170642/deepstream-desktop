#include "CameraServiceImpl.hpp"

#include <stdexcept>

#include "CameraManager.hpp"
#include "CameraModel.hpp"
#include "logging/Logger.hpp"

namespace dsd {
namespace {

// Domain model -> proto message.
Camera toProto(const model::Camera& c) {
    Camera p;
    p.set_id(c.id);
    p.set_name(c.name);
    p.set_rtsp_url(c.rtsp_url);
    p.set_enabled(c.enabled);
    return p;
}

// Proto message -> domain model.
model::Camera fromProto(const Camera& p) {
    model::Camera c;
    c.id = p.id();
    c.name = p.name();
    c.rtsp_url = p.rtsp_url();
    c.enabled = p.enabled();
    return c;
}

}  // namespace

CameraServiceImpl::CameraServiceImpl(CameraManager& manager)
    : manager_(manager) {}

grpc::Status CameraServiceImpl::AddCamera(grpc::ServerContext* /*context*/,
                                          const AddCameraRequest* request,
                                          CameraResponse* response) {
    try {
        model::Camera input;
        input.name = request->name();
        input.rtsp_url = request->rtsp_url();
        input.enabled = request->enabled();

        *response->mutable_camera() = toProto(manager_.add(input));
        return grpc::Status::OK;
    } catch (const std::invalid_argument& e) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
    } catch (const std::exception& e) {
        Logger::get()->error("AddCamera failed: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status CameraServiceImpl::UpdateCamera(grpc::ServerContext* /*context*/,
                                             const UpdateCameraRequest* request,
                                             CameraResponse* response) {
    try {
        const std::optional<model::Camera> updated =
            manager_.update(fromProto(request->camera()));
        if (!updated) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "camera not found");
        }
        *response->mutable_camera() = toProto(*updated);
        return grpc::Status::OK;
    } catch (const std::invalid_argument& e) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
    } catch (const std::exception& e) {
        Logger::get()->error("UpdateCamera failed: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status CameraServiceImpl::DeleteCamera(grpc::ServerContext* /*context*/,
                                             const DeleteCameraRequest* request,
                                             DeleteCameraResponse* /*response*/) {
    try {
        if (!manager_.remove(request->id())) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "camera not found");
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::get()->error("DeleteCamera failed: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status CameraServiceImpl::ListCameras(grpc::ServerContext* /*context*/,
                                            const ListCamerasRequest* /*request*/,
                                            ListCamerasResponse* response) {
    try {
        for (const model::Camera& c : manager_.list()) {
            *response->add_cameras() = toProto(c);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::get()->error("ListCameras failed: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

}  // namespace dsd
