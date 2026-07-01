#include "pipeline/DeepStreamPipeline.hpp"

#include <chrono>
#include <cstdlib>   // std::getenv
#include <utility>

#include "gstnvdsmeta.h"   // NvDsBatchMeta, NvDsFrameMeta, NvDsObjectMeta, ...
#include "logging/Logger.hpp"
#include <nvbufsurface.h>   // NvBufSurface (raw frames for the encoder)

namespace dsd {
namespace {

// nvinfer config path inside the container; overridable via env so the same
// binary works with a config mounted at a different location.
std::string inferConfigPath() {
    if (const char* env = std::getenv("DSD_NVINFER_CONFIG")) {
        return env;
    }
    return "/configs/config_infer_primary.txt";
}

}  // namespace

DeepStreamPipeline::DeepStreamPipeline()
    : infer_config_path_(inferConfigPath()) {
    // gst_init is idempotent — safe to call here once per pipeline.
    gst_init(nullptr, nullptr);
}

DeepStreamPipeline::~DeepStreamPipeline() {
    stop();  // RAII: tear down the pipeline and join the loop thread
}

bool DeepStreamPipeline::start() {
    if (running_.load()) {
        return true;  // already running
    }
    if (!buildPipeline()) {
        Logger::get()->error("DeepStreamPipeline: failed to build pipeline");
        return false;
    }

    obj_ctx_ = nvds_obj_enc_create_context(0);  // gpu_id = 0
    if (!obj_ctx_) {
        Logger::get()->error("DeepStreamPipeline: failed to create JPEG encoder");
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        streammux_ = nullptr;
        return false;
    }

    // Move the whole pipeline to PLAYING.
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        Logger::get()->error("DeepStreamPipeline: cannot set state to PLAYING");
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        streammux_ = nullptr;
        return false;
    }

    // Create the loop BEFORE spawning the thread so stop() can always quit it
    // (avoids a race where stop() runs before runLoop() assigns loop_).
    loop_ = g_main_loop_new(nullptr, FALSE);
    running_.store(true);
    loop_thread_ = std::thread(&DeepStreamPipeline::runLoop, this);

    Logger::get()->info("DeepStreamPipeline started");
    return true;
}

void DeepStreamPipeline::runLoop() {
    g_main_loop_run(loop_);   // returns on EOS (bus quits) or stop()
    running_.store(false);    // so PipelineManager can detect an EOS-driven stop
}

void DeepStreamPipeline::stop() {
    const bool was_active = (loop_ != nullptr) || (pipeline_ != nullptr);
    running_.store(false);

    if (loop_) {
        g_main_loop_quit(loop_);
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    if (loop_) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        streammux_ = nullptr;
    }
    if (obj_ctx_) {
        nvds_obj_enc_destroy_context(obj_ctx_);
        obj_ctx_ = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        for (auto& entry : sources_) {
            gst_object_unref(entry.second.mux_sink_pad);
        }
        sources_.clear();
        source_to_camera_.clear();
        next_source_id_ = 0;
    }
    if (was_active) {
        Logger::get()->info("DeepStreamPipeline stopped");
    }
}


bool DeepStreamPipeline::isRunning() const {
    return running_.load();
}

std::size_t DeepStreamPipeline::sourceCount() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    return sources_.size();
}

void DeepStreamPipeline::setFrameCallback(FrameCallback callback) {
    callback_ = std::move(callback);
}

