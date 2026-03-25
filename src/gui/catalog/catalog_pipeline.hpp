#pragma once
// ============================================================================
// CatalogPipeline — discovery, probing, and grouping of ROM files
// ============================================================================
//
// The pipeline walks configured scan roots, probes discovered files via
// the existing probe infrastructure, and persists results to CatalogStore.
//
// Phases (§13.2):
//   1. Discovery   — walk scan roots, enumerate files  (I/O-bound)
//   2. Probing     — probe_file() on each discovery    (CPU-bound)
//   3. Grouping    — cluster probe results into titles  (in-memory)
//   4. Enrichment  — optional, future (TOSEC/No-Intro)
//
// The pipeline runs asynchronously.  Progress is tracked via atomic
// counters that the UI polls each frame for status display.
// ============================================================================

#ifndef CERMU_NO_SQLITE

#include "gui/catalog/catalog_store.hpp"
#include "gui/scan_root_manager.hpp"
#include "core/formats/format_registry.hpp"
#include "core/formats/format_handler.hpp"
#include "core/system_registry.hpp"
#include "core/system.hpp"

#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <mutex>

namespace catalog {

// ============================================================================
// Pipeline state / progress
// ============================================================================

enum class PipelinePhase : uint8_t {
    Idle,
    Discovery,
    Probing,
    Grouping,
    Done,
};

struct PipelineProgress {
    PipelinePhase phase          = PipelinePhase::Idle;
    int           files_found    = 0;
    int           files_probed   = 0;
    int           files_total    = 0;
    int           groups_created = 0;
    bool          running        = false;
};

// ============================================================================
// CatalogPipeline
// ============================================================================

class CatalogPipeline {
public:
    CatalogPipeline() = default;
    ~CatalogPipeline() { stop(); }

    // Non-copyable
    CatalogPipeline(const CatalogPipeline&) = delete;
    CatalogPipeline& operator=(const CatalogPipeline&) = delete;

    /// Start a full scan of all configured roots.
    void start(scan_roots::ScanRootManager& roots, CatalogStore& store) {
        if (running_.load()) return;

        // Collect root paths
        std::vector<std::string> root_paths;
        for (const auto& r : roots.get_roots()) {
            if (r.exists) root_paths.push_back(r.path);
        }

        if (root_paths.empty()) return;

        running_.store(true);
        phase_.store(PipelinePhase::Discovery);
        files_found_.store(0);
        files_probed_.store(0);
        files_total_.store(0);
        groups_created_.store(0);

        worker_ = std::thread([this, root_paths, &store]() {
            run_pipeline(root_paths, store);
        });
    }

    /// Stop the pipeline (if running).
    void stop() {
        cancel_.store(true);
        if (worker_.joinable()) worker_.join();
        cancel_.store(false);
        running_.store(false);
    }

    /// Current progress (call from main thread).
    PipelineProgress get_progress() const {
        PipelineProgress p;
        p.phase          = phase_.load();
        p.files_found    = files_found_.load();
        p.files_probed   = files_probed_.load();
        p.files_total    = files_total_.load();
        p.groups_created = groups_created_.load();
        p.running        = running_.load();
        return p;
    }

    bool is_running() const { return running_.load(); }

private:
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<PipelinePhase> phase_{PipelinePhase::Idle};
    std::atomic<int>  files_found_{0};
    std::atomic<int>  files_probed_{0};
    std::atomic<int>  files_total_{0};
    std::atomic<int>  groups_created_{0};

    // ---- Main pipeline function (runs on worker thread) -------------------

    void run_pipeline(const std::vector<std::string>& roots, CatalogStore& store) {
        // Phase 1: Discovery
        phase_.store(PipelinePhase::Discovery);
        std::vector<DiscoveredFile> discovered;

        for (const auto& root : roots) {
            if (cancel_.load()) break;
            discover_files(root, root, discovered);
        }

        files_total_.store(static_cast<int>(discovered.size()));

        if (cancel_.load()) { finish(); return; }

        // Phase 2: Probing
        phase_.store(PipelinePhase::Probing);

        for (auto& df : discovered) {
            if (cancel_.load()) break;

            // Skip if already probed and mtime unchanged
            if (!store.needs_probe(df.vfs_path, df.mtime)) {
                files_probed_.fetch_add(1);
                continue;
            }

            probe_and_store(df, store);
            files_probed_.fetch_add(1);
        }

        if (cancel_.load()) { finish(); return; }

        // Phase 3: Grouping
        phase_.store(PipelinePhase::Grouping);
        assign_groups(store);

        finish();
    }

