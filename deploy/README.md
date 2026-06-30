# Deploy — DeepStreamService container

The service runs in the DeepStream 8.0 container on the GPU; the Qt desktop runs natively on
the host and talks to it over gRPC (`localhost:50051`).

## Build & run

```bash
# from the repo root
docker compose -f deploy/docker-compose.yaml up --build   # build image + run service
./build/desktop/desktop                                   # host desktop (separate terminal)
```

In the desktop: **Cameras → Add** a camera with
`rtsp_url = file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4`,
then **Live View → camera id → Start** to see video + boxes + FPS.

Volumes (survive `down`; removed only by `down -v`):
- `engine-cache` → the TensorRT engine (see below)
- `service-db` → the SQLite camera DB

## ⚠️ TensorRT engine caching gotcha

`nvinfer` uses `model-engine-file` (in `configs/config_infer_primary.txt`) to **load** an
engine, but when it **builds** one it serializes it **next to the ONNX** (in the image,
ephemeral) — it does *not* write to `model-engine-file`. So the engine is rebuilt (~30–60s)
on every fresh container unless we pre-seed the cache.

**Pre-build the engine once into the `engine-cache` volume** (build + copy to the load path):

```bash
ENG=/opt/nvidia/deepstream/deepstream-8.0/samples/models/Primary_Detector/resnet18_trafficcamnet_pruned.onnx_b4_gpu0_fp16.engine
docker run --rm --gpus all \
  -v deploy_engine-cache:/engine-cache \
  -v "$(pwd)/configs:/configs" \
  --entrypoint bash \
  nvcr.io/nvidia/deepstream:8.0-triton-multiarch -lc "
    gst-launch-1.0 -e \
      uridecodebin uri=file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4 ! \
      m.sink_0 nvstreammux name=m batch-size=1 width=1920 height=1080 ! \
      nvinfer config-file-path=/configs/config_infer_primary.txt ! fakesink sync=0 >/dev/null 2>&1
    cp '$ENG' /engine-cache/trafficcamnet_b4_gpu0_fp16.engine
  "
# verify:
docker run --rm -v deploy_engine-cache:/c alpine ls -la /c   # should show the .engine
```

After this, `StartCamera` loads the cached engine in a few seconds instead of rebuilding.
(TODO: fold this into an entrypoint/init step so it's automatic.)

## Notes
- First `StartCamera` blocks while nvinfer prepares the engine, so the desktop's StartCamera
  RPC uses a generous deadline. Pre-building the engine keeps it fast.
- On EOS (the sample file ending) the pipeline **auto-stops** (it does not loop). Press Stop
  then Start — or just Start — to replay; the pipeline is recreated.
