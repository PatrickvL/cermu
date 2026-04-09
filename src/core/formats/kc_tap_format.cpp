/**
 * KC TAP Format Handler — Implementation
 *
 * Parses KC85 .tap / .kcc tape files.
 * Identifies by the block structure and typical KC header patterns.
 *
 * Supports:
 *   - KC TAP: block-structured with header block containing filename/type
 *   - KCC: simpler headerless format (128-byte blocks with checksums)
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/kc_tap_format.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t KC_BLOCK_SIZE     = 130;   // 1 (blk#) + 128 (data) + 1 (checksum)
static constexpr size_t KC_DATA_PER_BLOCK = 128;

// Header block file type bytes
static constexpr uint8_t KC_TYPE_COM   = 0xC3;  // Machine code
static constexpr uint8_t KC_TYPE_BASIC = 0xD3;  // BASIC program

// A KC TAP file starts with a signature: $C3 $4B $43 $2D $54 $41 $50 ("·KC-TAP")
// followed by $00 (NUL) and tape data blocks
static constexpr size_t KC_TAP_MAGIC_SIZE = 16;
static const uint8_t KC_TAP_MAGIC[] = {
    0xC3, 'K', 'C', '-', 'T', 'A', 'P', 0x00
};

// ============================================================================
// Internal: Verify block checksum
// ============================================================================

static bool kc_verify_block(const uint8_t* block) {
    uint8_t sum = 0;
    for (int i = 0; i < 129; ++i)  // block# + 128 data bytes
        sum += block[i];
    return sum == block[129];
}

// ============================================================================
// Internal: Parse KC TAP format
// ============================================================================

static bool kc_tap_parse(const uint8_t* data, size_t size,
                         tape::block_t& block,
                         uint8_t** out_data, size_t* out_size) {
    // Check for KC-TAP signature
    bool has_kctap_header = false;
    size_t pos = 0;

    if (size >= 16 && std::memcmp(data, KC_TAP_MAGIC, 8) == 0) {
        has_kctap_header = true;
        pos = 16;  // skip "KC-TAP" header + padding
    }

    // First block should be block $00 (header block) or $01 (data)
    if (pos + KC_BLOCK_SIZE > size) return false;

    // Try reading header block (block number $00)
    const uint8_t* hdr_block = data + pos;
    if (hdr_block[0] == 0x00) {
        // Header block: filename at offset 1 (8 bytes)
        std::memcpy(block.name, hdr_block + 1, 8);
        block.name[8] = '\0';
        // Trim trailing spaces
        for (int j = 7; j >= 0 && block.name[j] == ' '; --j)
            block.name[j] = '\0';

        uint8_t file_type = hdr_block[9];
        block.type = (file_type == KC_TYPE_COM) ? tape::TAPE_BLOCK_CODE
                   : (file_type == KC_TYPE_BASIC) ? tape::TAPE_BLOCK_BASIC
                   : tape::TAPE_BLOCK_UNKNOWN;

        pos += KC_BLOCK_SIZE;
    } else {
        // No header block — KCC format, use generic name
        snprintf(block.name, sizeof(block.name), "KC_PROGRAM");
        block.type = tape::TAPE_BLOCK_CODE;
    }

    // First data block ($01) may contain address info for COM files
    if (pos + KC_BLOCK_SIZE > size) return false;

    const uint8_t* first_data = data + pos;
    if (first_data[0] == 0x01 && block.type == tape::TAPE_BLOCK_CODE) {
        // COM file: block 1 contains load_addr(2) + end_addr(2) + exec_addr(2)
        if (KC_DATA_PER_BLOCK >= 6) {
            block.load_addr = format_read_le16(first_data + 1);
            uint16_t end_addr = format_read_le16(first_data + 3);
            block.exec_addr = format_read_le16(first_data + 5);
            block.autorun = true;
            (void)end_addr;
        }
    } else {
        // BASIC or unknown: default addresses
        block.load_addr = 0x0401;  // KC85 BASIC program area
        block.exec_addr = 0;
        block.autorun = (block.type == tape::TAPE_BLOCK_BASIC);
    }

    // Collect all data blocks
    size_t alloc = 65536;
    uint8_t* buf = static_cast<uint8_t*>(std::malloc(alloc));
    if (!buf) return false;

    size_t total = 0;
    while (pos + KC_BLOCK_SIZE <= size) {
        const uint8_t* blk = data + pos;
        uint8_t blk_num = blk[0];

        // End marker: block $FF
        if (blk_num == 0xFF) break;

        // Skip header block
        if (blk_num == 0x00) {
            pos += KC_BLOCK_SIZE;
            continue;
        }

        // Copy data (skip block number byte, skip checksum)
        size_t data_start = 1;
        size_t data_len = KC_DATA_PER_BLOCK;

        // For block 1 of COM files, skip the address bytes
        if (blk_num == 0x01 && block.type == tape::TAPE_BLOCK_CODE) {
            data_start = 7;   // skip blk# + 3×addr
            data_len = KC_DATA_PER_BLOCK - 6;
        }

        if (total + data_len > alloc) {
            alloc *= 2;
            uint8_t* bigger = static_cast<uint8_t*>(std::realloc(buf, alloc));
            if (!bigger) { std::free(buf); return false; }
            buf = bigger;
        }

        std::memcpy(buf + total, blk + data_start, data_len);
        total += data_len;
        pos += KC_BLOCK_SIZE;
    }

    *out_data = buf;
    *out_size = total;
    return total > 0;
}

// ============================================================================
// Identification
// ============================================================================

static float kc_tap_identify(const uint8_t* data, size_t file_size,
                             const char* extension) {
    // KC-TAP signature
    if (data && file_size >= 16 && std::memcmp(data, KC_TAP_MAGIC, 8) == 0)
        return 0.95f;

    // Check for valid KC block structure (block 0 header)
    if (data && file_size >= KC_BLOCK_SIZE) {
        if (data[0] == 0x00 &&
            (data[9] == KC_TYPE_COM || data[9] == KC_TYPE_BASIC))
            return 0.6f;
    }

    // Extension checks — .tap is very ambiguous, .kcc is KC-specific
    if (extension) {
        if (format_ext_match(extension, ".kcc")) return 0.85f;
        // .tap too ambiguous — don't claim it here (Oric, C64, Spectrum all use it)
    }

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool kc_tap_load(const uint8_t* data, size_t size,
                        format_load_result_t* out) {
    if (!data || size < KC_BLOCK_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "KC TAP: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    tape::block_t block{};
    uint8_t* file_data = nullptr;
    size_t file_size = 0;

    if (!kc_tap_parse(data, size, block, &file_data, &file_size)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "KC TAP: Failed to parse tape data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    const uint8_t* ptr = file_data;
    bool ok = tape::load_best_entry(&block, 1, &ptr, &file_size,
                                     "KC TAP", out);

    std::free(file_data);
    return ok;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* kc_tap_extensions[] = { ".kcc", ".tap", nullptr };

const format_descriptor_t KC_TAP_FORMAT_DESCRIPTOR = {
    "KC TAP",
    "KC85 Tape File (TAP/KCC)",
    kc_tap_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    kc_tap_identify,
    kc_tap_load,
    nullptr,    // list_entries — single file per tape
    nullptr     // extract_entry
};

REGISTER_FORMAT(KC_TAP, &KC_TAP_FORMAT_DESCRIPTOR)
