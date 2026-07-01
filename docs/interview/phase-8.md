# Phase 8 — Notification Module: Interview Questions

Technical questions covering the concepts Phase 8 exercised (8a: NotificationManager +
pure Payloads; 8b: libcurl HTTP/SMTP channels + env factory; 8c: GetStatus/SendTest gRPC +
desktop Settings UI). Each has a short answer pointer to the code.

## Fan-out & async delivery

1. **An alert already streams to the desktop over gRPC. How do you add notifications without changing the AlertManager?**
   Fan out at the boundary, not inside the producer. The alert sink lambda in `main` calls
   **both** `alert_service.broadcastAlert(alert)` and `notification_manager.notify(alert)` —
   a `shared_ptr` copy to the stream, the last reference moved into notifications. See
   [main.cpp](../../service/src/main.cpp) `setAlertSink`. AlertManager keeps its single-sink API.

2. **Network sends are slow. How do you keep a hung Slack/SMTP endpoint from stalling alert persistence?**
   `NotificationManager::notify()` only enqueues; a dedicated worker thread does the blocking
   sends — the same async design as AlertManager. The alert worker returns immediately after
   enqueuing. See [NotificationManager.cpp](../../service/src/notify/NotificationManager.cpp) `run`.

3. **`notify()` is fire-and-forget. What is `flush()` for, then?**
   A test/shutdown barrier: it blocks until `pending_ == 0` (an `idle_cv_` the worker notifies
   when the queue drains), so a test can assert on channel calls without `sleep`. In production
   the destructor's drain covers shutdown; `flush()` is mainly for deterministic tests. See `flush`.

4. **One channel throws mid-batch. What happens to the others?**
   Each channel is dispatched under its own `try/catch` in the worker loop, so a throw (or a
   `false` return) is logged and the remaining channels still receive the alert. See `run`.

5. **What bounds the queue if every channel stalls, and why is the drop path careful about `pending_`?**
   A `kMaxQueue` cap drops the oldest **and decrements `pending_`** for the dropped item —
   otherwise `flush()` would wait forever on a count that includes discarded alerts. See `enqueue`.

## Channels & the pure/transport split

6. **Why are the payload builders (`buildSlackBody`, etc.) separated from the channels that send them?**
   The builders are pure functions of the alert (+ config), so tests assert on the exact wire
   string with no socket; the channel only adds libcurl transport. Same "host-testable vs
   faithful" split as FakePipeline vs DeepStreamPipeline. See
   [Payloads.cpp](../../service/src/notify/Payloads.cpp) and [payloads_test.cpp](../../tests/payloads_test.cpp).

7. **A detection label is `person "A"`. Why doesn't that corrupt the JSON payload?**
   `jsonEscape` handles `"`, `\`, and control chars, and it runs *before* the value is spliced
   into the body. The escape-order (`\\` in the same switch as `"`) stops a backslash from
   escaping the following quote. See `jsonEscape` + the `LabelWithQuoteStaysValidJson` test.

8. **Why do the channels take their config in the constructor instead of reading `getenv` themselves?**
   Env parsing lives only in `ChannelFactory`; injected config makes `enabled()` trivial
   (`!url.empty()`) and lets a test construct a channel with/without config, no environment.
   See [ChannelFactory.cpp](../../service/src/notify/ChannelFactory.cpp) and the channel ctors.

9. **How does a channel report "configured" vs "inert", and who decides not to call it?**
   Each channel's `enabled()` returns whether its required config is present; the factory always
   constructs all four, and the worker (and `sendTest`) skip `!enabled()` ones. See
   [NotificationChannel.hpp](../../service/src/notify/NotificationChannel.hpp).

## libcurl transport

10. **One libcurl detail that matters for correctness off the main thread?**
    `CURLOPT_NOSIGNAL, 1` — libcurl otherwise uses `SIGALRM` for timeouts, which is unsafe on a
    non-main thread. Also `CURLOPT_COPYPOSTFIELDS` so libcurl owns the body copy. See
    [Http.cpp](../../service/src/notify/Http.cpp) `postJson`.

