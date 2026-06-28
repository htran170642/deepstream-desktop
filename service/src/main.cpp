#include "logging/Logger.hpp"
#include "health.pb.h"

int main() {
    dsd::Logger::init("service");
    auto log = dsd::Logger::get();

    log->info("Starting DeepStreamService");

#ifdef DSD_WITH_DEEPSTREAM
    log->info("DeepStream support: ENABLED");
#else
    log->info("DeepStream support: disabled (host build)");
#endif

    // Sanity check that the generated protobuf types compile and link.
    dsd::HealthCheckResponse response;
    response.set_status(dsd::HealthCheckResponse::SERVING);
    log->info("Health status initialized to {}",
              static_cast<int>(response.status()));

    return 0;
}
