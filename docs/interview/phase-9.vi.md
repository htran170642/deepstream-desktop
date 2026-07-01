# Phase 9 — Dashboard: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 9 đã thực hiện (9a: SystemMonitor lấy mẫu
`/proc` + SystemService + DashboardPage; 9b: metric GPU qua NVML). Mỗi câu có con trỏ ngắn
tới code.

## Lấy mẫu hệ thống

1. **CPU usage không phải một giá trị đọc được — nó là một tỷ lệ. Làm sao tính phần trăm từ `/proc/stat`?**
   `/proc/stat` cho jiffies tích lũy (busy + idle). CPU% là *hiệu* giữa hai lần đọc:
   `(Δtotal − Δidle) / Δtotal`. Monitor giữ `CpuTimes` trước đó; lần `sample()` đầu tiên không có
   hiệu nên báo 0% và chỉ gieo baseline. Xem
   [SystemMonitor.cpp](../../service/src/system/SystemMonitor.cpp) `cpuPercent` / `sample`.

2. **Vì sao phân tích `MemAvailable` thay vì `MemFree` cho "bộ nhớ đã dùng"?**
   `MemFree` loại trừ page cache/buffer có thể thu hồi, nên làm một máy khỏe trông gần như đầy.
   `MemAvailable` là ước lượng của kernel về bộ nhớ cấp phát được, nên `used = total − available`
   phản ánh áp lực thật. Xem `parseMemInfo`.

3. **Làm sao unit-test các parser `/proc` mà không phụ thuộc phần cứng máy chạy test?**
   `parseCpuTimes` / `parseMemInfo` là hàm thuần theo một chuỗi; phần I/O file là một wrapper mỏng
   `readFile`. Test nạp các đoạn `/proc` mẫu và assert số chính xác. Cùng kiểu tách thuần/vận-chuyển
   như Payloads. Xem [system_monitor_test.cpp](../../tests/system_monitor_test.cpp).

4. **`idle + iowait` được tính là idle. Vì sao, và chuyện gì xảy ra nếu bộ đếm đi lùi giữa hai lần lấy mẫu?**
   iowait là thời gian CPU không thực thi công việc, nên không phải "busy". `cpuPercent` kẹp một
   hiệu lùi/bằng 0 về 0 (phòng thủ trước reset bộ đếm hoặc hai poller đan xen), tránh tràn ngược
   phép trừ unsigned. Xem `cpuPercent`.

## FPS & lớp biên

5. **FPS là thuộc tính của pipeline nhưng bạn tính nó trong service gRPC. Vì sao, và thế nào?**
   Pipeline chỉ phơi ra một bộ đếm frame tích lũy `std::atomic` (`framesProcessed()`, tăng bằng
   `relaxed` trong `onFrame`); service suy ra FPS từ hiệu bộ đếm trên khoảng thời gian thực giữa
   hai lần `GetStats`. Giữ pipeline không cần biết FPS. Xem
   [PipelineManager.cpp](../../service/src/pipeline/PipelineManager.cpp) và
   [SystemServiceImpl.cpp](../../service/src/SystemServiceImpl.cpp) `computeFps`.

6. **Vì sao một atomic `relaxed` là lựa chọn đúng cho bộ đếm frame?**
   Nó nằm trên đường nóng theo từng frame; ta chỉ cần đếm đơn điệu, không cần thứ tự so với bộ nhớ
   khác, nên `fetch_add(1, relaxed)` gần như miễn phí — không lock, không hàng rào.

7. **`SystemServiceImpl` đọc ba nguồn. Vai trò của nó là gì, và nó KHÔNG phải đổi cái gì?**
   Nó là lớp biên gộp một snapshot từ `SystemMonitor` (CPU/mem/GPU), `CameraManager::list()`, và
   `PipelineManager` (`isRunning`/`runningCount`/`framesProcessed`). Các manager đã đủ dùng — không
   API manager nào đổi ngoài bộ đếm frame mang tính bổ sung. Cùng vai trò "biên điều phối" như
   StreamService.

