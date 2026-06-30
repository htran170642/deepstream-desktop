# Bản đồ Callback

Tất cả callback trong hệ thống, vì sao có, và các callback `std::function` gọi nhau ra sao.
Đi cùng [detection-callback-flow.vi.md](detection-callback-flow.vi.md) và
[live-view-dataflow.vi.md](live-view-dataflow.vi.md).

## Một mô hình tư duy
Callback = "đưa cho ai đó một hàm để họ gọi lại mình sau." Cả hệ thống chỉ có **3 vị**;
mọi callback đều là một trong ba.

## Tất cả callback, nhóm theo vị

### Vị 1 — `std::function` của mình (observer) — 3 cái, giống nhau → dễ rối nhất

| Callback | Set bởi | Gọi bởi | Thread | Mục đích |
|---|---|---|---|---|
| `Pipeline::DetectionCallback` | `PipelineManager::start` | Fake/DeepStreamPipeline | pipeline worker | pipeline → manager (detection **thô**) |
| `PipelineManager::DetectionSink` | `main` (lambda → broadcast) | `PipelineManager::onDetections` | pipeline worker | manager → StreamService (**đã lọc**) |
| `StreamClient::FrameCallback` | `LiveViewPage::startSelected` | `StreamClient::runStream` | stream worker (desktop) | client → page (`FrameUpdate`) |

### Vị 2 — con trỏ hàm C (API GStreamer, chỉ ở DeepStreamPipeline)

| Callback | Set bởi | Gọi bởi | Mục đích |
|---|---|---|---|
| `onPadAdded` | `g_signal_connect` | GStreamer | nối uridecodebin → nvstreammux |
| `onBufferProbe` | `gst_pad_add_probe` | GStreamer (mỗi buffer) | đọc NvDsBatchMeta → Detection |
| bus-watch lambda | `gst_bus_add_watch` | GMainLoop | log lỗi / EOS |

### Vị 3 — Qt signal/slot + invokeMethod (chỉ ở desktop UI)

| Callback | Set bởi | Gọi bởi | Mục đích |
|---|---|---|---|
| `connect(button, clicked, slot)` | `connect()` | Qt (UI thread) | bấm nút → hành động |
| `connect(timer, timeout, updateFps)` | `connect()` | Qt (mỗi 1s) | cập nhật FPS |
| `connect(sidebar, currentRowChanged, stack)` | `connect()` | Qt | đổi trang |
| `QMetaObject::invokeMethod(this, …, QueuedConnection)` | gọi tay | Qt event loop | chuyển worker → UI thread |
| `QFutureWatcher::finished → slot` | `connect()` | Qt | xong tác vụ off-thread (Camera/Health) |

## Các `std::function` gọi nhau ra sao — SET vs CALL

Mỗi callback có 2 vế: **SET** (cất lambda vào một biến member) và **CALL** (gọi biến đó =
chạy lambda; thân lambda gọi tầng kế).

```cpp
// (1) SET trong PipelineManager::start
pipeline_->setDetectionCallback([this](auto& dets){ onDetections(dets); });
// (1) CALL trong pipeline
callback_(batch);                 // == onDetections(batch)

// (2) SET trong main
pipeline_manager.setDetectionSink([&s](auto& d){ s.broadcast(d); });
// (2) CALL trong PipelineManager::onDetections
if (sink_) sink_(filtered);       // == stream_service.broadcast(filtered)

// (3) SET trong LiveViewPage::startSelected
client_->subscribe(id, [this](auto& f){ QMetaObject::invokeMethod(this,[this,f]{onFrame(f);},Queued); });
// (3) CALL trong StreamClient::runStream
on_frame(fromProto(frame));       // == marshal → onFrame
```

### Gọi nhau = phép THẾ
```
callback_(raw)
  = onDetections(raw)                                 // callback_ chứa [..]{onDetections}
  = { filtered = processor_.process(raw); sink_(filtered); }
  = { filtered = process(raw); broadcast(filtered); } // sink_ chứa [..]{broadcast}
  = { filtered = process(raw); push DetectionFrame vào queue }
```
Một `callback_(raw)` "nở" ra cả chuỗi vì thân mỗi callback gọi method tầng kế, mà method đó
lại gọi callback đã cất của *nó*.

## Data biến hình dọc chuỗi
```
[pipeline]    std::vector<model::Detection>  raw
  callback_ → onDetections
[manager]     processor_.process → filtered             (bỏ confidence thấp, clamp bbox)
  sink_ → broadcast
[stream svc]  gom theo camera_id → DetectionFrame (proto)  (model::Detection → proto Detection)
  queue → (đổi thread) gRPC Write
  ============ network (localhost:50051) ============
[client worker] Read() → DetectionFrame (proto)
  fromProto → FrameUpdate                                (proto Detection → DetectionBox)
  on_frame → invokeMethod  ===> (đổi sang UI thread)
[LiveViewPage] onFrame → DetectionView::setFrame
[DetectionView] paintEvent: bbox [0,1] × kích thước widget → vẽ
```
Ba lần đổi hình tại ba ranh giới: `model::Detection` → proto `Detection` → `DetectionBox`/`FrameUpdate`.

## Vì sao phải qua biến trung gian?
Lúc viết `PipelineManager`, nó chưa biết ai sẽ nhận detection (StreamService chưa tồn tại
trong file đó). Nên nó để sẵn một biến `std::function sink_`; `main` tiêm hành vi thật lúc
chạy. Tách "viết class" khỏi "nối dây" → tầng dưới không phụ thuộc tầng trên.

## Đọc nhanh một callback bất kỳ — hỏi 4 câu
1. Vị nào? (`std::function` / con trỏ hàm C / Qt signal)
2. Ai SET? (tiêm hành vi)
3. Ai CALL? (phát sự kiện)
4. Thread nào? (cần marshal / khóa không?)
