#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "alert.grpc.pb.h"

namespace dsd {

class AlertRepository;
namespace model { struct Alert; }  // forward-declare the domain type

// gRPC boundary for the Alert module. Serves alert history + snapshots from the
// AlertRepository, and fans newly-fired alerts out to subscribed StreamAlerts
// clients. broadcastAlert() is registered as AlertManager's alert sink and is
// called from a pipeline worker thread.
class AlertServiceImpl final : public AlertService::Service {
public:
    explicit AlertServiceImpl(AlertRepository& repo);

    grpc::Status ListAlerts(grpc::ServerContext* context,
                            const AlertFilter* request,
                            AlertList* response) override;
    grpc::Status GetSnapshot(grpc::ServerContext* context,
                             const SnapshotRequest* request,
                             Snapshot* response) override;
    grpc::Status StreamAlerts(grpc::ServerContext* context,
                              const AlertStreamRequest* request,
                              grpc::ServerWriter<Alert>* writer) override;

    // Alert sink: thread-safe, called on a pipeline worker thread.
    void broadcastAlert(std::shared_ptr<const model::Alert> alert);

private:
    // One connected StreamAlerts client: a bounded queue + wakeup.
    struct Subscriber {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::shared_ptr<const Alert>> queue;  // proto Alert
    };

    AlertRepository& repo_;

    std::mutex subscribers_mutex_;
    std::vector<std::shared_ptr<Subscriber>> subscribers_;  // all-camera stream
};

}  // namespace dsd
