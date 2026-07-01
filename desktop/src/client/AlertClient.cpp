#include "client/AlertClient.hpp"

#include <chrono>
#include <utility>

#include "client/GrpcSupport.hpp"
#include "logging/Logger.hpp"

namespace dsd {
namespace {

// proto Alert -> UI AlertRecord.
AlertRecord fromProto(const Alert& a) {
    AlertRecord r;
    r.id = a.id();
    r.camera_id = a.camera_id();
    r.label = a.label();
    r.confidence = a.confidence();
    r.timestamp_ms = a.timestamp_ms();
    r.has_snapshot = a.has_snapshot();
    return r;
}

std::chrono::system_clock::time_point deadlineFrom(
    std::chrono::milliseconds timeout) {
    return std::chrono::system_clock::now() + timeout;
}

}  // namespace

AlertClient::AlertClient(const std::string& address)
    : channel_(makeChannel(address)),
      stub_(AlertService::NewStub(channel_)) {}

AlertClient::~AlertClient() {
    unsubscribe();  // RAII: cancel + join the worker
}

std::optional<std::vector<AlertRecord>> AlertClient::list(
    const AlertQuery& query, std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    AlertFilter request;
    request.set_camera_id(query.camera_id);
    request.set_label(query.label);
    request.set_from_ms(query.from_ms);
    request.set_to_ms(query.to_ms);
    request.set_limit(query.limit);

    AlertList response;
    const grpc::Status status = stub_->ListAlerts(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("ListAlerts failed: {}", status.error_message());
        return std::nullopt;
    }

    std::vector<AlertRecord> alerts;
    alerts.reserve(response.alerts_size());
    for (const Alert& a : response.alerts()) {
        alerts.push_back(fromProto(a));
    }
    return alerts;
}

std::optional<std::vector<std::uint8_t>> AlertClient::snapshot(
    std::int64_t id, std::chrono::milliseconds timeout) {
    grpc::ClientContext context;
    context.set_deadline(deadlineFrom(timeout));

    SnapshotRequest request;
    request.set_alert_id(id);

    Snapshot response;
    const grpc::Status status = stub_->GetSnapshot(&context, request, &response);
    if (!status.ok()) {
        Logger::get()->warn("GetSnapshot({}) failed: {}", id,
                            status.error_message());
        return std::nullopt;
    }

    const std::string& bytes = response.jpeg();
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

void AlertClient::subscribe(AlertCallback on_alert) {
    unsubscribe();  // replace any active stream

    // Create the context BEFORE the worker so unsubscribe() can always cancel
    // (avoids a race where the worker hasn't set it yet).
    context_ = std::make_unique<grpc::ClientContext>();
    streaming_.store(true);
    worker_ = std::thread(&AlertClient::runStream, this, std::move(on_alert));
}

void AlertClient::unsubscribe() {
    streaming_.store(false);
    if (context_) {
        context_->TryCancel();  // break the blocking Read()
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    context_.reset();
}

void AlertClient::runStream(AlertCallback on_alert) {
    AlertStreamRequest request;

    std::unique_ptr<grpc::ClientReader<Alert>> reader(
        stub_->StreamAlerts(context_.get(), request));

    Alert alert;
    while (streaming_.load() && reader->Read(&alert)) {
        if (on_alert) {
            on_alert(std::make_shared<const AlertRecord>(fromProto(alert)));
        }
        alert.Clear();
    }

    reader->Finish();  // collect final status (ignored; stream ended/cancelled)
}

}  // namespace dsd
