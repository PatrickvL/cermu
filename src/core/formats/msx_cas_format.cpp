/**
 * MSX CAS Format Handler — Implementation
 *
 * Parses MSX .cas cassette files.
 * Identifies by the 8-byte CAS signature: $1F $A6 $DE $BA $CC $13 $7D $74.
 *
 * Supports binary headers (with load/exec address) and BASIC headers.
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/msx_cas_format.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t CAS_HEADER_SIZE = 8;

static const uint8_t CAS_SIGNATURE[CAS_HEADER_SIZE] = {
    0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74
};

// Block type identifiers (10 repeated bytes after the CAS signature)
static constexpr uint8_t CAS_BINARY_ID = 0xEA;
static constexpr uint8_t CAS_BASIC_ID  = 0xD3;
static constexpr uint8_t CAS_ASCII_ID  = 0xEA;  // same as binary, distinguished by context

// ============================================================================
// Internal: Find next CAS signature
// ============================================================================

static size_t find_cas_header(const uint8_t* data, size_t size, size_t pos) {
    while (pos + CAS_HEADER_SIZE <= size) {
        if (std::memcmp(data + pos, CAS_SIGNATURE, CAS_HEADER_SIZE) == 0)
            return pos;
        pos++;
    }
    return size;
}

/// Detect block type by reading the 10-byte identifier after the CAS header.
/// Returns 'B' for binary, 'P' for BASIC (program), 'A' for ASCII, '?' unknown.
static char detect_block_type(const uint8_t* data, size_t size, size_t pos) {
    if (pos + 10 > size) return '?';

    bool all_ea = true, all_d3 = true;
    for (int i = 0; i < 10; ++i) {
        if (data[pos + i] != 0xEA) all_ea = false;
        if (data[pos + i] != 0xD3) all_d3 = false;
    }

    if (all_d3) return 'P';  // BASIC program
    if (all_ea) return 'B';  // Binary (may also be ASCII, but binary takes priority)
    return '?';
}

// ============================================================================
// Internal: Parse CAS blocks
// ============================================================================

struct cas_parsed_t {
    tape::block_t   blocks[tape::MAX_BLOCKS];
    uint8_t*        data_bufs[tape::MAX_BLOCKS];   // Owned, must free
    size_t          data_sizes[tape::MAX_BLOCKS];
    int             count;
};

static void cas_parsed_free(cas_parsed_t& p) {
    for (int i = 0; i < p.count; ++i) {
        std::free(p.data_bufs[i]);
        p.data_bufs[i] = nullptr;
    }
}

static bool cas_parse(const uint8_t* data, size_t size, cas_parsed_t& out) {
    out.count = 0;

    size_t pos = find_cas_header(data, size, 0);
    if (pos >= size) return false;

    while (pos < size && out.count < tape::MAX_BLOCKS) {
        // Skip past CAS signature
        pos += CAS_HEADER_SIZE;
        if (pos >= size) break;

        char btype = detect_block_type(data, size, pos);

        if (btype == 'B') {
            // Binary block: 10× $EA + start_addr(2) + end_addr(2) + exec_addr(2) + data
            pos += 10;  // skip identifier
            if (pos + 6 > size) break;

            uint16_t start_addr = format_read_le16(data + pos);
            uint16_t end_addr   = format_read_le16(data + pos + 2);
            uint16_t exec_addr  = format_read_le16(data + pos + 4);
            pos += 6;

            size_t data_len = 0;
            if (end_addr >= start_addr)
                data_len = static_cast<size_t>(end_addr - start_addr + 1);

            // Find the next CAS header or end of file to delimit data
            size_t next_hdr = find_cas_header(data, size, pos);
            size_t avail = next_hdr - pos;
            if (data_len > avail) data_len = avail;

            tape::block_t& b = out.blocks[out.count];
            snprintf(b.name, sizeof(b.name), "BINARY_%d", out.count);
            b.type = tape::TAPE_BLOCK_CODE;
            b.load_addr = start_addr;
            b.exec_addr = exec_addr;
            b.autorun = true;

            out.data_bufs[out.count] = static_cast<uint8_t*>(std::malloc(data_len));
            if (!out.data_bufs[out.count]) break;
            std::memcpy(out.data_bufs[out.count], data + pos, data_len);
            out.data_sizes[out.count] = data_len;
            out.count++;

            pos = next_hdr;

        } else if (btype == 'P') {
            // BASIC block: 10× $D3 + filename(6) + data until next CAS header
            pos += 10;  // skip identifier
            if (pos + 6 > size) break;

            tape::block_t& b = out.blocks[out.count];
            std::memcpy(b.name, data + pos, 6);
            b.name[6] = '\0';
            // Trim trailing spaces
            for (int j = 5; j >= 0 && b.name[j] == ' '; --j) b.name[j] = '\0';
            pos += 6;

            b.type = tape::TAPE_BLOCK_BASIC;
            b.load_addr = 0x8001;  // MSX BASIC program area
            b.exec_addr = 0;
            b.autorun = true;

            // Data until next CAS header
            size_t next_hdr = find_cas_header(data, size, pos);
            size_t data_len = next_hdr - pos;

            out.data_bufs[out.count] = static_cast<uint8_t*>(std::malloc(data_len));
            if (!out.data_bufs[out.count]) break;
            std::memcpy(out.data_bufs[out.count], data + pos, data_len);
            out.data_sizes[out.count] = data_len;
            out.count++;

            pos = next_hdr;

        } else {
            // Unknown block type — skip to next CAS header
            pos = find_cas_header(data, size, pos);
        }
    }

    return out.count > 0;
}

// ============================================================================
// Identification
// ============================================================================

static float msx_cas_identify(const uint8_t* data, size_t file_size,
                              const char* extension) {
    // Content check: CAS signature at start
    if (data && file_size >= CAS_HEADER_SIZE) {
        if (std::memcmp(data, CAS_SIGNATURE, CAS_HEADER_SIZE) == 0)
            return 0.95f;
    }

    if (extension && format_ext_match(extension, ".cas")) return 0.7f;

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool msx_cas_load(const uint8_t* data, size_t size,
                         format_load_result_t* out) {
    if (!data || size < CAS_HEADER_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "MSX CAS: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    cas_parsed_t parsed{};
    if (!cas_parse(data, size, parsed)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "MSX CAS: No valid blocks found");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Prepare const pointer array for tape::load_best_entry
    const uint8_t* ptrs[tape::MAX_BLOCKS];
    for (int i = 0; i < parsed.count; ++i)
        ptrs[i] = parsed.data_bufs[i];

    bool ok = tape::load_best_entry(parsed.blocks, parsed.count,
                                     ptrs, parsed.data_sizes,
                                     "MSX CAS", out);

    cas_parsed_free(parsed);
    return ok;
}

// ============================================================================
// List Entries
// ============================================================================

static int msx_cas_list(const uint8_t* data, size_t size,
                        format_container_entry_t* entries, int max_entries) {
    if (!data || size < CAS_HEADER_SIZE) return -1;

    cas_parsed_t parsed{};
    if (!cas_parse(data, size, parsed)) return -1;

    int result = tape::list_entries_common(parsed.blocks, parsed.count,
                                            parsed.data_sizes,
                                            entries, max_entries);

    cas_parsed_free(parsed);
    return result;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* msx_cas_extensions[] = { ".cas", nullptr };

const format_descriptor_t MSX_CAS_FORMAT_DESCRIPTOR = {
    "MSX CAS",
    "MSX Cassette File",
    msx_cas_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_METADATA,
    msx_cas_identify,
    msx_cas_load,
    msx_cas_list,
    nullptr     // extract_entry
};

REGISTER_FORMAT(MSX_CAS, &MSX_CAS_FORMAT_DESCRIPTOR)
