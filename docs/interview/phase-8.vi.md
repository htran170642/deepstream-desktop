# Phase 8 — Notification Module: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 8 đã thực hiện (8a: NotificationManager +
Payloads thuần; 8b: các kênh HTTP/SMTP dùng libcurl + factory đọc env; 8c: gRPC GetStatus/SendTest
+ UI Settings trên desktop). Mỗi câu có con trỏ ngắn tới code.

## Fan-out & giao nhận bất đồng bộ

1. **Alert đã stream tới desktop qua gRPC. Làm sao thêm notification mà không đổi AlertManager?**
   Fan out tại biên, không phải bên trong producer. Lambda sink của alert trong `main` gọi **cả**
   `alert_service.broadcastAlert(alert)` lẫn `notification_manager.notify(alert)` — một bản copy
   `shared_ptr` cho stream, tham chiếu cuối cùng move vào notification. Xem
   [main.cpp](../../service/src/main.cpp) `setAlertSink`. AlertManager giữ nguyên API sink đơn.

2. **Gửi qua mạng thì chậm. Làm sao một endpoint Slack/SMTP treo không làm nghẽn persistence alert?**
   `NotificationManager::notify()` chỉ enqueue; một worker thread riêng làm các lần gửi chặn —
   cùng thiết kế bất đồng bộ như AlertManager. Worker alert trả về ngay sau khi enqueue. Xem
   [NotificationManager.cpp](../../service/src/notify/NotificationManager.cpp) `run`.

3. **`notify()` là fire-and-forget. Vậy `flush()` để làm gì?**
   Một rào chắn cho test/tắt máy: nó chặn tới khi `pending_ == 0` (một `idle_cv_` worker báo khi
   hàng đợi cạn), để test có thể assert lên các lời gọi kênh mà không `sleep`. Trong production,
   việc rút cạn ở destructor lo phần tắt máy; `flush()` chủ yếu cho test xác định. Xem `flush`.

4. **Một kênh throw giữa chừng. Chuyện gì xảy ra với các kênh còn lại?**
   Mỗi kênh được dispatch trong `try/catch` riêng ở vòng lặp worker, nên một throw (hoặc trả về
   `false`) được log và các kênh còn lại vẫn nhận alert. Xem `run`.

5. **Cái gì giới hạn hàng đợi nếu mọi kênh đều treo, và vì sao đường drop phải cẩn thận với `pending_`?**
   Một trần `kMaxQueue` bỏ phần tử cũ nhất **và giảm `pending_`** cho phần tử bị bỏ — nếu không
   `flush()` sẽ chờ mãi trên một bộ đếm bao gồm cả các alert đã bị loại. Xem `enqueue`.

## Các kênh & sự tách bạch thuần/vận-chuyển

6. **Vì sao các hàm dựng payload (`buildSlackBody`, v.v.) được tách khỏi kênh gửi chúng?**
   Các hàm dựng là hàm thuần theo alert (+ config), nên test assert lên đúng chuỗi wire mà không
   cần socket; kênh chỉ thêm phần vận chuyển bằng libcurl. Cùng kiểu tách "test được trên host vs
   trung thực" như FakePipeline vs DeepStreamPipeline. Xem
   [Payloads.cpp](../../service/src/notify/Payloads.cpp) và [payloads_test.cpp](../../tests/payloads_test.cpp).

7. **Một label detection là `person "A"`. Vì sao nó không làm hỏng payload JSON?**
   `jsonEscape` xử lý `"`, `\`, và các ký tự control, và nó chạy *trước khi* giá trị được ghép vào
   body. Thứ tự escape (`\\` cùng nhánh switch với `"`) chặn một dấu backslash escape nhầm dấu nháy
   ngay sau. Xem `jsonEscape` + test `LabelWithQuoteStaysValidJson`.

8. **Vì sao các kênh nhận config qua constructor thay vì tự đọc `getenv`?**
   Việc phân tích env chỉ nằm ở `ChannelFactory`; config được tiêm làm `enabled()` cực đơn giản
   (`!url.empty()`) và cho phép test dựng kênh có/không config mà không cần môi trường. Xem
   [ChannelFactory.cpp](../../service/src/notify/ChannelFactory.cpp) và các constructor kênh.

9. **Một kênh báo "đã cấu hình" vs "trơ" thế nào, và ai quyết định không gọi nó?**
   `enabled()` của mỗi kênh trả về config bắt buộc đã có hay chưa; factory luôn dựng cả bốn, và
   worker (cùng `sendTest`) bỏ qua những kênh `!enabled()`. Xem
   [NotificationChannel.hpp](../../service/src/notify/NotificationChannel.hpp).

## Vận chuyển libcurl

10. **Một chi tiết libcurl quan trọng cho tính đúng đắn khi chạy ngoài main thread?**
    `CURLOPT_NOSIGNAL, 1` — nếu không libcurl dùng `SIGALRM` cho timeout, vốn không an toàn trên
    thread không phải main. Ngoài ra `CURLOPT_COPYPOSTFIELDS` để libcurl tự sở hữu bản copy body.
    Xem [Http.cpp](../../service/src/notify/Http.cpp) `postJson`.

