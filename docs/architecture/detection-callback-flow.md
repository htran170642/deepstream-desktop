# Detection Callback Flow — Service ↔ Pipeline

How a detection travels from the video pipeline to a Live View client, and how the classes
connect through **two callback layers**. Relevant to Phase 5 (pipeline) + Phase 6 (streaming).

## Cast of classes

| Class | Layer | Responsibility |
|---|---|---|
| `Pipeline` (interface) | pipeline | Contract: `start/stop/addSource` + `setDetectionCallback` |
| `FakePipeline` / `DeepStreamPipeline` | pipeline | Produce **raw** detections; invoke the `DetectionCallback` on a worker/streaming thread |
| `DetectionProcessor` | pipeline | Stateless filter (confidence/class) + bbox normalize |
| `PipelineManager` | pipeline | Owns the pipeline; registers itself as the pipeline's callback; filters; exposes a `DetectionSink` outward |
| `StreamServiceImpl` | service (gRPC) | Detection **sink** + fan-out to subscribed clients; Start/StopCamera |
| `CameraManager` | service | Camera CRUD (look up rtsp_url by id) |
| `main` | wiring | The only place that knows both `PipelineManager` and `StreamServiceImpl`; connects them |

## Two callback layers (the key idea)

```
Pipeline::DetectionCallback        PipelineManager::DetectionSink
   pipeline ──► manager (RAW)         manager ──► outside world (FILTERED)
   set by PipelineManager::start      set by main (lambda -> broadcast)
```

- **DetectionCallback** — produced by the Pipeline, consumed by PipelineManager. Raw data.
- **DetectionSink** — produced by PipelineManager (after filtering), consumed by
  StreamServiceImpl. Filtered data.
- Same `std::function<void(const std::vector<model::Detection>&)>` signature, different role.
- Why two: PipelineManager adds the `DetectionProcessor` step in the middle and keeps the
  pipeline ignorant of streaming, and the stream service ignorant of the pipeline.

## Static relationships (who knows whom)

```
                main  ──owns──►  PipelineManager ──owns──► Pipeline (Fake/DeepStream)
                  │                    │  └─owns─► DetectionProcessor
                  │ owns               │
                  ▼                    │ DetectionSink (std::function)
            StreamServiceImpl ◄────────┘  (set by main; calls broadcast)
                  │ refs
                  ├──► CameraManager   (look up camera by id)
                  └──► PipelineManager (start/stop)

   PipelineManager does NOT #include StreamServiceImpl.
   StreamServiceImpl does NOT #include the pipeline impls.
   main is the only coupler.
```

## Wiring (where each callback is set)

```cpp
// (A) main.cpp — DetectionSink: manager -> stream service
pipeline_manager.setDetectionSink(
    [&stream_service](const std::vector<model::Detection>& dets) {
        stream_service.broadcast(dets);
    });

// (B) PipelineManager::start() — DetectionCallback: pipeline -> manager
pipeline_->setDetectionCallback(
    [this](const std::vector<model::Detection>& dets) { onDetections(dets); });
```

## Runtime flow (one frame of detections)

```
[pipeline worker / GStreamer streaming thread]  ── all synchronous calls ──┐
  DeepStreamPipeline probe  (raw NvDsBatchMeta -> model::Detection)        │
    └► DetectionCallback  ──► PipelineManager::onDetections(raw)           │  SAME
         └► DetectionProcessor::process(raw) -> filtered                   │  THREAD
              └► DetectionSink  ──► StreamServiceImpl::broadcast(filtered) │
                   └► group by camera_id -> DetectionFrame                 │
                        └► push into each subscriber's queue + notify  ────┘
                                              │
                              ── THREAD BOUNDARY (bounded queue + condition_variable) ──
                                              │
[gRPC streaming thread, one per client]       ▼
  StreamServiceImpl::StreamDetections loop: pop frame -> writer->Write() -> desktop
```

- Everything from the probe through `broadcast` runs on the **pipeline worker thread**
  (plain synchronous calls — no thread hop). So `broadcast` must be cheap: it only enqueues.
- The slow part (network `Write`) runs on the **gRPC streaming thread**, decoupled by the
  per-subscriber queue. Slow consumer → `broadcast` drops the oldest frame (bounded queue).

## Thread-safety summary
- `subscribers_mutex_` guards the `camera_id → subscribers` registry (producer thread writes
  via `broadcast`; gRPC threads add/remove on subscribe/unsubscribe).
- Each `Subscriber` has its own `mutex` + `condition_variable` for its queue.
- `DetectionProcessor` is stateless/immutable → no lock needed on the hot path.
- Lifetime: the `DetectionSink` lambda captures `&stream_service` by reference, so
  `main` calls `pipeline_manager.stopAll()` before teardown to join workers while the
  stream service is still alive (no sink-after-free).
