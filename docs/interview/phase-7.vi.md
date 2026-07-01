# Phase 7 — Alert Module: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 7 đã thực hiện (7a: AlertRepository +
AlertManager + AlertService; 7b: desktop AlertClient + AlertPage). Mỗi câu có con trỏ ngắn
tới code.

## Thiết kế repository SQLite

1. **AlertRepository bị đụng bởi hai luồng không liên quan — worker persistence và các thread handler gRPC. Làm sao cho an toàn?**
   Làm nó *tự tuần tự hóa* (self-serializing): mọi method public (`add`/`list`/`snapshot`) đều
   giữ một `std::mutex` nội bộ, để kết nối SQLite duy nhất không bao giờ bị điều khiển đồng thời.
   Xem [AlertRepository.cpp](../../service/src/AlertRepository.cpp). `busy_timeout` là tuyến phòng
   thủ thứ hai chống `SQLITE_BUSY` từ bất kỳ writer nào khác.

2. **Sao không để mỗi caller tự lock, như CameraManager làm với CameraRepository?**
   CameraRepository có một người gác cổng duy nhất (CameraManager sở hữu nó riêng tư). AlertRepository
   được *chia sẻ* bởi AlertManager (writer) và AlertServiceImpl (reader) với các mutex riêng —
   không có người gác cổng duy nhất — nên lock phải nằm bên trong repository.

3. **`list()` chuyển tiếp tới `list(Filter)`, cả hai đều public. Làm sao tránh deadlock double-lock trên một mutex không đệ quy?**
   Chỉ overload `Filter` mới lock; `list()` không tham số chuyển tiếp *mà không* lock. Xem
   [AlertRepository.cpp](../../service/src/AlertRepository.cpp) `list()`.

4. **Truy vấn có filter làm sao vừa an toàn khỏi injection vừa hỗ trợ filter tùy chọn?**
   Dựng mệnh đề `WHERE` chỉ từ những trường bị ràng buộc, rồi bind giá trị theo vị trí cùng thứ
   tự — giá trị luôn được bind, không bao giờ nối chuỗi. Xem `list(const Filter&)`.

5. **Vì sao `list()` cố tình KHÔNG nạp BLOB snapshot, mà tách riêng lời gọi `snapshot(id)`?**
   List/stream giữ nhẹ (chỉ metadata); JPEG có thể lớn được lấy theo yêu cầu. List chọn
   `snapshot IS NOT NULL` để báo `has_snapshot` mà không đọc bytes.

## AlertManager — luật + persistence bất đồng bộ

6. **Luật sinh alert là gì, và làm sao chặn frame rate cao khỏi làm ngập store?**
   Kích hoạt khi label là mục tiêu và `confidence >= min_confidence`, được điều tiết bởi cooldown
   theo từng `(camera_id, label)`. Xem [AlertManager.cpp](../../service/src/AlertManager.cpp)
   `shouldFire`.

7. **Ghi persistence trên thread frame-sink làm chặn Live View. Làm sao tách I/O DB khỏi đường frame?**
   `onFrame()` chỉ chạy luật (rẻ) dưới `mutex_`, rồi *enqueue* các alert đã kích hoạt; một worker
   thread duy nhất làm `repo_.add()` + sink. Thread frame trả về mà không đụng DB. Xem `onFrame` / `run`.

8. **Vì sao giữ quyết định cooldown trên thread caller thay vì chuyển vào worker?**
   Điều tiết phải phản ánh thời điểm frame đến một cách xác định và đúng thứ tự; làm bất đồng bộ
   sẽ để độ trễ persistence làm nhiễu cooldown. Chỉ *tác dụng phụ* (ghi + notify) mới bị hoãn.

9. **Một test gọi `onFrame(...)` rồi assert ngay `repo.list().size() == 1`. Với persistence bất đồng bộ điều đó bị race. Làm sao giữ test xác định mà không `sleep`?**
   Một rào chắn `flush()` chặn tới khi `pending_ == 0` (một `idle_cv_` worker báo khi hàng đợi cạn).
   Xem `flush` và [alert_manager_test.cpp](../../tests/alert_manager_test.cpp).

10. **Cái gì giới hạn hàng đợi nội bộ nếu DB bị đơ?**
    Một mức trần (`kMaxQueue`) với drop-oldest + cảnh báo — một van an toàn, không phải đường bình
    thường (cooldown đã điều tiết đầu vào). Xem `enqueue`.

