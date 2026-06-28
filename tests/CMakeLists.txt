find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(smoke_test
  smoke_test.cpp
)
target_link_libraries(smoke_test PRIVATE dsd_logging GTest::gtest GTest::gtest_main)

gtest_discover_tests(smoke_test)
