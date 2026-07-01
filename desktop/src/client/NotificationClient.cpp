#include "client/NotificationClient.hpp"

#include "client/GrpcSupport.hpp"
#include "logging/Logger.hpp"

namespace dsd {

NotificationClient::NotificationClient(const std::string& address)
    : channel_(makeChannel(address)),
      stub_(NotificationService::NewStub(channel_)) {}

NotificationClient::~NotificationClient() = default;

std::optional<std::vector<ChannelStatusRecord>> NotificationClient::status(
    std::chrono::milliseconds timeout) {
    grpc::ClientContext ctx;
    setDeadline(ctx, timeout);
    NotificationStatusRequest req;
    NotificationStatus resp;
    const grpc::Status s = stub_->GetStatus(&ctx, req, &resp);
    if (!s.ok()) {
        Logger::get()->warn("GetStatus failed: {}", s.error_message());
        return std::nullopt;
    }
    std::vector<ChannelStatusRecord> out;
    out.reserve(resp.channels_size());
    for (const auto& ch : resp.channels()) {
        out.push_back({ch.name(), ch.enabled()});
    }
    return out;
}

std::optional<std::vector<DeliveryRecord>> NotificationClient::sendTest(
    std::chrono::milliseconds timeout) {
    grpc::ClientContext ctx;
    setDeadline(ctx, timeout);
    SendTestRequest req;
    SendTestResult resp;
    const grpc::Status s = stub_->SendTest(&ctx, req, &resp);
    if (!s.ok()) {
        Logger::get()->warn("SendTest failed: {}", s.error_message());
        return std::nullopt;
    }
    std::vector<DeliveryRecord> out;
    out.reserve(resp.results_size());
    for (const auto& r : resp.results()) {
        out.push_back({r.name(), r.ok()});
    }
    return out;
}

}  // namespace dsd
