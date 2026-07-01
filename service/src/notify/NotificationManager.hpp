#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "notify/NotificationChannel.hpp"

namespace dsd {

namespace model { struct Alert; }  // forward-declare the domain type

// Fans each fired alert out to every configured notification channel. Owns the
// channels and a single worker thread: notify() only enqueues, the worker does
// the (slow, blocking) network/SMTP sends. This mirrors AlertManager's async
// design so a hung endpoint can never stall the alert-persistence worker that
// calls notify() as its sink.
class NotificationManager {
public:
    explicit NotificationManager(
        std::vector<std::unique_ptr<NotificationChannel>> channels);
    ~NotificationManager();  // stops + drains the worker thread

    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;

    // Enqueue an alert for delivery to every enabled channel. Returns quickly;
    // the send happens on the worker. Signature matches AlertManager::AlertSink
    // so main can wire it straight into the fan-out sink.
    void notify(std::shared_ptr<const model::Alert> alert);

    // Block until every enqueued alert has been dispatched. Tests + shutdown.
    void flush();

    // Read-only status of one channel, for the desktop Settings view.
    struct ChannelStatus {
        std::string name;
        bool enabled;
    };

    // Result of a synchronous test send to one channel.
    struct DeliveryResult {
        std::string name;
        bool ok;
    };

    // Name + enabled state for every channel. Independent of the worker/queue
    // (channels_ is immutable after construction), so it's safe to call any time.
    std::vector<ChannelStatus> channels() const;

    // Synchronously send a synthetic test alert to every enabled channel and
    // return the per-channel result. Runs on the CALLER's thread (a gRPC
    // handler), not the worker — the manual Settings "test" button wants a real
    // pass/fail, not fire-and-forget. Safe alongside the worker: each channel's
    // send() is re-entrant (own curl handle, immutable config).
    std::vector<DeliveryResult> sendTest();

private:
    // Worker loop: dispatch queued alerts to channels until stopped and drained.
    void run();
    // Push an alert to the worker (bounded; drops oldest if it can't keep up).
    void enqueue(std::shared_ptr<const model::Alert> alert);

    std::vector<std::unique_ptr<NotificationChannel>> channels_;

    std::mutex queue_mutex_;             // guards queue_, pending_, running_
    std::condition_variable queue_cv_;   // wakes the worker on new work / stop
    std::condition_variable idle_cv_;    // wakes flush() when pending_ hits 0
    std::deque<std::shared_ptr<const model::Alert>> queue_;
    std::size_t pending_ = 0;  // enqueued but not yet dispatched
    bool running_ = true;
    std::thread worker_;  // declared last: starts only after all state above
};

}  // namespace dsd
