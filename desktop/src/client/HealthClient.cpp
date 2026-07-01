#include "client/HealthClient.hpp"

#include "client/GrpcSupport.hpp"
#include "logging/Logger.hpp"

namespace dsd {

HealthClient::HealthClient(const std::string& address)
    : channel_(makeChannel(address)),
      stub_(Health::NewStub(channel_)) {}

ServiceHealth HealthClient::check(std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    setDeadline(context, timeout);

    HealthCheckRequest request;
    request.set_service("deepstream");

    HealthCheckResponse response;
    const grpc::Status status = stub_->Check(&context, request, &response);

    if (!status.ok()) {
        Logger::get()->warn("Health.Check failed: {} ({})",
                            status.error_message(),
                            static_cast<int>(status.error_code()));
        return ServiceHealth::Unreachable;
    }

    return response.status() == HealthCheckResponse::SERVING
               ? ServiceHealth::Serving
               : ServiceHealth::NotServing;
}

}  // namespace dsd
