/**
 * Core OS Abstraction Layer — Archive Implementation
 *
 * All archive reading goes through libarchive.  Nothing outside this file
 * should include <archive.h> or any archive-library header.
 *
 * Supported formats (read-only): ZIP, 7-Zip, RAR (v2/v3/v5), tar (all
 * variants), ISO 9660, XAR, cab, cpio, ar, shar, and more.
 * Compression: gzip, bzip2, xz/lzma, zstd, lz4, lzop, compress — all
 * handled transparently by libarchive.
 */

#include "os.h"

#ifdef CERMU_NO_LIBARCHIVE
// Stub implementations when libarchive is not available
std::vector<OsArchiveEntry> os_archive_list(const char*) { return {}; }
std::vector<OsArchiveEntry> os_archive_list_from_memory(const uint8_t*, size_t) { return {}; }
uint8_t* os_archive_extract(const char*, const char*, size_t*) { return nullptr; }
uint8_t* os_archive_extract_from_memory(const uint8_t*, size_t, const char*, size_t*) { return nullptr; }
const char* const* os_archive_extensions() { static const char* e[] = { nullptr }; return e; }
#else // !CERMU_NO_LIBARCHIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/**
 * RAII wrapper for struct archive* (read).
 */
struct ArchiveReader {
    struct archive* a = nullptr;

    ArchiveReader() {
        a = archive_read_new();
        if (a) {
            archive_read_support_filter_all(a);
            archive_read_support_format_all(a);
        }
    }

    ~ArchiveReader() {
        if (a) archive_read_free(a);
    }

    explicit operator bool() const { return a != nullptr; }

    // Non-copyable
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
};

/**
 * Case-insensitive string comparison for entry matching.
 */
bool iequals(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) !=
            tolower(static_cast<unsigned char>(*b)))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * Read the full data of the current archive entry into a malloc'd buffer.
 * Assumes archive_read_next_header() has just returned ARCHIVE_OK.
 */
uint8_t* read_current_entry(struct archive* a, size_t expected_size,
                             size_t* out_size) {
    // If libarchive knows the size, trust it; otherwise grow dynamically
    size_t capacity = (expected_size > 0) ? expected_size : 65536;
    auto* buf = static_cast<uint8_t*>(malloc(capacity));
    if (!buf) return nullptr;

    size_t total = 0;
    for (;;) {
        const void* block;
        size_t block_size;
        la_int64_t offset;

        int r = archive_read_data_block(a, &block, &block_size, &offset);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) {
            free(buf);
            return nullptr;
        }

        // offset may indicate a sparse entry — ensure capacity
        size_t end = static_cast<size_t>(offset) + block_size;
        if (end > capacity) {
            while (capacity < end) capacity *= 2;
            auto* tmp = static_cast<uint8_t*>(realloc(buf, capacity));
            if (!tmp) { free(buf); return nullptr; }
            buf = tmp;
        }

        // Zero-fill any gap (sparse file support)
        if (static_cast<size_t>(offset) > total) {
            memset(buf + total, 0, static_cast<size_t>(offset) - total);
        }

        memcpy(buf + offset, block, block_size);
        if (end > total) total = end;
    }

    *out_size = total;
    return buf;
}

/**
 * Known archive extensions.  Must stay in sync with libarchive capabilities.
 */
static const char* s_archive_extensions[] = {
    ".zip",
    ".7z",
    ".rar",
    ".tar",
    ".tar.gz", ".tgz",
    ".tar.bz2", ".tbz2",
    ".tar.xz", ".txz",
    ".tar.zst", ".tzst",
    ".tar.lz4",
    ".gz",
    ".bz2",
    ".xz",
    ".zst",
    ".lz4",
    ".iso",
    ".cab",
    ".cpio",
    ".ar",
    nullptr
};

} // anonymous namespace

// ============================================================================
// Archive Listing — from disk
// ============================================================================

std::vector<OsArchiveEntry> os_archive_list(const char* archive_path) {
    std::vector<OsArchiveEntry> result;
    if (!archive_path) return result;

    ArchiveReader reader;
    if (!reader) return result;

    if (archive_read_open_filename(reader.a, archive_path, 16384) != ARCHIVE_OK)
        return result;

    struct archive_entry* entry;
    while (archive_read_next_header(reader.a, &entry) == ARCHIVE_OK) {
        OsArchiveEntry e;
        const char* pathname = archive_entry_pathname(entry);
        e.name = pathname ? pathname : "";
        e.is_dir = (archive_entry_filetype(entry) == AE_IFDIR);
        e.size = static_cast<size_t>(archive_entry_size(entry));

        // Normalise: strip trailing slash from directory names in the name field
        // (some archives include it, some don't)
        if (!e.name.empty() && e.name.back() == '/') {
            e.is_dir = true;
            e.name.pop_back();
        }

        if (!e.name.empty())
            result.push_back(std::move(e));

        archive_read_data_skip(reader.a);
    }

    return result;
}

