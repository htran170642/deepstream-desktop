#include "AlertRepository.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "logging/Logger.hpp"

namespace dsd {
namespace {

// Throws a runtime_error carrying SQLite's last error message.
[[noreturn]] void fail(sqlite3* db, const std::string& what) {
    throw std::runtime_error(what + ": " + sqlite3_errmsg(db));
}

// RAII owner for a prepared statement (finalize on scope exit, even on throw).
using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

StmtPtr prepare(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
        fail(db, "prepare statement");
    }
    return StmtPtr(raw, sqlite3_finalize);
}

constexpr int kDefaultLimit = 100;

}  // namespace

AlertRepository::AlertRepository(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw std::runtime_error("Failed to open database: " + msg);
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS alerts ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " camera_id INTEGER NOT NULL,"
        " label TEXT NOT NULL,"
        " confidence REAL NOT NULL,"
        " timestamp_ms INTEGER NOT NULL,"
        " snapshot BLOB"                       // NULL when there is no image
        ");"
        "CREATE INDEX IF NOT EXISTS idx_alerts_camera_time"
        " ON alerts (camera_id, timestamp_ms)";

    char* err = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        sqlite3_close(db_);
        throw std::runtime_error("Failed to create schema: " + msg);
    }

    // If a write ever contends on the database lock, wait up to 5s for it to
    // clear instead of failing immediately with SQLITE_BUSY. The mutex below
    // already serializes our own connection; this guards against other writers.
    sqlite3_busy_timeout(db_, 5000);

    Logger::get()->info("AlertRepository ready (db: {})", db_path);
}

AlertRepository::~AlertRepository() {
    sqlite3_close(db_);  // sqlite3_close(nullptr) is a safe no-op
}

model::Alert AlertRepository::add(const model::Alert& alert) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto stmt = prepare(
        db_,
        "INSERT INTO alerts (camera_id, label, confidence, timestamp_ms, snapshot)"
        " VALUES (?, ?, ?, ?, ?)");
    sqlite3_bind_int64(stmt.get(), 1, alert.camera_id);
    sqlite3_bind_text(stmt.get(), 2, alert.label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt.get(), 3, alert.confidence);
    sqlite3_bind_int64(stmt.get(), 4, alert.timestamp_ms);
    if (alert.snapshot.empty()) {
        sqlite3_bind_null(stmt.get(), 5);
    } else {
        sqlite3_bind_blob(stmt.get(), 5, alert.snapshot.data(),
                          static_cast<int>(alert.snapshot.size()),
                          SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        fail(db_, "insert alert");
    }

    model::Alert created = alert;
    created.id = sqlite3_last_insert_rowid(db_);
    return created;
}

std::vector<model::Alert> AlertRepository::list() {
    return list(Filter{});  // no lock here — the Filter overload takes it
}

std::vector<model::Alert> AlertRepository::list(const Filter& filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Build the WHERE clause from only the constrained fields, then bind the
    // matching values in the same order. Keeps one query instead of N overloads
    // and stays injection-safe (values are bound, never concatenated).
    std::string sql =
        "SELECT id, camera_id, label, confidence, timestamp_ms,"
        " snapshot IS NOT NULL FROM alerts";
    std::string where;
    const auto add_clause = [&](const char* clause) {
        where += where.empty() ? " WHERE " : " AND ";
        where += clause;
    };
    if (filter.camera_id != 0) add_clause("camera_id = ?");
    if (!filter.label.empty()) add_clause("label LIKE ?");
    if (filter.from_ms != 0)   add_clause("timestamp_ms >= ?");
    if (filter.to_ms != 0)     add_clause("timestamp_ms <= ?");
    sql += where;
    sql += " ORDER BY timestamp_ms DESC, id DESC LIMIT ?";

    auto stmt = prepare(db_, sql);

    int i = 1;  // 1-based bind index, advanced in the same order as above
    if (filter.camera_id != 0) {
        sqlite3_bind_int64(stmt.get(), i++, filter.camera_id);
    }
    std::string like;
    if (!filter.label.empty()) {
        like = "%" + filter.label + "%";
        sqlite3_bind_text(stmt.get(), i++, like.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (filter.from_ms != 0) sqlite3_bind_int64(stmt.get(), i++, filter.from_ms);
    if (filter.to_ms != 0)   sqlite3_bind_int64(stmt.get(), i++, filter.to_ms);
    const int limit = filter.limit > 0 ? filter.limit : kDefaultLimit;
    sqlite3_bind_int(stmt.get(), i++, limit);

    std::vector<model::Alert> alerts;
    int rc;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        model::Alert a;
        a.id = sqlite3_column_int64(stmt.get(), 0);
        a.camera_id = sqlite3_column_int64(stmt.get(), 1);
        a.label = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        a.confidence = static_cast<float>(sqlite3_column_double(stmt.get(), 3));
        a.timestamp_ms = sqlite3_column_int64(stmt.get(), 4);
        a.has_snapshot = sqlite3_column_int(stmt.get(), 5) != 0;
        // snapshot intentionally not loaded here — use snapshot(id).
        alerts.push_back(std::move(a));
    }
    if (rc != SQLITE_DONE) {
        fail(db_, "list alerts");
    }
    return alerts;
}

std::optional<std::vector<std::uint8_t>> AlertRepository::snapshot(std::int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto stmt = prepare(db_, "SELECT snapshot FROM alerts WHERE id = ?");
    sqlite3_bind_int64(stmt.get(), 1, id);

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
        return std::nullopt;  // no alert with that id
    }
    if (rc != SQLITE_ROW) {
        fail(db_, "read snapshot");
    }
    if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
        return std::nullopt;  // alert exists but has no image
    }
    const auto* bytes =
        static_cast<const std::uint8_t*>(sqlite3_column_blob(stmt.get(), 0));
    const int size = sqlite3_column_bytes(stmt.get(), 0);
    return std::vector<std::uint8_t>(bytes, bytes + size);
}

}  // namespace dsd
