# Phase 6 — Live View Streaming: Interview Questions

Technical questions covering the concepts Phase 6 exercised (6a: detection streaming +
wiring; 6b: DeepStream JPEG frames). Each has a short answer pointer to the code.

## gRPC streaming & lifecycle

1. **Unary vs server-streaming RPC — when would you use each?**
   Unary for request/response (StartCamera/StopCamera); server-streaming for a continuous
   feed (StreamDetections `returns (stream DetectionFrame)`). See [stream.proto](../../common/proto/stream.proto).

2. **A server-streaming handler is one long-running function. How do you know when the client disconnects?**
   Poll `ServerContext::IsCancelled()` and/or detect `ServerWriter::Write()` returning false.
   We `cv.wait_for(200ms)` then re-check both. See [StreamServiceImpl.cpp](../../service/src/StreamServiceImpl.cpp) `StreamDetections`.

3. **A fast producer + slow consumer over a stream — how do you avoid unbounded memory?**
   Bounded per-subscriber queue with drop-oldest (`kMaxQueue`). Newest frame matters most for
   video. See `broadcast()`.

4. **How do you cancel a blocking client-side `Read()` on a server-stream from another thread?**
   `grpc::ClientContext::TryCancel()` — created before spawning the worker so `unsubscribe()`
   can always cancel. See [StreamClient.cpp](../../desktop/src/client/StreamClient.cpp).

## Decoupling & the observer pattern

5. **Why does PipelineManager expose a `FrameSink` (std::function) instead of calling the StreamService directly?**
   To keep the pipeline layer ignorant of gRPC/streaming — `main` injects the sink at wiring
   time. Producer and consumer never depend on each other. See [callbacks-atlas](../architecture/callbacks-atlas.md).

6. **Where does orchestration between two managers (CameraManager + PipelineManager) live, and why there?**
   At the gRPC boundary (`StreamServiceImpl`). CameraManager stays CRUD-only; PipelineManager
   stays pipeline-only; the boundary coordinates. (Resolved architecture-review seam.)

## Qt threading

7. **A network frame arrives on a worker thread. Why can't you draw it directly, and how do you marshal it to the UI thread?**
   Qt widgets are GUI-thread-only. `QMetaObject::invokeMethod(this, functor, Qt::QueuedConnection)`
   posts the work to the receiver's event loop; using `this` as context makes Qt drop the event
   if the widget is destroyed. See [LiveViewPage.cpp](../../desktop/src/pages/LiveViewPage.cpp).

8. **When marshaling data across a thread boundary, why capture by value, not by reference?**
   The source data lives only during the worker call; by the time the queued lambda runs the
   reference would dangle. We pass `shared_ptr<const FrameUpdate>` (cheap to copy, shared, read-only).

9. **Why is `QMetaObject::invokeMethod(this, …, QueuedConnection)` safe even if the widget is destroyed mid-flight?**
   The event is tied to the receiver QObject; Qt discards pending events for a destroyed object.

## DeepStream / GStreamer

10. **You need the encoded frame AND its detections, correlated, per source. Why use `nvds_obj_enc` in the metadata probe rather than a `tee → nvjpegenc → appsink` branch?**
    The probe already has the `NvDsFrameMeta` (detections) and the `NvBufSurface` (pixels) from
    the same batched buffer — encoding there keeps frame↔meta↔source correlation trivial. A tee
    branch would re-split the batched surface and require re-correlating. See [DeepStreamPipeline.cpp](../../service/src/pipeline/DeepStreamPipeline.cpp) `handleBatchMeta`.

11. **Why did `gst_element_release_request_pad` on nvstreammux hang after the source reached EOS, and how did you fix it?**
    Releasing a request pad on a live mux for an EOS'd source blocks on the mux's aggregation
    state. Fix: when stopping the last source, tear down the whole pipeline (set NULL + unref)
    so pads are released as part of element destruction. (Per-source removal of an EOS'd source
    needs the runtime-src-del EOS-probe procedure — TODO.)

12. **The bus watch is a C callback; how does it reach the C++ object, and why can't it just capture `this`?**
    A captureless lambda converts to a C function pointer; `this` is passed through `user_data`
    and recovered with `static_cast`. A capturing lambda can't convert to a function pointer.

13. **EOS auto-stop: why can't you tear the pipeline down directly from the bus watch?**
    The bus watch runs on the GMainLoop thread; tearing down joins that same thread → self-join
    deadlock. Instead EOS `g_main_loop_quit`s (parks the pipeline, `running_=false`); the manager
    tears it down lazily on the next stop/start (a different thread).

## Codec-agnostic wire format

14. **You shipped 6a with detections only, 6b adds JPEG. How do you avoid a breaking proto change, and how would H.264 slot in later?**
    `DetectionFrame.frame` (bytes) + a `FrameCodec` enum (NONE/JPEG/H264). A new codec is a
    non-breaking enum add + one desktop decode branch; old clients keep working. See
    [live-view-streaming.md](../architecture/live-view-streaming.md).

15. **Rendering: why store normalized [0,1] bounding boxes instead of pixel coordinates?**
    Resolution independence — the desktop scales boxes to the widget size regardless of source
    resolution (`d.x * w`, `d.y * h`). See [DetectionView.cpp](../../desktop/src/pages/DetectionView.cpp).
