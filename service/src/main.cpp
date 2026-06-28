#include "GrpcServer.hpp"
#include "logging/Logger.hpp"

namespace {
#ifdef DSD_WITH_DEEPSTREAM
// Container build: reachable from the desktop host over the Docker network.
constexpr char kServerAddress[] = "0.0.0.0:50051";
#else
// Host build: localhost only — don't expose the service on other interfaces.
constexpr char kServerAddress[] = "127.0.0.1:50051";
#endif
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

    dsd::GrpcServer server(kServerAddress);
    if (server.start() == 0) {
        log->error("Service failed to start; exiting");
        return 1;
    }

    // Block here serving requests until the process is terminated.
    server.wait();
    return 0;
}