bool DeepStreamPipeline::buildPipeline() {
    pipeline_ = gst_pipeline_new("dsd-pipeline");
    streammux_ = gst_element_factory_make("nvstreammux", "streammux");
    GstElement* nvinfer = gst_element_factory_make("nvinfer", "primary-infer");
    GstElement* tracker = gst_element_factory_make("nvtracker", "tracker");
    GstElement* sink = gst_element_factory_make("fakesink", "sink");

    if (!pipeline_ || !streammux_ || !nvinfer || !tracker || !sink) {
        Logger::get()->error("DeepStreamPipeline: failed to create elements "
                             "(is this running in the DeepStream container?)");
        // None are owned by a bin yet — unref the floating refs we created.
        if (streammux_) gst_object_unref(streammux_);
        if (nvinfer) gst_object_unref(nvinfer);
        if (tracker) gst_object_unref(tracker);
        if (sink) gst_object_unref(sink);
        if (pipeline_) gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        streammux_ = nullptr;
        return false;
    }

    // nvstreammux: batches all sources into one buffer for nvinfer.
    g_object_set(streammux_,
                 "batch-size", 4,
                 "width", 1920,
                 "height", 1080,
                 "batched-push-timeout", 40000,  // 40 ms
                 "live-source", 0,               // file sources (set 1 for RTSP)
                 nullptr);

    // nvinfer: the TrafficCamNet primary detector from our config.
    g_object_set(nvinfer, "config-file-path", infer_config_path_.c_str(), nullptr);

    // nvtracker: NvDCF tracker (assigns track_id across frames).
    g_object_set(
        tracker,
        "ll-lib-file",
        "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file",
        "/opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/"
        "config_tracker_NvDCF_perf.yml",
        nullptr);

    // Assemble: streammux -> nvinfer -> nvtracker -> fakesink.
    gst_bin_add_many(GST_BIN(pipeline_), streammux_, nvinfer, tracker, sink,
                     nullptr);
    if (!gst_element_link_many(streammux_, nvinfer, tracker, sink, nullptr)) {
        Logger::get()->error("DeepStreamPipeline: failed to link elements");
        gst_object_unref(pipeline_);  // owns the elements added above
        pipeline_ = nullptr;
        streammux_ = nullptr;
        return false;
    }

    // Buffer probe on the tracker src pad: metadata is complete here (after
    // inference + tracking). This is where we read detections.
    GstPad* src_pad = gst_element_get_static_pad(tracker, "src");
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER,
                      &DeepStreamPipeline::onBufferProbe, this, nullptr);
    gst_object_unref(src_pad);

    // Bus watch: log pipeline errors and end-of-stream. A captureless lambda
    // converts to the C callback; `this` is passed through user_data.
    GstBus* bus = gst_element_get_bus(pipeline_);

    gst_bus_add_watch(
        bus,
        +[](GstBus*, GstMessage* msg, gpointer user_data) -> gboolean {
            auto* self = static_cast<DeepStreamPipeline*>(user_data);
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    Logger::get()->error("GStreamer error: {}", err->message);
                    g_error_free(err);
                    g_free(debug);
                    break;
                }
                case GST_MESSAGE_EOS:
                    Logger::get()->info("End-of-stream; stopping pipeline");
                    g_main_loop_quit(self->loop_);  // exit runLoop -> parks pipeline
                    break;
                default:
                    break;
            }
            return TRUE;
        },
        this);
    gst_object_unref(bus);

    return true;
}

bool DeepStreamPipeline::addSource(std::int64_t camera_id, const std::string& uri) {
    if (uri.empty()) {
        return false;  // invalid source
    }

    guint source_id;
    GstElement* bin;
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        if (sources_.count(camera_id) > 0) {
            return false;  // already a source
        }
        source_id = next_source_id_++;

        const std::string bin_name = "source-" + std::to_string(source_id);
        bin = gst_element_factory_make("uridecodebin", bin_name.c_str());
        if (!bin) {
            return false;
        }

        // Request a dedicated sink pad on the streammux for this source.
        const std::string pad_name = "sink_" + std::to_string(source_id);
        GstPad* mux_pad =
            gst_element_request_pad_simple(streammux_, pad_name.c_str());
        if (!mux_pad) {
            gst_object_unref(bin);
            return false;
        }

        sources_[camera_id] = Source{bin, mux_pad, source_id};
        source_to_camera_[source_id] = camera_id;
    }

    // Configure + wire the bin OUTSIDE the lock (pad-added may fire here and
    // would otherwise deadlock on sources_mutex_).
    g_object_set(bin, "uri", uri.c_str(), nullptr);
    g_object_set_data(G_OBJECT(bin), "source-id", GUINT_TO_POINTER(source_id));
    g_signal_connect(bin, "pad-added",
                     G_CALLBACK(&DeepStreamPipeline::onPadAdded), this);

    gst_bin_add(GST_BIN(pipeline_), bin);
    gst_element_sync_state_with_parent(bin);  // bring it to the pipeline's state

    Logger::get()->info("Added source: camera {} (source_id {})", camera_id,
                        source_id);
    return true;
}

// Static thunk: links a decodebin's new video pad to its streammux sink pad.
void DeepStreamPipeline::onPadAdded(GstElement* decodebin, GstPad* pad,
                                    gpointer user_data) {
    auto* self = static_cast<DeepStreamPipeline*>(user_data);

    // uridecodebin can expose audio too — only link decoded video.
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }
    if (!caps || gst_caps_get_size(caps) == 0) {
        if (caps) {
            gst_caps_unref(caps);
        }
        return;  // no negotiated caps yet — nothing to link
    }
    const gchar* media =
        gst_structure_get_name(gst_caps_get_structure(caps, 0));
    const bool is_video = g_str_has_prefix(media, "video/");
    gst_caps_unref(caps);
    if (!is_video) {
        return;
    }


    const guint source_id =
        GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(decodebin), "source-id"));

    GstPad* mux_pad = nullptr;
    {
        std::lock_guard<std::mutex> lock(self->sources_mutex_);
        auto it = self->source_to_camera_.find(source_id);
        if (it != self->source_to_camera_.end()) {
            mux_pad = self->sources_[it->second].mux_sink_pad;
        }
    }
    if (!mux_pad) {
        return;  // source was removed already
    }

    if (gst_pad_link(pad, mux_pad) != GST_PAD_LINK_OK) {
        Logger::get()->error("Failed to link source {} to streammux", source_id);
    }
}

