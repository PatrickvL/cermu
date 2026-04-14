/**
 * Virtual File System (VFS) — Implementation
 *
 * Pure routing and nesting logic.  All filesystem and archive I/O is
 * delegated to the core OS layer (os/os.h).  This file contains ZERO
 * platform-specific code or archive-library includes.
 */

#include "core/vfs/vfs.hpp"
#include "core/os/os.hpp"

#include <cstring>
#include <algorithm>
#include <mutex>

// ============================================================================
// Internal Helpers
// ============================================================================

namespace {

/**
 * Build a VfsEntry list from raw OsArchiveEntry data, applying subpath
 * filtering and directory deduplication.
 *
 * @param raw_entries    Full flat entry list from os_archive_list*()
 * @param subpath        Only include entries under this prefix ("" for root)
 * @param base_vfs_path  Prefix for full_path construction (the archive path)
 */
std::vector<VfsEntry> build_filtered_listing(
        const std::vector<OsArchiveEntry>& raw_entries,
        const char* subpath,
        const char* base_vfs_path) {
    std::vector<VfsEntry> result;

    std::string prefix = subpath ? subpath : "";
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    std::vector<std::string> seen_dirs;

    for (const auto& raw : raw_entries) {
        const std::string& name = raw.name;

        // Skip entries not under our prefix
        if (!prefix.empty()) {
            if (name.size() <= prefix.size()) continue;
            if (name.compare(0, prefix.size(), prefix) != 0) continue;
        }

        std::string relative = name.substr(prefix.size());
        if (relative.empty()) continue;

        // Check if this is a direct child or nested
        size_t slash_pos = relative.find('/');

        if (slash_pos != std::string::npos && slash_pos < relative.size() - 1) {
            // Nested — emit the immediate subdirectory if not already seen
            std::string dir_name = relative.substr(0, slash_pos);
            bool already = false;
            for (const auto& d : seen_dirs) {
                if (d == dir_name) { already = true; break; }
            }
            if (!already) {
                seen_dirs.push_back(dir_name);
                VfsEntry entry;
                entry.name = dir_name;
                entry.full_path = std::string(base_vfs_path) +
                                  VFS_ARCHIVE_DELIMITER + prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
            continue;
        }

        // Direct child — directory or file
        if (raw.is_dir || (slash_pos == relative.size() - 1)) {
            std::string dir_name = relative;
            if (!dir_name.empty() && dir_name.back() == '/')
                dir_name.pop_back();
            if (dir_name.empty()) continue;

            bool already = false;
            for (const auto& d : seen_dirs) {
                if (d == dir_name) { already = true; break; }
            }
            if (!already) {
                seen_dirs.push_back(dir_name);
                VfsEntry entry;
                entry.name = dir_name;
                entry.full_path = std::string(base_vfs_path) +
                                  VFS_ARCHIVE_DELIMITER + prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
        } else {
            // Regular file
            VfsEntry entry;
            entry.name = relative;
            entry.full_path = std::string(base_vfs_path) +
                              VFS_ARCHIVE_DELIMITER + prefix + relative;
            entry.size = raw.size;

            const char* ext = os_find_extension(relative.c_str());
            entry.type = (ext && vfs_is_archive_extension(ext))
                         ? VfsEntryType::Archive
                         : VfsEntryType::File;

            result.push_back(std::move(entry));
        }
    }

    return result;
}

// ============================================================================
// Archive Cache
//
// Single-slot MRU cache: keeps one archive's raw bytes and entry listing
// in memory so that repeated accesses (browsing directories, extracting
// multiple files) don't re-open and re-parse the archive from disk.
//
// Thread-safe: the GUI thread browses archives while the emulation thread
// may load files concurrently.
// ============================================================================

struct ArchiveCache {
    std::mutex              mutex;
    std::string             archive_path;   // Key: real filesystem path
    std::vector<uint8_t>    raw_data;       // Archive file contents
    std::vector<OsArchiveEntry> entries;    // Pre-scanned flat entry list
    bool                    entries_valid = false;  // entries populated?

    /**
     * Ensure the cache is populated for the given archive path.
     * If already cached, this is a no-op.  Otherwise reads the archive
     * from disk and replaces any previously cached data.
     *
     * Returns true if the cache is now valid for `path`.
     */
    bool ensure(const char* path) {
        if (archive_path == path && !raw_data.empty())
            return true;

        // Evict previous
        archive_path.clear();
        raw_data.clear();
        entries.clear();
        entries_valid = false;

        // Read archive file from disk into memory
        size_t file_size = 0;
        uint8_t* file_data = os_read_file(path, &file_size);
        if (!file_data || file_size == 0) {
            if (file_data) free(file_data);
            return false;
        }

        archive_path = path;
        raw_data.assign(file_data, file_data + file_size);
        free(file_data);
        return true;
    }

    /**
     * Get the flat entry listing, scanning lazily on first access.
     */
    const std::vector<OsArchiveEntry>& get_entries() {
        if (!entries_valid && !raw_data.empty()) {
            entries = os_archive_list_from_memory(raw_data.data(),
                                                   raw_data.size());
            entries_valid = true;
        }
        return entries;
    }

    /**
     * Extract an entry from the cached archive data.
     * Caller must free() the returned buffer.
     */
    uint8_t* extract(const char* entry_name, size_t* out_size) {
        if (raw_data.empty()) return nullptr;
        return os_archive_extract_from_memory(raw_data.data(), raw_data.size(),
                                               entry_name, out_size);
    }

    /**
     * Flush all cached data.
     */
    void flush() {
        archive_path.clear();
        raw_data.clear();
        raw_data.shrink_to_fit();
        entries.clear();
        entries.shrink_to_fit();
        entries_valid = false;
    }
};

static ArchiveCache s_cache;

} // anonymous namespace

// ============================================================================
// Public API — Path Parsing
// ============================================================================

bool vfs_is_archive_path(const char* path) {
    if (!path) return false;
    return strstr(path, VFS_ARCHIVE_DELIMITER) != nullptr;
}

VfsPathComponents vfs_parse_path(const char* path) {
    VfsPathComponents result;
    if (!path) return result;

    const char* delim = strstr(path, VFS_ARCHIVE_DELIMITER);
    if (!delim) {
        result.real_path = path;
        return result;
    }

    result.real_path = std::string(path, delim);
    result.archive_path = std::string(delim + VFS_ARCHIVE_DELIMITER_LEN);
    return result;
}

// ============================================================================
// Public API — File I/O
// ============================================================================

uint8_t* vfs_read_file(const char* path, size_t* out_size) {
    if (!path || !out_size) return nullptr;
    *out_size = 0;

    if (!vfs_is_archive_path(path)) {
        // Plain filesystem read
        return os_read_file(path, out_size);
    }

    // Parse the outermost archive path
    VfsPathComponents parts = vfs_parse_path(path);

    // Check for nesting: does archive_path itself contain "!/"?
    const char* nested_delim = strstr(parts.archive_path.c_str(),
                                       VFS_ARCHIVE_DELIMITER);
    if (nested_delim) {
        // Nested case: outer.zip!/inner.zip!/file.nes
        std::string inner_archive_name(parts.archive_path.c_str(), nested_delim);
        std::string remaining(nested_delim + VFS_ARCHIVE_DELIMITER_LEN);

        // Extract the inner archive from the outer (via cache)
        size_t inner_size = 0;
        uint8_t* inner_data = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_cache.mutex);
            if (s_cache.ensure(parts.real_path.c_str())) {
                inner_data = s_cache.extract(inner_archive_name.c_str(), &inner_size);
            }
        }
        if (!inner_data) {
            // Cache miss or load failed — fall back to direct disk read
            inner_data = os_archive_extract(parts.real_path.c_str(),
                                             inner_archive_name.c_str(),
                                             &inner_size);
        }
        if (!inner_data) return nullptr;

        // Check if remaining still has nesting
        const char* more_nested = strstr(remaining.c_str(), VFS_ARCHIVE_DELIMITER);
        uint8_t* result;
        if (more_nested) {
            std::string next_entry(remaining.c_str(), more_nested);
            std::string deeper_remaining(more_nested + VFS_ARCHIVE_DELIMITER_LEN);

            size_t next_size = 0;
            uint8_t* next_data = os_archive_extract_from_memory(
                inner_data, inner_size, next_entry.c_str(), &next_size);
            free(inner_data);
            if (!next_data) return nullptr;

            result = os_archive_extract_from_memory(
                next_data, next_size, deeper_remaining.c_str(), out_size);
            free(next_data);
        } else {
            result = os_archive_extract_from_memory(
                inner_data, inner_size, remaining.c_str(), out_size);
            free(inner_data);
        }
        return result;
    }