    void finish() {
        phase_.store(PipelinePhase::Done);
        running_.store(false);
    }

    // ---- Discovery --------------------------------------------------------

    struct DiscoveredFile {
        std::string vfs_path;
        std::string filename;
        int64_t     file_size;
        int64_t     mtime;
        std::string source_root;
    };

    void discover_files(const std::string& dir, const std::string& root,
                        std::vector<DiscoveredFile>& out) {
        namespace fs = std::filesystem;
        std::error_code ec;

        for (auto it = fs::recursive_directory_iterator(
                 dir, fs::directory_options::skip_permission_denied, ec);
             it != fs::end(it); ++it)
        {
            if (cancel_.load()) return;
            if (ec) { it.increment(ec); continue; }

            if (!it->is_regular_file(ec)) continue;
            if (ec) continue;

            const auto& path = it->path();
            std::string ext = path.extension().string();

            // Only consider files with known ROM/image extensions
            if (!is_rom_extension(ext)) continue;

            auto ftime = fs::last_write_time(path, ec);
            int64_t mtime = 0;
            if (!ec) {
                mtime = std::chrono::duration_cast<std::chrono::seconds>(
                    ftime.time_since_epoch()).count();
            }

            int64_t fsize = static_cast<int64_t>(it->file_size(ec));

            DiscoveredFile df;
            df.vfs_path    = path.string();
            df.filename    = path.filename().string();
            df.file_size   = fsize;
            df.mtime       = mtime;
            df.source_root = root;

            out.push_back(std::move(df));
            files_found_.fetch_add(1);
        }
    }

