#include "AlertManager.hpp"

#include <memory>
#include <utility>

#include "AlertRepository.hpp"
#include "logging/Logger.hpp"
#include "pipeline/Frame.hpp"

namespace dsd {
namespace {

// Cap on undrained alerts. Persistence is fast and the cooldown throttles the
// input, so this is only a safety valve against a stalled DB; drop-oldest keeps
// the queue bounded rather than growing without limit.
constexpr std::size_t kMaxQueue = 1024;

}  // namespace

AlertManager::AlertManager(AlertRepository& repo)
    : AlertManager(repo, Config{}) {}  // delegate: one place starts the worker

AlertManager::AlertManager(AlertRepository& repo, Config config)
    : repo_(repo),
      config_(std::move(config)),
      worker_(&AlertManager::run, this) {}

AlertManager::~AlertManager() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        running_ = false;  // run() drains what's left, then exits
    }
    queue_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void AlertManager::setAlertSink(AlertSink sink) {
    sink_ = std::move(sink);
}

bool AlertManager::shouldFire(std::int64_t camera_id, const std::string& label,
                              std::int64_t now_ms) {
    // Not a target class -> never fires.
    if (!config_.targets.empty() && config_.targets.count(label) == 0) {
        return false;
    }
    auto& last = last_fired_[camera_id][label];  // inserts 0 on first sight
    if (last != 0 && now_ms - last < config_.cooldown_ms) {
        return false;  // still cooling down
    }
    last = now_ms;
    return true;
}

void AlertManager::onFrame(const model::Frame& frame) {
    // Evaluate the rule here (cheap; guards the cooldown map) and collect the
    // alerts to fire. Persistence + notification happen on the worker thread so
    // this call — on the pipeline frame path — never blocks on the database.
    std::vector<model::Alert> fired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& det : frame.detections) {
            if (det.confidence < config_.min_confidence) {
                continue;
            }
            if (!shouldFire(frame.camera_id, det.label, frame.timestamp_ms)) {
                continue;
            }
            model::Alert alert;
            alert.camera_id = frame.camera_id;
            alert.label = det.label;
            alert.confidence = det.confidence;
            alert.timestamp_ms = frame.timestamp_ms;
            alert.snapshot = frame.jpeg;  // copy the image only when firing
            fired.push_back(std::move(alert));
        }
    }

    for (auto& alert : fired) {
        enqueue(std::move(alert));
    }
}

void AlertManager::enqueue(model::Alert alert) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.size() >= kMaxQueue) {
            // Persistence can't keep up; drop the oldest to stay bounded.
            Logger::get()->warn("AlertManager queue full; dropping oldest alert");
            queue_.pop_front();
            --pending_;
        }
        queue_.push_back(std::move(alert));
        ++pending_;
    }
    queue_cv_.notify_one();
}

void AlertManager::run() {
    for (;;) {
        model::Alert alert;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [&] { return !queue_.empty() || !running_; });
            if (queue_.empty()) {
                return;  // woken only by stop, nothing left to drain
            }
            alert = std::move(queue_.front());
            queue_.pop_front();
        }

        // Persist + announce outside the lock so onFrame() never waits on the DB.
        try {
            model::Alert stored = repo_.add(alert);  // assigns the id
            Logger::get()->info("Alert #{}: {} on camera {} ({:.2f})", stored.id,
                                stored.label, stored.camera_id, stored.confidence);
            // Record image presence, then drop the bytes: the stream carries
            // metadata only (JPEG stays in the DB, fetched via GetSnapshot).
            stored.has_snapshot = !stored.snapshot.empty();
            stored.snapshot.clear();
            if (sink_) {
                sink_(std::make_shared<const model::Alert>(std::move(stored)));
            }
        } catch (const std::exception& e) {
            // A DB error must not kill the worker (it would std::terminate).
            Logger::get()->error("AlertManager persist failed: {}", e.what());
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (--pending_ == 0) {
                idle_cv_.notify_all();  // wake any flush() waiting for a drain
            }
        }
    }
}

void AlertManager::flush() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    idle_cv_.wait(lock, [&] { return pending_ == 0; });
}

}  // namespace dsd
