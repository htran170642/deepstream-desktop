# Phase 11 — Deployment: Interview Questions

Technical questions covering the concepts Phase 11 exercised (containerizing the service,
GPU passthrough, TensorRT engine caching, compose config). Each has a short answer pointer to
the artifact.

## Containerizing the service

1. **Why build the service *inside* the DeepStream base image rather than compiling on the host and copying a binary in?**
   The service links DeepStream/GStreamer/CUDA libraries that only exist in the DeepStream
   container; building there guarantees ABI/runtime compatibility. The host build stays
   SDK-free (`ENABLE_DEEPSTREAM=OFF`) via `FakePipeline`. See [deploy/Dockerfile](../../deploy/Dockerfile).

2. **The image builds with `-DBUILD_DESKTOP=OFF -DBUILD_TESTING=OFF -DENABLE_DEEPSTREAM=ON`. Why each flag?**
   The container ships only the backend: no Qt (desktop runs natively), no test binaries (smaller
   image, faster build), and DeepStream ON so the real pipeline (not `FakePipeline`) is compiled.
   See the `cmake` invocation in the Dockerfile.

3. **How does the container get GPU access?**
   compose reserves an NVIDIA device (`deploy.resources.reservations.devices: driver: nvidia`),
   which the nvidia-container-toolkit maps into the container at runtime. See
   [docker-compose.yaml](../../deploy/docker-compose.yaml).

## TensorRT engine caching

4. **What's the caching gotcha with `nvinfer`, and why does a naive container rebuild the engine every run?**
   `nvinfer` *loads* the engine from `model-engine-file`, but when it *builds* one it serializes
   it next to the ONNX inside the (ephemeral) image — not to `model-engine-file`. So a fresh
   container has nothing at the load path and rebuilds (~30–60s). See
   [config_infer_primary.txt](../../configs/config_infer_primary.txt) and [deploy/README.md](../../deploy/README.md).

5. **How does the entrypoint make the engine build a one-time cost, and why is that idempotent?**
   On start it checks the `engine-cache` volume for the engine; if absent it runs a throwaway
   `gst-launch` pipeline (so `nvinfer` builds it), copies the result into the volume, then execs
   the service. The volume persists across `down`, so the check short-circuits on later runs.
   See [entrypoint.sh](../../deploy/entrypoint.sh).

6. **Why `exec` the service at the end of the entrypoint instead of just running it?**
   `exec` replaces the shell process, so the service becomes PID 1 and receives Docker's
   SIGTERM/SIGINT directly — clean shutdown (which drains the AlertManager/NotificationManager
   workers). Without `exec`, signals would hit the shell, not the service.

## Compose configuration

7. **How are notification secrets (Slack/SMTP tokens) supplied to the container without committing them?**
   An `env_file: .env` with `required: false`; `deploy/.env` is git-ignored and only
   `deploy/.env.example` is committed. Mirrors the service's env-var config (`ChannelFactory`).
   See [docker-compose.yaml](../../deploy/docker-compose.yaml) and [.env.example](../../deploy/.env.example).

8. **Why `required: false` on the env_file?**
   So the stack runs with zero notification config — a missing `.env` isn't an error, and unset
   channels are simply skipped by the factory. Deployment shouldn't hard-depend on secrets.

9. **The service's `working_dir` is `/data`, a mounted volume. Why not `/workspace` where it was built?**
   The SQLite DB (`deepstream.db`) is written to the cwd; putting cwd on the `service-db` volume
   persists it across restarts. Mounting a volume over `/workspace` would shadow the built binary.

10. **What does the healthcheck actually verify, and why the long `start_period`?**
    A dependency-free `bash` TCP probe of port 50051 — advisory "is the gRPC server bound". The
    90s `start_period` covers the cold-cache engine build so the container isn't flagged unhealthy
    while it legitimately warms up. See the `healthcheck` block.

11. **What does `.dockerignore` exclude and why does it matter for `COPY . /workspace`?**
    `build/`, `.git/`, `*.db`, `.claude/` — keeps the build context small/clean so a huge host
    `build/` tree or local DB isn't shipped into the image (faster builds, no stale artifacts).
    See [.dockerignore](../../.dockerignore).
