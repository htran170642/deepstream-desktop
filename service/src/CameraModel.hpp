#pragma once

#include <cstdint>
#include <string>

namespace dsd::model {

// Plain domain model for a camera — independent of protobuf and SQLite types.
// The service layers (repository, manager) work with this; CameraServiceImpl
// maps it to/from the generated dsd::Camera proto message at the gRPC boundary.
struct Camera {
    std::int64_t id = 0;   // 0 until persisted; assigned by SQLite
    std::string name;
    std::string rtsp_url;
    bool enabled = false;
};

}  // namespace dsd::model

