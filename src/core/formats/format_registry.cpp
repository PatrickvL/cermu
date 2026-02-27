/**
 * Format Registry — Implementation
 *
 * The registry dispatches entirely through format_descriptor_t callbacks.
 * It contains ZERO format-specific logic.
 */

#include "format_registry.h"
#include "vfs/vfs.h"
#include "system_registry.h"
#include "emulated_system.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

// ============================================================================
// FormatRegistry singleton
// ============================================================================

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry registry;
    return registry;
}

void FormatRegistry::register_format(const format_descriptor_t* descriptor) {
    if (!descriptor) return;
    printf("FormatRegistry: Registered format: %s (%s)\n",
           descriptor->name, descriptor->description);
    formats_.push_back(descriptor);
}

const std::vector<const format_descriptor_t*>& FormatRegistry::get_formats() const {
    return formats_;
}

const format_descriptor_t* FormatRegistry::find_by_extension(const char* extension) const {
    if (!extension) return nullptr;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++) {
            if (format_ext_match(extension, *ext))
                return fmt;
        }
    }
    return nullptr;
}

const format_descriptor_t* FormatRegistry::identify(const uint8_t* data, size_t file_size,
                                                     const char* extension) const {
    float best = 0.0f;
    const format_descriptor_t* best_fmt = nullptr;

    for (auto* fmt : formats_) {
        if (!fmt->identify) continue;
        float conf = fmt->identify(data, file_size, extension);
        if (conf > best) {
            best = conf;
            best_fmt = fmt;
        }
    }
    return (best > 0.0f) ? best_fmt : nullptr;
}

std::vector<std::string> FormatRegistry::get_all_extensions() const {
    std::vector<std::string> result;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++)
            result.push_back(*ext);
    }
    return result;
}

std::string FormatRegistry::get_file_dialog_filter() const {
    std::string filter;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++) {
            if (!filter.empty()) filter += ',';
            filter += *ext;
        }
    }
    return filter;
}

// ============================================================================
// Unified Load — pure dispatch through descriptor->load()
// ============================================================================

bool FormatRegistry::load_file(const char* filepath, format_load_result_t* out) const {
    if (!filepath || !out) return false;
    memset(out, 0, sizeof(*out));
    out->type = FORMAT_LOAD_ERROR;

    /* Determine the file extension — use VFS-aware extraction for archive paths.
     * For "game.zip!/rom.nes", we want ".nes" not ".zip".  */
    std::string ext_str = vfs_extension(filepath);
    const char* ext = ext_str.empty() ? nullptr : ext_str.c_str();

    /* Identify format — try content-based first if we can read the file.
     * format_read_entire_file() is VFS-aware and can extract from archives. */
    const format_descriptor_t* fmt = nullptr;
    {
        size_t peek_size = 0;
        uint8_t* peek = format_read_entire_file(filepath, &peek_size);
        if (peek) {
            fmt = identify(peek, peek_size, ext);
            free(peek);
        } else if (ext) {
            fmt = find_by_extension(ext);
        }
    }

    if (!fmt) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Unrecognised file format: %s", filepath);
        return false;
    }

    if (!fmt->load) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Format '%s' does not support direct loading", fmt->name);
        return false;
    }

    bool ok = fmt->load(filepath, out);
    out->format = fmt;
    return ok;
}

// ============================================================================
// Archive Scanning — format + system identification
// ============================================================================

ArchiveScan FormatRegistry::scan_archive(const char* archive_path) const {
    ArchiveScan scan;
    scan.archive_path = archive_path ? archive_path : "";
    scan.confidence = 0.0f;

    if (!archive_path) return scan;

    auto entries = vfs_list_entries(archive_path);

    // Collect system descriptors once
    auto system_descriptors = SystemRegistry::instance().get_all_descriptors();

    // Helper: find which system(s) claim a given format descriptor
    auto find_system_for_format = [&](const format_descriptor_t* fmt)
            -> const char* {
        for (const auto& sd : system_descriptors) {
            if (!sd.supported_formats) continue;
            if (format_in_list(fmt, sd.supported_formats))
                return sd.short_name;
        }
        return nullptr;
    };

    // Per-system vote accumulator
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

        // 1) Extension-based: find a registered format for this extension
        std::string ext = vfs_extension(entry.name.c_str());
        if (ext.empty()) continue;

        const format_descriptor_t* fmt = find_by_extension(ext.c_str());
        if (!fmt) continue;

        // 2) Content-based: extract and run identify() for higher confidence
        float confidence = 0.0f;
        {
            size_t file_size = 0;
            uint8_t* data = vfs_read_file(entry.full_path.c_str(), &file_size);
            if (data) {
                const format_descriptor_t* id_fmt = identify(data, file_size,
                                                              ext.c_str());
                if (id_fmt) {
                    // Use the content-identified format (may differ from extension)
                    fmt = id_fmt;
                    confidence = fmt->identify(data, file_size, ext.c_str());
                }
                free(data);
            }
        }

        // Fall back to a modest extension-only confidence if identify failed
        if (confidence <= 0.0f) {
            confidence = 0.5f;
        }

        // 3) Map format → system
        const char* sys = find_system_for_format(fmt);
        if (!sys) continue;

        scan.loadable_files.push_back(entry);
        auto& vote = find_or_add(sys);
        vote.total_confidence += confidence;
        vote.file_count++;
    }

    // Pick the system with the highest total confidence
    float best = 0.0f;
    for (const auto& v : votes) {
        if (v.total_confidence > best) {
            best = v.total_confidence;
            scan.suggested_system = v.name;
            scan.confidence = best / static_cast<float>(v.file_count);
        }
    }

    return scan;
}

// ============================================================================
// C API wrapper
// ============================================================================

bool format_load_file(const char* filepath, format_load_result_t* out) {
    return FormatRegistry::instance().load_file(filepath, out);
}
