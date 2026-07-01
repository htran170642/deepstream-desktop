#include "SystemServiceImpl.hpp"

#include "CameraManager.hpp"
#include "pipeline/PipelineManager.hpp"
#include "system/SystemMonitor.hpp"

namespace dsd {

SystemServiceImpl::SystemServiceImpl(SystemMonitor& monitor,
                                     CameraManager& camera_manager,
                                     PipelineManager& pipeline_manager)
    : monitor_(monitor),
      camera_manager_(camera_manager),
      pipeline_manager_(pipeline_manager) {}

double SystemServiceImpl::computeFps(std::uint64_t frames_now) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(fps_mutex_);
    double fps = 0.0;
    if (prev_frames_) {
        const double secs =
            std::chrono::duration<double>(now - prev_time_).count();
        if (secs > 0.0 && frames_now >= *prev_frames_) {
            fps = static_cast<double>(frames_now - *prev_frames_) / secs;
        }
    }
    prev_frames_ = frames_now;
    prev_time_ = now;
    return fps;
}

grpc::Status SystemServiceImpl::GetStats(grpc::ServerContext* /*context*/,
                                         const SystemStatsRequest* /*request*/,
                                         SystemStats* response) {
    const SystemMonitor::Sample s = monitor_.sample();
    response->set_cpu_percent(s.cpu_percent);
    response->set_mem_used_bytes(s.mem_used_bytes);
    response->set_mem_total_bytes(s.mem_total_bytes);

    response->set_gpu_available(s.gpu_available);
    response->set_gpu_percent(s.gpu_percent);
    response->set_gpu_mem_used_bytes(s.gpu_mem_used_bytes);
    response->set_gpu_mem_total_bytes(s.gpu_mem_total_bytes);

    response->set_fps(computeFps(pipeline_manager_.framesProcessed()));
    const std::size_t running = pipeline_manager_.runningCount();
    response->set_active_cameras(static_cast<int>(running));
    response->set_pipeline_running(running > 0);

    for (const auto& cam : camera_manager_.list()) {
        auto* out = response->add_cameras();
        out->set_id(cam.id);
        out->set_name(cam.name);
        out->set_enabled(cam.enabled);
        out->set_running(pipeline_manager_.isRunning(cam.id));
    }
    return grpc::Status::OK;
}

}  // namespace dsd
