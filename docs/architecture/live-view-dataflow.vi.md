# Luồng dữ liệu Live View (Desktop ↔ Service)

Đường đi đầy đủ của một frame detection cho Live View, kèm **thread** mỗi bước chạy trên.
Đi cùng [detection-callback-flow.vi.md](detection-callback-flow.vi.md) (nội bộ service) và
[live-view-streaming.md](live-view-streaming.md) (quyết định transport).

## Start (một lần, khi người dùng bấm Start)

```
[Desktop UI thread] LiveViewPage::startSelected()
   ├─ StreamClient::startCamera(id)  ──unary RPC──► [Service] StartCamera
   │                                       → PipelineManager.start(camera) → pipeline PLAYING
   └─ StreamClient::subscribe(id, cb)
        → tạo ClientContext, spawn WORKER THREAD
            → stub_->StreamDetections(...) ──server-stream──► [Service] StreamDetections (đăng ký subscriber)
   └─ fps_timer_->start()
```

## Mỗi frame (lặp liên tục) — vòng chính

```
SERVICE
  [pipeline worker thread]
     probe → Detection → onDetections → sink → broadcast
          → đẩy DetectionFrame vào queue subscriber + notify
                     │  ◄── RANH GIỚI 1: queue có giới hạn (worker → gRPC thread)
  [gRPC stream thread] StreamDetections: pop queue → writer->Write(frame)
                     │  ◄── RANH GIỚI 2: network (gRPC, localhost:50051)
DESKTOP
  [stream worker thread] runStream: reader->Read(&frame) bừng dậy
     → fromProto(frame) → FrameUpdate → on_frame(FrameUpdate)
     → QMetaObject::invokeMethod(this, [f]{ onFrame(f); }, Qt::QueuedConnection)
                     │  ◄── RANH GIỚI 3: post event (worker → UI thread)
  [UI thread] onFrame(f) → DetectionView::setFrame(f) → update() ; ++frame_count_
  [UI thread] paintEvent → QPainter vẽ box/label (toạ độ chuẩn hóa [0,1] × kích thước widget)
  [UI thread] mỗi 1s: fps_timer → updateFps() → "FPS: N"
```

## Stop (người dùng bấm Stop)

```
[Desktop UI thread] LiveViewPage::stopSelected()
   ├─ StreamClient::unsubscribe()
   │     → streaming_=false; context_->TryCancel()
   │         → worker: Read() trả false → vòng lặp thoát → join worker
   ├─ StreamClient::stopCamera(id)  ──unary──► [Service] StopCamera
   │                                    → PipelineManager.stop (pipeline dừng)
   │                                    → subscriber.cancelled=true + notify
   │                                        → [gRPC stream thread] loop break → stream kết thúc
   └─ reset UI (fps timer/label, clear view, đổi nút)
```

## Ba ranh giới thread (vì sao cần)

| Ranh giới | Tách | Lý do |
|---|---|---|
| 1. Queue (service) | pipeline worker ↔ gRPC stream thread | `broadcast` chỉ enqueue (rẻ) → I/O mạng không bao giờ chặn pipeline |
| 2. Network | service ↔ desktop | hai process (desktop host, service container) |
| 3. invokeMethod (desktop) | stream worker ↔ UI thread | Qt widget chỉ được đụng từ UI thread; vẽ từ worker là UB |

## Nguyên tắc vàng
Việc blocking (Read mạng) → worker thread. Việc UI (vẽ, label) → UI thread.
Cầu nối giữa hai là `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

## Cơ chế shutdown sạch (mỗi ranh giới một cái)
- Queue: có giới hạn + drop-oldest (consumer chậm không làm phình bộ nhớ).
- Worker: `ClientContext::TryCancel()` bẻ gãy `Read()` đang blocking.
- Stream service: `Subscriber::cancelled` + `notify` kết thúc `StreamDetections`.
- UI marshal: `invokeMethod(this, …)` — Qt tự bỏ event đang chờ nếu widget đã hủy.

## Code anchors
- Ranh giới 1: `StreamServiceImpl::broadcast` (push) ↔ `StreamServiceImpl::StreamDetections` (pop + Write)
- Ranh giới 2: `StreamClient::runStream` `reader->Read` ↔ service `writer->Write`
- Ranh giới 3: `LiveViewPage::startSelected` (invokeMethod) → `onFrame` → `DetectionView::setFrame` → `paintEvent`