    // Simple case: archive.zip!/file.nes — use cache
    {
        std::lock_guard<std::mutex> lock(s_cache.mutex);
        if (s_cache.ensure(parts.real_path.c_str())) {
            uint8_t* result = s_cache.extract(parts.archive_path.c_str(), out_size);
            if (result) return result;
            // Cache extract failed (e.g. large solid 7z) — fall through
            // to disk-based extraction which may use external tools.
        }
    }

    // Fallback: direct disk extraction (may use 7z CLI for solid archives)
    return os_archive_extract(parts.real_path.c_str(),
                               parts.archive_path.c_str(),
                               out_size);
}

uint8_t* vfs_read_from_memory_archive(const uint8_t* archive_data,
                                       size_t archive_size,
                                       const char* entry_path,
                                       size_t* out_size) {
    if (!archive_data || !entry_path || !out_size) return nullptr;
    *out_size = 0;
    return os_archive_extract_from_memory(archive_data, archive_size,
                                           entry_path, out_size);
}

// ============================================================================
// Public API — Directory / Archive Listing
// ============================================================================

std::vector<VfsEntry> vfs_list_entries(const char* path) {
    if (!path) return {};

    if (vfs_is_archive_path(path)) {
        // Path into an archive: archive.zip!/subdir
        VfsPathComponents parts = vfs_parse_path(path);

        // Check for nesting
        const char* nested = strstr(parts.archive_path.c_str(),
                                     VFS_ARCHIVE_DELIMITER);
        if (nested) {
            std::string inner_name(parts.archive_path.c_str(), nested);
            std::string remaining(nested + VFS_ARCHIVE_DELIMITER_LEN);

            // Extract the inner archive via cache
            size_t inner_size = 0;
            uint8_t* inner_data = nullptr;
            {
                std::lock_guard<std::mutex> lock(s_cache.mutex);
                if (s_cache.ensure(parts.real_path.c_str())) {
                    inner_data = s_cache.extract(inner_name.c_str(), &inner_size);
                }
            }
            if (!inner_data) {
                inner_data = os_archive_extract(
                    parts.real_path.c_str(), inner_name.c_str(), &inner_size);
            }
            if (!inner_data) return {};

            auto result = vfs_list_entries_from_memory(
                inner_data, inner_size, remaining.c_str());
            free(inner_data);

            // Fix up full_path to include the full prefix
            std::string prefix = parts.real_path + VFS_ARCHIVE_DELIMITER + inner_name;
            for (auto& e : result) {
                e.full_path = prefix + VFS_ARCHIVE_DELIMITER +
                    (remaining.empty() ? "" : remaining + "/") + e.name;
            }
            return result;
        }

        // Non-nested: list from cached entry listing
        {
            std::lock_guard<std::mutex> lock(s_cache.mutex);
            if (s_cache.ensure(parts.real_path.c_str())) {
                const auto& raw = s_cache.get_entries();
                return build_filtered_listing(raw, parts.archive_path.c_str(),
                                               parts.real_path.c_str());
            }
        }
        // Fallback
        auto raw = os_archive_list(parts.real_path.c_str());
        return build_filtered_listing(raw, parts.archive_path.c_str(),
                                       parts.real_path.c_str());
    }

    // Check if path is a regular file that happens to be an archive
    if (os_is_regular_file(path)) {
        const char* ext = os_find_extension(path);
        if (ext && vfs_is_archive_extension(ext)) {
            // Listing archive root — use cache
            {
                std::lock_guard<std::mutex> lock(s_cache.mutex);
                if (s_cache.ensure(path)) {
                    const auto& raw = s_cache.get_entries();
                    return build_filtered_listing(raw, nullptr, path);
                }
            }
            auto raw = os_archive_list(path);
            return build_filtered_listing(raw, nullptr, path);
        }
    }

    // Real filesystem directory
    auto dir_entries = os_list_directory(path);

    std::vector<VfsEntry> result;
    result.reserve(dir_entries.size());

    std::string base = path;
    if (!base.empty() && base.back() != '/' && base.back() != '\\')
        base += '/';

    for (auto& de : dir_entries) {
        VfsEntry entry;
        entry.name = de.name;
        entry.full_path = base + de.name;
        entry.size = de.size;

        if (de.is_dir) {
            entry.type = VfsEntryType::Directory;
        } else {
            const char* ext = os_find_extension(de.name.c_str());
            entry.type = (ext && vfs_is_archive_extension(ext))
                         ? VfsEntryType::Archive
                         : VfsEntryType::File;
        }

        result.push_back(std::move(entry));
    }

    // Sort: directories first, then alphabetically
    std::sort(result.begin(), result.end(),
              [](const VfsEntry& a, const VfsEntry& b) {
        bool a_dir = (a.type == VfsEntryType::Directory);
        bool b_dir = (b.type == VfsEntryType::Directory);
        if (a_dir != b_dir) return a_dir;
        return a.name < b.name;
    });

    return result;
}

