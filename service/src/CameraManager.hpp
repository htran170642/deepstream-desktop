#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "CameraModel.hpp"
#include "CameraRepository.hpp"

namespace dsd {

// Business layer for cameras: validates input and serializes all database
// access, since the gRPC server handles requests on multiple threads.
// Owns the repository.
class CameraManager {
public:
    explicit CameraManager(const std::string& db_path);

    // Throws std::invalid_argument if name or rtsp_url is empty.
    model::Camera add(const model::Camera& camera);

    std::vector<model::Camera> list();

    // Throws std::invalid_argument on bad input; returns nullopt if id unknown.
    std::optional<model::Camera> update(const model::Camera& camera);

    bool remove(std::int64_t id);

private:
    std::mutex mutex_;
    CameraRepository repository_;
};

}  // namespace dsd
