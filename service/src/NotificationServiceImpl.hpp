#pragma once

#include "notification.grpc.pb.h"

namespace dsd {

class NotificationManager;

// gRPC boundary for the notification module. Read-only status + a manual test
// trigger for the desktop Settings page; delegates entirely to the
// NotificationManager and holds no state of its own.
class NotificationServiceImpl final : public NotificationService::Service {
public:
    explicit NotificationServiceImpl(NotificationManager& manager);

    grpc::Status GetStatus(grpc::ServerContext* context,
                           const NotificationStatusRequest* request,
                           NotificationStatus* response) override;
    grpc::Status SendTest(grpc::ServerContext* context,
                          const SendTestRequest* request,
                          SendTestResult* response) override;

private:
    NotificationManager& manager_;
};

}  // namespace dsd
