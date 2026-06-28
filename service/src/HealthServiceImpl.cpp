#include "HealthServiceImpl.hpp"

#include "logging/Logger.hpp"

namespace dsd {

grpc::Status HealthServiceImpl::Check(grpc::ServerContext* /*context*/,
                                      const HealthCheckRequest* request,
                                      HealthCheckResponse* response) {
    Logger::get()->info("Health.Check requested for service='{}'",
                        request->service());

    response->set_status(HealthCheckResponse::SERVING);
    return grpc::Status::OK;
}

}  // namespace dsd
