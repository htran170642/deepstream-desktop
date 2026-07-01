# Phase 9 — Dashboard: Interview Questions

Technical questions covering the concepts Phase 9 exercised (9a: SystemMonitor `/proc`
sampling + SystemService + DashboardPage; 9b: GPU metrics via NVML). Each has a short answer
pointer to the code.

## System sampling

1. **CPU usage isn't a value you can read — it's a rate. How do you compute a percentage from `/proc/stat`?**
   `/proc/stat` gives cumulative jiffies (busy + idle). CPU% is the *delta* between two
   readings: `(Δtotal − Δidle) / Δtotal`. The monitor keeps the previous `CpuTimes`; the first
   `sample()` has no delta, so it reports 0% and just seeds the baseline. See
   [SystemMonitor.cpp](../../service/src/system/SystemMonitor.cpp) `cpuPercent` / `sample`.

2. **Why parse `MemAvailable` instead of `MemFree` for "used memory"?**
   `MemFree` excludes reclaimable page cache/buffers, so it makes a healthy machine look nearly
   full. `MemAvailable` is the kernel's estimate of allocatable memory, so `used = total −
   available` reflects real pressure. See `parseMemInfo`.

3. **How are the `/proc` parsers unit-tested without depending on the test machine's hardware?**
   `parseCpuTimes` / `parseMemInfo` are pure functions of a string; the file I/O is a thin
   `readFile` wrapper. Tests feed canned `/proc` snippets and assert exact numbers. Same
   pure/transport split as Payloads. See [system_monitor_test.cpp](../../tests/system_monitor_test.cpp).

4. **`idle + iowait` counts as idle. Why, and what happens if counters go backwards between samples?**
   iowait is time the CPU wasn't executing work, so it's not "busy". `cpuPercent` clamps a
   backwards/zero delta to 0 (defensive against counter resets or two pollers interleaving),
   avoiding an unsigned-subtraction underflow. See `cpuPercent`.

## FPS & the boundary

5. **FPS is a pipeline property but you compute it in the gRPC service. Why, and how?**
   The pipeline only exposes a cumulative `std::atomic` frame counter (`framesProcessed()`,
   incremented `relaxed` in `onFrame`); the service derives FPS from the count delta over the
   wall-clock interval between two `GetStats` calls. Keeps the pipeline FPS-agnostic. See
   [PipelineManager.cpp](../../service/src/pipeline/PipelineManager.cpp) and
   [SystemServiceImpl.cpp](../../service/src/SystemServiceImpl.cpp) `computeFps`.

6. **Why is a `relaxed` atomic the right choice for the frame counter?**
   It's on the hot per-frame path; we only need monotonic counting, not ordering against other
   memory, so `fetch_add(1, relaxed)` is essentially free — no lock, no fence.

7. **`SystemServiceImpl` reads three sources. What is its role, and what did it NOT have to change?**
   It's the boundary that assembles one snapshot from `SystemMonitor` (CPU/mem/GPU),
   `CameraManager::list()`, and `PipelineManager` (`isRunning`/`runningCount`/`framesProcessed`).
   The managers were already sufficient — no manager API changed except the additive frame
   counter. Same "boundary orchestrates" role as StreamService.

8. **Each camera reports both `enabled` and `running`. Why surface both?**
   `enabled` is persisted intent (from the camera record); `running` is live reality
   (`PipelineManager::isRunning`). Showing both exposes the mismatches a dashboard exists to
   catch (enabled-but-not-started, or running-but-disabled). See `GetStats`.

## Wire format & unary polling

9. **Live dashboard, but `GetStats` is unary, not server-streaming. Why?**
   A dashboard is periodic and low-frequency; a unary call polled on a timer avoids a
   streaming-subscriber registry and its lifecycle. The desktop polls every ~1s. See
   [system.proto](../../common/proto/system.proto).

10. **How does the wire format keep GPU optional without a breaking change?**
    A `gpu_available` bool gate: false on a host without NVML, and the desktop shows "N/A"
    without special-casing each GPU field. 9b flips it true with zero proto/UI change. See
    `SystemStats` and `SystemMonitor::Sample`.

## Desktop polling

11. **What stops the 1s poll timer from stacking requests if the service is slow?**
    A `busy_` guard: `poll()` returns early if a `GetStats` is still in flight, and clears the
    flag in the finished-slot. So a hung service can't pile up overlapping calls. See
    [DashboardPage.cpp](../../desktop/src/pages/DashboardPage.cpp) `poll`.

12. **The poll runs on a worker; the widget could be destroyed mid-flight. How is that safe?**
    The `QtConcurrent::run` lambda captures a `shared_ptr<SystemClient>`, never `this`; results
    reach the UI only through the QObject-tied `QFutureWatcher`, which Qt drops if the page is
    gone. The destructor `waitForFinished()` before the result unwinds. Same rule as the other pages.

13. **Why show memory as a percentage bar *and* GiB text rather than just a percent?**
    Percent alone hides scale (80% of 4 GiB ≠ 80% of 128 GiB). The bar value is the percent;
    `setFormat` overlays the `used / total GiB` detail. See `onStatsFetched`.

## GPU metrics via NVML (9b)

14. **How is NVML kept optional so the host build still compiles and runs?**
    `find_library(nvidia-ml)` + `find_path(nvml.h)` gate a `DSD_WITH_NVML` compile definition,
    exactly like `find_package(CURL)`/`ENABLE_DEEPSTREAM`. Without it, all NVML code is
    `#ifdef`'d out and `gpu_available` stays false. See [service/CMakeLists.txt](../../service/CMakeLists.txt).

15. **How do you store the `nvmlDevice_t` handle without leaking `<nvml.h>` into the always-compiled header?**
    As an opaque `void*` (`nvmlDevice_t` is a pointer typedef); the header stays NVML-free with
    no `#ifdef`, and the `.cpp` casts back inside the guarded block. See
    [SystemMonitor.hpp](../../service/src/system/SystemMonitor.hpp) `gpu_device_` and
    `SystemMonitor.cpp` `sampleGpu`.

16. **Why can `sampleGpu` read the device handle without a lock even though CPU sampling takes one?**
    `gpu_ready_`/`gpu_device_` are set once in the constructor and only read afterward, and NVML
    query calls are thread-safe — so there's no shared mutable state to guard, unlike the CPU
    baseline (`prev_cpu_`) which `sample()` mutates under `mutex_`. See `sampleGpu`.
