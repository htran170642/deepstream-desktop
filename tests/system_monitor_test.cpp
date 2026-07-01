#include <string>

#include <gtest/gtest.h>

#include "system/SystemMonitor.hpp"

namespace {

TEST(SystemMonitor, ParsesMemInfo) {
    const std::string meminfo =
        "MemTotal:       16384000 kB\n"
        "MemFree:         1000000 kB\n"
        "MemAvailable:    8192000 kB\n"
        "Buffers:          200000 kB\n";
    const auto m = dsd::parseMemInfo(meminfo);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->total_bytes, 16384000LL * 1024);
    EXPECT_EQ(m->used_bytes, (16384000LL - 8192000LL) * 1024);  // total - available
}

TEST(SystemMonitor, ParsesCpuTimes) {
    const auto t = dsd::parseCpuTimes("cpu  100 0 50 800 50 0 0 0 0 0\n");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->total, 1000u);      // 100+0+50+800+50
    EXPECT_EQ(t->idle, 850u);        // idle(800) + iowait(50)
}

TEST(SystemMonitor, CpuPercentFromDelta) {
    // delta total +1000, delta idle +900 -> busy 100/1000 = 10%.
    const dsd::CpuTimes prev{850, 1000};
    const dsd::CpuTimes cur{1750, 2000};
    EXPECT_DOUBLE_EQ(dsd::cpuPercent(prev, cur), 10.0);
}

TEST(SystemMonitor, CpuPercentZeroWhenNoTimeElapsed) {
    const dsd::CpuTimes t{850, 1000};
    EXPECT_DOUBLE_EQ(dsd::cpuPercent(t, t), 0.0);
}

TEST(SystemMonitor, RejectsGarbage) {
    EXPECT_FALSE(dsd::parseCpuTimes("intr 1 2 3").has_value());
    EXPECT_FALSE(dsd::parseMemInfo("Nonsense: 5 kB").has_value());
}

}  // namespace
