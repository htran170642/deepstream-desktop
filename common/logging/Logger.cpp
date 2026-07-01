#include "logging/Logger.hpp"

#include <mutex>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace dsd {
namespace {
// One-time init guard shared by init() and get(). Once the logger is created,
// the pointer is never written again, so post-init get() is just a lock-free
// atomic flag check + shared_ptr copy — no per-call mutex. std::once_flag also
// serializes concurrent first-callers (e.g. a component's constructor thread
// and its worker thread), so spdlog::stdout_color_mt is never called twice for
// the same name (which would throw).
std::once_flag g_init_flag;

std::shared_ptr<spdlog::logger> makeLogger(const std::string& name,
                                           spdlog::level::level_enum level) {
    auto logger = spdlog::stdout_color_mt(name);
    logger->set_level(level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    return logger;
}
}  // namespace

std::shared_ptr<spdlog::logger>& Logger::instance() {
    static std::shared_ptr<spdlog::logger> logger;
    return logger;
}

void Logger::init(const std::string& name, spdlog::level::level_enum level) {
    // First caller wins; a later init() (or the lazy get() default) is a no-op.
    std::call_once(g_init_flag,
                   [&] { instance() = makeLogger(name, level); });
}

std::shared_ptr<spdlog::logger> Logger::get() {
    std::call_once(g_init_flag,
                   [] { instance() = makeLogger("dsd", spdlog::level::info); });
    return instance();  // never rewritten post-init: lock-free copy
}

}  // namespace dsd
