# Live View Data Flow (Desktop ↔ Service)

End-to-end path of a detection frame for Live View, with the thread each step runs on.
Pairs with [detection-callback-flow.md](detection-callback-flow.md) (service-internal) and
[live-view-streaming.md](live-view-streaming.md) (transport decision).

## Start (once, when the user clicks Start)

```
[Desktop UI thread] LiveViewPage::startSelected()
   ├─ StreamClient::startCamera(id)  ──unary RPC──► [Service] StartCamera
   │                                       → PipelineManager.start(camera) → pipeline PLAYING
   └─ StreamClient::subscribe(id, cb)
        → create ClientContext, spawn WORKER THREAD
            → stub_->StreamDetections(...) ──server-stream──► [Service] StreamDetections (registers subscriber)
   └─ fps_timer_->start()
```

## Per-frame (continuous) — the main loop

```
SERVICE
  [pipeline worker thread]
     probe → Detection → onDetections → sink → broadcast
          → push DetectionFrame into subscriber queue + notify
                     │  ◄── BOUNDARY 1: bounded queue (worker → gRPC thread)
  [gRPC stream thread] StreamDetections: pop queue → writer->Write(frame)
                     │  ◄── BOUNDARY 2: network (gRPC, localhost:50051)
DESKTOP
  [stream worker thread] runStream: reader->Read(&frame) unblocks
     → fromProto(frame) → FrameUpdate → on_frame(FrameUpdate)
     → QMetaObject::invokeMethod(this, [f]{ onFrame(f); }, Qt::QueuedConnection)
                     │  ◄── BOUNDARY 3: post event (worker → UI thread)
  [UI thread] onFrame(f) → DetectionView::setFrame(f) → update() ; ++frame_count_
  [UI thread] paintEvent → QPainter draws boxes/labels (normalized [0,1] × widget size)
  [UI thread] every 1s: fps_timer → updateFps() → "FPS: N"
```

## Stop (user clicks Stop)

```
[Desktop UI thread] LiveViewPage::stopSelected()
   ├─ StreamClient::unsubscribe()
   │     → streaming_=false; context_->TryCancel()
   │         → worker: Read() returns false → loop exits → join worker
   ├─ StreamClient::stopCamera(id)  ──unary──► [Service] StopCamera
   │                                    → PipelineManager.stop (pipeline down)
   │                                    → subscriber.cancelled=true + notify
   │                                        → [gRPC stream thread] loop breaks → stream ends
   └─ reset UI (fps timer/label, clear view, toggle buttons)
```

## The three thread boundaries (why each exists)

| Boundary | Splits | Why |
|---|---|---|
| 1. Queue (service) | pipeline worker ↔ gRPC stream thread | `broadcast` only enqueues (cheap) so network I/O never blocks the pipeline |
| 2. Network | service ↔ desktop | two processes (desktop host, service container) |
| 3. invokeMethod (desktop) | stream worker ↔ UI thread | Qt widgets may be touched only from the UI thread; drawing from a worker is UB |

## Golden rule
Blocking work (network `Read`) → worker thread. UI work (paint, labels) → UI thread.
The bridge between them is `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

## Clean-shutdown mechanisms (one per boundary)
- Queue: bounded + drop-oldest (slow consumer can't grow memory).
- Worker: `ClientContext::TryCancel()` breaks the blocking `Read()`.
- Service stream: `Subscriber::cancelled` + `notify` ends `StreamDetections`.
- UI marshal: `invokeMethod(this, …)` — Qt drops pending events if the widget is destroyed.

## Code anchors
- Boundary 1: `StreamServiceImpl::broadcast` (push) ↔ `StreamServiceImpl::StreamDetections` (pop + Write)
- Boundary 2: `StreamClient::runStream` `reader->Read` ↔ service `writer->Write`
- Boundary 3: `LiveViewPage::startSelected` (invokeMethod) → `onFrame` → `DetectionView::setFrame` → `paintEvent`
