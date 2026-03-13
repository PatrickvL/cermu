/**
 * Spectrum TAP Format Handler — Implementation
 *
 * Parses ZX Spectrum .tap tape files.  Identifies the first Code or Program
 * block and extracts it as a loadable program.
 *
 * For BASIC programs (type 0), the load address is implicitly the BASIC system
 * area.  For CODE blocks (type 3), param1 contains the explicit load address.
 *
 * The format also supports container-style browsing: list_entries() returns
 * all header+data block pairs found in the file.
 */

#include "core/formats/spectrum_tap_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Internal: Parse all blocks from a .tap file
// ============================================================================

struct tap_block_t {
    uint8_t  flag;           // $00 = header, $FF = data
    const uint8_t* payload;  // Pointer into source data
    size_t   payload_size;   // Excluding flag and checksum
    bool     valid_checksum;
};

static int tap_parse_blocks(const uint8_t* data, size_t size,
                            tap_block_t* blocks, int max_blocks) {
    int count = 0;
    size_t pos = 0;

    while (pos + 2 <= size && count < max_blocks) {
        uint16_t block_len = format_read_le16(data + pos);
        pos += 2;
        if (block_len == 0 || pos + block_len > size) break;

        tap_block_t& b = blocks[count];
        b.flag = data[pos];
        b.payload = data + pos + 1;
        b.payload_size = block_len >= 2 ? block_len - 2 : 0;  // Minus flag and checksum

        // Validate checksum (XOR of all bytes in the block)
        uint8_t xor_check = 0;
        for (uint16_t i = 0; i < block_len; ++i)
            xor_check ^= data[pos + i];
        b.valid_checksum = (xor_check == 0);

        pos += block_len;
        count++;
    }
    return count;
}

// Parse a 17-byte header payload
static bool tap_parse_header(const tap_block_t& block, spectrum_tap_header_t* out) {
    if (block.flag != 0x00 || block.payload_size < 17) return false;
    std::memset(out, 0, sizeof(*out));

    const uint8_t* p = block.payload;
    out->type = p[0];
    std::memcpy(out->filename, p + 1, 10);
    out->filename[10] = '\0';
    // Trim trailing spaces
    for (int i = 9; i >= 0 && out->filename[i] == ' '; --i)
        out->filename[i] = '\0';
    out->data_length = format_read_le16(p + 11);
    out->param1      = format_read_le16(p + 13);
    out->param2      = format_read_le16(p + 15);
    return true;
}

// ============================================================================
// Format Identification
// ============================================================================

