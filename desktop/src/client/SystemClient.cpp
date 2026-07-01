#include "client/SystemClient.hpp"

#include "client/GrpcSupport.hpp"
#include "logging/Logger.hpp"

namespace dsd {

SystemClient::SystemClient(const std::string& address)
    : channel_(makeChannel(address)),
      stub_(SystemService::NewStub(channel_)) {}

SystemClient::~SystemClient() = default;

std::optional<SystemStatsRecord> SystemClient::stats(
    std::chrono::milliseconds timeout) {
    grpc::ClientContext ctx;
    setDeadline(ctx, timeout);
    SystemStatsRequest req;
    SystemStats resp;
    const grpc::Status s = stub_->GetStats(&ctx, req, &resp);
    if (!s.ok()) {
        Logger::get()->warn("GetStats failed: {}", s.error_message());
        return std::nullopt;
    }

    SystemStatsRecord out;
    out.cpu_percent = resp.cpu_percent();
    out.mem_used_bytes = resp.mem_used_bytes();
    out.mem_total_bytes = resp.mem_total_bytes();
    out.gpu_available = resp.gpu_available();
    out.gpu_percent = resp.gpu_percent();
    out.gpu_mem_used_bytes = resp.gpu_mem_used_bytes();
    out.gpu_mem_total_bytes = resp.gpu_mem_total_bytes();
    out.fps = resp.fps();
    out.pipeline_running = resp.pipeline_running();
    out.active_cameras = resp.active_cameras();
    for (const auto& c : resp.cameras()) {
        out.cameras.push_back({c.id(), c.name(), c.enabled(), c.running()});
    }
    return out;
}

}  // namespace dsd
