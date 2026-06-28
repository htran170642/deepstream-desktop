#include <gtest/gtest.h>

#include "logging/Logger.hpp"

TEST(LoggerTest, InitializesAndReturnsLogger) {
    dsd::Logger::init("test");
    auto logger = dsd::Logger::get();
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "test");
}
