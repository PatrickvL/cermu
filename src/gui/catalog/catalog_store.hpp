#pragma once
// ============================================================================
// CatalogStore — SQLite-backed persistent storage for the ROM catalog
// ============================================================================
//
// The catalog stores discovered ROM files and their probe results.
// It is keyed by VFS path + file mtime so that unchanged files are not
// re-probed on subsequent launches.
//
// Entry lifecycle states (§13.1):
//   present       — file exists, probe result current
//   stale         — file mtime changed since last probe (re-probe queued)
//   file_missing  — root configured, file not found
//   root_removed  — source root was de-listed by user
//   orphaned      — file_missing grace period expired
//
// Data flow:
//   CatalogPipeline discovers files → probes → writes entries here.
//   TitleBrowser reads entries + grouping for display.
//
// Thread safety:
//   All public methods are internally serialized (single SQLite connection
//   with explicit locking).  The pipeline's worker threads batch results
//   and post them to the main thread, which calls add/update here.
// ============================================================================

#ifndef CERMU_NO_SQLITE

#include <sqlite3.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>

namespace catalog {

// ============================================================================
// Catalog entry — one row per file
// ============================================================================

enum class EntryState : uint8_t {
    Present     = 0,
    Stale       = 1,
    FileMissing = 2,
    RootRemoved = 3,
    Orphaned    = 4,
};

/// Content fingerprint for rename/relocation detection (§14.2).
struct Fingerprint {
    int64_t  file_size  = 0;
    uint32_t head_crc32 = 0;
    uint32_t tail_crc32 = 0;
};

struct CatalogEntry {
    int64_t     id          = 0;
    std::string vfs_path;               ///< Full VFS path (may include archive)
    std::string filename;               ///< Base filename for display
    int64_t     file_size   = 0;
    int64_t     mtime       = 0;        ///< Last-known file modification time
    int64_t     scan_time   = 0;        ///< When this entry was last probed

    // Probe results
    std::string system_id;              ///< System short_name from probe
    float       confidence  = 0.0f;
    std::string format;                 ///< File format (d64, prg, nes, etc.)

    // Grouping
    std::string title_key;              ///< Normalized key for title grouping
    int         group_id    = 0;        ///< Group this entry belongs to

    // State
    EntryState  state       = EntryState::Present;

    // Fingerprint
    Fingerprint fingerprint;

    // Source root
    std::string source_root;            ///< Which scan root this came from
};

// ============================================================================
// Title group — cluster of entries representing the same game
// ============================================================================

struct TitleGroup {
    int         group_id    = 0;
    std::string title;                  ///< Display title
    std::string system_id;              ///< Primary system
    int         entry_count = 0;
};

// ============================================================================
// CatalogStore — SQLite database wrapper
// ============================================================================

class CatalogStore {
public:
    CatalogStore() = default;
    ~CatalogStore() { close(); }

    // Non-copyable
    CatalogStore(const CatalogStore&) = delete;
    CatalogStore& operator=(const CatalogStore&) = delete;

    // ---- Lifecycle --------------------------------------------------------

    /// Open (or create) the catalog database at the given path.
    bool open(const std::string& db_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (db_) close_unlocked();

        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "CatalogStore: failed to open %s: %s\n",
                    db_path.c_str(), sqlite3_errmsg(db_));
            db_ = nullptr;
            return false;
        }

        // WAL mode for concurrent read performance
        exec("PRAGMA journal_mode=WAL");
        exec("PRAGMA synchronous=NORMAL");

