#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "notification.grpc.pb.h"

namespace dsd {

// UI-facing records, decoupled from protobuf.
struct ChannelStatusRecord {
    std::string name;
    bool enabled = false;
};

struct DeliveryRecord {
    std::string name;
    bool ok = false;
};

// Qt-free gRPC client for the NotificationService. Both calls are blocking
// unary calls — run them off the UI thread. status() is cheap; sendTest() can
// take a few seconds because the server does real network sends.
class NotificationClient {
public:
    explicit NotificationClient(const std::string& address = "localhost:50051");
    ~NotificationClient();

    NotificationClient(const NotificationClient&) = delete;
    NotificationClient& operator=(const NotificationClient&) = delete;

    std::optional<std::vector<ChannelStatusRecord>> status(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    std::optional<std::vector<DeliveryRecord>> sendTest(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(15000));

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<NotificationService::Stub> stub_;
};

}  // namespace dsd
