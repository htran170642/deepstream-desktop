#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "pipeline/Pipeline.hpp"

namespace dsd {

// Host/test implementation of Pipeline. A single worker thread emits one
// synthetic detection per active source every tick, until stopped — no GPU,
// no DeepStream. Mirrors the real "one pipeline, many sources" model.
class FakePipeline final : public Pipeline {
public:
    FakePipeline();
    ~FakePipeline() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    bool addSource(std::int64_t camera_id, const std::string& rtsp_url) override;
    void removeSource(std::int64_t camera_id) override;
    std::size_t sourceCount() const override;

    void setFrameCallback(FrameCallback callback) override;

private:
    void run();  // worker loop: emit one detection per active source per tick

    FrameCallback callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;

    mutable std::mutex sources_mutex_;  // guards sources_
    std::unordered_map<std::int64_t, std::string> sources_;  // camera_id -> rtsp_url
};

}  // namespace dsd

