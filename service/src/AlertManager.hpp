#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "pipeline/Alert.hpp"

namespace dsd {

class AlertRepository;
namespace model { struct Frame; }  // forward-declare the domain type

// Turns processed detection frames into persisted alerts. Rule: a detection
// whose label is a target and whose confidence >= min_confidence fires an
// alert, throttled by a per-(camera, label) cooldown so a high frame rate does
// not flood the store. gRPC-free: new alerts are announced via an injected sink
// (main wires it to the AlertService stream), mirroring PipelineManager.
//
// Persistence runs off the frame thread: onFrame() evaluates the (cheap) rule
// and hands any fired alerts to an internal worker thread that does the SQLite
// write and calls the sink. This keeps DB I/O out of the Live View frame path.
class AlertManager {
public:
    struct Config {
        // Labels that may raise an alert. Empty = every label is a target.
        std::unordered_set<std::string> targets;
        float min_confidence = 0.5f;      // ignore weaker detections
        std::int64_t cooldown_ms = 10000; // min gap between alerts per cam+label
    };

    // Called with a new alert (already persisted, with its id) on the internal
    // worker thread. shared_ptr<const>: cheap to fan out, read-only, no copies.
    using AlertSink = std::function<void(std::shared_ptr<const model::Alert>)>;

    AlertManager(AlertRepository& repo);
    AlertManager(AlertRepository& repo, Config config);
    ~AlertManager();  // stops + drains the worker thread

    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;

    // Register the sink before the pipeline starts (read without a lock — the
    // queue mutex provides the visibility edge to the worker).
    void setAlertSink(AlertSink sink);

    // Frame sink target: evaluate the rule over this frame's detections and
    // enqueue any fired alert for the worker to persist/announce. Takes the
    // frame by const-ref — it copies the JPEG snapshot only when an alert fires.
    // Returns quickly: no DB I/O or sink call happens on the caller's thread.
    void onFrame(const model::Frame& frame);

    // Block until every alert enqueued so far has been persisted and announced.
    // Mainly for deterministic tests and graceful shutdown.
    void flush();

private:
    // Worker loop: persist + announce queued alerts until stopped and drained.
    void run();
    // Push a fired alert to the worker (bounded; drops oldest if it can't keep up).
    void enqueue(model::Alert alert);

    // True if `label` may alert and enough time has passed since the last one
    // for (camera_id, label). Updates the cooldown clock. Caller holds mutex_.
    bool shouldFire(std::int64_t camera_id, const std::string& label,
                    std::int64_t now_ms);

    AlertRepository& repo_;
    Config config_;
    AlertSink sink_;

    std::mutex mutex_;  // guards the cooldown map (rule evaluation on the caller)
    // (camera_id -> (label -> last-fired timestamp_ms))
    std::unordered_map<std::int64_t,
                       std::unordered_map<std::string, std::int64_t>>
        last_fired_;

    // Async persistence: onFrame() enqueues here; run() drains on worker_.
    std::mutex queue_mutex_;             // guards queue_, pending_, running_
    std::condition_variable queue_cv_;   // wakes the worker on new work / stop
    std::condition_variable idle_cv_;    // wakes flush() when pending_ hits 0
    std::deque<model::Alert> queue_;
    std::size_t pending_ = 0;  // enqueued but not yet fully processed
    bool running_ = true;
    std::thread worker_;  // declared last: starts only after all state above
};

}  // namespace dsd
