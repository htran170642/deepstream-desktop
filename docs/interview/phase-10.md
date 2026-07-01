# Phase 10 — Polish: Interview Questions

Technical questions covering the concepts Phase 10 exercised (refactor/DRY cleanup, test
coverage of validation paths, UI theming). Each has a short answer pointer to the code.

## Refactor & DRY

1. **All six gRPC clients repeated the same channel construction. How did you remove the duplication without coupling them?**
   Extracted two free helpers into a header — `makeChannel(address)` and `setDeadline(ctx,
   timeout)` — that each client calls. No base class or inheritance; just shared functions, so
   the clients stay independent. See [GrpcSupport.hpp](../../desktop/src/client/GrpcSupport.hpp).

2. **Why free `inline` functions in a header rather than a shared base `Client` class?**
   The clients differ in stub type, RPC set, and threading (some stream, some don't); a base
   class would force a false hierarchy. The only real duplication was two stateless one-liners,
   which are exactly what free helpers are for — composition over inheritance (a CLAUDE.md rule).

3. **The build had duplicated CMake entries (CameraServiceImpl listed twice, the DeepStream block twice). Why does that matter and how was it resolved?**
   Duplicate source entries are harmless (CMake dedups) but misleading; a duplicated
   `if(ENABLE_DEEPSTREAM)` block risks the two copies drifting apart. Kept the complete
   GStreamer/DeepStreamPipeline block and deleted the partial one. See
   [service/CMakeLists.txt](../../service/CMakeLists.txt).

## Test coverage

4. **The AlertRepository and camera round-trip were already tested. What gap did `camera_manager_test` fill?**
   The *manager's* contract, not the repository's: that `add()` throws `std::invalid_argument`
   on empty name/url (the "never persist bad input" guard), and that unknown ids yield
   `update()==nullopt` / `remove()==false`. See [camera_manager_test.cpp](../../tests/camera_manager_test.cpp).

5. **Why assert on the *error* paths at all, when the happy path already passes?**
   Validation and not-found handling are where regressions hide — a refactor that drops a guard
   still passes happy-path tests. Pinning the throw + the `nullopt`/`false` branches locks the
   contract. `EXPECT_THROW` / `EXPECT_FALSE` express it directly.

6. **How do these tests stay fast and isolated?**
   Each constructs a `CameraManager(":memory:")` — a private in-memory SQLite DB per test, no
   shared state, no disk, no server. Mirrors the existing repository tests.

## UI theming

7. **How is consistent styling applied across every page without touching each widget?**
   A single global Qt Style Sheet set on the `QApplication` root cascades to all pages, tables,
   buttons, and progress bars. See [main.cpp](../../desktop/src/main.cpp) `app.setStyleSheet`.

8. **The sidebar has a distinct dark style. How is that scoped so it doesn't bleed onto other lists?**
   The stylesheet targets `QListWidget#sidebar`, and `MainWindow` sets `setObjectName("sidebar")`
   on that one list — an id selector, so any other `QListWidget` is unaffected. See
   [MainWindow.cpp](../../desktop/src/MainWindow.cpp).

9. **Why `setApplicationName`/`setApplicationDisplayName` in addition to the window title?**
   The window title is per-window; the application name feeds the window manager/taskbar and the
   `QSettings` storage path, giving the app a proper OS-level identity. See `main.cpp`.
