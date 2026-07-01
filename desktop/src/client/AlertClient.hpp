#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "alert.grpc.pb.h"

namespace dsd {

// Plain, UI-facing alert record — decoupled from the generated proto types so
// the UI never touches protobuf. Metadata only; the JPEG is fetched on demand
// via snapshot().
struct AlertRecord {
    std::int64_t id = 0;
    std::int64_t camera_id = 0;
    std::string label;
    float confidence = 0.0f;
    std::int64_t timestamp_ms = 0;
    bool has_snapshot = false;
};

// Optional history filters; 0 / empty means "no constraint on that field",
// matching the server's AlertFilter semantics.
struct AlertQuery {
    std::int64_t camera_id = 0;  // 0 = any camera
    std::string label;           // "" = any label (substring match)
    std::int64_t from_ms = 0;    // 0 = no lower bound
    std::int64_t to_ms = 0;      // 0 = no upper bound
    int limit = 0;               // 0 = server default
};

// Qt-free gRPC client for the Alert service. list()/snapshot() are blocking
// unary calls (run them off the UI thread); subscribe() opens the server-stream
// on a worker thread and delivers each new alert via the callback.
class AlertClient {
public:
    // New alerts are passed as shared_ptr<const ...> to fan out cheaply across
    // the worker -> UI thread boundary.
    using AlertCallback = std::function<void(std::shared_ptr<const AlertRecord>)>;

    explicit AlertClient(const std::string& address = "localhost:50051");
    ~AlertClient();

    AlertClient(const AlertClient&) = delete;
    AlertClient& operator=(const AlertClient&) = delete;

    // History with optional filters. std::nullopt on failure (error logged).
    std::optional<std::vector<AlertRecord>> list(
        const AlertQuery& query,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    // The JPEG snapshot for one alert. std::nullopt on failure; an empty vector
    // means the alert exists but has no image.
    std::optional<std::vector<std::uint8_t>> snapshot(
        std::int64_t id,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    // Opens the alert stream on a worker thread. Replaces any active stream.
    void subscribe(AlertCallback on_alert);
    // Cancels the active stream and joins the worker.
    void unsubscribe();

private:
    void runStream(AlertCallback on_alert);

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<AlertService::Stub> stub_;

    std::thread worker_;
    std::unique_ptr<grpc::ClientContext> context_;  // for cancelling the stream
    std::atomic<bool> streaming_{false};
};

}  // namespace dsd
