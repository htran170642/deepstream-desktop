#include "CameraManager.hpp"
#include "CameraServiceImpl.hpp"
#include "GrpcServer.hpp"
#include "HealthServiceImpl.hpp"
#include "StreamServiceImpl.hpp"
#include "logging/Logger.hpp"
#include "pipeline/PipelineManager.hpp"

namespace {
#ifdef DSD_WITH_DEEPSTREAM
constexpr char kServerAddress[] = "0.0.0.0:50051";
#else
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
        dsd::HealthServiceImpl health_service;
        dsd::CameraManager camera_manager(kDatabasePath);
        dsd::CameraServiceImpl camera_service(camera_manager);

        // pipeline_manager is declared before stream_service (which takes a
        // reference to it). We stop it explicitly below before teardown so a
        // pipeline worker can't call the sink after stream_service is gone.
        dsd::PipelineManager pipeline_manager;
        dsd::StreamServiceImpl stream_service(camera_manager, pipeline_manager);

        // Filtered detections flow from the pipeline into the stream service,
        // which fans them out to subscribed Live View clients.
        pipeline_manager.setDetectionSink(
            [&stream_service](const std::vector<dsd::model::Detection>& dets) {
                stream_service.broadcast(dets);
            });

        dsd::GrpcServer server(kServerAddress);
        server.registerService(&health_service);
        server.registerService(&camera_service);
        server.registerService(&stream_service);

        if (server.start() == 0) {
            log->error("Service failed to start; exiting");
            return 1;
        }

        server.wait();               // serve requests until shut down
        pipeline_manager.stopAll();  // join pipeline workers before teardown
    } catch (const std::exception& e) {
        log->error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
