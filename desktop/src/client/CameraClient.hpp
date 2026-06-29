#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "camera.grpc.pb.h"

namespace dsd {

// Plain, UI-facing camera record — decoupled from the generated proto types so
// the UI never touches protobuf.
struct CameraInfo {
    std::int64_t id = 0;
    std::string name;
    std::string rtspUrl;
    bool enabled = false;
};

// Qt-free gRPC client for the Camera service. Each call returns std::nullopt /
// false on failure (the error is logged); the UI treats that as "operation
// failed" without ever touching gRPC itself.
class CameraClient {
public:
    explicit CameraClient(const std::string& address = "localhost:50051");

    std::optional<std::vector<CameraInfo>> list(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    std::optional<CameraInfo> add(
        const CameraInfo& camera,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    std::optional<CameraInfo> update(
        const CameraInfo& camera,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    bool remove(
        std::int64_t id,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<CameraService::Stub> stub_;
};

}  // namespace dsd
