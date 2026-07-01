#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

namespace dsd {

// Shared gRPC client helpers. Every *Client opens an insecure channel to the
// service and sets a per-call deadline; these remove that repeated boilerplate.
inline std::shared_ptr<grpc::Channel> makeChannel(const std::string& address) {
    return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

inline void setDeadline(grpc::ClientContext& ctx,
                        std::chrono::milliseconds timeout) {
    ctx.set_deadline(std::chrono::system_clock::now() + timeout);
}

}  // namespace dsd
