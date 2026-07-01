# Phase 10 — Polish: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 10 đã thực hiện (dọn refactor/DRY, phủ test
cho các đường validation, theming UI). Mỗi câu có con trỏ ngắn tới code.

## Refactor & DRY

1. **Cả sáu client gRPC lặp lại cùng đoạn dựng channel. Làm sao loại bỏ trùng lặp mà không làm chúng dính vào nhau?**
   Trích hai hàm tự do vào một header — `makeChannel(address)` và `setDeadline(ctx, timeout)` —
   để mỗi client gọi. Không lớp cơ sở, không kế thừa; chỉ là hàm dùng chung, nên các client vẫn
   độc lập. Xem [GrpcSupport.hpp](../../desktop/src/client/GrpcSupport.hpp).

2. **Vì sao dùng hàm `inline` tự do trong header thay vì một lớp cơ sở `Client` chung?**
   Các client khác nhau về kiểu stub, tập RPC, và threading (một số stream, một số không); một lớp
   cơ sở sẽ ép một phân cấp giả tạo. Trùng lặp thực sự chỉ là hai câu lệnh một dòng không trạng
   thái — đúng thứ mà hàm tự do sinh ra để giải quyết — ưu tiên composition hơn inheritance (một
   quy tắc trong CLAUDE.md).

3. **Build có các mục CMake trùng (CameraServiceImpl liệt kê hai lần, khối DeepStream hai lần). Vì sao đó là vấn đề và khắc phục thế nào?**
   Mục nguồn trùng thì vô hại (CMake khử trùng) nhưng gây hiểu lầm; một khối `if(ENABLE_DEEPSTREAM)`
   trùng lặp có nguy cơ hai bản phân kỳ theo thời gian. Giữ khối GStreamer/DeepStreamPipeline đầy
   đủ và xóa bản không đầy đủ. Xem [service/CMakeLists.txt](../../service/CMakeLists.txt).

## Phủ test

4. **AlertRepository và round-trip camera đã được test. `camera_manager_test` lấp khe hở nào?**
   Hợp đồng của *manager*, không phải của repository: rằng `add()` ném `std::invalid_argument` khi
   name/url rỗng (chốt "không bao giờ lưu input xấu"), và rằng id không tồn tại cho
   `update()==nullopt` / `remove()==false`. Xem [camera_manager_test.cpp](../../tests/camera_manager_test.cpp).

5. **Vì sao assert lên các đường *lỗi* trong khi happy path đã pass?**
   Validation và xử lý không-tìm-thấy là nơi regression ẩn nấp — một refactor làm rớt một chốt vẫn
   pass happy-path. Ghim nhánh throw + `nullopt`/`false` khóa chặt hợp đồng. `EXPECT_THROW` /
   `EXPECT_FALSE` diễn đạt điều đó trực tiếp.

6. **Làm sao các test này nhanh và cô lập?**
   Mỗi test dựng một `CameraManager(":memory:")` — một DB SQLite trong bộ nhớ riêng cho từng test,
   không trạng thái chia sẻ, không đĩa, không server. Mô phỏng các test repository sẵn có.

## Theming UI

7. **Làm sao áp styling nhất quán khắp mọi page mà không đụng từng widget?**
   Một Qt Style Sheet toàn cục đặt trên gốc `QApplication` lan xuống mọi page, bảng, nút, và
   progress bar. Xem [main.cpp](../../desktop/src/main.cpp) `app.setStyleSheet`.

8. **Sidebar có style tối riêng biệt. Làm sao khoanh vùng để nó không lây sang các list khác?**
   Stylesheet nhắm tới `QListWidget#sidebar`, và `MainWindow` đặt `setObjectName("sidebar")` cho
   đúng list đó — một selector theo id, nên bất kỳ `QListWidget` nào khác đều không bị ảnh hưởng.
   Xem [MainWindow.cpp](../../desktop/src/MainWindow.cpp).

9. **Vì sao dùng `setApplicationName`/`setApplicationDisplayName` bên cạnh tiêu đề cửa sổ?**
   Tiêu đề cửa sổ là theo từng cửa sổ; tên ứng dụng cấp cho window manager/taskbar và đường lưu
   `QSettings`, cho ứng dụng một danh tính ở mức OS. Xem `main.cpp`.
