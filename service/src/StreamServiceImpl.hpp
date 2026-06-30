#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "stream.grpc.pb.h"

namespace dsd {

class CameraManager;
class PipelineManager;
namespace model { struct Frame; }  // forward-declare the domain type

// gRPC boundary for Live View. Orchestrates CameraManager (look up cameras) and
// PipelineManager (start/stop pipelines), and fans detections out to subscribed
// StreamDetections clients. broadcast() is registered as PipelineManager's
// detection sink and is called from a pipeline worker thread.
class StreamServiceImpl final : public StreamService::Service {
public:
    StreamServiceImpl(CameraManager& cameras, PipelineManager& pipelines);

    grpc::Status StartCamera(grpc::ServerContext* context,
                             const CameraStreamRequest* request,
                             CameraStreamResponse* response) override;
    grpc::Status StopCamera(grpc::ServerContext* context,
                            const CameraStreamRequest* request,
                            CameraStreamResponse* response) override;
    grpc::Status StreamDetections(
        grpc::ServerContext* context, const CameraStreamRequest* request,
        grpc::ServerWriter<DetectionFrame>* writer) override;

    // Frame sink: thread-safe, called on a pipeline worker thread.
    void broadcast(model::Frame frame);

private:
    // One connected StreamDetections client: a bounded queue + wakeup.
    struct Subscriber {
        std::int64_t camera_id = 0;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::shared_ptr<const DetectionFrame>> queue;
        bool cancelled = false;
    };

    CameraManager& cameras_;
    PipelineManager& pipelines_;

    std::mutex subscribers_mutex_;
    std::unordered_map<std::int64_t, std::vector<std::shared_ptr<Subscriber>>>
        subscribers_;  // camera_id -> active subscribers
};

}  // namespace dsd
