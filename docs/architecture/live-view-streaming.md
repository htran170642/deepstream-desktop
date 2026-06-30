# Live View Streaming — Architecture & Scaling

Status: accepted (Phase 6). Applies to how the DeepStream service delivers live video +
detections to the Qt desktop.

## Decision

Live View streams **encoded frames + detections together** over a **gRPC server-streaming
RPC** (`StreamService.StreamDetections`), and the **desktop draws the overlay** (boxes,
labels) with `QPainter`. Stage 6a streams detections only (boxes on a canvas); Stage 6b
adds JPEG frames.

The wire message is **codec-agnostic**:

```proto
enum FrameCodec { FRAME_CODEC_NONE = 0; FRAME_CODEC_JPEG = 1; FRAME_CODEC_H264 = 2; }

message DetectionFrame {
  int64 camera_id = 1;
  int64 timestamp_ms = 2;
  repeated Detection detections = 3;
  int32 frame_width = 4;
  int32 frame_height = 5;
  bytes frame = 6;        // encoded bytes (empty in 6a)
  FrameCodec codec = 7;   // how to decode `frame`
}
```

## Why (vs alternatives)

| Approach | gRPC-only | Sync frame↔box | Desktop deps | Bandwidth | Verdict |
|---|---|---|---|---|---|
| **MJPEG over gRPC + desktop overlay** ✓ | ✅ | perfect (same msg) | none (QImage) | high (ok on localhost) | **chosen** |
| H.264 over gRPC | ✅ | hard (GOP/reorder) | H264 decoder | low | upgrade path |
| RTSP + burn-in (nvdsosd) | ❌ breaks rule | perfect | RTSP player | low | not now |
| WebRTC | ❌ | — | heavy stack | low | overkill |

Rationale: respects CLAUDE.md "all Desktop↔Service comms via gRPC", gives free
frame↔detection sync (one message), keeps the desktop dependency-free, and lets the desktop
draw **interactive** overlays (toggle/filter/click) instead of burned-in pixels.

## Why it stays extensible (the seams)

Each future change lands in exactly one layer:

| Layer | Isolates | Change absorbed here |
|---|---|---|
| `Pipeline` / `DeepStreamPipeline` | how frames/detections are produced | codec, fps, resolution, inference interval, lazy-encode branch |
| `StreamService` + subscriber registry | what/when is sent | throttle fps, drop-oldest, on-demand video |
| `DetectionFrame` proto | wire format | new `FrameCodec` value |
| `DetectionView` (Qt) | rendering | new decoder per `codec` |

## Resource-reduction roadmap (lower CPU/GPU/bandwidth later)

| Lever | Saving | Where it changes | Effort |
|---|---|---|---|
| JPEG → H.264/H.265 | bandwidth 10–20× | encoder element + add `FRAME_CODEC_H264` + desktop decoder | medium |
| Lower stream fps (30→10) | ~3× | throttle in StreamService/pipeline | low |
| Downscale preview (1080p→720p) | ~2–4× | `nvvideoconvert` before encode | low |
| `nvinfer interval` (infer every Nth frame, track between) | GPU inference ~2–3× | one config line | very low |
| **Lazy-encode** — only run the encode branch while a client is subscribed | drops all encode cost when nobody watches | add/remove encode pad dynamically (same pattern as runtime source add/remove) | medium |
| Split streams — detections full-rate, video low-rate/on-demand | video cost only when needed | proto + StreamService | medium |
| Batch cameras through one nvinfer | 1 inference for N cameras | **already done** (nvstreammux) | — |

Highest value / lowest cost: `nvinfer interval` + lower fps/resolution. Highest structural
win: **lazy-encode** (inference keeps running for alerts; video encode only when a viewer is
attached) — enabled by the dynamic-pad design already used for source add/remove.

## Consequences

- Stage 6a verifies the hard parts (streaming lifecycle, sync, UI overlay) before any encode
  work, because frames are optional (`FRAME_CODEC_NONE`).
- Switching JPEG→H264 later is a non-breaking enum add + one decode branch; no RPC change,
  old clients keep working.