        create_tables();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        close_unlocked();
    }

    bool is_open() const { return db_ != nullptr; }

    /// Default catalog path (~/.config/cermu/catalog.db)
    static std::string default_path() {
        const char* xdg = std::getenv("XDG_DATA_HOME");
        std::string base;
        if (xdg && xdg[0]) {
            base = xdg;
        } else {
            const char* home = std::getenv("HOME");
            if (home) base = std::string(home) + "/.local/share";
            else base = "/tmp";
        }
        return base + "/cermu/catalog.db";
    }

    // ---- Entry operations -------------------------------------------------

    /// Insert or update an entry.  Matched by vfs_path.
    bool upsert_entry(const CatalogEntry& e) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return false;

        const char* sql = R"(
            INSERT INTO entries (
                vfs_path, filename, file_size, mtime, scan_time,
                system_id, confidence, format, title_key, group_id,
                state, fp_size, fp_head_crc32, fp_tail_crc32, source_root
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(vfs_path) DO UPDATE SET
                filename     = excluded.filename,
                file_size    = excluded.file_size,
                mtime        = excluded.mtime,
                scan_time    = excluded.scan_time,
                system_id    = excluded.system_id,
                confidence   = excluded.confidence,
                format       = excluded.format,
                title_key    = excluded.title_key,
                group_id     = excluded.group_id,
                state        = excluded.state,
                fp_size      = excluded.fp_size,
                fp_head_crc32 = excluded.fp_head_crc32,
                fp_tail_crc32 = excluded.fp_tail_crc32,
                source_root  = excluded.source_root
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, e.vfs_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, e.filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, e.file_size);
        sqlite3_bind_int64(stmt, 4, e.mtime);
        sqlite3_bind_int64(stmt, 5, e.scan_time);
        sqlite3_bind_text(stmt, 6, e.system_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 7, static_cast<double>(e.confidence));
        sqlite3_bind_text(stmt, 8, e.format.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, e.title_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 10, e.group_id);
        sqlite3_bind_int(stmt, 11, static_cast<int>(e.state));
        sqlite3_bind_int64(stmt, 12, e.fingerprint.file_size);
        sqlite3_bind_int(stmt, 13, static_cast<int>(e.fingerprint.head_crc32));
        sqlite3_bind_int(stmt, 14, static_cast<int>(e.fingerprint.tail_crc32));
        sqlite3_bind_text(stmt, 15, e.source_root.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    /// Get all entries (optionally filtered by state).
    std::vector<CatalogEntry> get_entries(EntryState* filter_state = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CatalogEntry> result;
        if (!db_) return result;

        std::string sql = "SELECT * FROM entries";
        if (filter_state) {
            sql += " WHERE state = " + std::to_string(static_cast<int>(*filter_state));
        }
        sql += " ORDER BY filename COLLATE NOCASE";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return result;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.push_back(read_entry(stmt));
        }
        sqlite3_finalize(stmt);
        return result;
    }

    /// Get entries under a specific source root.
    std::vector<CatalogEntry> get_entries_by_root(const std::string& root) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CatalogEntry> result;
        if (!db_) return result;

        const char* sql = "SELECT * FROM entries WHERE source_root = ? ORDER BY filename COLLATE NOCASE";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return result;
        sqlite3_bind_text(stmt, 1, root.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.push_back(read_entry(stmt));
        }
        sqlite3_finalize(stmt);
        return result;
    }

    /// Get entries for a specific system.
    std::vector<CatalogEntry> get_entries_by_system(const std::string& system_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CatalogEntry> result;
        if (!db_) return result;

        const char* sql = "SELECT * FROM entries WHERE system_id = ? AND state = 0 ORDER BY filename COLLATE NOCASE";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return result;
        sqlite3_bind_text(stmt, 1, system_id.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.push_back(read_entry(stmt));
        }
        sqlite3_finalize(stmt);
        return result;
    }

    /// Mark entries under a removed scan root as RootRemoved.
    int mark_root_removed(const std::string& root) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return 0;

        const char* sql = "UPDATE entries SET state = ? WHERE source_root = ? AND state != ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_int(stmt, 1, static_cast<int>(EntryState::RootRemoved));
        sqlite3_bind_text(stmt, 2, root.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, static_cast<int>(EntryState::RootRemoved));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return sqlite3_changes(db_);
    }

    /// Check if a path needs re-probing (not in DB or mtime changed).
    bool needs_probe(const std::string& vfs_path, int64_t current_mtime) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return true;

        const char* sql = "SELECT mtime FROM entries WHERE vfs_path = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return true;
        sqlite3_bind_text(stmt, 1, vfs_path.c_str(), -1, SQLITE_TRANSIENT);

        bool result = true;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t stored_mtime = sqlite3_column_int64(stmt, 0);
            result = (stored_mtime != current_mtime);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    /// Get total entry count.
    int count() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return 0;

        const char* sql = "SELECT COUNT(*) FROM entries";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        int n = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            n = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return n;
    }

    /// Get count of entries in a specific state.
    int count_by_state(EntryState state) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return 0;

        const char* sql = "SELECT COUNT(*) FROM entries WHERE state = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_int(stmt, 1, static_cast<int>(state));
        int n = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            n = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return n;
    }

    // ---- Title group operations -------------------------------------------

    /// Get all title groups.
    std::vector<TitleGroup> get_title_groups() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TitleGroup> result;
        if (!db_) return result;

        const char* sql = R"(
            SELECT group_id, title_key, system_id, COUNT(*) as cnt
            FROM entries
            WHERE state = 0 AND group_id > 0
            GROUP BY group_id
            ORDER BY title_key COLLATE NOCASE
        )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return result;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TitleGroup g;
            g.group_id    = sqlite3_column_int(stmt, 0);
            g.title       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            g.system_id   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            g.entry_count = sqlite3_column_int(stmt, 3);
            result.push_back(std::move(g));
        }
        sqlite3_finalize(stmt);
        return result;
    }

    /// Get entries for a specific group.
    std::vector<CatalogEntry> get_group_entries(int group_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CatalogEntry> result;
        if (!db_) return result;

        const char* sql = "SELECT * FROM entries WHERE group_id = ? ORDER BY filename COLLATE NOCASE";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return result;
        sqlite3_bind_int(stmt, 1, group_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.push_back(read_entry(stmt));
        }
        sqlite3_finalize(stmt);
        return result;
    }

    // ---- Fingerprint search -----------------------------------------------

    /// Find entries matching a fingerprint (for relocation detection).
    std::vector<CatalogEntry> find_by_fingerprint(const Fingerprint& fp) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CatalogEntry> result;
        if (!db_) return result;

        const char* sql = R"(
            SELECT * FROM entries
            WHERE fp_size = ? AND fp_head_crc32 = ? AND fp_tail_crc32 = ?
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return result;
        sqlite3_bind_int64(stmt, 1, fp.file_size);
        sqlite3_bind_int(stmt, 2, static_cast<int>(fp.head_crc32));
        sqlite3_bind_int(stmt, 3, static_cast<int>(fp.tail_crc32));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.push_back(read_entry(stmt));
        }
        sqlite3_finalize(stmt);
        return result;
    }

