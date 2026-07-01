#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "system.grpc.pb.h"

namespace dsd {

struct CameraStatusRecord {
    std::int64_t id = 0;
    std::string name;
    bool enabled = false;
    bool running = false;
};

// UI-facing snapshot, decoupled from protobuf.
struct SystemStatsRecord {
    double cpu_percent = 0.0;
    std::int64_t mem_used_bytes = 0;
    std::int64_t mem_total_bytes = 0;
    bool gpu_available = false;
    double gpu_percent = 0.0;
    std::int64_t gpu_mem_used_bytes = 0;
    std::int64_t gpu_mem_total_bytes = 0;
    double fps = 0.0;
    bool pipeline_running = false;
    int active_cameras = 0;
    std::vector<CameraStatusRecord> cameras;
};

// Qt-free gRPC client for the SystemService. stats() is a blocking unary call —
// run it off the UI thread (the Dashboard polls it on a timer).
class SystemClient {
public:
    explicit SystemClient(const std::string& address = "localhost:50051");
    ~SystemClient();

    SystemClient(const SystemClient&) = delete;
    SystemClient& operator=(const SystemClient&) = delete;

    std::optional<SystemStatsRecord> stats(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<SystemService::Stub> stub_;
};

}  // namespace dsd
