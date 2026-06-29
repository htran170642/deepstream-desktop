#include "CameraRepository.hpp"

#include <memory>
#include <stdexcept>
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

StmtPtr prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
        fail(db, "prepare statement");
    }
    return StmtPtr(raw, sqlite3_finalize);
}

}  // namespace

CameraRepository::CameraRepository(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_)  != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw std::runtime_error("Failed to open database: " + msg);
    }

    const char* schema = 
        "CREATE TABLE IF NOT EXISTS cameras ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " rtsp_url TEXT NOT NULL,"
        " enabled INTEGER NOT NULL DEFAULT 0"
        ")";

    char* err = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        sqlite3_close(db_);
        throw std::runtime_error("Failed to create schema: " + msg);
    }

    Logger::get()->info("CameraRepository ready (db: {})", db_path);
} 

CameraRepository::~CameraRepository() {
    sqlite3_close(db_);  // sqlite3_close(nullptr) is a safe no-op
}

model::Camera CameraRepository::add(const model::Camera& camera) {
    auto stmt = prepare(
        db_, "INSERT INTO cameras (name, rtsp_url, enabled) VALUES (?, ?, ?)");
    sqlite3_bind_text(stmt.get(), 1, camera.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, camera.rtsp_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 3, camera.enabled ? 1 : 0);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        fail(db_, "insert camera");
    }

    model::Camera created = camera;
    created.id = sqlite3_last_insert_rowid(db_);
    return created;
}

std::vector<model::Camera> CameraRepository::list() {
    auto stmt =
        prepare(db_, "SELECT id, name, rtsp_url, enabled FROM cameras ORDER BY id");

    std::vector<model::Camera> cameras;
    int rc;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        model::Camera c;
        c.id = sqlite3_column_int64(stmt.get(), 0);
        c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        c.rtsp_url =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        c.enabled = sqlite3_column_int(stmt.get(), 3) != 0;
        cameras.push_back(std::move(c));
    }
    if (rc != SQLITE_DONE) {
        fail(db_, "list cameras");
    }
    return cameras;
}

std::optional<model::Camera> CameraRepository::update(const model::Camera& camera) {
    auto stmt = prepare(
        db_, "UPDATE cameras SET name = ?, rtsp_url = ?, enabled = ? WHERE id = ?");
    sqlite3_bind_text(stmt.get(), 1, camera.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, camera.rtsp_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 3, camera.enabled ? 1 : 0);
    sqlite3_bind_int64(stmt.get(), 4, camera.id);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        fail(db_, "update camera");
    }
    if (sqlite3_changes(db_) == 0) {
        return std::nullopt;  // no row with that id
    }
    return camera;
}

bool CameraRepository::remove(std::int64_t id) {
    auto stmt = prepare(db_, "DELETE FROM cameras WHERE id = ?");
    sqlite3_bind_int64(stmt.get(), 1, id);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        fail(db_, "delete camera");
    }
    return sqlite3_changes(db_) > 0;
}

}  // namespace dsd
