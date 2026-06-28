#include "logging/Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace dsd {

std::shared_ptr<spdlog::logger>& Logger::instance() {
    static std::shared_ptr<spdlog::logger> logger;
    return logger;
}

void Logger::init(const std::string& name, spdlog::level::level_enum level) {
    auto& logger = instance();
    if (logger) {
        return;  // Already initialized.
    }
    logger = spdlog::stdout_color_mt(name);
    logger->set_level(level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
}

std::shared_ptr<spdlog::logger> Logger::get() {
    auto& logger = instance();
    if (!logger) {
        init("dsd");  // Lazy default so get() is always safe.
    }
    return logger;
}

}  // namespace dsd
