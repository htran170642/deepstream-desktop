#include "CameraManager.hpp"

#include <stdexcept>

namespace dsd {
namespace {

// Validates fields common to add/update. Throws std::invalid_argument on failure.
void validate(const model::Camera& camera) {
    if (camera.name.empty()) {
        throw std::invalid_argument("camera name must not be empty");
    }
    if (camera.rtsp_url.empty()) {
        throw std::invalid_argument("camera rtsp_url must not be empty");
    }
}

}  // namespace

CameraManager::CameraManager(const std::string& db_path)
    : repository_(db_path) {}

model::Camera CameraManager::add(const model::Camera& camera) {
    validate(camera);
    std::lock_guard<std::mutex> lock(mutex_);
    return repository_.add(camera);
}

std::vector<model::Camera> CameraManager::list() {
    std::lock_guard<std::mutex> lock(mutex_);
    return repository_.list();
}

std::optional<model::Camera> CameraManager::update(const model::Camera& camera) {
    validate(camera);
    std::lock_guard<std::mutex> lock(mutex_);
    return repository_.update(camera);
}

bool CameraManager::remove(std::int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return repository_.remove(id);
}

}  // namespace dsd
