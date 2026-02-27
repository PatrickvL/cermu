/**
 * Archive Scanner — Implementation
 *
 * Bridges VFS, FormatRegistry, and SystemRegistry to identify
 * loadable files inside archives and suggest an emulated system.
 */

#include "archive_scanner.h"
#include "formats/format_registry.h"
#include "formats/format_handler.h"
#include "system_registry.h"
#include "emulated_system.h"
#include "vfs/vfs.h"

#include <cstdlib>

ArchiveScan scan_archive(const char* archive_path) {
    ArchiveScan scan;
    scan.archive_path = archive_path ? archive_path : "";
    scan.confidence = 0.0f;

    if (!archive_path) return scan;

    auto entries = vfs_list_entries(archive_path);

    const auto& registry = FormatRegistry::instance();

    // Collect system descriptors once
    auto system_descriptors = SystemRegistry::instance().get_all_descriptors();

    // Helper: find which system claims a given format descriptor
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

        const format_descriptor_t* fmt = registry.find_by_extension(ext.c_str());
        if (!fmt) continue;

        // 2) Content-based: extract and run identify() for higher confidence
        float confidence = 0.0f;
        {
            size_t file_size = 0;
            uint8_t* data = vfs_read_file(entry.full_path.c_str(), &file_size);
            if (data) {
                const format_descriptor_t* id_fmt = registry.identify(
                    data, file_size, ext.c_str());
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
