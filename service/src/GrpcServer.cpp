#include "GrpcServer.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>

#include "logging/Logger.hpp"

namespace dsd {

GrpcServer::GrpcServer(std::string address) : address_(std::move(address)) {}

GrpcServer::~GrpcServer() {
    shutdown();
}

void GrpcServer::registerService(grpc::Service* service) {
    services_.push_back(service);
}

int GrpcServer::start() {
    grpc::ServerBuilder builder;

    // Insecure credentials are fine for a local Desktop <-> Service link.
    // `bound_port_` receives the actual port (relevant when address uses :0).
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials(),
                             &bound_port_);
    for (grpc::Service* service : services_) {
        builder.RegisterService(service);
    }

    server_ = builder.BuildAndStart();
    if (!server_ || bound_port_ == 0) {
        Logger::get()->error("Failed to start gRPC server on {}", address_);
        return 0;
    }

    Logger::get()->info("gRPC server listening on port {} ({} service(s))",
                        bound_port_, services_.size());
    return bound_port_;
}

void GrpcServer::wait() {
    if (server_) {
        server_->Wait();
    }
}

void GrpcServer::shutdown() {
    if (server_) {
        // Give in-flight RPCs a moment, then force shutdown so we never hang.
        const auto deadline =
            std::chrono::system_clock::now() + std::chrono::seconds(2);
        server_->Shutdown(deadline);
        server_.reset();
    }
}

}  // namespace dsd
