#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <gst/gst.h>
#include <nvbufsurface.h>
#include "gstnvdsmeta.h"
#include "nvds_obj_encode.h"

#include "pipeline/Pipeline.hpp"

namespace dsd {

// Real DeepStream implementation of Pipeline: ONE GStreamer pipeline that
// batches all camera sources through nvstreammux -> nvinfer -> nvtracker, with
// a buffer probe converting NvDsBatchMeta into model::Detection. Sources are
// added/removed at runtime. Compiled only when ENABLE_DEEPSTREAM is set.
class DeepStreamPipeline final : public Pipeline {
public:
    DeepStreamPipeline();
    ~DeepStreamPipeline() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    bool addSource(std::int64_t camera_id, const std::string& uri) override;
    void removeSource(std::int64_t camera_id) override;
    std::size_t sourceCount() const override;

    void setFrameCallback(FrameCallback callback) override;

private:
    struct Source {
        GstElement* bin = nullptr;        // uridecodebin for this source
        GstPad* mux_sink_pad = nullptr;   // requested pad on nvstreammux
        guint source_id = 0;              // == streammux pad index (NvDsFrameMeta.source_id)
    };

    bool buildPipeline();   // streammux -> nvinfer -> nvtracker -> fakesink
    void runLoop();         // GMainLoop on the worker thread

    // GStreamer C callbacks (static thunks dispatching to the instance).
    static void onPadAdded(GstElement* decodebin, GstPad* pad, gpointer user_data);
    static GstPadProbeReturn onBufferProbe(GstPad* pad, GstPadProbeInfo* info,
                                           gpointer user_data);
    void handleBatchMeta(GstBuffer* buffer);

    FrameCallback callback_;
    NvDsObjEncCtxHandle obj_ctx_ = nullptr;  // GPU JPEG encoder context

    std::atomic<bool> running_{false};
    std::thread loop_thread_;
    GMainLoop* loop_ = nullptr;

    GstElement* pipeline_ = nullptr;
    GstElement* streammux_ = nullptr;

    mutable std::mutex sources_mutex_;
    std::unordered_map<std::int64_t, Source> sources_;        // camera_id -> source
    std::unordered_map<guint, std::int64_t> source_to_camera_;  // source_id -> camera_id
    guint next_source_id_ = 0;

    std::string infer_config_path_;  // path to nvinfer_primary.txt
};

}  // namespace dsd
