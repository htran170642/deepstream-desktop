# Phase 6 — Live View Streaming: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 6 đã thực hiện (6a: streaming detection +
đấu nối; 6b: khung hình JPEG từ DeepStream). Mỗi câu có con trỏ ngắn tới code.

## gRPC streaming & vòng đời

1. **Unary vs server-streaming RPC — khi nào dùng cái nào?**
   Unary cho request/response (StartCamera/StopCamera); server-streaming cho luồng liên tục
   (StreamDetections `returns (stream DetectionFrame)`). Xem [stream.proto](../../common/proto/stream.proto).

2. **Một handler server-streaming là một hàm chạy dài. Làm sao biết client đã ngắt kết nối?**
   Poll `ServerContext::IsCancelled()` và/hoặc phát hiện `ServerWriter::Write()` trả về false.
   Ta `cv.wait_for(200ms)` rồi kiểm tra lại cả hai. Xem [StreamServiceImpl.cpp](../../service/src/StreamServiceImpl.cpp) `StreamDetections`.

3. **Producer nhanh + consumer chậm trên một stream — làm sao tránh dùng bộ nhớ không giới hạn?**
   Hàng đợi có giới hạn cho mỗi subscriber, kèm drop-oldest (`kMaxQueue`). Với video, khung hình
   mới nhất là quan trọng nhất. Xem `broadcast()`.

4. **Làm sao hủy một lời gọi `Read()` đang chặn ở phía client trên server-stream từ một thread khác?**
   `grpc::ClientContext::TryCancel()` — được tạo trước khi spawn worker để `unsubscribe()`
   luôn có thể hủy. Xem [StreamClient.cpp](../../desktop/src/client/StreamClient.cpp).

## Tách rời (decoupling) & observer pattern

5. **Vì sao PipelineManager phơi ra một `FrameSink` (std::function) thay vì gọi thẳng StreamService?**
   Để tầng pipeline không cần biết gì về gRPC/streaming — `main` tiêm sink vào lúc đấu nối.
   Producer và consumer không phụ thuộc lẫn nhau. Xem [callbacks-atlas](../architecture/callbacks-atlas.md).

6. **Việc điều phối giữa hai manager (CameraManager + PipelineManager) đặt ở đâu, và vì sao ở đó?**
   Tại biên gRPC (`StreamServiceImpl`). CameraManager chỉ làm CRUD; PipelineManager chỉ lo pipeline;
   biên điều phối cả hai. (Giải quyết một khe hở đã nêu trong review kiến trúc.)

## Qt threading

7. **Một khung hình đến qua mạng trên worker thread. Vì sao không vẽ trực tiếp được, và marshal về UI thread thế nào?**
   Qt widget chỉ được đụng từ luồng GUI. `QMetaObject::invokeMethod(this, functor, Qt::QueuedConnection)`
   đưa công việc vào event loop của receiver; dùng `this` làm context khiến Qt bỏ event nếu widget
   đã bị hủy. Xem [LiveViewPage.cpp](../../desktop/src/pages/LiveViewPage.cpp).

8. **Khi marshal dữ liệu qua ranh giới thread, vì sao capture theo giá trị chứ không theo tham chiếu?**
   Dữ liệu nguồn chỉ sống trong lời gọi worker; đến lúc lambda đã hàng đợi chạy thì tham chiếu
   sẽ treo (dangle). Ta truyền `shared_ptr<const FrameUpdate>` (rẻ để copy, chia sẻ, chỉ đọc).

9. **Vì sao `QMetaObject::invokeMethod(this, …, QueuedConnection)` an toàn ngay cả khi widget bị hủy giữa chừng?**
   Event gắn với receiver QObject; Qt loại bỏ các event đang chờ của một object đã bị hủy.

## DeepStream / GStreamer

10. **Cần khung hình đã encode VÀ các detection của nó, đồng bộ, theo từng nguồn. Vì sao dùng `nvds_obj_enc` trong probe metadata thay vì nhánh `tee → nvjpegenc → appsink`?**
    Probe đã có sẵn `NvDsFrameMeta` (detections) và `NvBufSurface` (pixel) từ cùng một buffer đã
    batch — encode ngay tại đó giữ tương quan khung↔meta↔nguồn cực đơn giản. Một nhánh tee sẽ phải
    tách lại surface đã batch và đồng bộ lại. Xem [DeepStreamPipeline.cpp](../../service/src/pipeline/DeepStreamPipeline.cpp) `handleBatchMeta`.

11. **Vì sao `gst_element_release_request_pad` trên nvstreammux bị treo sau khi nguồn đạt EOS, và khắc phục thế nào?**
    Giải phóng một request pad trên mux đang live cho một nguồn đã EOS bị chặn bởi trạng thái tổng
    hợp (aggregation) của mux. Khắc phục: khi dừng nguồn cuối cùng, tháo dỡ toàn bộ pipeline (đặt
    NULL + unref) để pad được giải phóng như một phần của việc hủy element. (Xóa từng nguồn EOS
    cần quy trình EOS-probe runtime-src-del — TODO.)

12. **Bus watch là một callback C; làm sao nó tới được đối tượng C++, và vì sao không thể capture `this`?**
    Một lambda không-capture chuyển được thành con trỏ hàm C; `this` được truyền qua `user_data`
    và khôi phục bằng `static_cast`. Một lambda có capture không chuyển được thành con trỏ hàm.

13. **EOS auto-stop: vì sao không thể tháo dỡ pipeline trực tiếp từ bus watch?**
    Bus watch chạy trên luồng GMainLoop; tháo dỡ sẽ join chính luồng đó → deadlock tự-join. Thay
    vào đó EOS gọi `g_main_loop_quit` (đỗ pipeline lại, `running_=false`); manager tháo dỡ nó lười
    (lazily) ở lần stop/start kế tiếp (một luồng khác).

## Định dạng wire độc lập codec

14. **Bạn ship 6a chỉ có detection, 6b thêm JPEG. Làm sao tránh thay đổi proto gây gãy tương thích, và H.264 sẽ chèn vào sau thế nào?**
    `DetectionFrame.frame` (bytes) + một enum `FrameCodec` (NONE/JPEG/H264). Codec mới chỉ là một
    lần thêm enum không gãy + một nhánh decode ở desktop; client cũ vẫn chạy. Xem
    [live-view-streaming.md](../architecture/live-view-streaming.md).

15. **Render: vì sao lưu bounding box chuẩn hóa [0,1] thay vì tọa độ pixel?**
    Độc lập độ phân giải — desktop co giãn box theo kích thước widget bất kể độ phân giải nguồn
    (`d.x * w`, `d.y * h`). Xem [DetectionView.cpp](../../desktop/src/pages/DetectionView.cpp).
