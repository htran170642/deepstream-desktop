#include "CameraManager.hpp"
#include "CameraServiceImpl.hpp"
#include "GrpcServer.hpp"
#include "HealthServiceImpl.hpp"
#include "logging/Logger.hpp"

namespace {
#ifdef DSD_WITH_DEEPSTREAM
// Container build: reachable from the desktop host over the Docker network.
constexpr char kServerAddress[] = "0.0.0.0:50051";
#else
// Host build: localhost only — don't expose the service on other interfaces.
constexpr char kServerAddress[] = "127.0.0.1:50051";
#endif

constexpr char kDatabasePath[] = "deepstream.db";
}  // namespace

int main() {
    dsd::Logger::init("service");
    auto log = dsd::Logger::get();

    log->info("Starting DeepStreamService");

#ifdef DSD_WITH_DEEPSTREAM
    log->info("DeepStream support: ENABLED");
#else
    log->info("DeepStream support: disabled (host build)");
#endif

    try {
        // Service objects must outlive the server (it only stores pointers).
        dsd::HealthServiceImpl health_service;
        dsd::CameraManager camera_manager(kDatabasePath);
        dsd::CameraServiceImpl camera_service(camera_manager);

        dsd::GrpcServer server(kServerAddress);
        server.registerService(&health_service);
        server.registerService(&camera_service);

        if (server.start() == 0) {
            log->error("Service failed to start; exiting");
            return 1;
        }

        server.wait();  // block, serving requests until terminated
    } catch (const std::exception& e) {
        log->error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
