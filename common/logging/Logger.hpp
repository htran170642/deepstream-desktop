#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace dsd {

// Thin wrapper around a process-wide spdlog logger.
class Logger {
public:
    static void init(const std::string& name,
                     spdlog::level::level_enum level = spdlog::level::info);

    static std::shared_ptr<spdlog::logger> get();

private:
    static std::shared_ptr<spdlog::logger>& instance();
};

}  // namespace dsd
