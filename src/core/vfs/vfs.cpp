/**
 * Virtual File System (VFS) — Implementation
 *
 * ZIP archive support via miniz.  Designed for easy extension to other
 * archive formats.
 */

#include "vfs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>

// --- Platform includes for directory listing ---
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#else
#include <windows.h>
#endif

// --- miniz (ZIP support) ---
#include <miniz.h>

// ============================================================================
// Internal Helpers
// ============================================================================

namespace {

/**
 * Read a plain filesystem file.  Returns malloc'd buffer, caller frees.
 */
uint8_t* read_real_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return nullptr; }

    auto* buf = static_cast<uint8_t*>(malloc(static_cast<size_t>(sz)));
    if (!buf) { fclose(f); return nullptr; }

    if (fread(buf, 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
        free(buf);
        fclose(f);
        return nullptr;
    }

    fclose(f);
    *out_size = static_cast<size_t>(sz);
    return buf;
}

/** Case-insensitive extension compare (both must include the dot). */
bool ext_match_ci(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) !=
            tolower(static_cast<unsigned char>(*b)))
            return false;
        a++; b++;
    }
    return *a == *b;
}

/** Known archive extensions.  Extend this list as backends are added. */
static const char* s_archive_extensions[] = {
    ".zip",
    // Future: ".7z", ".tar", ".tar.gz", ".tgz", ".tar.bz2", ".rar"
    nullptr
};

/** Find the extension in a filename (returns pointer to the dot, or nullptr). */
const char* find_extension(const char* path) {
    const char* dot = nullptr;
    const char* sep = path;
    // Walk to the last path component, then find the last dot
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') sep = p + 1;
    }
    for (const char* p = sep; *p; ++p) {
        if (*p == '.') dot = p;
    }
    return dot;
}

/**
 * Extract a single file from a ZIP archive in memory.
 * Returns malloc'd buffer, caller frees.  NULL on failure.
 */
uint8_t* zip_extract_from_memory(const uint8_t* zip_data, size_t zip_size,
                                  const char* entry_name, size_t* out_size) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zip_data, zip_size, 0)) {
        return nullptr;
    }

    // Try exact match first
    int file_index = mz_zip_reader_locate_file(&zip, entry_name, nullptr, 0);
    if (file_index < 0) {
        // Try case-insensitive match
        file_index = mz_zip_reader_locate_file(&zip, entry_name, nullptr,
                                                MZ_ZIP_FLAG_CASE_SENSITIVE);
        if (file_index < 0) {
            // Brute-force case-insensitive search
            int num = static_cast<int>(mz_zip_reader_get_num_files(&zip));
            for (int i = 0; i < num; ++i) {
                char fname[512];
                mz_zip_reader_get_filename(&zip, static_cast<mz_uint>(i),
                                           fname, sizeof(fname));
                // Case-insensitive compare
                if (strlen(fname) == strlen(entry_name)) {
                    bool match = true;
                    for (size_t j = 0; j < strlen(entry_name); ++j) {
                        if (tolower(static_cast<unsigned char>(fname[j])) !=
                            tolower(static_cast<unsigned char>(entry_name[j]))) {
                            match = false;
                            break;
                        }
                    }
                    if (match) { file_index = i; break; }
                }
            }
        }
    }

    if (file_index < 0) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(file_index), &stat)) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    size_t uncomp_size = static_cast<size_t>(stat.m_uncomp_size);
    auto* buf = static_cast<uint8_t*>(malloc(uncomp_size));
    if (!buf) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    if (!mz_zip_reader_extract_to_mem(&zip, static_cast<mz_uint>(file_index),
                                       buf, uncomp_size, 0)) {
        free(buf);
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    mz_zip_reader_end(&zip);
    *out_size = uncomp_size;
    return buf;
}

/**
 * Extract a single file from a ZIP file on disk.
 * Returns malloc'd buffer, caller frees.  NULL on failure.
 */
