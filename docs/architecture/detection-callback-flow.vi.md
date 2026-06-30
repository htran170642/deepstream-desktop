# Luồng Callback Detection — Service ↔ Pipeline

Một detection đi từ pipeline video tới client Live View như thế nào, và các class nối với
nhau ra sao qua **hai lớp callback**. Liên quan Phase 5 (pipeline) + Phase 6 (streaming).

## Danh sách class

| Class | Tầng | Trách nhiệm |
|---|---|---|
| `Pipeline` (interface) | pipeline | Hợp đồng: `start/stop/addSource` + `setDetectionCallback` |
| `FakePipeline` / `DeepStreamPipeline` | pipeline | Sinh detection **thô**; gọi `DetectionCallback` trên worker/streaming thread |
| `DetectionProcessor` | pipeline | Lọc stateless (confidence/class) + chuẩn hóa bbox |
| `PipelineManager` | pipeline | Sở hữu pipeline; tự đăng ký làm callback của pipeline; lọc; phơi `DetectionSink` ra ngoài |
| `StreamServiceImpl` | service (gRPC) | **Sink** detection + fan-out cho client subscribe; Start/StopCamera |
| `CameraManager` | service | Camera CRUD (tra rtsp_url theo id) |
| `main` | wiring | Nơi DUY NHẤT biết cả `PipelineManager` lẫn `StreamServiceImpl`; nối chúng |

## Hai lớp callback (ý tưởng cốt lõi)

```
Pipeline::DetectionCallback        PipelineManager::DetectionSink
   pipeline ──► manager (THÔ)         manager ──► thế giới ngoài (ĐÃ LỌC)
   set bởi PipelineManager::start     set bởi main (lambda -> broadcast)
```

- **DetectionCallback** — pipeline phát, PipelineManager nhận. Dữ liệu thô.
- **DetectionSink** — PipelineManager phát (sau khi lọc), StreamServiceImpl nhận. Dữ liệu đã lọc.
- Cùng chữ ký `std::function<void(const std::vector<model::Detection>&)>`, khác vai trò.
- Vì sao hai lớp: PipelineManager chèn bước `DetectionProcessor` ở giữa, giữ pipeline "mù"
  về streaming, và stream service "mù" về pipeline.

## Quan hệ tĩnh (ai biết ai)

```
                main  ──sở hữu──►  PipelineManager ──sở hữu──► Pipeline (Fake/DeepStream)
                  │                    │  └─sở hữu─► DetectionProcessor
                  │ sở hữu             │
                  ▼                    │ DetectionSink (std::function)
            StreamServiceImpl ◄────────┘  (main set; gọi broadcast)
                  │ tham chiếu
                  ├──► CameraManager   (tra camera theo id)
                  └──► PipelineManager (start/stop)

   PipelineManager KHÔNG #include StreamServiceImpl.
   StreamServiceImpl KHÔNG #include các pipeline impl.
   main là chỗ ghép duy nhất.
```

## Wiring (mỗi callback set ở đâu)

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

## Luồng runtime (một frame detection)

```
[worker pipeline / streaming thread GStreamer]  ── toàn lời gọi đồng bộ ──┐
  DeepStreamPipeline probe  (NvDsBatchMeta thô -> model::Detection)        │
    └► DetectionCallback  ──► PipelineManager::onDetections(thô)           │  CÙNG
         └► DetectionProcessor::process(thô) -> đã lọc                     │  MỘT
              └► DetectionSink  ──► StreamServiceImpl::broadcast(đã lọc)   │  THREAD
                   └► gom theo camera_id -> DetectionFrame                 │
                        └► đẩy vào queue mỗi subscriber + notify  ─────────┘
                                              │
                          ── RANH GIỚI THREAD (queue có giới hạn + condition_variable) ──
                                              │
[thread streaming gRPC, mỗi client một cái]   ▼
  StreamServiceImpl::StreamDetections: lấy frame -> writer->Write() -> desktop
```

- Từ probe tới `broadcast` chạy trên **thread worker pipeline** (lời gọi đồng bộ — không nhảy
  thread). Nên `broadcast` phải rẻ: chỉ đẩy vào queue.
- Phần chậm (Write mạng) chạy trên **thread streaming gRPC**, tách ra nhờ queue mỗi subscriber.
  Consumer chậm → `broadcast` drop frame cũ nhất (queue có giới hạn).

## Tóm tắt thread-safety
- `subscribers_mutex_` bảo vệ registry `camera_id → subscribers` (producer ghi qua
  `broadcast`; thread gRPC add/remove khi subscribe/unsubscribe).
- Mỗi `Subscriber` có `mutex` + `condition_variable` riêng cho queue của nó.
- `DetectionProcessor` stateless/bất biến → không cần khóa trên hot path.
- Lifetime: lambda `DetectionSink` bắt `&stream_service` bằng tham chiếu, nên `main` gọi
  `pipeline_manager.stopAll()` trước teardown để join worker khi stream service còn sống
  (tránh sink-after-free).
