#pragma once

#include "health.grpc.pb.h"

namespace dsd {

// gRPC implementation of the Health service defined in health.proto.
// For now Check always reports SERVING; later phases can reflect real state
// (pipeline up, GPU available, etc.).
class HealthServiceImpl final : public Health::Service {
public:
    grpc::Status Check(grpc::ServerContext* context,
                       const HealthCheckRequest* request,
                       HealthCheckResponse* response) override;
};

}  // namespace dsd