uint8_t* zip_extract_from_file(const char* zip_path, const char* entry_name,
                                size_t* out_size) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        return nullptr;
    }

    // Try exact match first, then case-insensitive
    int file_index = mz_zip_reader_locate_file(&zip, entry_name, nullptr, 0);
    if (file_index < 0) {
        // Brute-force case-insensitive search
        int num = static_cast<int>(mz_zip_reader_get_num_files(&zip));
        for (int i = 0; i < num; ++i) {
            char fname[512];
            mz_zip_reader_get_filename(&zip, static_cast<mz_uint>(i),
                                       fname, sizeof(fname));
            if (strlen(fname) == strlen(entry_name)) {
                bool match = true;
                for (size_t j = 0; j < strlen(entry_name); ++j) {
                    if (tolower(static_cast<unsigned char>(fname[j])) !=
                        tolower(static_cast<unsigned char>(entry_name[j]))) {
                        match = false;
                        break;
                    }
                }
                if (match) { file_index = i; break; }
            }
        }
    }

    if (file_index < 0) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(file_index), &stat)) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    size_t uncomp_size = static_cast<size_t>(stat.m_uncomp_size);
    auto* buf = static_cast<uint8_t*>(malloc(uncomp_size));
    if (!buf) {
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    if (!mz_zip_reader_extract_to_mem(&zip, static_cast<mz_uint>(file_index),
                                       buf, uncomp_size, 0)) {
        free(buf);
        mz_zip_reader_end(&zip);
        return nullptr;
    }

    mz_zip_reader_end(&zip);
    *out_size = uncomp_size;
    return buf;
}

/**
 * List entries in a ZIP archive from memory.
 * If subpath is non-empty, only lists entries under that directory.
 */
std::vector<VfsEntry> zip_list_from_memory(const uint8_t* zip_data, size_t zip_size,
                                            const char* subpath,
                                            const char* base_vfs_path) {
    std::vector<VfsEntry> result;
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zip_data, zip_size, 0))
        return result;

    std::string prefix = subpath ? subpath : "";
    // Normalize: ensure trailing slash for subdir matching
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    // For root listing, prefix is empty

    int num = static_cast<int>(mz_zip_reader_get_num_files(&zip));
    // Track directories we've already emitted
    std::vector<std::string> seen_dirs;

    for (int i = 0; i < num; ++i) {
        char fname[512];
        mz_zip_reader_get_filename(&zip, static_cast<mz_uint>(i),
                                   fname, sizeof(fname));
        std::string name(fname);

        // Skip entries not under our prefix
        if (!prefix.empty()) {
            if (name.size() <= prefix.size()) continue;
            if (name.compare(0, prefix.size(), prefix) != 0) continue;
        }

        // Get the part after the prefix
        std::string relative = name.substr(prefix.size());
        if (relative.empty()) continue;

        // Check if this is a direct child or nested
        size_t slash_pos = relative.find('/');
        bool is_dir = mz_zip_reader_is_file_a_directory(&zip, static_cast<mz_uint>(i));

        if (slash_pos != std::string::npos && slash_pos < relative.size() - 1) {
            // Nested entry — emit the immediate subdirectory (if not already seen)
            std::string dir_name = relative.substr(0, slash_pos);
            bool already = false;
            for (const auto& d : seen_dirs) {
                if (d == dir_name) { already = true; break; }
            }
            if (!already) {
                seen_dirs.push_back(dir_name);
                VfsEntry entry;
                entry.name = dir_name;
                entry.full_path = std::string(base_vfs_path) + VFS_ARCHIVE_DELIMITER +
                                  prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
            continue;
        }

        // Direct child (file or directory entry)
        if (is_dir || (slash_pos == relative.size() - 1)) {
            // Directory entry itself — skip the trailing slash for the name
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
                entry.full_path = std::string(base_vfs_path) + VFS_ARCHIVE_DELIMITER +
                                  prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
        } else {
            // Regular file
            mz_zip_archive_file_stat stat;
            mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(i), &stat);

            VfsEntry entry;
            entry.name = relative;
            entry.full_path = std::string(base_vfs_path) + VFS_ARCHIVE_DELIMITER +
                              prefix + relative;
            entry.size = static_cast<size_t>(stat.m_uncomp_size);

            // Check if this file is itself an archive
            const char* ext = find_extension(relative.c_str());
            entry.type = (ext && vfs_is_archive_extension(ext))
                         ? VfsEntryType::Archive
                         : VfsEntryType::File;

            result.push_back(std::move(entry));
        }
    }

    mz_zip_reader_end(&zip);
    return result;
}

/**
 * List entries in a ZIP archive on disk.
 */
