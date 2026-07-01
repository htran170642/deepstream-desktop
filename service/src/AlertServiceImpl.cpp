#include "AlertServiceImpl.hpp"

#include <algorithm>   // std::remove
#include <chrono>
#include <memory>

#include "AlertRepository.hpp"
#include "logging/Logger.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {
namespace {

constexpr std::size_t kMaxQueue = 32;  // bounded per-subscriber queue (drop oldest)

// Map a domain alert to its wire form (metadata only; no image bytes).
Alert toProto(const model::Alert& a) {
    Alert out;
    out.set_id(a.id);
    out.set_camera_id(a.camera_id);
    out.set_label(a.label);
    out.set_confidence(a.confidence);
    out.set_timestamp_ms(a.timestamp_ms);
    out.set_has_snapshot(a.has_snapshot || !a.snapshot.empty());
    return out;
}

}  // namespace

AlertServiceImpl::AlertServiceImpl(AlertRepository& repo) : repo_(repo) {}

grpc::Status AlertServiceImpl::ListAlerts(grpc::ServerContext* /*context*/,
                                          const AlertFilter* request,
                                          AlertList* response) {
    try {
        AlertRepository::Filter filter;
        filter.camera_id = request->camera_id();
        filter.label = request->label();
        filter.from_ms = request->from_ms();
        filter.to_ms = request->to_ms();
        if (request->limit() > 0) {
            filter.limit = request->limit();
        }

        for (const model::Alert& a : repo_.list(filter)) {
            *response->add_alerts() = toProto(a);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::get()->error("ListAlerts failed: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status AlertServiceImpl::GetSnapshot(grpc::ServerContext* /*context*/,
                                           const SnapshotRequest* request,
                                           Snapshot* response) {
    try {
        auto image = repo_.snapshot(request->alert_id());
        if (image) {
            response->set_jpeg(image->data(), image->size());
        }
        // No image -> empty bytes (still OK; the client checks emptiness).
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::get()->error("GetSnapshot({}) failed: {}", request->alert_id(),
                             e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

void AlertServiceImpl::broadcastAlert(std::shared_ptr<const model::Alert> alert) {
    // Build the wire message once and share it (pointer) with every subscriber.
    auto shared = std::make_shared<const Alert>(toProto(*alert));

    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    for (const std::shared_ptr<Subscriber>& sub : subscribers_) {
        std::lock_guard<std::mutex> sub_lock(sub->mutex);
        if (sub->queue.size() >= kMaxQueue) {
            sub->queue.pop_front();
        }
        sub->queue.push_back(shared);
        sub->cv.notify_one();
    }
}

grpc::Status AlertServiceImpl::StreamAlerts(grpc::ServerContext* context,
                                            const AlertStreamRequest* /*request*/,
                                            grpc::ServerWriter<Alert>* writer) {
    auto sub = std::make_shared<Subscriber>();
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_.push_back(sub);
    }
    Logger::get()->info("AlertService: client subscribed to alerts");

    // Push new alerts to the client until it cancels or disconnects.
    while (!context->IsCancelled()) {
        std::shared_ptr<const Alert> alert;
        {
            std::unique_lock<std::mutex> lock(sub->mutex);
            sub->cv.wait_for(lock, std::chrono::milliseconds(200), [&] {
                return !sub->queue.empty();
            });
            if (sub->queue.empty()) {
                continue;  // timeout -> re-check IsCancelled()
            }
            alert = std::move(sub->queue.front());
            sub->queue.pop_front();
        }
        if (!writer->Write(*alert)) {
            break;  // client went away
        }
    }

    // Deregister this subscriber.
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        subscribers_.erase(
            std::remove(subscribers_.begin(), subscribers_.end(), sub),
            subscribers_.end());
    }
    Logger::get()->info("AlertService(StreamAlerts): client left");
    return grpc::Status::OK;
}

}  // namespace dsd
