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

#include "core/os/os.hpp"

#ifdef CERMU_NO_LIBARCHIVE
// Stub implementations when libarchive is not available
std::vector<OsArchiveEntry> os_archive_list(const char*) { return {}; }
std::vector<OsArchiveEntry> os_archive_list_from_memory(const uint8_t*, size_t) { return {}; }
uint8_t* os_archive_extract(const char*, const char*, size_t*) { return nullptr; }
uint8_t* os_archive_extract_from_memory(const uint8_t*, size_t, const char*, size_t*) { return nullptr; }
uint8_t* os_decompress(const uint8_t*, size_t, size_t*) { return nullptr; }
const char* const* os_archive_extensions() { static const char* e[] = { nullptr }; return e; }
#else // !CERMU_NO_LIBARCHIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <string>
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

/**
 * Fallback extraction using the 7z command-line tool.
 *
 * libarchive cannot reliably iterate large solid 7z archives (it stops
 * after ~200 entries when the LZMA2 solid block ends).  The 7z CLI
 * handles this correctly, so we shell out as a last resort.
 *
 * Uses: 7z e -so <archive> <entry>   →   extracted bytes on stdout.
 */
static uint8_t* extract_via_7z_cli(const char* archive_path,
                                    const char* entry_name,
                                    size_t* out_size) {
    // Build command: 7z e -so -- 'archive' 'entry' 2>/dev/null
    // We need to escape single quotes in paths for the shell.
    auto shell_escape = [](const char* s) -> std::string {
        std::string result = "'";
        for (const char* p = s; *p; ++p) {
            if (*p == '\'')
                result += "'\\''";
            else
                result += *p;
        }
        result += "'";
        return result;
    };

    std::string cmd = "7z e -so -- ";
    cmd += shell_escape(archive_path);
    cmd += " ";
    cmd += shell_escape(entry_name);
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return nullptr;

    size_t capacity = 65536;
    auto* buf = static_cast<uint8_t*>(malloc(capacity));
    if (!buf) { pclose(pipe); return nullptr; }

    size_t total = 0;
    while (true) {
        size_t n = fread(buf + total, 1, capacity - total, pipe);
        if (n == 0) break;
        total += n;
        if (total == capacity) {
            capacity *= 2;
            auto* tmp = static_cast<uint8_t*>(realloc(buf, capacity));
            if (!tmp) { free(buf); pclose(pipe); return nullptr; }
            buf = tmp;
        }
    }

    int status = pclose(pipe);
    if (status != 0 || total == 0) {
        free(buf);
        return nullptr;
    }

    *out_size = total;
    return buf;
}

// ============================================================================
// Common — Entry Drain Helper
// ============================================================================

/**
 * Drain (read and discard) the current archive entry's data.
 *
 * For most archive formats archive_read_data_skip() is sufficient, but
 * 7-Zip solid archives use a single LZMA2 stream across many entries.
 * libarchive's skip path may not advance the decompression cursor,
 * causing later reads to fail with "Truncated 7-Zip file body".
 *
 * The workaround: consume all data blocks before advancing to the next
 * header.  The cost is negligible (a few memcpy's that get discarded)
 * and it ensures correct behavior for all archive formats.
 */
static void drain_entry(struct archive* a) {
    const void* block;
    size_t      block_size;
    la_int64_t  offset;
    while (archive_read_data_block(a, &block, &block_size, &offset) == ARCHIVE_OK)
        ;  // discard
}

// ============================================================================
// Archive Listing — 7z CLI fallback
// ============================================================================

/**
 * List archive entries using the 7z command-line tool.
 *
 * Used as a fallback when libarchive truncates solid 7z archives.
 * Parses the output of: 7z l -slt -- <archive>
 */
