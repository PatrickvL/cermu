#pragma once

/**
 * Archive Scanner — identifies loadable content inside archives
 *
 * Higher-level glue that ties together:
 *   - VFS             (list/read archive entries)
 *   - FormatRegistry  (quick extension filter for candidate entries)
 *   - SystemRegistry  (identify_system() — the single authority for
 *                       mapping a file to an emulated system)
 *
 * Intentionally kept separate from all three subsystems so that none
 * of them needs to know about the others' internals.
 */

#include "core/vfs/vfs.hpp"

#include <string>
#include <vector>

/**
 * Result of scanning an archive for loadable content.
 */
struct ArchiveScan {
    std::string  archive_path;           /**< Path to the archive file */
    std::vector<VfsEntry> loadable_files; /**< Files that match known formats */
    std::string  suggested_system;       /**< Best-guess system short name, or "" */
    float        confidence;             /**< Confidence in the system guess (0.0–1.0) */
};

/**
 * Scan an archive for loadable content and suggest a system.
 *
 * Lists entries via VFS, filters by recognised format extension, then
 * delegates per-entry system identification to
 * SystemRegistry::identify_system() — the same path used for direct
 * file detection.  Accumulates per-system confidence votes and returns
 * the system with the highest total.
 *
 * @param archive_path  Path to the archive file (not a VFS "!/"-path)
 * @return              Scan result with loadable entries and system guess
 */
ArchiveScan scan_archive(const char* archive_path);
