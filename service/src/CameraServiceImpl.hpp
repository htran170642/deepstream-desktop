#pragma once

#include "camera.grpc.pb.h"

namespace dsd {

class CameraManager;  // forward-declare: only a reference is used here

// gRPC boundary for the Camera service. Translates between proto messages and
// the domain model, delegates to CameraManager, and maps errors to gRPC status
// codes. Holds no state of its own beyond a reference to the manager.
class CameraServiceImpl final : public CameraService::Service {
public:
    explicit CameraServiceImpl(CameraManager& manager);

    grpc::Status AddCamera(grpc::ServerContext* context,
                           const AddCameraRequest* request,
                           CameraResponse* response) override;

    grpc::Status UpdateCamera(grpc::ServerContext* context,
                              const UpdateCameraRequest* request,
                              CameraResponse* response) override;

    grpc::Status DeleteCamera(grpc::ServerContext* context,
                              const DeleteCameraRequest* request,
                              DeleteCameraResponse* response) override;

    grpc::Status ListCameras(grpc::ServerContext* context,
                             const ListCamerasRequest* request,
                             ListCamerasResponse* response) override;

private:
    CameraManager& manager_;
};

}  // namespace dsd
