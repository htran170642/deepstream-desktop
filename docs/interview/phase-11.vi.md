# Phase 11 — Deployment: Câu hỏi phỏng vấn

Các câu hỏi kỹ thuật bao quát những khái niệm Phase 11 đã thực hiện (đóng gói service vào
container, truyền GPU, cache TensorRT engine, cấu hình compose). Mỗi câu có con trỏ ngắn tới
artifact.

## Đóng gói service vào container

1. **Vì sao build service *bên trong* image DeepStream thay vì biên dịch trên host rồi copy binary vào?**
   Service liên kết các thư viện DeepStream/GStreamer/CUDA chỉ tồn tại trong container DeepStream;
   build ở đó bảo đảm tương thích ABI/runtime. Host build vẫn không cần SDK
   (`ENABLE_DEEPSTREAM=OFF`) nhờ `FakePipeline`. Xem [deploy/Dockerfile](../../deploy/Dockerfile).

2. **Image build với `-DBUILD_DESKTOP=OFF -DBUILD_TESTING=OFF -DENABLE_DEEPSTREAM=ON`. Mỗi cờ để làm gì?**
   Container chỉ chứa backend: không Qt (desktop chạy native), không binary test (image nhỏ hơn,
   build nhanh hơn), và DeepStream ON để pipeline thật (không phải `FakePipeline`) được biên dịch.
   Xem lệnh `cmake` trong Dockerfile.

3. **Container lấy quyền truy cập GPU thế nào?**
   compose đặt trước một thiết bị NVIDIA (`deploy.resources.reservations.devices: driver: nvidia`),
   được nvidia-container-toolkit ánh xạ vào container lúc runtime. Xem
   [docker-compose.yaml](../../deploy/docker-compose.yaml).

## Cache TensorRT engine

4. **Cái bẫy caching của `nvinfer` là gì, và vì sao một container ngây thơ rebuild engine mỗi lần chạy?**
   `nvinfer` *nạp* engine từ `model-engine-file`, nhưng khi *build* một cái nó serialize cạnh ONNX
   bên trong image (phù du) — không phải vào `model-engine-file`. Nên một container mới không có gì
   ở đường nạp và rebuild (~30–60s). Xem
   [config_infer_primary.txt](../../configs/config_infer_primary.txt) và [deploy/README.md](../../deploy/README.md).

5. **Entrypoint biến việc build engine thành chi phí một-lần thế nào, và vì sao nó idempotent?**
   Lúc khởi động nó kiểm tra volume `engine-cache` xem có engine chưa; nếu chưa, chạy một pipeline
   `gst-launch` bỏ đi (để `nvinfer` build), copy kết quả vào volume, rồi exec service. Volume tồn
   tại qua `down`, nên lần sau việc kiểm tra thoát sớm. Xem [entrypoint.sh](../../deploy/entrypoint.sh).

6. **Vì sao `exec` service ở cuối entrypoint thay vì chỉ chạy nó?**
   `exec` thay thế tiến trình shell, nên service trở thành PID 1 và nhận trực tiếp SIGTERM/SIGINT
   của Docker — tắt sạch (rút cạn các worker AlertManager/NotificationManager). Không có `exec`,
   tín hiệu sẽ tới shell chứ không phải service.

## Cấu hình compose

7. **Bí mật notification (token Slack/SMTP) được cấp cho container thế nào mà không commit?**
   Một `env_file: .env` với `required: false`; `deploy/.env` bị git-ignore và chỉ
   `deploy/.env.example` được commit. Mô phỏng cấu hình env-var của service (`ChannelFactory`).
   Xem [docker-compose.yaml](../../deploy/docker-compose.yaml) và [.env.example](../../deploy/.env.example).

8. **Vì sao `required: false` trên env_file?**
   Để stack chạy với zero cấu hình notification — thiếu `.env` không phải lỗi, và các kênh chưa
   set bị factory bỏ qua. Triển khai không nên phụ thuộc cứng vào bí mật.

9. **`working_dir` của service là `/data`, một volume gắn kết. Vì sao không phải `/workspace` nơi nó được build?**
   DB SQLite (`deepstream.db`) được ghi vào cwd; đặt cwd trên volume `service-db` giữ nó qua các
   lần khởi động lại. Gắn một volume đè lên `/workspace` sẽ che khuất binary đã build.

10. **Healthcheck thực sự kiểm tra gì, và vì sao `start_period` dài?**
    Một probe TCP cổng 50051 bằng `bash` không phụ thuộc gì — mang tính khuyến cáo "server gRPC đã
    bind chưa". `start_period` 90s bao phủ việc build engine khi cache lạnh để container không bị
    gắn cờ unhealthy trong lúc đang khởi động hợp lệ. Xem khối `healthcheck`.

11. **`.dockerignore` loại trừ gì và vì sao quan trọng với `COPY . /workspace`?**
    `build/`, `.git/`, `*.db`, `.claude/` — giữ build context nhỏ/sạch để cây `build/` khổng lồ của
    host hay DB cục bộ không bị đưa vào image (build nhanh hơn, không artifact cũ). Xem
    [.dockerignore](../../.dockerignore).
