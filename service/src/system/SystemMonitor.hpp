#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace dsd {

// Aggregate CPU jiffies from /proc/stat's first ("cpu") line.
struct CpuTimes {
    std::uint64_t idle = 0;   // idle + iowait
    std::uint64_t total = 0;  // sum of all fields
};

// Total + used physical memory, in bytes.
struct MemInfo {
    std::int64_t total_bytes = 0;
    std::int64_t used_bytes = 0;
};

// Pure parsers (no I/O) so tests can feed sample /proc strings directly.
std::optional<CpuTimes> parseCpuTimes(const std::string& proc_stat);
std::optional<MemInfo> parseMemInfo(const std::string& proc_meminfo);

// CPU busy percent in [0,100] between two cumulative samples; 0 if no time
// elapsed (or counters went backwards).
double cpuPercent(const CpuTimes& prev, const CpuTimes& cur);

// Samples host CPU% and memory from /proc. CPU% needs two readings, so the
// monitor remembers the previous CpuTimes: the first sample() reports 0% and
// just seeds the baseline. Thread-safe — a mutex guards the stored baseline,
// since gRPC handlers may call it from multiple threads.
class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    struct Sample {
        double cpu_percent = 0.0;
        std::int64_t mem_used_bytes = 0;
        std::int64_t mem_total_bytes = 0;
        bool gpu_available = false;
        double gpu_percent = 0.0;
        std::int64_t gpu_mem_used_bytes = 0;
        std::int64_t gpu_mem_total_bytes = 0;
    };

    Sample sample();

private:
    std::mutex mutex_;
    std::optional<CpuTimes> prev_cpu_;

    void sampleGpu(Sample& out);  // fills GPU fields if available; no-op without NVML

    bool gpu_ready_ = false;
    void* gpu_device_ = nullptr;  // opaque nvmlDevice_t; set only when NVML is on

};

}  // namespace dsd
