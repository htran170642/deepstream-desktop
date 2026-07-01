#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

#include "system.grpc.pb.h"

namespace dsd {

class SystemMonitor;
class CameraManager;
class PipelineManager;

// gRPC boundary for the dashboard. Assembles one SystemStats snapshot from the
// SystemMonitor (CPU/mem), CameraManager (camera list), and PipelineManager
// (which cameras are running + a frame counter). FPS is derived from the
// frame-count delta between calls, so the service holds that little state.
class SystemServiceImpl final : public SystemService::Service {
public:
    SystemServiceImpl(SystemMonitor& monitor, CameraManager& camera_manager,
                      PipelineManager& pipeline_manager);

    grpc::Status GetStats(grpc::ServerContext* context,
                          const SystemStatsRequest* request,
                          SystemStats* response) override;

private:
    double computeFps(std::uint64_t frames_now);

    SystemMonitor& monitor_;
    CameraManager& camera_manager_;
    PipelineManager& pipeline_manager_;

    std::mutex fps_mutex_;
    std::optional<std::uint64_t> prev_frames_;
    std::chrono::steady_clock::time_point prev_time_;
};

}  // namespace dsd
