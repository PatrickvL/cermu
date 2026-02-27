/**
 * Archive Scanner — Implementation
 *
 * Bridges VFS, FormatRegistry, and SystemRegistry to identify
 * loadable files inside archives and suggest an emulated system.
 *
 * File-to-system identification is delegated entirely to
 * SystemRegistry::identify_system() — the single authority for that
 * mapping.  FormatRegistry is only used as a quick extension filter
 * to avoid reading entries that have no recognised format.
 */

#include "archive_scanner.h"
#include "formats/format_registry.h"
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

    const auto& format_reg = FormatRegistry::instance();
    const auto& system_reg = SystemRegistry::instance();

    // Per-system vote accumulator
    struct SystemVote {
        float total_confidence = 0.0f;
        int   file_count       = 0;
    };
    std::vector<std::pair<std::string, SystemVote>> votes;

    auto find_or_add = [&](const std::string& sys) -> SystemVote& {
        for (auto& [name, vote] : votes) {
            if (name == sys) return vote;
        }
        votes.push_back({sys, {}});
        return votes.back().second;
    };

    for (const auto& entry : entries) {
        if (entry.type == VfsEntryType::Directory) continue;

        // Quick extension filter — skip files with no recognised format
        std::string ext = vfs_extension(entry.name.c_str());
        if (ext.empty()) continue;
        if (!format_reg.find_by_extension(ext.c_str())) continue;

        // Read entry content for system identification
        size_t file_size = 0;
        uint8_t* data = vfs_read_file(entry.full_path.c_str(), &file_size);
        if (!data) continue;

        // Delegate to the single file-to-system authority
        auto match = system_reg.identify_system(
            entry.full_path.c_str(), data, file_size);
        free(data);

        if (match.confidence <= 0.0f || match.system_name.empty()) continue;

        scan.loadable_files.push_back(entry);
        auto& vote = find_or_add(match.system_name);
        vote.total_confidence += match.confidence;
        vote.file_count++;
    }

    // Pick the system with the highest total confidence
    float best = 0.0f;
    for (const auto& [name, vote] : votes) {
        if (vote.total_confidence > best) {
            best = vote.total_confidence;
            scan.suggested_system = name;
            scan.confidence = best / static_cast<float>(vote.file_count);
        }
    }

    return scan;
}
