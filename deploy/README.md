# Deploy — DeepStreamService container

The service runs in the DeepStream 8.0 container on the GPU; the Qt desktop runs natively on
the host and talks to it over gRPC (`localhost:50051`).

## Prerequisites

- NVIDIA GPU + driver
- Docker with the **[nvidia-container-toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)**
  (so the container can access the GPU via `driver: nvidia`)

## Build & run

```bash
# from the repo root
docker compose -f deploy/docker-compose.yaml up --build   # build image + run service
./build/desktop/desktop                                   # host desktop (separate terminal)
```

On the **first** run the container pre-builds the TensorRT engine (~30–60s, see below);
subsequent runs start in a few seconds. In the desktop: **Cameras → Add** a camera with
`rtsp_url = file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4`,
then **Live View → camera id → Start** to see video + boxes + FPS.

Volumes (survive `down`; removed only by `down -v`):
- `engine-cache` → the cached TensorRT engine
- `service-db` → the SQLite camera + alert DB

## Configuration (`.env`)

Notification channels and other `DSD_*` settings are passed to the container via an optional
env file:

```bash
cp deploy/.env.example deploy/.env    # then fill in Slack/Telegram/SMTP/... as needed
```

`deploy/.env` is git-ignored (it holds secrets); the compose `env_file` is `required: false`,
so it's fine to omit — unset channels are simply skipped.

## TensorRT engine caching (now automatic)

`nvinfer` uses `model-engine-file` (in `configs/config_infer_primary.txt`) to **load** an
engine, but when it **builds** one it serializes it **next to the ONNX** (in the image,
ephemeral) — not to `model-engine-file`. So without help the engine would be rebuilt on every
fresh container.

[`entrypoint.sh`](entrypoint.sh) handles this automatically: on start, if
`/engine-cache/trafficcamnet_b4_gpu0_fp16.engine` is missing, it runs a throwaway `gst-launch`
pipeline so `nvinfer` builds the engine, copies it into the `engine-cache` volume, then execs
the service. It's idempotent — once the volume holds the engine (it survives `down`), startup
skips the build. The first `docker compose up` therefore takes the one-time build hit; every
run after is fast.

## Notes

- **Healthcheck**: compose probes TCP `50051`; `start_period` (90s) covers the cold-cache
  engine build, so the container isn't marked unhealthy while it warms up.
- First `StartCamera` after a cold start still blocks briefly while `nvinfer` loads the engine,
  so the desktop's StartCamera RPC uses a generous deadline.
- On EOS (the sample file ending) the pipeline **auto-stops** (it does not loop). Press Stop
  then Start — or just Start — to replay; the pipeline is recreated.
