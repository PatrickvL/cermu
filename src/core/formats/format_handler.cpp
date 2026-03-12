/**
 * Format Handler — Shared utility implementations
 */

#include "format_handler.h"
#include "format_registry.h"
#include "vfs/vfs.h"
#include "../cermu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>

// ============================================================================
// Program Data
// ============================================================================

void program_data_t::release() {
    if (data) {
        free(data);
        data = NULL;
        data_size = 0;
    }
}

// ============================================================================
// Load Result
// ============================================================================

const char* format_load_type_name(format_load_type_t type) {
    switch (type) {
        case FORMAT_LOAD_PROGRAM:  return "PROGRAM";
        case FORMAT_LOAD_ARCHIVE:  return "ARCHIVE";
        case FORMAT_LOAD_METADATA: return "METADATA";
        case FORMAT_LOAD_RAW:      return "RAW";
        case FORMAT_LOAD_ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
}

void format_load_result_t::release() {
    program.release();
    for (int i = 0; i < file_count; i++)
        files[i].release();
    memset(this, 0, sizeof(*this));
}

// ============================================================================
// File I/O
// ============================================================================

/**
 * Try to extract a file from a container format (D64, T64, LNX, …) found in
 * a VFS path.  This handles paths that the standard VFS/libarchive layer
 * cannot resolve because the innermost "archive" is a Commodore container:
 *
 *   /path/archive.zip!/disk.d64!/GAME.prg
 *   /path/disk.d64!/GAME.prg
 *
 * The function finds the last "!/" boundary whose left side is a container
 * format, reads the container via VFS, matches the entry name against the
 * container's directory listing, and extracts via the format's extract_entry()
 * callback.
 *
 * Returns a malloc'd buffer (caller frees) or nullptr on failure.
 */
static uint8_t* format_try_container_extract(const char* filepath, size_t* out_size) {
    if (!filepath || !out_size) return nullptr;
    if (!vfs_is_archive_path(filepath)) return nullptr;

    std::string path(filepath);

    // Find the last "!/" delimiter — the entry name is the tail.
    size_t last_delim = path.rfind("!/");
    if (last_delim == std::string::npos || last_delim == 0) return nullptr;

    std::string container_path = path.substr(0, last_delim);
    std::string entry_name     = path.substr(last_delim + 2);

    if (container_path.empty() || entry_name.empty()) return nullptr;

    // Check if the container has a known container-format extension.
    std::string container_ext = vfs_extension(container_path.c_str());
    if (container_ext.empty()) return nullptr;

    const auto* fmt = FormatRegistry::instance().find_by_extension(container_ext.c_str());
    if (!fmt || !(fmt->capabilities & FORMAT_CAP_CONTAINER)) return nullptr;
    if (!fmt->list_entries || !fmt->extract_entry) return nullptr;

    // Read the container data via VFS (handles archives, nesting, etc.)
    size_t container_size = 0;
    uint8_t* container_data = vfs_read_file(container_path.c_str(), &container_size);
    if (!container_data) return nullptr;

    // List entries and find the one matching the requested name.
    format_container_entry_t entries[FORMAT_CONTAINER_MAX_ENTRIES];
    int count = fmt->list_entries(container_data, container_size,
                                  entries, FORMAT_CONTAINER_MAX_ENTRIES);

    int match_index = -1;
    for (int i = 0; i < count; ++i) {
        if (cermu_strcasecmp(entries[i].display_name, entry_name.c_str()) == 0) {
            match_index = entries[i].index;
            break;
        }
    }

    if (match_index < 0) {
        free(container_data);
        return nullptr;
    }

    // Extract the entry via the format handler.
    uint8_t* entry_data = nullptr;
    size_t   entry_size = 0;
    bool ok = fmt->extract_entry(container_data, container_size,
                                 match_index, &entry_data, &entry_size);
    free(container_data);

    if (!ok || !entry_data) return nullptr;

    *out_size = entry_size;
    return entry_data;
}

uint8_t* format_read_entire_file(const char* filepath, size_t* out_size) {
    // Delegate to VFS — transparently handles both plain files and archive paths
    // (e.g. "game.zip!/rom.nes")
    uint8_t* data = vfs_read_file(filepath, out_size);
    if (data) return data;

    // VFS/libarchive couldn't resolve the path — try extracting from a
    // container format (D64, T64, LNX, …) that may be in the path.
    // This handles paths like "archive.zip!/disk.d64!/GAME.prg" where
    // the D64 is not a standard archive format.
    return format_try_container_extract(filepath, out_size);
}

std::string format_effective_extension(const char* filepath) {
    std::string ext = vfs_extension(filepath);
    if (!ext.empty()) return ext;

    // Files inside Commodore container formats (D64, T64, LNX) have no
    // filename extension.  Infer .prg from the parent container type.
    if (!filepath || !vfs_is_archive_path(filepath)) return ext;

    std::string path(filepath);
    size_t last_delim = path.rfind("!/");
    if (last_delim == std::string::npos) return ext;

    std::string container_ext = vfs_extension(path.substr(0, last_delim).c_str());
    if (format_ext_match(container_ext.c_str(), ".d64") ||
        format_ext_match(container_ext.c_str(), ".t64") ||
        format_ext_match(container_ext.c_str(), ".lnx"))
        return ".prg";

    return ext;
}

bool format_ext_match(const char* ext, const char* target) {
    if (!ext || !target) return false;
    while (*ext && *target) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*target)) return false;
        ext++; target++;
    }
    return *ext == *target;
}

bool format_in_list(const format_descriptor_t* fmt,
                    const format_descriptor_t* const* list) {
    if (!fmt || !list) return false;
    for (; *list; list++) {
        if (*list == fmt) return true;
    }
    return false;
}

// ============================================================================
// C++ Helpers — format descriptor list utilities
// ============================================================================

std::vector<std::string> format_list_extensions(
    const format_descriptor_t* const* formats) {
    std::vector<std::string> out;
    if (!formats) return out;
    for (; *formats; formats++) {
        const format_descriptor_t* f = *formats;
        if (!f->extensions) continue;
        for (const char** ext = f->extensions; *ext; ext++)
            out.push_back(*ext);
    }
    return out;
}

std::string format_list_dialog_filter(
    const format_descriptor_t* const* formats,
    const char* label) {
    if (!formats || !label) return ".*";
    // Build collection filter: "C64 Files{.prg,.d64,.t64,...}"
    std::string filter = std::string(label) + " Files{";
    bool first = true;
    for (const format_descriptor_t* const* p = formats; *p; p++) {
        const format_descriptor_t* f = *p;
        if (!f->extensions) continue;
        for (const char** ext = f->extensions; *ext; ext++) {
            if (!first) filter += ',';
            const char* e = *ext;
            if (e[0] == '.') e++;
            filter += '.';
            filter += e;
            first = false;
        }
    }
    filter += "},.*";
    return filter;
}