11. **`curl_global_init` is not thread-safe. How is it called exactly once, and what was the review fix?**
    A single `ensureCurlGlobalInit()` in Http.cpp guarded by `std::call_once`, shared by the HTTP
    channels and EmailChannel. **Review fix:** the two files originally had independent once-flags,
    which could call `curl_global_init` twice/concurrently; consolidating to one removed that. See
    `ensureCurlGlobalInit`.

12. **Email uses libcurl too but differently. How is the message delivered?**
    SMTP with an upload read-callback: `readPayload` streams the RFC-5322 message (Date/From/To/
    Subject + CRLF body) in chunks, returning 0 at EOF; `CURLUSESSL_ALL` upgrades to STARTTLS. See
    [EmailChannel.cpp](../../service/src/notify/EmailChannel.cpp).

13. **Why can the `readPayload` subtraction never underflow?**
    `offset` only ever advances by `min(room, remaining)`, so it never exceeds `data->size()`;
    `size() - offset` stays non-negative. See `readPayload`.

## Optional dependency & config

14. **libcurl isn't always present. How does the build stay green without it, mirroring which existing pattern?**
    `find_package(CURL)` (not REQUIRED) gates the channel sources behind `DSD_WITH_CURL`, exactly
    like `ENABLE_DEEPSTREAM`. Without curl, `ChannelFactory` returns an empty list and the service
    still runs. See [service/CMakeLists.txt](../../service/CMakeLists.txt) and `ChannelFactory.cpp` `#else`.

15. **Where do secrets (webhook URLs, tokens, SMTP creds) come from, and how do you keep them out of logs?**
    Environment variables read only in `ChannelFactory`; channels log only `name()` and transport
    errors, never the token/URL. The Telegram bot token is a URL path segment and never logged. See
    `ChannelFactory` and the channel `send()` methods.

## Logger concurrency (review find)

16. **Constructing NotificationManager surfaced a latent crash in Logger. What was it?**
    `Logger::get()` did `if (!logger) init()` with no synchronization; the manager's constructor
    thread and its worker thread both raced to lazily create the `"dsd"` logger, and the second
    `spdlog::stdout_color_mt("dsd")` throws "already exists". See [Logger.cpp](../../common/logging/Logger.cpp).

17. **How is it fixed without paying a mutex on every `get()`?**
    `init()` and `get()` funnel through one `std::once_flag`: the first caller creates the logger,
    all others block once, and after that the pointer never changes — so steady-state `get()` is a
    lock-free atomic flag check plus a `shared_ptr` copy. See `Logger::get`.

## gRPC status/test service

18. **Why is `SendTest` implemented synchronously instead of going through the async queue?**
    The Settings "test" button wants a real per-channel pass/fail, not fire-and-forget. `sendTest()`
    dispatches on the caller (gRPC handler) thread and returns results; it's safe alongside the
    worker because each channel's `send()` is re-entrant (own curl handle, immutable config). See
    [NotificationManager.cpp](../../service/src/notify/NotificationManager.cpp) `sendTest` and
    [NotificationServiceImpl.cpp](../../service/src/NotificationServiceImpl.cpp).

19. **The service and the shutdown drain both reference the manager. What declaration order keeps that safe?**
    `notification_manager` is declared **before** `alert_manager` (so the alert shutdown-drain can
    still call `notify`), and `notification_service` **after** the manager (destroyed first). In the
    round-trip test: `manager → service → server`, so teardown is `server → service → manager`. See
    [main.cpp](../../service/src/main.cpp) and [notification_roundtrip_test.cpp](../../tests/notification_roundtrip_test.cpp).

## Desktop Settings UI

20. **`GetStatus`/`SendTest` touch the network. How does the Settings page stay responsive, and why capture the client not `this`?**
    Both run on `QtConcurrent::run` with a `QFutureWatcher` finished-slot; `SendTest` uses a 15s
    client deadline since the server does real sends. The worker lambdas capture a
    `shared_ptr<NotificationClient>` (keeps the client alive) but never `this`, so a late result
    can't touch a destroyed page — results reach the UI only through the QObject-tied watcher. The
    destructor `waitForFinished()` on in-flight watchers before their result vectors unwind. See
    [SettingsPage.cpp](../../desktop/src/pages/SettingsPage.cpp).
