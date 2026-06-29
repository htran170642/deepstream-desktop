#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CameraModel.hpp"

// Forward-declare the opaque SQLite handle so this header doesn't pull in
// <sqlite3.h>. The .cpp includes the real header.
struct sqlite3;

namespace dsd {

// SQLite-backed storage for cameras. Owns the database connection (RAII) and
// ensures the schema exists on construction. Not thread-safe by itself —
// CameraManager serializes access.
class CameraRepository {
public:
    // `db_path` is a SQLite filename, or ":memory:" for an in-memory DB (tests).
    // Throws std::runtime_error if the database cannot be opened.
    explicit CameraRepository(const std::string& db_path);
    ~CameraRepository();

    // Non-copyable: it owns a live DB connection.
    CameraRepository(const CameraRepository&) = delete;
    CameraRepository& operator=(const CameraRepository&) = delete;

    // Inserts a new camera; returns it with the id assigned by SQLite.
    model::Camera add(const model::Camera& camera);

    // Returns all cameras ordered by id.
    std::vector<model::Camera> list();

    // Updates the camera identified by camera.id. Returns the updated camera,
    // or std::nullopt if no row with that id exists.
    std::optional<model::Camera> update(const model::Camera& camera);

    // Deletes the camera with the given id. Returns false if no such row.
    bool remove(std::int64_t id);

private:
    sqlite3* db_ = nullptr;
};

}  // namespace dsd