std::vector<VfsEntry> vfs_list_entries_from_memory(const uint8_t* archive_data,
                                                    size_t archive_size,
                                                    const char* subpath) {
    auto raw = os_archive_list_from_memory(archive_data, archive_size);
    return build_filtered_listing(raw, subpath, "(memory)");
}

// ============================================================================
// Public API — Queries
// ============================================================================

bool vfs_exists(const char* path) {
    if (!path) return false;

    if (vfs_is_archive_path(path)) {
        size_t size = 0;
        uint8_t* data = vfs_read_file(path, &size);
        if (data) {
            free(data);
            return true;
        }
        return false;
    }

    return os_file_exists(path);
}

bool vfs_is_archive_extension(const char* ext) {
    if (!ext) return false;
    for (const char* const* p = os_archive_extensions(); *p; ++p) {
        if (os_extension_match(ext, *p)) return true;
    }
    return false;
}

const char* const* vfs_archive_extensions() {
    return os_archive_extensions();
}

// ============================================================================
// Public API — Path Utilities
// ============================================================================

std::string vfs_filename(const char* path) {
    if (!path) return {};

    // If archive path, get the last component after the final "!/"
    std::string p(path);
    size_t delim_pos = p.rfind(VFS_ARCHIVE_DELIMITER);
    std::string leaf;
    if (delim_pos != std::string::npos) {
        leaf = p.substr(delim_pos + VFS_ARCHIVE_DELIMITER_LEN);
    } else {
        leaf = p;
    }

    size_t sep = leaf.find_last_of("/\\");
    if (sep != std::string::npos) {
        return leaf.substr(sep + 1);
    }
    return leaf;
}

