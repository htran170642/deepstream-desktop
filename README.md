# DeepStreamDesktop

A modern **Qt6 / C++20** desktop application for managing NVIDIA **DeepStream** AI
video-analytics services. The system is split into two independent applications
that communicate exclusively over **gRPC**:

- **`desktop/`** — Qt6 Widgets UI (cameras, live view, alerts, dashboard, settings).
- **`service/`** — the AI backend (DeepStream pipeline, detection processing,
  alerts, notifications), persisted in SQLite.

See [CLAUDE.md](CLAUDE.md) for the full architecture and roadmap.

---

## Features

- **Cameras** — add / edit / delete / enable / disable RTSP cameras (SQLite-backed CRUD).
- **Live View** — per-camera RTSP streaming with detection bounding boxes, labels, confidence, and FPS.
- **Alerts** — history with search/filter, JPEG snapshots on demand, and a live alert stream.
- **Notifications** — Email (SMTP), Slack, Telegram, and generic Webhook channels, configured via environment.
- **Dashboard** — live CPU / memory / GPU usage, pipeline FPS, and per-camera status.

All Desktop ↔ Service communication is over gRPC; the UI never touches DeepStream directly.

---

## Dependencies

| Dependency | Purpose | Ubuntu/Debian package |
|---|---|---|
| CMake ≥ 3.24 | Build system | `cmake` |
| Ninja | Build generator (used by the default preset) | `ninja-build` |
| C++20 compiler | g++ ≥ 10 or clang | `build-essential` |
| pkg-config | Locating gRPC / protobuf / GStreamer | `pkg-config` |
| Qt6 (Widgets, Concurrent) | Desktop UI | *Qt online installer — see below* |
| gRPC (grpc++) | Desktop ↔ Service transport | `libgrpc++-dev` |
| Protocol Buffers | Message definitions + `protoc` | `libprotobuf-dev` `protobuf-compiler` |
| gRPC C++ plugin | `grpc_cpp_plugin` for codegen | `protobuf-compiler-grpc` |
| SQLite3 | Camera + alert storage | `libsqlite3-dev` |
| spdlog | Logging | `libspdlog-dev` |
| GoogleTest | Unit / round-trip tests | `libgtest-dev` |
| libcurl | Notifications: Slack/Telegram/Webhook (HTTP) + Email (SMTP) | `libcurl4-openssl-dev` |

### Optional — DeepStream (GPU / container only, `ENABLE_DEEPSTREAM=ON`)

| Dependency | Package |
|---|---|
| NVIDIA DeepStream SDK | (NVIDIA DeepStream base image / SDK installer) |
| GStreamer 1.0 | `libgstreamer1.0-dev` |
| CUDA runtime | (NVIDIA driver + CUDA toolkit) |

The host build (`ENABLE_DEEPSTREAM=OFF`, the default) runs **without** the DeepStream
SDK, CUDA, or a GPU — a `FakePipeline` stands in so the full service, gRPC, and tests
build and run on any machine. Notifications degrade gracefully too: without libcurl
the channels compile out (guarded by `DSD_WITH_CURL`) and the service still runs.

---

## Install (Ubuntu / Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  libspdlog-dev libsqlite3-dev libgtest-dev \
  libprotobuf-dev protobuf-compiler protobuf-compiler-grpc \
  libgrpc++-dev libcurl4-openssl-dev
```

### Qt6

Qt6 is not installed via apt here; the default CMake preset expects it under
`~/Qt/<version>/gcc_64` (see [CMakePresets.json](CMakePresets.json)). Install it with the
[Qt Online Installer](https://www.qt.io/download-qt-installer) (select Qt 6.x → Desktop
gcc_64), then adjust `CMAKE_PREFIX_PATH` in the preset to match your version. Distro Qt6
packages (`qt6-base-dev`) also work if you drop the `CMAKE_PREFIX_PATH` override.

---

## Build & test

```bash
cmake --preset default        # configure (Ninja, host build, DeepStream off)
cmake --build build           # build desktop + service + tests
ctest --test-dir build --output-on-failure   # run the test suite
```

Binaries land in `build/desktop/desktop` and `build/service/service`.

### Run

```bash
./build/service/service        # gRPC backend on 127.0.0.1:50051
./build/desktop/desktop        # Qt UI (connects to localhost:50051)
```

### Notifications (Phase 8) configuration

Notification channels are configured via environment variables; a channel is active
only when its variables are set (so unconfigured channels are silently skipped):

```bash
export DSD_SLACK_WEBHOOK="https://hooks.slack.com/services/…"
export DSD_WEBHOOK_URL="https://example.com/hooks/deepstream"
export DSD_TELEGRAM_TOKEN="123456:ABC…"  DSD_TELEGRAM_CHAT="<chat_id>"
export DSD_SMTP_HOST="smtp.example.com" DSD_SMTP_PORT="587" \
       DSD_SMTP_USER="…" DSD_SMTP_PASS="…" \
       DSD_SMTP_FROM="deepstream@example.com" DSD_SMTP_TO="ops@example.com"
./build/service/service
```

---

## Screenshots

> Captured from the running desktop app (`build/desktop/desktop`). See
> [docs/screenshots/](docs/screenshots/) for the shot list.

| Dashboard | Live View |
|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Live View](docs/screenshots/live-view.png) |

| Alerts | Settings |
|---|---|
| ![Alerts](docs/screenshots/alerts.png) | ![Settings](docs/screenshots/settings.png) |

---

## Study notes

Per-phase technical interview Q&A (English + Vietnamese) covering the engineering
concepts each phase exercised — see [docs/interview/](docs/interview/).

---

## Deployment

The desktop app runs natively; the service runs inside the NVIDIA DeepStream container on the
GPU. **Prerequisites:** an NVIDIA GPU + driver and Docker with the
[nvidia-container-toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html).

```bash
cp deploy/.env.example deploy/.env    # optional: notification tokens / SMTP creds
docker compose -f deploy/docker-compose.yaml up --build   # build + run the service on GPU
./build/desktop/desktop                                   # native desktop, connects to :50051
```

The container builds the service with `ENABLE_DEEPSTREAM=ON` and, on first run, pre-builds and
caches the TensorRT engine (a one-time ~30–60s cost). See [deploy/README.md](deploy/README.md)
for engine caching, volumes, and the healthcheck.
