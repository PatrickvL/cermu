#pragma once

/**
 * Archive Scanner — identifies loadable content inside archives
 *
 * Higher-level glue that ties together:
 *   - VFS        (list/read archive entries)
 *   - FormatRegistry  (identify file formats by extension + content)
 *   - SystemRegistry  (map formats to emulated systems)
 *
 * Intentionally kept separate from all three subsystems so that none
 * of them needs to know about the others' internals.
 */

#include "vfs/vfs.h"

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
 * Lists entries via VFS, identifies loadable files by extension and
 * content-based identify(), then maps formats to systems via
 * SystemRegistry's SystemDescriptor::supported_formats lists.
 *
 * @param archive_path  Path to the archive file (not a VFS "!/"-path)
 * @return              Scan result with loadable entries and system guess
 */
ArchiveScan scan_archive(const char* archive_path);
