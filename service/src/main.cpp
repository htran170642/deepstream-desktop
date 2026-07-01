#include "AlertManager.hpp"
#include "AlertRepository.hpp"
#include "AlertServiceImpl.hpp"
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

        // Alerts share the same SQLite file (separate table + connection).
        // Declare alert_service BEFORE alert_manager so it is destroyed AFTER
        // it: alert_manager's destructor drains its worker, which calls the sink
        // (-> alert_service), so alert_service must still be alive at that point.
        dsd::AlertRepository alert_repo(kDatabasePath);
        dsd::AlertServiceImpl alert_service(alert_repo);
        dsd::AlertManager alert_manager(alert_repo);          // default rule config

        // pipeline_manager is declared before the sink consumers; we stop it
        // explicitly below before teardown so a pipeline worker can't call the
        // sink after stream_service / alert_manager are gone.
        dsd::PipelineManager pipeline_manager;
        dsd::StreamServiceImpl stream_service(camera_manager, pipeline_manager);

        // New alerts flow from the AlertManager to the AlertService stream.
        alert_manager.setAlertSink(
            [&alert_service](std::shared_ptr<const dsd::model::Alert> alert) {
                alert_service.broadcastAlert(std::move(alert));
            });

        // Each processed frame fans out to two consumers: the alert rule (reads
        // detections, copies the JPEG only when an alert fires) and the Live
        // View stream (takes ownership of the frame). Alert first (by const-ref),
        // then move into broadcast.
        pipeline_manager.setFrameSink(
            [&stream_service, &alert_manager](dsd::model::Frame frame) {
                alert_manager.onFrame(frame);
                stream_service.broadcast(std::move(frame));
            });

        dsd::GrpcServer server(kServerAddress);
        server.registerService(&health_service);
        server.registerService(&camera_service);
        server.registerService(&stream_service);
        server.registerService(&alert_service);

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