    /// Check if a file extension is a known ROM/disk image format.
    static bool is_rom_extension(const std::string& ext) {
        if (ext.empty()) return false;
        // Convert to lowercase
        std::string lext = ext;
        for (auto& c : lext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // Common ROM/image extensions across all supported systems
        static const char* known_exts[] = {
            ".d64", ".t64", ".prg", ".p00", ".tap", ".crt",
            ".d71", ".d81", ".g64", ".g71",
            ".nes", ".fds", ".unf", ".unif",
            ".a26", ".bin", ".rom",
            ".ch8", ".c8", ".ch16",
            ".sfc", ".smc",
            ".gb", ".gbc", ".gba",
            ".md", ".smd", ".gen",
            ".z80", ".sna", ".tzx",
            ".atr", ".xex", ".atx",
            ".dsk", ".nib", ".do", ".po",
            ".zip", ".7z", ".gz",
            nullptr
        };

        for (const char** p = known_exts; *p; ++p) {
            if (lext == *p) return true;
        }
        return false;
    }

    // ---- Probing ----------------------------------------------------------

    void probe_and_store(const DiscoveredFile& df, CatalogStore& store) {
        CatalogEntry entry;
        entry.vfs_path    = df.vfs_path;
        entry.filename    = df.filename;
        entry.file_size   = df.file_size;
        entry.mtime       = df.mtime;
        entry.scan_time   = static_cast<int64_t>(std::time(nullptr));
        entry.source_root = df.source_root;
        entry.state       = EntryState::Present;

        // Read file data for probing
        size_t data_size = 0;
        uint8_t* data = format_read_entire_file(df.vfs_path.c_str(), &data_size);

        if (data && data_size > 0) {
            // Use the system registry to identify the file
            auto& registry = SystemRegistry::instance();
            auto match = registry.identify_system(df.vfs_path.c_str(), data, data_size);

            if (!match.system_name.empty()) {
                entry.system_id  = match.system_name;
                entry.confidence = match.confidence;
            }

            if (match.matched_format) {
                entry.format = match.matched_format->name;
            } else {
                // Fallback to extension
                std::string ext = std::filesystem::path(df.filename).extension().string();
                if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                entry.format = ext;
            }

            free(data);
        } else {
            // Could not read — store with extension only
            std::string ext = std::filesystem::path(df.filename).extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            entry.format = ext;
        }

        // Compute title key (normalized filename for grouping)
        entry.title_key = normalize_title(df.filename);

        // Compute fingerprint
        entry.fingerprint = compute_fingerprint(df.vfs_path, df.file_size);

        store.upsert_entry(entry);
    }

    // ---- Grouping ---------------------------------------------------------

    void assign_groups(CatalogStore& store) {
        // Simple grouping: entries with the same title_key get the same group_id.
        // A more sophisticated approach could use fingerprints and system matching.
        auto entries = store.get_entries();
        if (entries.empty()) return;

        // Sort by title_key
        std::sort(entries.begin(), entries.end(),
                  [](const CatalogEntry& a, const CatalogEntry& b) {
                      return a.title_key < b.title_key;
                  });

        int next_group = 1;
        std::string current_key;
        int current_group = 0;

        for (auto& e : entries) {
            if (e.title_key.empty()) continue;
            if (e.title_key != current_key) {
                current_key = e.title_key;
                current_group = next_group++;
                groups_created_.fetch_add(1);
            }
            e.group_id = current_group;
            store.upsert_entry(e);
        }
    }

    // ---- Utility ----------------------------------------------------------

    /// Normalize a filename into a title key for grouping.
    /// Strips extensions, region codes, revision markers, format tags.
    static std::string normalize_title(const std::string& filename) {
        // Remove extension
        std::string name = std::filesystem::path(filename).stem().string();

        // Convert to lowercase
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // Remove common suffixes in parentheses/brackets
        // e.g. "(EUR)", "[!]", "(v1.1)", "(Disk 1)", "(Side A)"
        std::string result;
        bool in_paren = false;
        bool in_bracket = false;
        for (size_t i = 0; i < name.size(); i++) {
            char c = name[i];
            if (c == '(' || c == '[') {
                in_paren = (c == '(');
                in_bracket = (c == '[');
                continue;
            }
            if ((in_paren && c == ')') || (in_bracket && c == ']')) {
                in_paren = false;
                in_bracket = false;
                continue;
            }
            if (!in_paren && !in_bracket) {
                result += c;
            }
        }

        // Trim trailing spaces, dots, underscores, hyphens
        while (!result.empty()) {
            char c = result.back();
            if (c == ' ' || c == '.' || c == '_' || c == '-')
                result.pop_back();
            else
                break;
        }

        // Replace separators with spaces
        for (auto& c : result) {
            if (c == '_' || c == '-' || c == '.') c = ' ';
        }

        // Collapse multiple spaces
        std::string collapsed;
        bool prev_space = false;
        for (char c : result) {
            if (c == ' ') {
                if (!prev_space) collapsed += c;
                prev_space = true;
            } else {
                collapsed += c;
                prev_space = false;
            }
        }

        return collapsed;
    }

    /// Compute content fingerprint (§14.2).
    static Fingerprint compute_fingerprint(const std::string& path, int64_t file_size) {
        Fingerprint fp;
        fp.file_size = file_size;

        constexpr int64_t HEAD_BYTES = 64 * 1024;  // 64 KB
        constexpr int64_t TAIL_BYTES = 64 * 1024;

        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return fp;

        // Head CRC32
        {
            int64_t head_len = std::min(file_size, HEAD_BYTES);
            std::vector<uint8_t> buf(static_cast<size_t>(head_len));
            size_t read = fread(buf.data(), 1, buf.size(), f);
            fp.head_crc32 = crc32_compute(buf.data(), read);
        }

        // Tail CRC32
        if (file_size > HEAD_BYTES) {
            int64_t tail_start = file_size - std::min(file_size, TAIL_BYTES);
            fseek(f, static_cast<long>(tail_start), SEEK_SET);
            int64_t tail_len = file_size - tail_start;
            std::vector<uint8_t> buf(static_cast<size_t>(tail_len));
            size_t read = fread(buf.data(), 1, buf.size(), f);
            fp.tail_crc32 = crc32_compute(buf.data(), read);
        } else {
            fp.tail_crc32 = fp.head_crc32;
        }

        fclose(f);
        return fp;
    }

    /// Simple CRC32 implementation (standard polynomial).
    static uint32_t crc32_compute(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        return ~crc;
    }
};

} // namespace catalog

#endif // CERMU_NO_SQLITE