private:
    sqlite3* db_ = nullptr;
    std::mutex mutex_;

    void close_unlocked() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool exec(const char* sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "CatalogStore SQL error: %s\n", err ? err : "unknown");
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    void create_tables() {
        exec(R"(
            CREATE TABLE IF NOT EXISTS entries (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                vfs_path        TEXT NOT NULL UNIQUE,
                filename        TEXT NOT NULL,
                file_size       INTEGER NOT NULL DEFAULT 0,
                mtime           INTEGER NOT NULL DEFAULT 0,
                scan_time       INTEGER NOT NULL DEFAULT 0,
                system_id       TEXT NOT NULL DEFAULT '',
                confidence      REAL NOT NULL DEFAULT 0.0,
                format          TEXT NOT NULL DEFAULT '',
                title_key       TEXT NOT NULL DEFAULT '',
                group_id        INTEGER NOT NULL DEFAULT 0,
                state           INTEGER NOT NULL DEFAULT 0,
                fp_size         INTEGER NOT NULL DEFAULT 0,
                fp_head_crc32   INTEGER NOT NULL DEFAULT 0,
                fp_tail_crc32   INTEGER NOT NULL DEFAULT 0,
                source_root     TEXT NOT NULL DEFAULT ''
            )
        )");

        exec("CREATE INDEX IF NOT EXISTS idx_entries_system ON entries(system_id)");
        exec("CREATE INDEX IF NOT EXISTS idx_entries_state ON entries(state)");
        exec("CREATE INDEX IF NOT EXISTS idx_entries_root ON entries(source_root)");
        exec("CREATE INDEX IF NOT EXISTS idx_entries_group ON entries(group_id)");
        exec("CREATE INDEX IF NOT EXISTS idx_entries_fingerprint ON entries(fp_size, fp_head_crc32, fp_tail_crc32)");
        exec("CREATE INDEX IF NOT EXISTS idx_entries_title ON entries(title_key COLLATE NOCASE)");
    }

    CatalogEntry read_entry(sqlite3_stmt* stmt) {
        CatalogEntry e;
        e.id         = sqlite3_column_int64(stmt, 0);
        e.vfs_path   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        e.filename   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.file_size  = sqlite3_column_int64(stmt, 3);
        e.mtime      = sqlite3_column_int64(stmt, 4);
        e.scan_time  = sqlite3_column_int64(stmt, 5);
        e.system_id  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        e.confidence = static_cast<float>(sqlite3_column_double(stmt, 7));
        e.format     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        e.title_key  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        e.group_id   = sqlite3_column_int(stmt, 10);
        e.state      = static_cast<EntryState>(sqlite3_column_int(stmt, 11));
        e.fingerprint.file_size  = sqlite3_column_int64(stmt, 12);
        e.fingerprint.head_crc32 = static_cast<uint32_t>(sqlite3_column_int(stmt, 13));
        e.fingerprint.tail_crc32 = static_cast<uint32_t>(sqlite3_column_int(stmt, 14));
        e.source_root = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        return e;
    }
};

} // namespace catalog

#endif // CERMU_NO_SQLITE
