/**
 * CRT Format Handler — Implementation
 */

#include "core/formats/crt_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstring>

// ============================================================================
// CRT Header Reading
// ============================================================================

bool commodore_crt_read_header_mem(const uint8_t* data, size_t data_size, commodore_crt_header_t* out_header) {
    if (!data || data_size < 64 || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    const uint8_t* raw = data;

    /* Validate signature — accept both C64 and VIC-20 */
    if (memcmp(raw, "C64 CARTRIDGE   ", 16) != 0 &&
        memcmp(raw, "VIC20 CARTRIDGE ", 16) != 0) {
        printf("CRTFormat: Invalid CRT signature\n");
        return false;
    }

    memcpy(out_header->signature, raw, 16);
    out_header->header_length = format_read_be32(raw + 16);
    out_header->version       = format_read_be16(raw + 20);
    out_header->hardware_type = format_read_be16(raw + 22);
    out_header->exrom         = raw[24];
    out_header->game          = raw[25];
    memcpy(out_header->reserved, raw + 26, 6);
    memcpy(out_header->name, raw + 32, 32);
    out_header->name[31] = '\0';

    printf("CRTFormat: \"%s\" hw_type=%d EXROM=%d GAME=%d\n",
           out_header->name, out_header->hardware_type,
           out_header->exrom, out_header->game);
    return true;
}

// ============================================================================
// Format Identification
// ============================================================================

static float crt_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (data && file_size >= 64) {
        if (memcmp(data, "C64 CARTRIDGE   ", 16) == 0) return 0.95f;
        if (memcmp(data, "VIC20 CARTRIDGE ", 16) == 0) return 0.95f;
    }
    if (extension && format_ext_match(extension, ".crt")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool crt_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    commodore_crt_header_t hdr;
    if (commodore_crt_read_header_mem(data, size, &hdr)) {
        out->type = FORMAT_LOAD_METADATA;
        memcpy(out->metadata, &hdr, sizeof(hdr));
        out->metadata_size = sizeof(hdr);
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg),
             "Failed to read CRT header from memory");
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// CHIP Packet Iterator
// ============================================================================

int commodore_crt_iterate_chips(
    const uint8_t* data, size_t data_size,
    const commodore_crt_header_t* header,
    commodore_crt_chip_callback_t callback,
    void* user_data)
{
    if (!data || !header || !callback) return -1;

    size_t offset = header->header_length;
    int count = 0;

    while (offset + 16 <= data_size) {
        // Validate CHIP signature
        if (memcmp(data + offset, "CHIP", 4) != 0) break;

        // Parse CHIP packet header (all fields big-endian)
        commodore_crt_chip_t chip;
        memcpy(chip.signature, data + offset, 4);
        chip.packet_length = format_read_be32(data + offset + 4);
        chip.chip_type     = format_read_be16(data + offset + 8);
        chip.bank_number   = format_read_be16(data + offset + 10);
        chip.load_address  = format_read_be16(data + offset + 12);
        chip.rom_size      = format_read_be16(data + offset + 14);

        // Validate packet bounds
        if (chip.packet_length < 16 || offset + chip.packet_length > data_size) {
            printf("CRTFormat: CHIP packet %d: invalid length %u at offset %zu\n",
                   count, chip.packet_length, offset);
            break;
        }

        // ROM payload starts immediately after the 16-byte CHIP header
        const uint8_t* rom_data = data + offset + 16;

        // Ensure rom_size doesn't exceed the packet payload
        uint32_t payload_size = chip.packet_length - 16;
        if (chip.rom_size > payload_size) {
            printf("CRTFormat: CHIP packet %d: rom_size %u > payload %u\n",
                   count, chip.rom_size, payload_size);
            break;
        }

        if (!callback(&chip, rom_data, user_data)) break;

        count++;
        offset += chip.packet_length;
    }

    return count;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* crt_extensions[] = { ".crt", NULL };

const format_descriptor_t CRT_FORMAT_DESCRIPTOR = {
    "CRT",
    "Cartridge Image",
    crt_extensions,
    FORMAT_CAP_METADATA,
    crt_identify,
    crt_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(CRT, &CRT_FORMAT_DESCRIPTOR)