std::vector<VfsEntry> zip_list_from_file(const char* zip_path, const char* subpath) {
    std::vector<VfsEntry> result;
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, zip_path, 0))
        return result;

    std::string prefix = subpath ? subpath : "";
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    int num = static_cast<int>(mz_zip_reader_get_num_files(&zip));
    std::vector<std::string> seen_dirs;

    for (int i = 0; i < num; ++i) {
        char fname[512];
        mz_zip_reader_get_filename(&zip, static_cast<mz_uint>(i),
                                   fname, sizeof(fname));
        std::string name(fname);

        if (!prefix.empty()) {
            if (name.size() <= prefix.size()) continue;
            if (name.compare(0, prefix.size(), prefix) != 0) continue;
        }

        std::string relative = name.substr(prefix.size());
        if (relative.empty()) continue;

        size_t slash_pos = relative.find('/');
        bool is_dir = mz_zip_reader_is_file_a_directory(&zip, static_cast<mz_uint>(i));

        if (slash_pos != std::string::npos && slash_pos < relative.size() - 1) {
            std::string dir_name = relative.substr(0, slash_pos);
            bool already = false;
            for (const auto& d : seen_dirs) {
                if (d == dir_name) { already = true; break; }
            }
            if (!already) {
                seen_dirs.push_back(dir_name);
                VfsEntry entry;
                entry.name = dir_name;
                entry.full_path = std::string(zip_path) + VFS_ARCHIVE_DELIMITER +
                                  prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
            continue;
        }

        if (is_dir || (slash_pos == relative.size() - 1)) {
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
                entry.full_path = std::string(zip_path) + VFS_ARCHIVE_DELIMITER +
                                  prefix + dir_name;
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
                result.push_back(std::move(entry));
            }
        } else {
            mz_zip_archive_file_stat stat;
            mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(i), &stat);

            VfsEntry entry;
            entry.name = relative;
            entry.full_path = std::string(zip_path) + VFS_ARCHIVE_DELIMITER +
                              prefix + relative;
            entry.size = static_cast<size_t>(stat.m_uncomp_size);

            const char* ext = find_extension(relative.c_str());
            entry.type = (ext && vfs_is_archive_extension(ext))
                         ? VfsEntryType::Archive
                         : VfsEntryType::File;

            result.push_back(std::move(entry));
        }
    }

    mz_zip_reader_end(&zip);
    return result;
}

/**
 * List entries in a real filesystem directory.
 */
std::vector<VfsEntry> list_real_directory(const char* dir_path) {
    std::vector<VfsEntry> result;
#ifndef _WIN32
    DIR* dir = opendir(dir_path);
    if (!dir) return result;

    std::string base = dir_path;
    if (!base.empty() && base.back() != '/') base += '/';

    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        if (de->d_name[0] == '.') continue; // Skip . and ..

        VfsEntry entry;
        entry.name = de->d_name;

        std::string full = base + de->d_name;
        entry.full_path = full;

        struct stat st;
        if (stat(full.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                entry.type = VfsEntryType::Directory;
                entry.size = 0;
            } else {
                entry.size = static_cast<size_t>(st.st_size);
                const char* ext = find_extension(de->d_name);
                entry.type = (ext && vfs_is_archive_extension(ext))
                             ? VfsEntryType::Archive
                             : VfsEntryType::File;
            }
        } else {
            entry.type = VfsEntryType::File;
            entry.size = 0;
        }

        result.push_back(std::move(entry));
    }
    closedir(dir);