// ============================================================================
// Archive Listing — from memory
// ============================================================================

std::vector<OsArchiveEntry> os_archive_list_from_memory(
        const uint8_t* data, size_t data_size) {
    std::vector<OsArchiveEntry> result;
    if (!data || data_size == 0) return result;

    ArchiveReader reader;
    if (!reader) return result;

    if (archive_read_open_memory(reader.a, data, data_size) != ARCHIVE_OK)
        return result;

    struct archive_entry* entry;
    while (archive_read_next_header(reader.a, &entry) == ARCHIVE_OK) {
        OsArchiveEntry e;
        const char* pathname = archive_entry_pathname(entry);
        e.name = pathname ? pathname : "";
        e.is_dir = (archive_entry_filetype(entry) == AE_IFDIR);
        e.size = static_cast<size_t>(archive_entry_size(entry));

        if (!e.name.empty() && e.name.back() == '/') {
            e.is_dir = true;
            e.name.pop_back();
        }

        if (!e.name.empty())
            result.push_back(std::move(e));

        archive_read_data_skip(reader.a);
    }

    return result;
}

// ============================================================================
// Archive Extraction — from disk
// ============================================================================

uint8_t* os_archive_extract(const char* archive_path,
                             const char* entry_name,
                             size_t* out_size) {
    if (!archive_path || !entry_name || !out_size) return nullptr;
    *out_size = 0;

    // Strategy: scan through entries looking for exact match, then
    // fall back to case-insensitive match on a second pass if needed.
    // libarchive is streaming, so we may need to open the archive twice
    // for the fallback.  First pass: try exact match.

    for (int pass = 0; pass < 2; ++pass) {
        ArchiveReader reader;
        if (!reader) return nullptr;

        if (archive_read_open_filename(reader.a, archive_path, 16384) != ARCHIVE_OK)
            return nullptr;

        struct archive_entry* entry;
        while (archive_read_next_header(reader.a, &entry) == ARCHIVE_OK) {
            const char* pathname = archive_entry_pathname(entry);
            if (!pathname) {
                archive_read_data_skip(reader.a);
                continue;
            }

            // Strip trailing slash for comparison
            std::string name_clean = pathname;
            if (!name_clean.empty() && name_clean.back() == '/')
                name_clean.pop_back();

            bool match = (pass == 0)
                ? (name_clean == entry_name)
                : iequals(name_clean.c_str(), entry_name);

            if (match) {
                size_t expected = static_cast<size_t>(archive_entry_size(entry));
                return read_current_entry(reader.a, expected, out_size);
            }

            archive_read_data_skip(reader.a);
        }
    }

    return nullptr;
}

// ============================================================================
// Archive Extraction — from memory
// ============================================================================

uint8_t* os_archive_extract_from_memory(const uint8_t* data, size_t data_size,
                                         const char* entry_name,
                                         size_t* out_size) {
    if (!data || data_size == 0 || !entry_name || !out_size) return nullptr;
    *out_size = 0;

    for (int pass = 0; pass < 2; ++pass) {
        ArchiveReader reader;
        if (!reader) return nullptr;

        if (archive_read_open_memory(reader.a, data, data_size) != ARCHIVE_OK)
            return nullptr;

        struct archive_entry* entry;
        while (archive_read_next_header(reader.a, &entry) == ARCHIVE_OK) {
            const char* pathname = archive_entry_pathname(entry);
            if (!pathname) {
                archive_read_data_skip(reader.a);
                continue;
            }

            std::string name_clean = pathname;
            if (!name_clean.empty() && name_clean.back() == '/')
                name_clean.pop_back();

            bool match = (pass == 0)
                ? (name_clean == entry_name)
                : iequals(name_clean.c_str(), entry_name);

            if (match) {
                size_t expected = static_cast<size_t>(archive_entry_size(entry));
                return read_current_entry(reader.a, expected, out_size);
            }

            archive_read_data_skip(reader.a);
        }
    }

    return nullptr;
}

// ============================================================================
// Archive Extensions
// ============================================================================

const char* const* os_archive_extensions() {
    return s_archive_extensions;
}

#endif // !CERMU_NO_LIBARCHIVE