11. **`~AlertManager` rút cạn hàng đợi và gọi sink. Điều đó áp đặt bất biến vòng đời gì, và nó cắn ở đâu?**
    Đối tượng đích của sink phải sống lâu hơn manager. Trong `main` ta khai báo `alert_service`
    *trước* `alert_manager` để lần rút cạn khi tắt không gọi vào một service đã bị hủy; các test
    cũng cần sắp xếp lại thứ tự tương tự. Xem [main.cpp](../../service/src/main.cpp).

12. **Vòng lặp worker làm sao rút-cạn-rồi-thoát sạch khi tắt?**
    `wait` trên `!queue_.empty() || !running_`; xử lý khi hàng đợi còn phần tử ngay cả sau khi
    `running_` là false; chỉ return khi bị đánh thức với hàng đợi rỗng. Xem `run`.

## Dịch vụ alert gRPC

13. **RPC nào là unary và cái nào streaming, vì sao?**
    `ListAlerts`/`GetSnapshot` là unary (truy vấn lịch sử + ảnh theo yêu cầu); `StreamAlerts` là
    server-streaming (`returns (stream Alert)`) cho các đẩy trực tiếp. Xem
    [alert.proto](../../common/proto/alert.proto).

14. **Một alert stream trực tiếp không mang bytes ảnh. Làm sao desktop vẫn biết có ảnh tồn tại?**
    Cờ metadata `has_snapshot`. **Lỗi đã sửa ở phase này:** worker phải set
    `has_snapshot = !snapshot.empty()` *trước khi* xóa bytes cho stream, nếu không subscriber sẽ
    thấy `false` cho một alert thực ra có ảnh. Xem [AlertManager.cpp](../../service/src/AlertManager.cpp) `run`.

15. **`broadcastAlert` chạy trên worker thread của AlertManager; `StreamAlerts` chạy theo từng client. Chúng gặp nhau an toàn thế nào?**
    Một registry được bảo vệ bởi `subscribers_mutex_`; `broadcastAlert` đẩy một proto message chia
    sẻ vào hàng đợi có giới hạn của từng subscriber dưới mutex riêng của subscriber đó. Xem
    [AlertServiceImpl.cpp](../../service/src/AlertServiceImpl.cpp).

## Desktop AlertPage (Qt)

16. **Bảng lịch sử, lấy snapshot, và stream trực tiếp đều đụng mạng. Làm sao giữ UI thread mượt?**
    Các lời gọi unary chạy trên `QtConcurrent::run` với slot finished của `QFutureWatcher`; stream
    chạy trên worker thread của AlertClient và marshal từng alert qua `QMetaObject::invokeMethod(...,
    QueuedConnection)`. Xem [AlertPage.cpp](../../desktop/src/pages/AlertPage.cpp).

17. **Người dùng bấm alert A (bắt đầu lấy snapshot), rồi bấm B trước khi A trả về. Làm sao tránh hiện ảnh của A trên B?**
    Ghi lại `snapshot_for_id_` cho lần lấy đang bay, và trong slot finished, bỏ kết quả nếu id của
    lựa chọn hiện tại không còn khớp. Xem `onSelectionChanged`/`onSnapshotFetched`.

18. **Alert trực tiếp chèn lên đầu bảng mãi — cái gì chặn tăng trưởng không giới hạn, và vì sao đường lịch sử không cần?**
    Một mức trần `kMaxLiveRows` bỏ hàng cũ nhất (model + table đồng bộ nhịp). Đường lịch sử đã bị
    giới hạn bởi `limit` của truy vấn. Xem `onLiveAlert`.

19. **Các lambda worker trong AlertPage capture client nhưng không bao giờ `this`. Vì sao quan trọng?**
    Chúng capture một bản copy `shared_ptr<AlertClient>` (giữ client sống) chứ không phải page, nên
    một tác vụ về muộn không thể đụng widget đã hủy. Kết quả tới UI chỉ qua watcher/queued-invoke
    gắn với QObject, thứ mà Qt loại bỏ nếu page đã biến mất.

20. **Filter camera-id biểu diễn "bất kỳ camera nào" trên UI và trên wire thế nào?**
    Giá trị `0` của `QSpinBox` hiển thị `"Any"` qua `setSpecialValueText`, ánh xạ thẳng tới ngữ
    nghĩa `camera_id == 0` "không ràng buộc" của server. Xem constructor `AlertPage` + `AlertQuery`.
