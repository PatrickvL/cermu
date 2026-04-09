#pragma once

/**
 * Core OS Abstraction Layer
 *
 * All platform-specific filesystem and archive I/O lives behind this API.
 * The rest of the codebase MUST use these functions instead of calling POSIX,
 * Win32, or archive-library APIs directly.
 *
 * Implementation is split across two translation units:
 *   os_filesystem.cpp — file I/O, existence checks, directory listing (POSIX / Win32)
 *   os_archive.cpp    — archive reading via libarchive
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// ============================================================================
// File I/O
// ============================================================================

/**
 * Read an entire file into a malloc'd buffer.
 * @param path      Filesystem path
 * @param out_size  Receives the number of bytes read
 * @return          malloc'd buffer the caller must free(), or nullptr on failure
 */
uint8_t* os_read_file(const char* path, size_t* out_size);

/**
 * Write a buffer to a file (atomic-ish: write-then-close).
 * Creates or overwrites the file.
 * @return true on success
 */
bool os_write_file(const char* path, const uint8_t* data, size_t size);

// ============================================================================
// Filesystem Queries
// ============================================================================

/** Check whether a path exists (file or directory). */
bool os_file_exists(const char* path);

/** Check whether a path is a regular file (not a directory/symlink-to-dir). */
bool os_is_regular_file(const char* path);

/** Check whether a path is a directory. */
bool os_is_directory(const char* path);

// ============================================================================
// Directory Listing
// ============================================================================

/** A single entry returned by os_list_directory(). */
struct OsDirEntry {
    std::string name;   /**< Filename only (no path prefix) */
    bool        is_dir; /**< true = directory, false = regular file */
    size_t      size;   /**< Uncompressed size in bytes (0 for directories) */
};

/**
 * List the immediate contents of a directory.
 * Skips "." and ".." entries.
 * Entries are returned in unspecified order (caller sorts if needed).
 */
std::vector<OsDirEntry> os_list_directory(const char* path);

// ============================================================================
// Path Utilities
// ============================================================================

/**
 * Find the extension in a filename.
 * Returns a pointer to the last dot in the last path component, or nullptr
 * if there is no extension.  Handles both '/' and '\\' separators.
 */
const char* os_find_extension(const char* path);

/**
 * Case-insensitive extension compare (both must include the dot).
 * Example: os_extension_match(".ZIP", ".zip") → true
 */
bool os_extension_match(const char* a, const char* b);

/**
 * Normalise all path separators to the platform's native separator in-place.
 * On Windows: '/' → '\\'.  On Unix: '\\' → '/'.
 */
void os_normalize_path(std::string& path);

/**
 * Create a directory (and all parent directories) in a cross-platform way.
 * @return 0 on success or if the directory already exists.
 */
int os_mkdir_p(const std::string& dir);

// ============================================================================
// Archive Reading
// ============================================================================

/**
 * An entry inside an archive, returned by the listing functions.
 */
struct OsArchiveEntry {
    std::string name;   /**< Full path within the archive (e.g. "dir/file.nes") */
    bool        is_dir; /**< true = directory entry */
    size_t      size;   /**< Uncompressed size in bytes (0 for directories) */
};

/**
 * List all entries in an archive file on disk.
 * Supports any format recognised by libarchive (ZIP, 7-Zip, tar.*, RAR, etc.).
 */
std::vector<OsArchiveEntry> os_archive_list(const char* archive_path);

/**
 * List all entries in an archive held in memory.
 */
std::vector<OsArchiveEntry> os_archive_list_from_memory(
    const uint8_t* data, size_t data_size);

/**
 * Extract a single entry from an archive file on disk.
 * @param archive_path  Filesystem path to the archive
 * @param entry_name    Path of the desired entry within the archive
 * @param out_size      Receives the uncompressed size
 * @return              malloc'd buffer the caller must free(), or nullptr
 *
 * Matching is exact first, then case-insensitive fallback.
 */
uint8_t* os_archive_extract(const char* archive_path,
                             const char* entry_name,
                             size_t* out_size);

/**
 * Extract a single entry from an in-memory archive.
 * Same semantics as os_archive_extract() but reads from a buffer.
 */
uint8_t* os_archive_extract_from_memory(const uint8_t* data, size_t data_size,
                                         const char* entry_name,
                                         size_t* out_size);

/**
 * Decompress a standalone compressed buffer (gzip, bzip2, xz, zstd, lz4).
 * Uses libarchive's "raw" format mode — no container needed, just a stream.
 * @param data       Compressed data
 * @param data_size  Size of compressed data
 * @param out_size   Receives the decompressed size
 * @return           malloc'd buffer the caller must free(), or nullptr if
 *                   the data is not recognised as compressed or on error.
 */
uint8_t* os_decompress(const uint8_t* data, size_t data_size, size_t* out_size);

/**
 * Known archive file extensions supported by the archive backend.
 * Includes the leading dot.  The list is nullptr-terminated.
 * Example entries: ".zip", ".7z", ".tar", ".tar.gz", ".rar", ...
 */
const char* const* os_archive_extensions();
