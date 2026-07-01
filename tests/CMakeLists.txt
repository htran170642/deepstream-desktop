find_package(GTest REQUIRED)
include(GoogleTest)

# Pure logic test — no Qt, so it can be auto-discovered at build time.
add_executable(smoke_test
  smoke_test.cpp
)
target_link_libraries(smoke_test PRIVATE dsd_logging GTest::gtest GTest::gtest_main)
gtest_discover_tests(smoke_test)

# UI navigation test. It owns main() (creates QApplication), so we link
# GTest::gtest WITHOUT gtest_main. Registered with add_test (not
# gtest_discover_tests) so the Qt binary is only run at ctest time, with the
# offscreen platform set — avoids needing a display during the build.
add_executable(navigation_test
  navigation_test.cpp
)
target_link_libraries(navigation_test PRIVATE desktop_lib GTest::gtest)
add_test(NAME navigation_test COMMAND navigation_test)
set_tests_properties(navigation_test PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# gRPC round-trip test — Qt-free (service_lib + client lib), auto-discovered.
add_executable(health_roundtrip_test
  health_roundtrip_test.cpp
)
target_link_libraries(health_roundtrip_test PRIVATE
  service_lib
  desktop_client_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(health_roundtrip_test)

# Camera repository test — SQLite CRUD against an in-memory database.
add_executable(camera_repository_test
  camera_repository_test.cpp
)
target_link_libraries(camera_repository_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(camera_repository_test)

# Camera round-trip test — full CameraClient -> gRPC -> service -> SQLite path.
add_executable(camera_roundtrip_test
  camera_roundtrip_test.cpp
)
target_link_libraries(camera_roundtrip_test PRIVATE
  service_lib
  desktop_client_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(camera_roundtrip_test)

# DetectionProcessor unit test — pure logic, auto-discovered.
add_executable(detection_processor_test
  detection_processor_test.cpp
)
target_link_libraries(detection_processor_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(detection_processor_test)

# PipelineManager test — lifecycle via an injected FakePipeline factory.
add_executable(pipeline_manager_test
  pipeline_manager_test.cpp
)
target_link_libraries(pipeline_manager_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(pipeline_manager_test)

# Stream round-trip test — full StreamClient -> gRPC -> StreamService -> pipeline.
add_executable(stream_roundtrip_test
  stream_roundtrip_test.cpp
)
target_link_libraries(stream_roundtrip_test PRIVATE
  service_lib
  desktop_client_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(stream_roundtrip_test)

# Alert repository test — SQLite alerts CRUD + filter + snapshot BLOB.
add_executable(alert_repository_test
  alert_repository_test.cpp
)
target_link_libraries(alert_repository_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(alert_repository_test)

# AlertManager unit test — rule (target/threshold) + cooldown + snapshot copy.
add_executable(alert_manager_test
  alert_manager_test.cpp
)
target_link_libraries(alert_manager_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(alert_manager_test)


# Alert round-trip test — onFrame -> AlertService -> ListAlerts/GetSnapshot/StreamAlerts.
add_executable(alert_roundtrip_test
  alert_roundtrip_test.cpp
)
target_link_libraries(alert_roundtrip_test PRIVATE
  service_lib
  GTest::gtest
  GTest::gtest_main
)
gtest_discover_tests(alert_roundtrip_test)
