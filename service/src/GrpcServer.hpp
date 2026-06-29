#pragma once

#include <memory>
#include <string>
#include <vector>

#include <grpcpp/server.h>

namespace grpc {
class Service;  // forward-declare: we only store pointers to registered services
}

namespace dsd {

// Owns the gRPC server lifecycle: builds it, listens, and shuts down cleanly
// on destruction (RAII). Services are registered by the caller before start().
class GrpcServer {
public:
    // `address` is a gRPC endpoint, e.g. "0.0.0.0:50051". Use port 0
    // (e.g. "localhost:0") to bind an ephemeral port — handy for tests.
    explicit GrpcServer(std::string address);
    ~GrpcServer();

    // Non-copyable, non-movable: it owns a live server and a port.
    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    // Registers a service to expose. Call before start(). The service must
    // outlive this server (the caller owns it).
    void registerService(grpc::Service* service);

    // Starts listening. Returns the actual bound port (useful when address
    // used port 0). Returns 0 if binding failed.
    int start();

    // Blocks until the server is shut down (used by the service executable).
    void wait();

    // Stops the server if it is running. Also called by the destructor.
    void shutdown();

private:
    std::string address_;
    std::vector<grpc::Service*> services_;
    std::unique_ptr<grpc::Server> server_;
    int bound_port_ = 0;
};

}  // namespace dsd
