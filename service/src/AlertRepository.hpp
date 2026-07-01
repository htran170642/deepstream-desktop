#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "pipeline/Alert.hpp"

// Forward-declare the opaque SQLite handle so this header doesn't pull in
// <sqlite3.h>. The .cpp includes the real header.
struct sqlite3;

namespace dsd {

// SQLite-backed storage for alerts. Owns the database connection (RAII) and
// ensures the schema exists on construction. Self-serializing: every public
// method locks an internal mutex, so the repository is safe to share across
// threads (the pipeline worker calling add() while gRPC handler threads call
// list()/snapshot()). A busy_timeout adds a second line of defence.
class AlertRepository {
public:
    // Optional query constraints; a default-constructed Filter matches all.
    // 0 / empty means "no constraint on that field".
    struct Filter {
        std::int64_t camera_id = 0;
        std::string label;           // substring match (SQL LIKE)
        std::int64_t from_ms = 0;    // inclusive lower bound on timestamp_ms
        std::int64_t to_ms = 0;      // inclusive upper bound on timestamp_ms
        int limit = 100;             // max rows; <=0 falls back to the default
    };

    // `db_path` is a SQLite filename, or ":memory:" for an in-memory DB (tests).
    // Throws std::runtime_error if the database cannot be opened.
    explicit AlertRepository(const std::string& db_path);
    ~AlertRepository();

    // Non-copyable: it owns a live DB connection.
    AlertRepository(const AlertRepository&) = delete;
    AlertRepository& operator=(const AlertRepository&) = delete;

    // Inserts a new alert (snapshot BLOB included); returns it with the id set.
    model::Alert add(const model::Alert& alert);

    // Returns alerts matching the filter, newest first. The snapshot BLOB is
    // NOT loaded here (kept light for lists/streams); use snapshot().
    std::vector<model::Alert> list();
    std::vector<model::Alert> list(const Filter& filter);

    // Returns the JPEG snapshot for one alert, or std::nullopt if the alert
    // does not exist or has no image.
    std::optional<std::vector<std::uint8_t>> snapshot(std::int64_t id);

private:
    // list() forwards to list(Filter) without locking to avoid re-locking a
    // non-recursive mutex; the Filter overload takes the lock.
    sqlite3* db_ = nullptr;
    std::mutex mutex_;  // serializes all access to db_
};

}  // namespace dsd
