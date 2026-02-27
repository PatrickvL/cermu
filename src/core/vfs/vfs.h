#pragma once

/**
 * Virtual File System (VFS) — Transparent archive access
 *
 * Provides a unified API for reading files from both the real filesystem
 * and from within archive files.  Archive entries are addressed using a
 * special path syntax:
 *
 *     /path/to/archive.zip!/inner/file.nes
 *
 * The delimiter "!/" separates the real filesystem path to the archive
 * from the path inside of it.  Nesting is supported:
 *
 *     /path/to/outer.zip!/inner.zip!/game.nes
 *
 * The VFS layer is designed to be a drop-in replacement for raw file I/O.
 * The format system's `format_read_entire_file()` delegates to the VFS
 * so that all format handlers automatically gain archive transparency.
 *
 * All platform-specific I/O and archive library calls are delegated to
 * the core OS layer (os/os.h).  This file contains only the VFS routing
 * and nesting logic — zero #ifdef or library-specific code.
 *
 * Archive support (via libarchive): ZIP, 7-Zip, RAR, tar (all variants),
 * ISO 9660, cab, cpio, and many more — with transparent decompression
 * (gzip, bzip2, xz, zstd, lz4, etc.).
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// ============================================================================
// Path Constants
// ============================================================================

/** Delimiter that separates real filesystem path from archive-internal path. */
#define VFS_ARCHIVE_DELIMITER "!/"

/** Length of the delimiter string (not counting NUL). */
#define VFS_ARCHIVE_DELIMITER_LEN 2

// ============================================================================
// VFS Entry Types
// ============================================================================

/** Type of an entry returned by vfs_list_entries(). */
enum class VfsEntryType {
    File,       /**< Regular file */
    Directory,  /**< Directory / folder */
    Archive     /**< A file that is itself an archive (can be "entered") */
};

/**
 * A single entry in a directory or archive listing.
 */
struct VfsEntry {
    std::string    name;        /**< Filename (no path prefix) */
    std::string    full_path;   /**< Full VFS path (including archive prefix) */
    VfsEntryType   type;        /**< Entry type */
    size_t         size;        /**< Uncompressed size in bytes (0 for directories) */
};

// ============================================================================
// Path Parsing
// ============================================================================

/**
 * Decomposed VFS path.
 *
 * For a plain filesystem path like "/data/game.nes":
 *   real_path    = "/data/game.nes"
 *   archive_path = ""
 *
 * For an archive path like "/data/roms.zip!/game.nes":
 *   real_path    = "/data/roms.zip"
 *   archive_path = "game.nes"
 *
 * For a nested archive path like "/data/outer.zip!/inner.zip!/game.nes":
 *   On first parse:
 *     real_path    = "/data/outer.zip"
 *     archive_path = "inner.zip!/game.nes"
 *   The caller recursively splits archive_path to handle nesting.
 */
struct VfsPathComponents {
    std::string real_path;      /**< Filesystem path to real file or archive */
    std::string archive_path;   /**< Path inside archive, empty if not an archive path */
};

/** Check if a path contains the archive delimiter. */
bool vfs_is_archive_path(const char* path);

/** Parse a VFS path into its outermost real/archive components. */
VfsPathComponents vfs_parse_path(const char* path);

// ============================================================================
// File I/O
// ============================================================================

/**
 * Read an entire file, transparently handling archive paths.
 *
 * For plain filesystem paths, reads the file directly.
 * For archive paths (containing "!/"), extracts from the archive.
 * Supports nested archives by recursive extraction.
 *
 * @param path      Filesystem path or VFS archive path
 * @param out_size  Receives the size of the returned buffer
 * @return          malloc'd buffer (caller frees), or NULL on failure
 */
uint8_t* vfs_read_file(const char* path, size_t* out_size);

/**
 * Read a file from within an already-loaded archive buffer in memory.
 *
 * This is the key function for nested archive support: after extracting
 * an inner archive from an outer archive, this reads a file from the
 * in-memory archive data.
 *
 * @param archive_data  Pointer to archive file contents in memory
 * @param archive_size  Size of archive data in bytes
 * @param entry_path    Path of the entry within the archive
 * @param out_size      Receives the size of the returned buffer
 * @return              malloc'd buffer (caller frees), or NULL on failure
 */
uint8_t* vfs_read_from_memory_archive(const uint8_t* archive_data,
                                       size_t archive_size,
                                       const char* entry_path,
                                       size_t* out_size);

// ============================================================================
// Directory / Archive Listing
// ============================================================================

/**
 * List entries at a VFS path.
 *
 * - For a real directory:  lists its files and subdirectories
 * - For a real archive file (no "!/" suffix):  lists archive root entries
 * - For an archive path (with "!/"):  lists entries at that path inside the archive
 *
 * Entries that are themselves archives (detected by extension) are tagged
 * as VfsEntryType::Archive so the caller can offer to "enter" them.
 */
std::vector<VfsEntry> vfs_list_entries(const char* path);

/**
 * List entries from an in-memory archive.
 * Used for nested archive browsing.
 */
std::vector<VfsEntry> vfs_list_entries_from_memory(const uint8_t* archive_data,
                                                    size_t archive_size,
                                                    const char* subpath);

// ============================================================================
// Queries
// ============================================================================

/** Check if a VFS path exists (filesystem or archive entry). */
bool vfs_exists(const char* path);

/** Check if a file extension is a known archive format (e.g. ".zip"). */
bool vfs_is_archive_extension(const char* ext);

/**
 * Get the null-terminated list of supported archive extensions (e.g. ".zip", ".7z", ...).
 * Each entry includes the leading dot. The list ends with a nullptr sentinel.
 */
const char* const* vfs_archive_extensions();

// ============================================================================
// Path Utilities
// ============================================================================

/**
 * Extract the filename component from a VFS path.
 * Handles archive paths: "/data/roms.zip!/subdir/game.nes" → "game.nes"
 */
std::string vfs_filename(const char* path);

/**
 * Extract the file extension from a VFS path (includes the dot).
 * Handles archive paths: "/data/roms.zip!/game.nes" → ".nes"
 */
std::string vfs_extension(const char* path);

/**
 * Build a VFS path from a real path and an archive-internal entry.
 * join_vfs_path("/data/roms.zip", "game.nes") → "/data/roms.zip!/game.nes"
 */
std::string vfs_join_path(const std::string& base, const std::string& entry);

// ============================================================================
// Archive-Aware System Detection
// ============================================================================

/**
 * Result of scanning an archive for loadable content.
 */
struct VfsArchiveScan {
    std::string  archive_path;           /**< Path to the archive file */
    std::vector<VfsEntry> loadable_files; /**< Files that match known formats */
    std::string  suggested_system;       /**< Best-guess system short name, or "" */
    float        confidence;             /**< Confidence in the system guess (0.0–1.0) */
};

/**
 * Scan an archive and identify loadable files + suggested system.
 *
 * Uses format extensions and SystemDescriptor::can_load_file callbacks
 * to determine which files inside the archive are loadable and which
 * emulated system they belong to.
 *
 * @param archive_path  Path to the archive file (not a VFS path)
 * @return              Scan result with loadable entries and system guess
 */
VfsArchiveScan vfs_scan_archive(const char* archive_path);