void DeepStreamPipeline::removeSource(std::int64_t camera_id) {
    Source src;
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        auto it = sources_.find(camera_id);
        if (it == sources_.end()) {
            return;  // not a source
        }
        src = it->second;
        sources_.erase(it);
        source_to_camera_.erase(src.source_id);
    }

    // Tear down OUTSIDE the lock.
    // TODO(multi-camera): release_request_pad below can hang if this source has
    // reached EOS while the streammux is still live. Removing one of several
    // sources needs the runtime-src-del procedure (send EOS to the mux sink pad,
    // wait via a probe, then release). Single-camera stop avoids this path by
    // tearing down the whole pipeline (PipelineManager::stop).
    gst_element_set_state(src.bin, GST_STATE_NULL);
    gst_element_release_request_pad(streammux_, src.mux_sink_pad);
    gst_object_unref(src.mux_sink_pad);
    gst_bin_remove(GST_BIN(pipeline_), src.bin);  // unrefs the bin

    Logger::get()->info("Removed source: camera {}", camera_id);
}

// Static thunk: every buffer passing the tracker src pad triggers this.
GstPadProbeReturn DeepStreamPipeline::onBufferProbe(GstPad* /*pad*/,
                                                    GstPadProbeInfo* info,
                                                    gpointer user_data) {
    auto* self = static_cast<DeepStreamPipeline*>(user_data);
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer) {
        self->handleBatchMeta(buffer);
    }
    return GST_PAD_PROBE_OK;  // let the buffer continue downstream
}

void DeepStreamPipeline::handleBatchMeta(GstBuffer* buffer) {
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buffer);
    if (!batch_meta) {
        return;
    }

    // Map the buffer to the NvBufSurface (raw frames) so the encoder can read it.
    GstMapInfo map_info;
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
        return;
    }
    auto* surface = reinterpret_cast<NvBufSurface*>(map_info.data);

    // Pass 1: request a full-frame JPEG encode for every source-frame.
    for (NvDsMetaList* l = batch_meta->frame_meta_list; l; l = l->next) {
        auto* frame_meta = static_cast<NvDsFrameMeta*>(l->data);
        NvDsObjEncUsrArgs args = {};
        args.isFrame = 1;          // encode the whole frame (not an object crop)
        args.saveImg = FALSE;
        args.attachUsrMeta = TRUE; // attach the JPEG as frame user meta
        args.quality = 80;
        nvds_obj_enc_process(obj_ctx_, &args, surface, nullptr, frame_meta);
    }
    nvds_obj_enc_finish(obj_ctx_);  // flush: encoded data is now attached

    const std::int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

    // Pass 2: build one model::Frame per source (detections + JPEG).
    for (NvDsMetaList* l = batch_meta->frame_meta_list; l; l = l->next) {
        auto* frame_meta = static_cast<NvDsFrameMeta*>(l->data);

        std::int64_t camera_id = -1;
        {
            std::lock_guard<std::mutex> lock(sources_mutex_);
            auto it = source_to_camera_.find(frame_meta->source_id);
            if (it != source_to_camera_.end()) {
                camera_id = it->second;
            }
        }
        if (camera_id < 0) {
            continue;
        }

        model::Frame frame;
        frame.camera_id = camera_id;
        frame.timestamp_ms = ts;
        frame.width = static_cast<int>(frame_meta->source_frame_width);
        frame.height = static_cast<int>(frame_meta->source_frame_height);

        const float fw = static_cast<float>(frame_meta->source_frame_width);
        const float fh = static_cast<float>(frame_meta->source_frame_height);
        for (NvDsMetaList* lo = frame_meta->obj_meta_list; lo; lo = lo->next) {
            auto* obj = static_cast<NvDsObjectMeta*>(lo->data);
            model::Detection d;
            d.camera_id = camera_id;
            d.class_id = obj->class_id;
            d.label = obj->obj_label;
            d.confidence = obj->confidence;
            d.track_id = static_cast<std::int64_t>(obj->object_id);
            d.timestamp_ms = ts;
            const NvOSD_RectParams& r = obj->rect_params;
            if (fw > 0.0f && fh > 0.0f) {
                d.box.x = r.left / fw;
                d.box.y = r.top / fh;
                d.box.width = r.width / fw;
                d.box.height = r.height / fh;
            }
            frame.detections.push_back(std::move(d));
        }

        // The encoded JPEG is attached to the frame as NVDS_CROP_IMAGE_META.
        for (NvDsMetaList* lu = frame_meta->frame_user_meta_list; lu;
             lu = lu->next) {
            auto* um = static_cast<NvDsUserMeta*>(lu->data);
            if (um->base_meta.meta_type != NVDS_CROP_IMAGE_META) {
                continue;
            }
            auto* enc = static_cast<NvDsObjEncOutParams*>(um->user_meta_data);
            if (enc && enc->outBuffer && enc->outLen > 0) {
                frame.jpeg.assign(enc->outBuffer, enc->outBuffer + enc->outLen);
            }
            break;
        }

        if (callback_) {
            callback_(std::move(frame));  // one frame per source
        }
    }

    gst_buffer_unmap(buffer, &map_info);
}

}  // namespace dsd