static std::vector<OsArchiveEntry> list_via_7z_cli(const char* archive_path) {
    std::vector<OsArchiveEntry> result;

    auto shell_escape = [](const char* s) -> std::string {
        std::string r = "'";
        for (const char* p = s; *p; ++p) {
            if (*p == '\'')
                r += "'\\''";
            else
                r += *p;
        }
        r += "'";
        return r;
    };

    std::string cmd = "7z l -slt -- ";
    cmd += shell_escape(archive_path);
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;

    // Parse -slt (show technical info) output.  Each entry is a block of
    // key = value lines separated by blank lines.  We look for:
    //   Path = <name>
    //   Size = <bytes>
    //   Folder = +   (if directory)
    // The first block before "----------" is the archive header (contains
    // the archive path itself, Type, Method, etc.) — skip it.
    char line[4096];
    std::string cur_path;
    size_t      cur_size  = 0;
    bool        cur_is_dir = false;
    bool        in_entry   = false;
    bool        past_header = false;

    while (fgets(line, sizeof(line), pipe)) {
        // Strip trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (len == 0) {
            // Blank line — flush current entry
            if (past_header && in_entry && !cur_path.empty()) {
                OsArchiveEntry e;
                e.name   = std::move(cur_path);
                e.size   = cur_size;
                e.is_dir = cur_is_dir;
                result.push_back(std::move(e));
            }
            cur_path.clear();
            cur_size   = 0;
            cur_is_dir = false;
            in_entry   = false;
            continue;
        }

        // The "----------" line separates the archive header from entries.
        if (!past_header && strncmp(line, "----------", 10) == 0) {
            past_header = true;
            continue;
        }

        if (strncmp(line, "Path = ", 7) == 0) {
            cur_path = line + 7;
            in_entry = true;
        } else if (strncmp(line, "Size = ", 7) == 0) {
            cur_size = static_cast<size_t>(strtoull(line + 7, nullptr, 10));
        } else if (strncmp(line, "Folder = +", 10) == 0) {
            cur_is_dir = true;
        }
    }

    // Flush last entry (file may not end with blank line)
    if (past_header && in_entry && !cur_path.empty()) {
        OsArchiveEntry e;
        e.name   = std::move(cur_path);
        e.size   = cur_size;
        e.is_dir = cur_is_dir;
        result.push_back(std::move(e));
    }

    pclose(pipe);
    return result;
}

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

        // Use drain_entry instead of archive_read_data_skip to correctly
        // advance the decompressor in solid 7z archives.
        drain_entry(reader.a);
    }

    // libarchive may truncate solid 7z archives (stops at LZMA2 solid
    // block boundaries).  For .7z files, try the 7z CLI and prefer
    // whichever listing has more entries.
    {
        const char* ext = strrchr(archive_path, '.');
        if (ext && strcasecmp(ext, ".7z") == 0) {
            auto cli_result = list_via_7z_cli(archive_path);
            if (cli_result.size() > result.size())
                return cli_result;
        }
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

        // Use drain_entry instead of archive_read_data_skip to correctly
        // advance the decompressor in solid 7z archives.
        drain_entry(reader.a);
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
                uint8_t* result = read_current_entry(reader.a, expected, out_size);
                if (result) return result;

                // read_current_entry failed — solid archive where skip
                // left the decompressor out of sync.  Retry with full
                // drain from the start.
                ArchiveReader r2;
                if (!r2) return nullptr;
                if (archive_read_open_filename(r2.a, archive_path, 16384) != ARCHIVE_OK)
                    return nullptr;

                struct archive_entry* e2;
                while (archive_read_next_header(r2.a, &e2) == ARCHIVE_OK) {
                    const char* p2 = archive_entry_pathname(e2);
                    std::string n2 = p2 ? p2 : "";
                    if (!n2.empty() && n2.back() == '/') n2.pop_back();
                    if (n2 == name_clean) {
                        expected = static_cast<size_t>(archive_entry_size(e2));
                        return read_current_entry(r2.a, expected, out_size);
                    }
                    drain_entry(r2.a);
                }
                return nullptr;
            }

            archive_read_data_skip(reader.a);
        }
    }

    // libarchive failed to find or extract the entry — fall back to
    // the 7z command-line tool (handles large solid archives correctly).
    return extract_via_7z_cli(archive_path, entry_name, out_size);
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
        // pass 0: exact match, skip-based scan
        // pass 1: case-insensitive match, skip-based scan
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
                uint8_t* result = read_current_entry(reader.a, expected, out_size);
                if (result) return result;

                // read_current_entry failed — solid 7z archive where
                // data_skip left the decompressor out of sync.
                // Retry with full drain from the start.
                ArchiveReader r2;
                if (!r2) return nullptr;
                if (archive_read_open_memory(r2.a, data, data_size) != ARCHIVE_OK)
                    return nullptr;

                struct archive_entry* e2;
                while (archive_read_next_header(r2.a, &e2) == ARCHIVE_OK) {
                    const char* p2 = archive_entry_pathname(e2);
                    std::string n2 = p2 ? p2 : "";
                    if (!n2.empty() && n2.back() == '/') n2.pop_back();
                    if (n2 == name_clean) {
                        expected = static_cast<size_t>(archive_entry_size(e2));
                        return read_current_entry(r2.a, expected, out_size);
                    }
                    drain_entry(r2.a);
                }
                return nullptr;
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

// ============================================================================
// Standalone Decompression — gzip, bzip2, xz, zstd, lz4
// ============================================================================

uint8_t* os_decompress(const uint8_t* data, size_t data_size, size_t* out_size) {
    if (!data || data_size == 0 || !out_size) return nullptr;
    *out_size = 0;

    struct archive* a = archive_read_new();
    if (!a) return nullptr;

    archive_read_support_filter_all(a);
    archive_read_support_format_raw(a);

    if (archive_read_open_memory(a, data, data_size) != ARCHIVE_OK) {
        archive_read_free(a);
        return nullptr;
    }

    struct archive_entry* entry;
    if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
        archive_read_free(a);
        return nullptr;
    }

    // Read decompressed data
    uint8_t* result = read_current_entry(a, 0, out_size);
    archive_read_free(a);
    return result;
}

#endif // !CERMU_NO_LIBARCHIVE