#else
    std::string pattern = std::string(dir_path) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return result;

    std::string base = dir_path;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';

    do {
        if (fd.cFileName[0] == '.') continue;

        VfsEntry entry;
        entry.name = fd.cFileName;
        entry.full_path = base + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            entry.type = VfsEntryType::Directory;
            entry.size = 0;
        } else {
            entry.size = static_cast<size_t>(
                (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow);
            const char* ext = find_extension(fd.cFileName);
            entry.type = (ext && vfs_is_archive_extension(ext))
                         ? VfsEntryType::Archive
                         : VfsEntryType::File;
        }

        result.push_back(std::move(entry));
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#endif

    // Sort: directories first, then alphabetically
    std::sort(result.begin(), result.end(), [](const VfsEntry& a, const VfsEntry& b) {
        bool a_dir = (a.type == VfsEntryType::Directory);
        bool b_dir = (b.type == VfsEntryType::Directory);
        if (a_dir != b_dir) return a_dir;
        return a.name < b.name;
    });

    return result;
}

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
        return read_real_file(path, out_size);
    }

    // Parse the outermost archive path
    VfsPathComponents parts = vfs_parse_path(path);

    // Check for nesting: does archive_path itself contain "!/"?
    const char* nested_delim = strstr(parts.archive_path.c_str(), VFS_ARCHIVE_DELIMITER);
    if (nested_delim) {
        // Nested case: outer.zip!/inner.zip!/file.nes
        // Split into: inner archive name + remaining path
        std::string inner_archive_name(parts.archive_path.c_str(), nested_delim);
        std::string remaining(nested_delim + VFS_ARCHIVE_DELIMITER_LEN);

        // Extract the inner archive from the outer archive
        size_t inner_size = 0;
        uint8_t* inner_data = zip_extract_from_file(parts.real_path.c_str(),
                                                     inner_archive_name.c_str(),
                                                     &inner_size);
        if (!inner_data) return nullptr;

        // Now recursively read from the inner archive
        // If remaining still has "!/", recurse further
        const char* more_nested = strstr(remaining.c_str(), VFS_ARCHIVE_DELIMITER);
        uint8_t* result;
        if (more_nested) {
            // Build a virtual path with the remaining nesting
            // We need to read from the in-memory inner archive, which may itself
            // contain another archive.
            std::string inner_entry(remaining.c_str(), more_nested);
            std::string deeper_remaining(more_nested + VFS_ARCHIVE_DELIMITER_LEN);

            // Extract the next level archive from inner_data
            size_t next_size = 0;
            uint8_t* next_data = vfs_read_from_memory_archive(
                inner_data, inner_size, inner_entry.c_str(), &next_size);
            free(inner_data);

            if (!next_data) return nullptr;

            // Continue recursion with the deeper archive
            // For simplicity, handle one more level here; true deep nesting
            // would need a recursive memory-archive reader
            result = vfs_read_from_memory_archive(
                next_data, next_size, deeper_remaining.c_str(), out_size);
            free(next_data);
        } else {
            // Last level: extract from the in-memory inner archive
            result = vfs_read_from_memory_archive(
                inner_data, inner_size, remaining.c_str(), out_size);
            free(inner_data);
        }
        return result;
    }

    // Simple case: archive.zip!/file.nes
    return zip_extract_from_file(parts.real_path.c_str(),
                                  parts.archive_path.c_str(),
                                  out_size);
}

uint8_t* vfs_read_from_memory_archive(const uint8_t* archive_data,
                                       size_t archive_size,
                                       const char* entry_path,
                                       size_t* out_size) {
    if (!archive_data || !entry_path || !out_size) return nullptr;
    *out_size = 0;
    return zip_extract_from_memory(archive_data, archive_size, entry_path, out_size);
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
        const char* nested = strstr(parts.archive_path.c_str(), VFS_ARCHIVE_DELIMITER);
        if (nested) {
            // Extract the intermediate archive and list from memory
            std::string inner_name(parts.archive_path.c_str(), nested);
            std::string remaining(nested + VFS_ARCHIVE_DELIMITER_LEN);

            size_t inner_size = 0;
            uint8_t* inner_data = zip_extract_from_file(
                parts.real_path.c_str(), inner_name.c_str(), &inner_size);
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

        return zip_list_from_file(parts.real_path.c_str(),
                                   parts.archive_path.c_str());
    }

    // Check if path is a regular file that happens to be an archive
    // (user passed "games.zip" without the "!/" suffix)
#ifndef _WIN32
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        const char* ext = find_extension(path);
        if (ext && vfs_is_archive_extension(ext)) {
            return zip_list_from_file(path, nullptr);
        }
    }
#else
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        const char* ext = find_extension(path);
        if (ext && vfs_is_archive_extension(ext)) {
            return zip_list_from_file(path, nullptr);
        }
    }
#endif

    // Real filesystem directory
    return list_real_directory(path);
}

std::vector<VfsEntry> vfs_list_entries_from_memory(const uint8_t* archive_data,
                                                    size_t archive_size,
                                                    const char* subpath) {
    return zip_list_from_memory(archive_data, archive_size,
                                subpath, "(memory)");
}

