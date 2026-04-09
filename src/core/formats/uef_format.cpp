/**
 * UEF Format Handler — Implementation
 *
 * Parses Acorn UEF tape files (.uef).
 * Identifies by "UEF File!" magic (may be gzip-compressed).
 *
 * Supports gzip-compressed UEF when ZLIB is available at build time.
 * Non-compressed UEF always works.
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/uef_format.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/format_registry.hpp"
#include "core/os/os.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t UEF_MAGIC_SIZE = 10;
static const char UEF_MAGIC[] = "UEF File!";  // 9 chars + NUL = 10 bytes

static constexpr uint16_t UEF_CHUNK_DATA        = 0x0100;
static constexpr uint16_t UEF_CHUNK_DATA_DEFINED = 0x0104;

// ============================================================================
// Internal: Parse UEF data blocks into tape blocks
// ============================================================================

struct uef_file_t {
    tape::block_t block;
    uint8_t*      data;      // Owned, must free
    size_t        data_size;
    bool          complete;  // All blocks received?
};

struct uef_parsed_t {
    uef_file_t  files[tape::MAX_BLOCKS];
    int          count;
};

static void uef_parsed_free(uef_parsed_t& p) {
    for (int i = 0; i < p.count; ++i) {
        std::free(p.files[i].data);
        p.files[i].data = nullptr;
    }
}

/// Parse a data block chunk, extracting the cassette file header if present.
/// Returns true if a new file header was found (block 0).
static bool parse_data_chunk(const uint8_t* chunk_data, size_t chunk_size,
                             uef_parsed_t& out) {
    if (chunk_size < 1) return false;

    const uint8_t* p = chunk_data;
    size_t remaining = chunk_size;

    // Look for sync byte ($2A = '*')
    if (p[0] != 0x2A) {
        // Not a file header block — could be continuation data
        // Append to current file if we have one
        if (out.count > 0) {
            uef_file_t& cur = out.files[out.count - 1];
            uint8_t* bigger = static_cast<uint8_t*>(
                std::realloc(cur.data, cur.data_size + remaining));
            if (bigger) {
                std::memcpy(bigger + cur.data_size, p, remaining);
                cur.data = bigger;
                cur.data_size += remaining;
            }
        }
        return false;
    }

    p++; remaining--;

    // Filename: NUL-terminated
    char filename[33] = {};
    size_t name_len = 0;
    while (remaining > 0 && *p != 0x00 && name_len < 32) {
        filename[name_len++] = static_cast<char>(*p);
        p++; remaining--;
    }
    filename[name_len] = '\0';
    if (remaining > 0 && *p == 0x00) { p++; remaining--; }

    // Need: load(4) + exec(4) + block_num(2) + block_len(2) + flags(1) = 13
    if (remaining < 13) return false;

    uint32_t load_addr  = format_read_le32(p);      p += 4;
    uint32_t exec_addr  = format_read_le32(p);      p += 4;
    uint16_t block_num  = format_read_le16(p);      p += 2;
    uint16_t block_len  = format_read_le16(p);      p += 2;
    uint8_t  flags      = *p;                        p += 1;
    remaining -= 13;

    // Next file address (4 bytes) + header CRC (2 bytes) — skip
    if (remaining >= 6) {
        p += 6;
        remaining -= 6;
    }

    // If block 0, this is a new file
    if (block_num == 0 && out.count < tape::MAX_BLOCKS) {
        uef_file_t& f = out.files[out.count];
        std::memcpy(f.block.name, filename, 33);
        f.block.load_addr = static_cast<uint16_t>(load_addr & 0xFFFF);
        f.block.exec_addr = static_cast<uint16_t>(exec_addr & 0xFFFF);
        f.block.autorun = (exec_addr != 0 && exec_addr != 0xFFFFFFFF);
        // BBC files with exec != load are typically machine code
        f.block.type = (exec_addr != load_addr && exec_addr != 0)
                       ? tape::TAPE_BLOCK_CODE
                       : tape::TAPE_BLOCK_BASIC;
        f.complete = (flags & 0x80) != 0;  // bit 7 = last block flag
        f.data = nullptr;
        f.data_size = 0;
        out.count++;
    }

    // Append block data to current file
    size_t copy_len = (block_len <= remaining) ? block_len : remaining;
    if (copy_len > 0 && out.count > 0) {
        uef_file_t& cur = out.files[out.count - 1];
        uint8_t* bigger = static_cast<uint8_t*>(
            std::realloc(cur.data, cur.data_size + copy_len));
        if (bigger) {
            std::memcpy(bigger + cur.data_size, p, copy_len);
            cur.data = bigger;
            cur.data_size += copy_len;
        }
        if (flags & 0x80) cur.complete = true;
    }

    return (block_num == 0);
}

/// Walk all UEF chunks and extract file blocks.
static bool uef_parse(const uint8_t* data, size_t size, uef_parsed_t& out) {
    out.count = 0;

    // Skip UEF header: magic(10) + minor(1) + major(1) = 12 bytes
    if (size < 12) return false;
    if (std::memcmp(data, UEF_MAGIC, UEF_MAGIC_SIZE) != 0) return false;

    size_t pos = 12;

    while (pos + 6 <= size) {
        uint16_t chunk_type = format_read_le16(data + pos);
        uint32_t chunk_len  = format_read_le32(data + pos + 2);
        pos += 6;

        if (pos + chunk_len > size) break;

        if (chunk_type == UEF_CHUNK_DATA || chunk_type == UEF_CHUNK_DATA_DEFINED) {
            parse_data_chunk(data + pos, chunk_len, out);
        }
        // Other chunk types (carrier, gaps, baudrate) are ignored for file extraction

        pos += chunk_len;
    }

    return out.count > 0;
}

// ============================================================================
// Identification
// ============================================================================

static float uef_identify(const uint8_t* data, size_t file_size,
                          const char* extension) {
    if (data && file_size >= UEF_MAGIC_SIZE) {
        // Check uncompressed magic
        if (std::memcmp(data, UEF_MAGIC, UEF_MAGIC_SIZE) == 0)
            return 0.95f;

        // Check gzip magic (UEF files are often gzipped)
        if (data[0] == 0x1F && data[1] == 0x8B)
            return 0.6f;  // Moderate — could be any gzipped file
    }

    if (extension && format_ext_match(extension, ".uef")) return 0.85f;

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool uef_load(const uint8_t* data, size_t size,
                     format_load_result_t* out) {
    if (!data || size < UEF_MAGIC_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg), "UEF: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Try gzip decompression if needed
    uint8_t* decompressed = nullptr;
    size_t dec_size = 0;
    if (data[0] == 0x1F && data[1] == 0x8B) {
        decompressed = os_decompress(data, size, &dec_size);
        if (!decompressed) {
            snprintf(out->error_msg, sizeof(out->error_msg),
                     "UEF: Gzip decompression failed");
            out->type = FORMAT_LOAD_ERROR;
            return false;
        }
        data = decompressed;
        size = dec_size;
    }

    uef_parsed_t parsed{};
    bool ok = uef_parse(data, size, parsed);

    if (!ok || parsed.count == 0) {
        uef_parsed_free(parsed);
        std::free(decompressed);
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "UEF: No valid file blocks found");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Prepare arrays for tape::load_best_entry
    tape::block_t blocks[tape::MAX_BLOCKS];
    const uint8_t* ptrs[tape::MAX_BLOCKS];
    size_t sizes[tape::MAX_BLOCKS];
    for (int i = 0; i < parsed.count; ++i) {
        blocks[i] = parsed.files[i].block;
        ptrs[i]   = parsed.files[i].data;
        sizes[i]  = parsed.files[i].data_size;
    }

    ok = tape::load_best_entry(blocks, parsed.count, ptrs, sizes, "UEF", out);

    uef_parsed_free(parsed);
    std::free(decompressed);
    return ok;
}

// ============================================================================
// List Entries
// ============================================================================

static int uef_list(const uint8_t* data, size_t size,
                    format_container_entry_t* entries, int max_entries) {
    if (!data || size < UEF_MAGIC_SIZE) return -1;

    uint8_t* decompressed = nullptr;
    size_t dec_size = 0;
    if (data[0] == 0x1F && data[1] == 0x8B) {
        decompressed = os_decompress(data, size, &dec_size);
        if (!decompressed) return -1;
        data = decompressed;
        size = dec_size;
    }

    uef_parsed_t parsed{};
    if (!uef_parse(data, size, parsed)) {
        std::free(decompressed);
        return -1;
    }

    size_t data_sizes[tape::MAX_BLOCKS];
    for (int i = 0; i < parsed.count; ++i)
        data_sizes[i] = parsed.files[i].data_size;

    tape::block_t blocks[tape::MAX_BLOCKS];
    for (int i = 0; i < parsed.count; ++i)
        blocks[i] = parsed.files[i].block;

    int result = tape::list_entries_common(blocks, parsed.count, data_sizes,
                                            entries, max_entries);

    uef_parsed_free(parsed);
    std::free(decompressed);
    return result;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* uef_extensions[] = { ".uef", nullptr };

const format_descriptor_t UEF_FORMAT_DESCRIPTOR = {
    "UEF",
    "Acorn Unified Emulator Format (Tape)",
    uef_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_METADATA,
    uef_identify,
    uef_load,
    uef_list,
    nullptr     // extract_entry
};

REGISTER_FORMAT(UEF, &UEF_FORMAT_DESCRIPTOR)
