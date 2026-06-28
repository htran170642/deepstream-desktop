#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "health.grpc.pb.h"

namespace dsd {

// Result of a health probe, decoupled from gRPC/protobuf types so the UI never
// touches gRPC enums directly.
enum class ServiceHealth {
    Serving,      // server reachable and reports SERVING
    NotServing,   // server reachable but reports NOT_SERVING / UNKNOWN
    Unreachable,  // RPC failed (server down, timeout, network error)
};

// Qt-free gRPC client for the Health service. Lives outside the widgets so the
// UI only orchestrates threading + display, never gRPC itself.
class HealthClient {
public:
    // Default target matches the service's listening address.
    explicit HealthClient(const std::string& address = "localhost:50051");

    // Blocking Check call with a deadline. Safe to run on a worker thread.
    ServiceHealth check(std::chrono::milliseconds timeout =
                            std::chrono::milliseconds(2000));

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<Health::Stub> stub_;
};

}  // namespace dsd