// ============================================================================
// Public API — Queries
// ============================================================================

bool vfs_exists(const char* path) {
    if (!path) return false;

    if (vfs_is_archive_path(path)) {
        // Try to read it — if we get data, it exists
        size_t size = 0;
        uint8_t* data = vfs_read_file(path, &size);
        if (data) {
            free(data);
            return true;
        }
        return false;
    }

    // Real filesystem
#ifndef _WIN32
    struct stat st;
    return stat(path, &st) == 0;
#else
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#endif
}

bool vfs_is_archive_extension(const char* ext) {
    if (!ext) return false;
    for (const char** p = s_archive_extensions; *p; ++p) {
        if (ext_match_ci(ext, *p)) return true;
    }
    return false;
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

    // Find last path separator
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

    // If base is already an archive path (has "!/"), append through the archive
    if (vfs_is_archive_path(base.c_str())) {
        // base = "foo.zip!/subdir", entry = "file.nes"
        // result = "foo.zip!/subdir/file.nes"
        std::string result = base;
        if (result.back() != '/') result += '/';
        result += entry;
        return result;
    }

    // Check if base is an archive file — if so, use "!/" delimiter
    const char* ext = find_extension(base.c_str());
    if (ext && vfs_is_archive_extension(ext)) {
        return base + VFS_ARCHIVE_DELIMITER + entry;
    }

    // Regular filesystem path join
    std::string result = base;
    if (result.back() != '/' && result.back() != '\\') result += '/';
    result += entry;
    return result;
}

// ============================================================================
// Public API — Archive Scanning / System Detection
// ============================================================================

VfsArchiveScan vfs_scan_archive(const char* archive_path) {
    VfsArchiveScan scan;
    scan.archive_path = archive_path ? archive_path : "";
    scan.confidence = 0.0f;

    if (!archive_path) return scan;

    // List all entries in the archive (recursing into subdirectories)
    auto entries = vfs_list_entries(archive_path);

    // Collect files with known emulator format extensions
    // Extension → system mapping for common ROM formats
    struct ExtSystemMap {
        const char* ext;
        const char* system;
        float       confidence;
    };
    static const ExtSystemMap ext_map[] = {
        // NES
        { ".nes",  "NES",   0.95f },
        { ".nsf",  "NES",   0.90f },
        { ".fds",  "NES",   0.85f },
        // Commodore 64
        { ".prg",  "C64",   0.70f },  // Could also be VIC-20/C16
        { ".d64",  "C64",   0.90f },
        { ".t64",  "C64",   0.85f },
        { ".tap",  "C64",   0.75f },  // Also VIC-20
        { ".sid",  "C64",   0.95f },
        { ".crt",  "C64",   0.90f },
        // VIC-20
        { ".a0",   "VIC20", 0.90f },  // VIC-20 autostart ROM
        // CHIP-8
        { ".ch8",  "CHIP8", 0.95f },
        { ".c8",   "CHIP8", 0.90f },
        // Commodore 264 series
        { ".lnx",  "C16",   0.85f },  // Lynx archive (C16/Plus4)
        { nullptr,  nullptr,  0.0f }
    };

    // Tally votes per system
    struct SystemVote {
        std::string name;
        float       total_confidence;
        int         file_count;
    };
    std::vector<SystemVote> votes;

    auto find_or_add = [&](const char* sys) -> SystemVote& {
        for (auto& v : votes) {
            if (v.name == sys) return v;
        }
        votes.push_back({sys, 0.0f, 0});
        return votes.back();
    };

    for (const auto& entry : entries) {
        if (entry.type == VfsEntryType::Directory) continue;

        const char* ext = find_extension(entry.name.c_str());
        if (!ext) continue;

        for (const ExtSystemMap* m = ext_map; m->ext; ++m) {
            if (ext_match_ci(ext, m->ext)) {
                scan.loadable_files.push_back(entry);
                auto& vote = find_or_add(m->system);
                vote.total_confidence += m->confidence;
                vote.file_count++;
                break;
            }
        }
    }

    // Pick the system with highest total confidence
    float best = 0.0f;
    for (const auto& v : votes) {
        if (v.total_confidence > best) {
            best = v.total_confidence;
            scan.suggested_system = v.name;
            scan.confidence = best / static_cast<float>(v.file_count); // Average
        }
    }

    return scan;
}
