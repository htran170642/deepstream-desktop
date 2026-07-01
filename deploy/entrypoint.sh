#!/usr/bin/env bash
# Container entrypoint for DeepStreamService.
#
# nvinfer loads its TensorRT engine from `model-engine-file` (a mounted volume),
# but when it *builds* an engine it serializes it next to the ONNX inside the
# image (ephemeral) — not to `model-engine-file`. So a fresh container would
# rebuild the engine (~30-60s) on the first StartCamera. To avoid that, we
# pre-build the engine once here and copy it into the /engine-cache volume;
# subsequent runs find it and skip the rebuild. Idempotent.
set -euo pipefail

# Load path nvinfer reads from (see configs/config_infer_primary.txt).
CACHED_ENGINE="/engine-cache/trafficcamnet_b4_gpu0_fp16.engine"
# Where nvinfer writes the freshly-built engine (next to the ONNX).
BUILT_ENGINE="/opt/nvidia/deepstream/deepstream/samples/models/Primary_Detector/resnet18_trafficcamnet_pruned.onnx_b4_gpu0_fp16.engine"
NVINFER_CONFIG="${DSD_NVINFER_CONFIG:-/configs/config_infer_primary.txt}"
SAMPLE_URI="file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4"

if [ -f "$CACHED_ENGINE" ]; then
    echo "[entrypoint] Using cached TensorRT engine: $CACHED_ENGINE"
else
    echo "[entrypoint] No cached engine; building once (this can take 30-60s)..."
    # Throwaway pipeline: nvinfer builds + serializes the engine as a side effect.
    gst-launch-1.0 -e \
        uridecodebin uri="$SAMPLE_URI" ! \
        m.sink_0 nvstreammux name=m batch-size=1 width=1920 height=1080 ! \
        nvinfer config-file-path="$NVINFER_CONFIG" ! fakesink sync=0 \
        >/dev/null 2>&1 || true

    if [ -f "$BUILT_ENGINE" ]; then
        mkdir -p "$(dirname "$CACHED_ENGINE")"
        cp "$BUILT_ENGINE" "$CACHED_ENGINE"
        echo "[entrypoint] Engine cached at $CACHED_ENGINE"
    else
        echo "[entrypoint] WARNING: pre-build did not produce $BUILT_ENGINE;" \
             "the service will build the engine on first StartCamera instead."
    fi
fi

# Hand off to the service (PID 1, receives signals).
exec /workspace/build/service/service "$@"
