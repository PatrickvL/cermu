/**
 * Oric TAP Format Handler — Implementation
 *
 * Parses Oric-1/Atmos .tap tape files.
 * Identifies by the $16 $16 $16 $24 sync preamble.
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/oric_tap_format.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t ORIC_TAP_MIN_HEADER = 13;  // Minimum header before filename
static constexpr uint8_t ORIC_SYNC_BYTE     = 0x16;
static constexpr uint8_t ORIC_SYNC_MARKER   = 0x24;

// ============================================================================
// Internal: Find the next program in an Oric TAP stream
// ============================================================================

/// Locate the sync preamble ($16 $16 $16 $24) starting at *pos.
/// Returns the offset of the first byte AFTER the sync marker,
/// or size if not found.
static size_t find_sync(const uint8_t* data, size_t size, size_t pos) {
    while (pos + 3 < size) {
        if (data[pos] == ORIC_SYNC_BYTE &&
            data[pos + 1] == ORIC_SYNC_BYTE &&
            data[pos + 2] == ORIC_SYNC_BYTE &&
            data[pos + 3] == ORIC_SYNC_MARKER) {
            return pos + 4;
        }
        pos++;
    }
    return size;
}

/// Parse a single Oric TAP program entry at the given offset.
/// Returns the offset past the program data, or 0 on failure.
static size_t parse_entry(const uint8_t* data, size_t size, size_t pos,
                          tape::block_t& block, const uint8_t** out_data,
                          size_t* out_size) {
    // Need at least: unused(1) + type(1) + autorun(1) + end_addr(2) + start_addr(2) + unused(1) = 8
    if (pos + 8 > size) return 0;

    // uint8_t unused0  = data[pos];        // byte after sync marker
    uint8_t file_type   = data[pos + 1];
    uint8_t autorun_flag = data[pos + 2];
    uint16_t end_addr   = format_read_be16(data + pos + 3);
    uint16_t start_addr = format_read_be16(data + pos + 5);
    // uint8_t unused1  = data[pos + 7];

    pos += 8;

    // Filename: NUL-terminated string
    size_t name_start = pos;
    while (pos < size && data[pos] != 0x00) pos++;
    size_t name_len = pos - name_start;
    if (name_len > 32) name_len = 32;
    std::memcpy(block.name, data + name_start, name_len);
    block.name[name_len] = '\0';

    if (pos < size) pos++;  // skip NUL terminator

    block.type = (file_type & 0x80) ? tape::TAPE_BLOCK_CODE : tape::TAPE_BLOCK_BASIC;
    block.autorun = (autorun_flag == 0xC7);
    block.load_addr = start_addr;
    block.exec_addr = (block.type == tape::TAPE_BLOCK_CODE) ? start_addr : 0;

    // Data: start_addr to end_addr (inclusive range -> size = end_addr - start_addr + 1)
    size_t data_len = 0;
    if (end_addr >= start_addr) {
        data_len = static_cast<size_t>(end_addr - start_addr + 1);
    }
    if (pos + data_len > size) {
        data_len = size - pos;  // truncate to available data
    }

    *out_data = data + pos;
    *out_size = data_len;

    return pos + data_len;
}

// ============================================================================
// Identification
// ============================================================================

static float oric_tap_identify(const uint8_t* data, size_t file_size,
                               const char* extension) {
    // Content check: sync preamble
    if (data && file_size >= ORIC_TAP_MIN_HEADER) {
        if (data[0] == ORIC_SYNC_BYTE &&
            data[1] == ORIC_SYNC_BYTE &&
            data[2] == ORIC_SYNC_BYTE &&
            data[3] == ORIC_SYNC_MARKER) {
            return 0.9f;
        }
    }

    // Extension fallback — but .tap is shared with C64 and Spectrum
    if (extension && format_ext_match(extension, ".tap")) {
        // Low confidence since .tap is ambiguous across systems
        return 0.3f;
    }

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool oric_tap_load(const uint8_t* data, size_t size,
                          format_load_result_t* out) {
    if (!data || size < ORIC_TAP_MIN_HEADER) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Oric TAP: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    tape::block_t blocks[tape::MAX_BLOCKS];
    const uint8_t* data_ptrs[tape::MAX_BLOCKS];
    size_t data_sizes[tape::MAX_BLOCKS];
    int count = 0;

    size_t pos = 0;
    while (count < tape::MAX_BLOCKS) {
        // Find next sync preamble
        pos = find_sync(data, size, pos);
        if (pos >= size) break;

        size_t next = parse_entry(data, size, pos, blocks[count],
                                  &data_ptrs[count], &data_sizes[count]);
        if (next == 0) break;
        count++;
        pos = next;
    }

    if (count == 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Oric TAP: No valid programs found");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    return tape::load_best_entry(blocks, count, data_ptrs, data_sizes,
                                 "Oric TAP", out);
}

// ============================================================================
// List Entries (multi-program tapes)
// ============================================================================

static int oric_tap_list(const uint8_t* data, size_t size,
                         format_container_entry_t* entries, int max_entries) {
    if (!data || size < ORIC_TAP_MIN_HEADER) return -1;

    tape::block_t blocks[tape::MAX_BLOCKS];
    const uint8_t* data_ptrs[tape::MAX_BLOCKS];
    size_t data_sizes[tape::MAX_BLOCKS];
    int count = 0;

    size_t pos = 0;
    while (count < tape::MAX_BLOCKS) {
        pos = find_sync(data, size, pos);
        if (pos >= size) break;

        size_t next = parse_entry(data, size, pos, blocks[count],
                                  &data_ptrs[count], &data_sizes[count]);
        if (next == 0) break;
        count++;
        pos = next;
    }

    return tape::list_entries_common(blocks, count, data_sizes,
                                     entries, max_entries);
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* oric_tap_extensions[] = { ".tap", nullptr };

const format_descriptor_t ORIC_TAP_FORMAT_DESCRIPTOR = {
    "Oric TAP",
    "Oric Tape File",
    oric_tap_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_METADATA,
    oric_tap_identify,
    oric_tap_load,
    oric_tap_list,
    nullptr     // extract_entry
};

REGISTER_FORMAT(ORIC_TAP, &ORIC_TAP_FORMAT_DESCRIPTOR)
