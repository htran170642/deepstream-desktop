# Phase 7 — Alert Module: Interview Questions

Technical questions covering the concepts Phase 7 exercised (7a: AlertRepository +
AlertManager + AlertService; 7b: desktop AlertClient + AlertPage). Each has a short
answer pointer to the code.

## SQLite repository design

1. **The AlertRepository is touched by two unrelated threads — the persistence worker and the gRPC handler threads. How do you make it safe?**
   Make it *self-serializing*: every public method (`add`/`list`/`snapshot`) takes an
   internal `std::mutex`, so the single SQLite connection is never driven concurrently.
   See [AlertRepository.cpp](../../service/src/AlertRepository.cpp). A `busy_timeout` is a
   second line of defence against `SQLITE_BUSY` from any other writer.

2. **Why not just let each caller lock, like CameraManager does for CameraRepository?**
   CameraRepository has a single gatekeeper (CameraManager owns it privately). AlertRepository
   is *shared* by AlertManager (writer) and AlertServiceImpl (reader) with separate mutexes —
   no single gatekeeper — so the lock must live inside the repository.

3. **`list()` forwards to `list(Filter)`, both public. How do you avoid a double-lock deadlock on a non-recursive mutex?**
   Only the `Filter` overload locks; the no-arg `list()` forwards *without* locking. See
   [AlertRepository.cpp](../../service/src/AlertRepository.cpp) `list()`.

4. **How does the filtered query stay injection-safe while supporting optional filters?**
   Build the `WHERE` clause from only the constrained fields, then bind values positionally in
   the same order — values are always bound, never concatenated. See `list(const Filter&)`.

5. **Why does `list()` deliberately NOT load the snapshot BLOB, with a separate `snapshot(id)` call?**
   Lists/streams stay light (metadata only); the potentially-large JPEG is fetched on demand.
   The list selects `snapshot IS NOT NULL` to report `has_snapshot` without reading the bytes.

## AlertManager — rule + async persistence

6. **What is the alert rule, and how do you stop a high frame rate from flooding the store?**
   Fire when the label is a target and `confidence >= min_confidence`, throttled by a
   per-`(camera_id, label)` cooldown. See [AlertManager.cpp](../../service/src/AlertManager.cpp)
   `shouldFire`.

7. **Persisting on the frame-sink thread blocks Live View. How did you decouple DB I/O from the frame path?**
   `onFrame()` runs only the cheap rule under `mutex_`, then *enqueues* fired alerts; a single
   worker thread does `repo_.add()` + sink. The frame thread returns without touching the DB.
   See `onFrame` / `run`.

8. **Why keep the cooldown decision on the caller thread instead of moving it into the worker?**
   Throttling must reflect the frame's arrival time deterministically and in-order; making it
   async would let persistence latency perturb the cooldown. Only the *side effect* (write +
   notify) is deferred.

9. **A test does `onFrame(...)` then immediately asserts `repo.list().size() == 1`. With async persistence that races. How do you keep tests deterministic without `sleep`?**
   A `flush()` barrier that blocks until `pending_ == 0` (an `idle_cv_` the worker notifies when
   the queue drains). See `flush` and [alert_manager_test.cpp](../../tests/alert_manager_test.cpp).

10. **What bounds the internal queue if the DB stalls?**
    A cap (`kMaxQueue`) with drop-oldest + a warning — a safety valve, not the normal path
    (the cooldown already throttles input). See `enqueue`.

11. **`~AlertManager` drains the queue and calls the sink. What lifetime invariant does that impose, and where did it bite?**
    The sink's target must outlive the manager. In `main` we declare `alert_service` *before*
    `alert_manager` so the shutdown drain doesn't call into a destroyed service; the same
    reordering was needed in the tests. See [main.cpp](../../service/src/main.cpp).

12. **How does the worker loop drain-then-exit cleanly on shutdown?**
    `wait` on `!queue_.empty() || !running_`; process while the queue is non-empty even after
    `running_` is false; return only when woken with an empty queue. See `run`.

## gRPC alert service

13. **Which RPCs are unary and which is streaming, and why?**
    `ListAlerts`/`GetSnapshot` are unary (history query + on-demand image); `StreamAlerts` is
    server-streaming (`returns (stream Alert)`) for live pushes. See
    [alert.proto](../../common/proto/alert.proto).

14. **A live-streamed alert carries no image bytes. How does the desktop still know an image exists?**
    The `has_snapshot` metadata flag. **Gotcha fixed this phase:** the worker must set
    `has_snapshot = !snapshot.empty()` *before* clearing the bytes for the stream, or subscribers
    see `false` for an alert that actually has an image. See [AlertManager.cpp](../../service/src/AlertManager.cpp) `run`.

15. **`broadcastAlert` runs on the AlertManager worker thread; `StreamAlerts` runs per client. How do they meet safely?**
    A `subscribers_mutex_`-guarded registry; `broadcastAlert` pushes a shared proto message into
    each subscriber's bounded queue under the subscriber's own mutex. See
    [AlertServiceImpl.cpp](../../service/src/AlertServiceImpl.cpp).

## Desktop AlertPage (Qt)

16. **The history table, snapshot fetch, and live stream all touch the network. How do you keep the UI thread responsive?**
    Unary calls run on `QtConcurrent::run` with a `QFutureWatcher` finished-slot; the stream runs
    on AlertClient's worker thread and marshals each alert via `QMetaObject::invokeMethod(...,
    QueuedConnection)`. See [AlertPage.cpp](../../desktop/src/pages/AlertPage.cpp).

17. **The user clicks alert A (snapshot fetch starts), then clicks B before it returns. How do you avoid showing A's image on B?**
    Record `snapshot_for_id_` for the in-flight fetch and, in the finished-slot, drop the result
    if the current selection's id no longer matches. See `onSelectionChanged`/`onSnapshotFetched`.

18. **Live alerts prepend to the table forever — what stops unbounded growth, and why doesn't the history path need it?**
    A `kMaxLiveRows` cap that drops the oldest row (model + table in lock-step). The history path
    is already bounded by the query `limit`. See `onLiveAlert`.

19. **Worker lambdas in AlertPage capture the client but never `this`. Why does that matter?**
    They capture a `shared_ptr<AlertClient>` copy (keeps the client alive) but not the page, so a
    late-finishing task can't touch a destroyed widget. Results reach the UI only through the
    QObject-tied watcher/queued-invoke, which Qt discards if the page is gone.

20. **How does the camera-id filter express "any camera" in the UI and on the wire?**
    `QSpinBox` value `0` shows `"Any"` via `setSpecialValueText`, mapping straight to the server's
    `camera_id == 0` "no constraint" semantics. See `AlertPage` constructor + `AlertQuery`.