8. **Mỗi camera báo cả `enabled` lẫn `running`. Vì sao đưa ra cả hai?**
   `enabled` là ý định đã lưu (từ bản ghi camera); `running` là thực tế sống (`PipelineManager::
   isRunning`). Hiện cả hai bộc lộ những sai lệch mà dashboard sinh ra để bắt (đã bật nhưng chưa
   khởi động, hoặc đang chạy nhưng đã tắt). Xem `GetStats`.

## Định dạng wire & polling unary

9. **Dashboard trực tiếp, nhưng `GetStats` là unary, không phải server-streaming. Vì sao?**
   Dashboard có tính định kỳ và tần suất thấp; một lời gọi unary poll theo timer tránh một registry
   subscriber streaming cùng vòng đời của nó. Desktop poll mỗi ~1s. Xem
   [system.proto](../../common/proto/system.proto).

10. **Định dạng wire giữ GPU tùy chọn mà không gây thay đổi gãy tương thích thế nào?**
    Một cờ bool `gpu_available`: false trên host không có NVML, và desktop hiện "N/A" mà không cần
    xử lý đặc biệt từng trường GPU. 9b bật nó true với thay đổi proto/UI bằng không. Xem
    `SystemStats` và `SystemMonitor::Sample`.

## Polling ở desktop

11. **Cái gì chặn timer poll 1s khỏi chồng chất request nếu service chậm?**
    Một cờ chặn `busy_`: `poll()` trả về sớm nếu một `GetStats` còn đang bay, và xóa cờ trong slot
    finished. Nên một service treo không thể chồng các lời gọi lên nhau. Xem
    [DashboardPage.cpp](../../desktop/src/pages/DashboardPage.cpp) `poll`.

12. **Poll chạy trên worker; widget có thể bị hủy giữa chừng. Sao vẫn an toàn?**
    Lambda `QtConcurrent::run` capture một `shared_ptr<SystemClient>`, không bao giờ `this`; kết quả
    tới UI chỉ qua `QFutureWatcher` gắn với QObject, thứ mà Qt bỏ nếu page đã biến mất. Destructor
    `waitForFinished()` trước khi kết quả bị hủy. Cùng quy tắc như các page khác.

13. **Vì sao hiển thị bộ nhớ dạng thanh phần trăm *và* chữ GiB thay vì chỉ phần trăm?**
    Chỉ phần trăm che mất quy mô (80% của 4 GiB ≠ 80% của 128 GiB). Giá trị thanh là phần trăm;
    `setFormat` phủ thêm chi tiết `used / total GiB`. Xem `onStatsFetched`.

## Metric GPU qua NVML (9b)

14. **Làm sao giữ NVML tùy chọn để host build vẫn biên dịch và chạy được?**
    `find_library(nvidia-ml)` + `find_path(nvml.h)` chặn một compile definition `DSD_WITH_NVML`,
    đúng như `find_package(CURL)`/`ENABLE_DEEPSTREAM`. Không có nó, mọi code NVML bị `#ifdef` loại
    bỏ và `gpu_available` giữ false. Xem [service/CMakeLists.txt](../../service/CMakeLists.txt).

15. **Làm sao lưu handle `nvmlDevice_t` mà không rò `<nvml.h>` vào header luôn được biên dịch?**
    Dưới dạng một `void*` mờ đục (`nvmlDevice_t` là một typedef con trỏ); header giữ sạch NVML mà
    không cần `#ifdef`, và `.cpp` ép kiểu lại bên trong khối được canh. Xem
    [SystemMonitor.hpp](../../service/src/system/SystemMonitor.hpp) `gpu_device_` và
    `SystemMonitor.cpp` `sampleGpu`.

16. **Vì sao `sampleGpu` đọc handle thiết bị mà không cần lock trong khi lấy mẫu CPU thì có?**
    `gpu_ready_`/`gpu_device_` được set một lần trong constructor và chỉ đọc sau đó, và các lời gọi
    truy vấn NVML là thread-safe — nên không có trạng thái khả biến chia sẻ để canh, khác với
    baseline CPU (`prev_cpu_`) mà `sample()` biến đổi dưới `mutex_`. Xem `sampleGpu`.