static float spectrum_tap_identify(const uint8_t* data, size_t file_size,
                                   const char* extension) {
    // The Commodore TAP uses "C64-TAPE-RAW" or "C16-TAPE-RAW" magic.
    // Spectrum TAP has no magic — differentiate by content.
    bool ext_match = extension && format_ext_match(extension, ".tap");

    // Reject Commodore TAP files (they have a 12-byte signature)
    if (file_size >= 12) {
        if (std::memcmp(data, "C64-TAPE-RAW", 12) == 0 ||
            std::memcmp(data, "C16-TAPE-RAW", 12) == 0) {
            return 0.0f;
        }
    }

    // Try parsing as Spectrum TAP: first block should be valid
    if (file_size >= 21) {  // Minimum: 2(len) + 19(header block)
        uint16_t first_len = format_read_le16(data);
        if (first_len == 19 && data[2] == 0x00) {
            // Looks like a standard header block (19 bytes, flag=0)
            uint8_t type = data[3];
            if (type <= 3) {
                // Valid header type — high confidence
                if (ext_match) return 0.95f;
                return 0.7f;
            }
        }
    }

    if (ext_match) return 0.5f;  // Extension matches but can't verify content
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool spectrum_tap_load(const uint8_t* data, size_t size,
                              format_load_result_t* out) {
    // Parse all blocks
    constexpr int MAX_BLOCKS = 256;
    tap_block_t blocks[MAX_BLOCKS];
    int block_count = tap_parse_blocks(data, size, blocks, MAX_BLOCKS);

    if (block_count < 2) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Spectrum TAP: No valid blocks found");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Collect all header+data pairs as separate loadable programs.
    // TAP files commonly contain a BASIC loader followed by one or more
    // CODE blocks.  We return them all so the system can place each block
    // at its correct address.
    out->file_count = 0;

    for (int i = 0; i < block_count - 1; ++i) {
        spectrum_tap_header_t header;
        if (!tap_parse_header(blocks[i], &header)) continue;

        // Next block should be data (flag = $FF)
        const tap_block_t& data_block = blocks[i + 1];
        if (data_block.flag != 0xFF) continue;

        // Determine load address
        uint16_t load_addr;
        if (header.type == SPECTRUM_TAP_CODE) {
            load_addr = header.param1;
        } else if (header.type == SPECTRUM_TAP_PROGRAM) {
            load_addr = 0x5CCB;  // Standard PROG location
        } else {
            i++;  // Skip data block
            continue;
        }

        size_t payload_size = data_block.payload_size;
        if (payload_size == 0) { i++; continue; }

        if (out->file_count < FORMAT_LOAD_MAX_FILES) {
            auto& prog = out->files[out->file_count];
            prog.data = static_cast<uint8_t*>(std::malloc(payload_size));
            if (!prog.data) continue;
            std::memcpy(prog.data, data_block.payload, payload_size);
            prog.data_size = payload_size;
            prog.load_addr = load_addr;
            prog.end_addr  = static_cast<uint16_t>(load_addr + payload_size);

            printf("Spectrum TAP: Block %d '%s' (type=%d, addr=$%04X, size=%zu)\n",
                   out->file_count, header.filename, header.type, load_addr, payload_size);
            out->file_count++;
        }

        // Store the first header in metadata (for system-level inspection)
        if (out->file_count == 1) {
            static_assert(sizeof(spectrum_tap_header_t) <= FORMAT_METADATA_MAX_SIZE,
                          "spectrum_tap_header_t must fit in metadata blob");
            std::memcpy(out->metadata, &header, sizeof(header));
            out->metadata_size = sizeof(header);
        }

        i++;  // Skip the data block
    }

    if (out->file_count == 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Spectrum TAP: No loadable Program or Code block found");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Single block → PROGRAM, multiple blocks → ARCHIVE
    if (out->file_count == 1) {
        out->program = out->files[0];
        out->files[0] = {};  // Ownership moved to program
        out->file_count = 0;
        out->type = FORMAT_LOAD_PROGRAM;
    } else {
        out->type = FORMAT_LOAD_ARCHIVE;
    }
    return true;
}

// ============================================================================
// Container: List Entries
// ============================================================================

static int spectrum_tap_list_entries(const uint8_t* data, size_t size,
                                     format_container_entry_t* entries,
                                     int max_entries) {
    constexpr int MAX_BLOCKS = 256;
    tap_block_t blocks[MAX_BLOCKS];
    int block_count = tap_parse_blocks(data, size, blocks, MAX_BLOCKS);

    int entry_count = 0;
    for (int i = 0; i < block_count - 1 && entry_count < max_entries; ++i) {
        spectrum_tap_header_t header;
        if (!tap_parse_header(blocks[i], &header)) continue;

        // Next block should be data
        const tap_block_t& data_block = blocks[i + 1];
        if (data_block.flag != 0xFF) continue;

        format_container_entry_t& e = entries[entry_count];
        const char* type_ext = ".bin";
        if (header.type == SPECTRUM_TAP_PROGRAM)  type_ext = ".bas";
        else if (header.type == SPECTRUM_TAP_CODE) type_ext = ".cod";

        snprintf(e.display_name, sizeof(e.display_name), "%s%s",
                 header.filename, type_ext);
        e.size = data_block.payload_size;
        e.index = i;
        entry_count++;
        i++;  // Skip the data block
    }
    return entry_count;
}

// ============================================================================
// Format Descriptor
// ============================================================================

// Note: extension ".tap" is shared with Commodore TAP.
// The identify() function differentiates by checking for Commodore magic bytes.
static const char* spectrum_tap_extensions[] = { ".tap", nullptr };

const format_descriptor_t SPECTRUM_TAP_FORMAT_DESCRIPTOR = {
    "Spectrum TAP",
    "ZX Spectrum Tape File",
    spectrum_tap_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    spectrum_tap_identify,
    spectrum_tap_load,
    spectrum_tap_list_entries,
    nullptr   // extract_entry — TODO
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(SPECTRUM_TAP, &SPECTRUM_TAP_FORMAT_DESCRIPTOR)