11. **`curl_global_init` không thread-safe. Làm sao gọi đúng một lần, và bản sửa từ review là gì?**
    Một hàm `ensureCurlGlobalInit()` duy nhất trong Http.cpp được bảo vệ bởi `std::call_once`, dùng
    chung bởi các kênh HTTP và EmailChannel. **Sửa từ review:** ban đầu hai file có once-flag riêng,
    có thể gọi `curl_global_init` hai lần/đồng thời; hợp nhất về một chỗ đã loại bỏ điều đó. Xem
    `ensureCurlGlobalInit`.

12. **Email cũng dùng libcurl nhưng khác. Thông điệp được giao thế nào?**
    SMTP với một read-callback upload: `readPayload` stream thông điệp RFC-5322 (Date/From/To/
    Subject + body CRLF) theo từng chunk, trả về 0 tại EOF; `CURLUSESSL_ALL` nâng cấp lên STARTTLS.
    Xem [EmailChannel.cpp](../../service/src/notify/EmailChannel.cpp).

13. **Vì sao phép trừ trong `readPayload` không bao giờ tràn ngược (underflow)?**
    `offset` chỉ tiến thêm `min(room, remaining)`, nên không bao giờ vượt `data->size()`;
    `size() - offset` luôn không âm. Xem `readPayload`.

## Phụ thuộc tùy chọn & cấu hình

14. **libcurl không phải lúc nào cũng có. Làm sao build vẫn xanh khi thiếu nó, mô phỏng theo pattern nào có sẵn?**
    `find_package(CURL)` (không REQUIRED) chặn các source kênh sau `DSD_WITH_CURL`, đúng như
    `ENABLE_DEEPSTREAM`. Không có curl, `ChannelFactory` trả về danh sách rỗng và service vẫn chạy.
    Xem [service/CMakeLists.txt](../../service/CMakeLists.txt) và `ChannelFactory.cpp` nhánh `#else`.

15. **Bí mật (webhook URL, token, thông tin SMTP) đến từ đâu, và làm sao giữ chúng khỏi log?**
    Biến môi trường chỉ được đọc ở `ChannelFactory`; kênh chỉ log `name()` và lỗi vận chuyển, không
    bao giờ token/URL. Bot token của Telegram là một đoạn path trong URL và không bao giờ được log.
    Xem `ChannelFactory` và các method `send()` của kênh.

## Đồng thời của Logger (phát hiện từ review)

16. **Việc dựng NotificationManager làm lộ một crash tiềm ẩn trong Logger. Đó là gì?**
    `Logger::get()` làm `if (!logger) init()` mà không đồng bộ hóa; luồng constructor của manager
    và worker thread của nó cùng đua nhau tạo lười logger `"dsd"`, và lần
    `spdlog::stdout_color_mt("dsd")` thứ hai throw "already exists". Xem
    [Logger.cpp](../../common/logging/Logger.cpp).

17. **Sửa thế nào mà không phải trả một mutex ở mỗi lần `get()`?**
    `init()` và `get()` cùng đi qua một `std::once_flag`: người gọi đầu tiên tạo logger, những
    người khác chặn một lần, và sau đó con trỏ không bao giờ đổi — nên `get()` ở trạng thái ổn định
    chỉ là một lần kiểm tra cờ atomic lock-free cộng một bản copy `shared_ptr`. Xem `Logger::get`.

## Dịch vụ status/test gRPC

18. **Vì sao `SendTest` được cài đặt đồng bộ thay vì đi qua hàng đợi bất đồng bộ?**
    Nút "test" ở Settings muốn một kết quả pass/fail thật theo từng kênh, không phải fire-and-forget.
    `sendTest()` dispatch trên luồng caller (handler gRPC) và trả kết quả; nó an toàn song song với
    worker vì `send()` của mỗi kênh là tái nhập (re-entrant) (curl handle riêng, config bất biến).
    Xem [NotificationManager.cpp](../../service/src/notify/NotificationManager.cpp) `sendTest` và
    [NotificationServiceImpl.cpp](../../service/src/NotificationServiceImpl.cpp).

19. **Cả service lẫn lần rút-cạn-khi-tắt đều tham chiếu manager. Thứ tự khai báo nào giữ điều đó an toàn?**
    `notification_manager` được khai báo **trước** `alert_manager` (để lần rút cạn khi tắt alert vẫn
    gọi được `notify`), và `notification_service` **sau** manager (bị hủy trước). Trong test round-
    trip: `manager → service → server`, nên tháo dỡ là `server → service → manager`. Xem
    [main.cpp](../../service/src/main.cpp) và [notification_roundtrip_test.cpp](../../tests/notification_roundtrip_test.cpp).

## UI Settings trên desktop

20. **`GetStatus`/`SendTest` đụng mạng. Làm sao trang Settings vẫn mượt, và vì sao capture client chứ không `this`?**
    Cả hai chạy trên `QtConcurrent::run` với slot finished của `QFutureWatcher`; `SendTest` dùng
    deadline client 15s vì server gửi thật. Các lambda worker capture một
    `shared_ptr<NotificationClient>` (giữ client sống) nhưng không bao giờ `this`, nên một kết quả
    về muộn không thể đụng một page đã hủy — kết quả tới UI chỉ qua watcher gắn với QObject.
    Destructor `waitForFinished()` các watcher đang bay trước khi các vector kết quả của chúng bị
    hủy. Xem [SettingsPage.cpp](../../desktop/src/pages/SettingsPage.cpp).
