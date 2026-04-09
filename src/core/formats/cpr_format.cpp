/**
 * CPR Format Handler — Implementation
 *
 * Parses Amstrad CPC Plus .cpr cartridge files (RIFF/AMS! container).
 * Identifies by "RIFF" magic + "AMS!" form type.
 *
 * Produces a single FORMAT_LOAD_RAW result containing all ROM banks
 * concatenated sequentially (bank 0 first).  The cpr_header_t metadata
 * tells the system how many banks are present.
 */

#include "core/cermu.hpp"
#include "core/formats/cpr_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t CPR_RIFF_HEADER_SIZE = 12;  // "RIFF" + size + "AMS!"
static constexpr size_t CPR_CHUNK_HEADER_SIZE = 8;  // chunk_id(4) + size(4)
static constexpr size_t CPR_BANK_SIZE = 16384;      // 16 KB per bank
static constexpr int    CPR_MAX_BANKS = 32;

// ============================================================================
// Identification
// ============================================================================

static float cpr_identify(const uint8_t* data, size_t file_size,
                          const char* extension) {
    // Content check: "RIFF" + skip 4 bytes + "AMS!"
    if (data && file_size >= CPR_RIFF_HEADER_SIZE) {
        if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'A' && data[9] == 'M' && data[10] == 'S' && data[11] == '!') {
            return 0.98f;   // Very unambiguous
        }
    }

    if (extension && format_ext_match(extension, ".cpr")) return 0.85f;

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool cpr_load(const uint8_t* data, size_t size,
                     format_load_result_t* out) {
    if (!data || size < CPR_RIFF_HEADER_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg), "CPR: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Verify RIFF + AMS! signature
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F' ||
        data[8] != 'A' || data[9] != 'M' || data[10] != 'S' || data[11] != '!') {
        snprintf(out->error_msg, sizeof(out->error_msg), "CPR: Invalid RIFF/AMS! header");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Allocate output buffer for max 32 × 16 KB = 512 KB
    size_t max_rom = CPR_MAX_BANKS * CPR_BANK_SIZE;
    uint8_t* rom_buf = static_cast<uint8_t*>(std::calloc(1, max_rom));
    if (!rom_buf) {
        snprintf(out->error_msg, sizeof(out->error_msg), "CPR: Out of memory");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Walk RIFF chunks looking for "cbNN" bank chunks
    size_t pos = CPR_RIFF_HEADER_SIZE;
    int num_banks = 0;
    uint32_t total_rom = 0;

    while (pos + CPR_CHUNK_HEADER_SIZE <= size) {
        char chunk_id[5];
        std::memcpy(chunk_id, data + pos, 4);
        chunk_id[4] = '\0';
        uint32_t chunk_size = format_read_le32(data + pos + 4);
        pos += CPR_CHUNK_HEADER_SIZE;

        // Check for "cbNN" bank chunk
        if (chunk_id[0] == 'c' && chunk_id[1] == 'b' &&
            chunk_id[2] >= '0' && chunk_id[2] <= '9') {
            int bank_num = (chunk_id[2] - '0') * 10;
            if (chunk_id[3] >= '0' && chunk_id[3] <= '9') {
                bank_num += (chunk_id[3] - '0');
            } else {
                bank_num = chunk_id[2] - '0';  // single digit
            }

            if (bank_num < CPR_MAX_BANKS && pos + chunk_size <= size) {
                size_t copy_size = chunk_size;
                if (copy_size > CPR_BANK_SIZE) copy_size = CPR_BANK_SIZE;
                std::memcpy(rom_buf + bank_num * CPR_BANK_SIZE,
                            data + pos, copy_size);
                if (bank_num >= num_banks) num_banks = bank_num + 1;
                total_rom += static_cast<uint32_t>(copy_size);
            }
        }

        // Advance to next chunk (pad to even boundary per RIFF spec)
        pos += chunk_size;
        if (chunk_size & 1) pos++;
    }

    if (num_banks == 0) {
        std::free(rom_buf);
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "CPR: No ROM banks found in RIFF container");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Trim to actual size
    size_t actual_size = static_cast<size_t>(num_banks) * CPR_BANK_SIZE;
    out->program.data = rom_buf;
    out->program.data_size = actual_size;
    out->program.load_addr = 0;
    out->program.end_addr = 0;

    // Store bank info in metadata
    cpr_header_t hdr{};
    hdr.num_banks = static_cast<uint8_t>(num_banks);
    hdr.total_rom_size = total_rom;
    static_assert(sizeof(cpr_header_t) <= FORMAT_METADATA_MAX_SIZE, "");
    std::memcpy(out->metadata, &hdr, sizeof(hdr));
    out->metadata_size = sizeof(hdr);

    out->type = FORMAT_LOAD_RAW;

    log_info("CPR: Loaded %d ROM banks (%u KB total)\n",
             num_banks, total_rom / 1024);
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* cpr_extensions[] = { ".cpr", nullptr };

const format_descriptor_t CPR_FORMAT_DESCRIPTOR = {
    "CPR",
    "Amstrad CPC Plus Cartridge",
    cpr_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    cpr_identify,
    cpr_load,
    nullptr,    // list_entries
    nullptr     // extract_entry
};

REGISTER_FORMAT(CPR, &CPR_FORMAT_DESCRIPTOR)
