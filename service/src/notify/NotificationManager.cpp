#include "notify/NotificationManager.hpp"

#include <exception>
#include <utility>
#include <chrono>

#include "logging/Logger.hpp"
#include "pipeline/Alert.hpp"

namespace dsd {
namespace {
// Safety valve: if every channel stalls (slow SMTP/HTTP) the queue can't grow
// without bound. The cooldown in AlertManager already throttles input, so this
// cap is only ever hit under a real outage.
constexpr std::size_t kMaxQueue = 1024;
}  // namespace

NotificationManager::NotificationManager(
    std::vector<std::unique_ptr<NotificationChannel>> channels)
    : channels_(std::move(channels)),
      worker_(&NotificationManager::run, this) {
    auto log = Logger::get();
    int enabled = 0;
    for (const auto& ch : channels_) {
        if (ch->enabled()) {
            ++enabled;
            log->info("Notification channel enabled: {}", ch->name());
        }
    }
    log->info("NotificationManager ready ({} of {} channel(s) enabled)",
              enabled, channels_.size());
}

NotificationManager::~NotificationManager() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        running_ = false;
    }
    queue_cv_.notify_all();
    worker_.join();  // drains remaining work first (see run())
}

void NotificationManager::notify(std::shared_ptr<const model::Alert> alert) {
    if (!alert) return;
    enqueue(std::move(alert));
}

void NotificationManager::enqueue(std::shared_ptr<const model::Alert> alert) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.size() >= kMaxQueue) {
            queue_.pop_front();  // drop oldest; it was never dispatched
            --pending_;
            Logger::get()->warn(
                "NotificationManager queue full ({}); dropping oldest alert",
                kMaxQueue);
        }
        queue_.push_back(std::move(alert));
        ++pending_;
    }
    queue_cv_.notify_one();
}

void NotificationManager::run() {
    auto log = Logger::get();
    for (;;) {
        std::shared_ptr<const model::Alert> alert;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock,
                           [&] { return !queue_.empty() || !running_; });
            // Drain remaining work even after a stop request; exit only when
            // woken with nothing left to do.
            if (queue_.empty()) return;
            alert = std::move(queue_.front());
            queue_.pop_front();
        }

        // Dispatch outside the lock: sends are slow and independent. One
        // channel throwing or failing must not stop the others.
        for (const auto& ch : channels_) {
            if (!ch->enabled()) continue;
            try {
                if (!ch->send(*alert)) {
                    log->warn("Notification via {} failed (rejected)",
                              ch->name());
                }
            } catch (const std::exception& e) {
                log->warn("Notification via {} threw: {}", ch->name(),
                          e.what());
            }
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (--pending_ == 0) idle_cv_.notify_all();
        }
    }
}

void NotificationManager::flush() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    // if pending == 0 true then return else lock and wait for idle_cv_ to be notified
    idle_cv_.wait(lock, [&] { return pending_ == 0; });
}

std::vector<NotificationManager::ChannelStatus>
NotificationManager::channels() const {
    std::vector<ChannelStatus> out;
    out.reserve(channels_.size());
    for (const auto& ch : channels_) {
        out.push_back({ch->name(), ch->enabled()});
    }
    return out;
}

std::vector<NotificationManager::DeliveryResult>
NotificationManager::sendTest() {
    model::Alert alert;
    alert.camera_id = 0;
    alert.label = "test";
    alert.confidence = 1.0f;
    alert.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

    std::vector<DeliveryResult> results;
    for (const auto& ch : channels_) {
        if (!ch->enabled()) continue;
        bool ok = false;
        try {
            ok = ch->send(alert);
        } catch (const std::exception& e) {
            Logger::get()->warn("Test notification via {} threw: {}",
                                ch->name(), e.what());
        }
        results.push_back({ch->name(), ok});
    }
    return results;
}


}  // namespace dsd