std::string vfs_extension(const char* path) {
    std::string name = vfs_filename(path);
    size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    return name.substr(dot);
}

std::string vfs_join_path(const std::string& base, const std::string& entry) {
    if (base.empty()) return entry;
    if (entry.empty()) return base;

    // If base is already an archive path (has "!/"), append within the archive
    if (vfs_is_archive_path(base.c_str())) {
        std::string result = base;
        if (result.back() != '/') result += '/';
        result += entry;
        return result;
    }

    // Check if base is an archive file — join with "!/" delimiter
    const char* ext = os_find_extension(base.c_str());
    if (ext && vfs_is_archive_extension(ext)) {
        return base + VFS_ARCHIVE_DELIMITER + entry;
    }

    // Regular filesystem path join
    std::string result = base;
    if (result.back() != '/' && result.back() != '\\') result += '/';
    result += entry;
    return result;
}

std::string vfs_parent_path(const char* path) {
    if (!path || !path[0]) return {};

    std::string p(path);

    // If archive path, check for content after the last "!/"
    size_t delim = p.rfind(VFS_ARCHIVE_DELIMITER);
    if (delim != std::string::npos) {
        std::string inside = p.substr(delim + VFS_ARCHIVE_DELIMITER_LEN);
        // Strip trailing slash
        while (!inside.empty() && inside.back() == '/') inside.pop_back();

        size_t sep = inside.find_last_of('/');
        if (sep != std::string::npos) {
            // archive.zip!/sub/file → archive.zip!/sub
            return p.substr(0, delim + VFS_ARCHIVE_DELIMITER_LEN) +
                   inside.substr(0, sep);
        }
        // archive.zip!/file → archive.zip (the archive root)
        return p.substr(0, delim);
    }

    // Plain filesystem path — strip trailing slash, then find last separator
    while (p.size() > 1 && (p.back() == '/' || p.back() == '\\'))
        p.pop_back();

    size_t sep = p.find_last_of("/\\");
    if (sep == std::string::npos) return {};
    if (sep == 0) return "/";
    return p.substr(0, sep);
}

// ============================================================================
// Public API — Archive Cache
// ============================================================================

void vfs_cache_flush() {
    std::lock_guard<std::mutex> lock(s_cache.mutex);
    s_cache.flush();
}
