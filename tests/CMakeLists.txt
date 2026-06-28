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

# gRPC round-trip test — Qt-free (server_lib + client lib), so it can be
# auto-discovered like the smoke test.
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
