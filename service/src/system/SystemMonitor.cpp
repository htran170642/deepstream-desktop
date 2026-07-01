#include "system/SystemMonitor.hpp"

#include <fstream>
#include <sstream>

#include "logging/Logger.hpp"

#ifdef DSD_WITH_NVML
#include <nvml.h>
#endif

namespace dsd {
namespace {

std::string readFile(const char* path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

std::optional<CpuTimes> parseCpuTimes(const std::string& proc_stat) {
    std::istringstream in(proc_stat);
    std::string label;
    in >> label;
    if (label != "cpu") return std::nullopt;

    // Fields: user nice system idle iowait irq softirq steal guest guest_nice.
    std::uint64_t vals[10] = {0};
    int n = 0;
    for (; n < 10 && (in >> vals[n]); ++n) {
    }
    if (n < 5) return std::nullopt;  // need at least through iowait

    CpuTimes t;
    for (int i = 0; i < n; ++i) t.total += vals[i];
    t.idle = vals[3] + vals[4];  // idle + iowait
    return t;
}

std::optional<MemInfo> parseMemInfo(const std::string& proc_meminfo) {
    std::istringstream in(proc_meminfo);
    std::int64_t total_kb = -1;
    std::int64_t avail_kb = -1;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        std::int64_t value = 0;
        ls >> key >> value;
        if (key == "MemTotal:") total_kb = value;
        else if (key == "MemAvailable:") avail_kb = value;
    }
    if (total_kb < 0 || avail_kb < 0) return std::nullopt;

    MemInfo m;
    m.total_bytes = total_kb * 1024;
    m.used_bytes = (total_kb - avail_kb) * 1024;  // "used" = total - available
    return m;
}

double cpuPercent(const CpuTimes& prev, const CpuTimes& cur) {
    const std::uint64_t dtotal =
        (cur.total >= prev.total) ? cur.total - prev.total : 0;
    const std::uint64_t didle =
        (cur.idle >= prev.idle) ? cur.idle - prev.idle : 0;
    if (dtotal == 0) return 0.0;
    const double busy =
        static_cast<double>(dtotal - didle) / static_cast<double>(dtotal);
    return busy * 100.0;
}

SystemMonitor::SystemMonitor() {
#ifdef DSD_WITH_NVML
    if (nvmlInit_v2() == NVML_SUCCESS) {
        nvmlDevice_t dev = nullptr;
        if (nvmlDeviceGetHandleByIndex_v2(0, &dev) == NVML_SUCCESS) {
            gpu_device_ = dev;  // nvmlDevice_t is a pointer typedef
            gpu_ready_ = true;
        } else {
            nvmlShutdown();
        }
    }
    if (!gpu_ready_) Logger::get()->info("NVML: no GPU available for stats");
#endif
}

SystemMonitor::~SystemMonitor() {
#ifdef DSD_WITH_NVML
    if (gpu_ready_) nvmlShutdown();
#endif
}

void SystemMonitor::sampleGpu(Sample& out) {
#ifdef DSD_WITH_NVML
    if (!gpu_ready_) return;
    auto dev = static_cast<nvmlDevice_t>(gpu_device_);
    nvmlUtilization_t util{};
    nvmlMemory_t mem{};
    if (nvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS &&
        nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS) {
        out.gpu_available = true;
        out.gpu_percent = static_cast<double>(util.gpu);
        out.gpu_mem_used_bytes = static_cast<std::int64_t>(mem.used);
        out.gpu_mem_total_bytes = static_cast<std::int64_t>(mem.total);
    }
#else
    (void)out;  // no NVML: GPU stays unavailable
#endif
}

SystemMonitor::Sample SystemMonitor::sample() {
    Sample s;
    if (const auto mem = parseMemInfo(readFile("/proc/meminfo"))) {
        s.mem_total_bytes = mem->total_bytes;
        s.mem_used_bytes = mem->used_bytes;
    }
    if (const auto cur = parseCpuTimes(readFile("/proc/stat"))) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (prev_cpu_) s.cpu_percent = cpuPercent(*prev_cpu_, *cur);
        prev_cpu_ = cur;
    }
    sampleGpu(s);
    return s;
}

}  // namespace dsd
