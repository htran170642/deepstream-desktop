#include "NotificationServiceImpl.hpp"

#include "logging/Logger.hpp"
#include "notify/NotificationManager.hpp"

namespace dsd {

NotificationServiceImpl::NotificationServiceImpl(NotificationManager& manager)
    : manager_(manager) {}

grpc::Status NotificationServiceImpl::GetStatus(
    grpc::ServerContext* /*context*/,
    const NotificationStatusRequest* /*request*/,
    NotificationStatus* response) {
    for (const auto& ch : manager_.channels()) {
        auto* out = response->add_channels();
        out->set_name(ch.name);
        out->set_enabled(ch.enabled);
    }
    return grpc::Status::OK;
}

grpc::Status NotificationServiceImpl::SendTest(
    grpc::ServerContext* /*context*/, const SendTestRequest* /*request*/,
    SendTestResult* response) {
    Logger::get()->info("NotificationService(SendTest): dispatching test alert");
    for (const auto& r : manager_.sendTest()) {
        auto* out = response->add_results();
        out->set_name(r.name);
        out->set_ok(r.ok);
    }
    return grpc::Status::OK;
}

}  // namespace dsd
