# Callbacks Atlas

Every callback in the system, why it exists, and how the `std::function` ones chain.
Companion to [detection-callback-flow.md](detection-callback-flow.md) and
[live-view-dataflow.md](live-view-dataflow.md).

## One mental model
A callback = "hand someone a function so they can call you back later." The whole system
has only **three flavors**; every callback is one of them.

## All callbacks, grouped by flavor

### Flavor 1 — our own `std::function` observers (3, look-alike → the confusing ones)

| Callback | Set by | Called by | Thread | Purpose |
|---|---|---|---|---|
| `Pipeline::DetectionCallback` | `PipelineManager::start` | Fake/DeepStreamPipeline | pipeline worker | pipeline → manager (**raw** detections) |
| `PipelineManager::DetectionSink` | `main` (lambda → broadcast) | `PipelineManager::onDetections` | pipeline worker | manager → StreamService (**filtered**) |
| `StreamClient::FrameCallback` | `LiveViewPage::startSelected` | `StreamClient::runStream` | stream worker (desktop) | client → page (`FrameUpdate`) |

### Flavor 2 — C function pointers (GStreamer API, DeepStreamPipeline only)

| Callback | Set by | Called by | Purpose |
|---|---|---|---|
| `onPadAdded` | `g_signal_connect` | GStreamer | link uridecodebin → nvstreammux |
| `onBufferProbe` | `gst_pad_add_probe` | GStreamer (per buffer) | read NvDsBatchMeta → Detection |
| bus-watch lambda | `gst_bus_add_watch` | GMainLoop | log errors / EOS |

### Flavor 3 — Qt signal/slot + invokeMethod (desktop UI only)

| Callback | Set by | Called by | Purpose |
|---|---|---|---|
| `connect(button, clicked, slot)` | `connect()` | Qt (UI thread) | button → action |
| `connect(timer, timeout, updateFps)` | `connect()` | Qt (every 1s) | FPS update |
| `connect(sidebar, currentRowChanged, stack)` | `connect()` | Qt | page navigation |
| `QMetaObject::invokeMethod(this, …, QueuedConnection)` | manual | Qt event loop | marshal worker → UI thread |
| `QFutureWatcher::finished → slot` | `connect()` | Qt | off-thread task done (Camera/Health) |

## How the `std::function` ones chain — SET vs CALL

Each callback has two sides: **SET** (store a lambda in a member) and **CALL** (running the
member runs that lambda; the lambda body calls the next layer).

```cpp
// (1) SET in PipelineManager::start
pipeline_->setDetectionCallback([this](auto& dets){ onDetections(dets); });
// (1) CALL in the pipeline
callback_(batch);                 // == onDetections(batch)

// (2) SET in main
pipeline_manager.setDetectionSink([&s](auto& d){ s.broadcast(d); });
// (2) CALL in PipelineManager::onDetections
if (sink_) sink_(filtered);       // == stream_service.broadcast(filtered)

// (3) SET in LiveViewPage::startSelected
client_->subscribe(id, [this](auto& f){ QMetaObject::invokeMethod(this,[this,f]{onFrame(f);},Queued); });
// (3) CALL in StreamClient::runStream
on_frame(fromProto(frame));       // == marshal → onFrame
```

### Chaining = substitution
```
callback_(raw)
  = onDetections(raw)                                 // callback_ holds [..]{onDetections}
  = { filtered = processor_.process(raw); sink_(filtered); }
  = { filtered = process(raw); broadcast(filtered); } // sink_ holds [..]{broadcast}
  = { filtered = process(raw); push DetectionFrame into queue }
```
One `callback_(raw)` "expands" into the whole chain because each callback's body calls the
next layer's method, which calls *its* stored callback.

## Data transformation along the chain
```
[pipeline]    std::vector<model::Detection>  raw
  callback_ → onDetections
[manager]     processor_.process → filtered             (drop low-confidence, clamp bbox)
  sink_ → broadcast
[stream svc]  group by camera_id → DetectionFrame (proto)  (model::Detection → proto Detection)
  queue → (thread hop) gRPC Write
  ============ network (localhost:50051) ============
[client worker] Read() → DetectionFrame (proto)
  fromProto → FrameUpdate                                (proto Detection → DetectionBox)
  on_frame → invokeMethod  ===> (thread hop to UI)
[LiveViewPage] onFrame → DetectionView::setFrame
[DetectionView] paintEvent: bbox [0,1] × widget size → draw
```
Three shape changes at three boundaries: `model::Detection` → proto `Detection` → `DetectionBox`/`FrameUpdate`.

## Why go through an intermediate member at all?
When `PipelineManager` is written, it does not know who will consume detections
(StreamService doesn't exist in that file). So it holds a `std::function sink_` placeholder;
`main` injects the real behavior at runtime. This separates "writing the class" from
"wiring it up", so a lower layer never depends on a higher one.

## Reading any callback quickly — ask 4 questions
1. Which flavor? (`std::function` / C pointer / Qt signal)
2. Who SETs it? (injects the behavior)
3. Who CALLs it? (fires the event)
4. Which thread? (do we need to marshal / lock?)
