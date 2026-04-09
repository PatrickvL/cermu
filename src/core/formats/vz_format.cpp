/**
 * VZ Format Handler — Implementation
 *
 * Parses VTech VZ200/VZ300 tape files (.vz).
 * Identifies by "VZF0"/"VZF1" magic header.
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/vz_format.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t VZ_HEADER_SIZE  = 24;   // 4 magic + 17 name + 1 type + 2 addr
static constexpr size_t VZ_NAME_OFFSET  = 4;
static constexpr size_t VZ_NAME_LEN     = 17;
static constexpr size_t VZ_TYPE_OFFSET  = 21;   // $15
static constexpr size_t VZ_ADDR_OFFSET  = 22;   // $16

static constexpr uint8_t VZ_TYPE_BASIC  = 0xF0;
static constexpr uint8_t VZ_TYPE_CODE   = 0xF1;

// ============================================================================
// Identification
// ============================================================================

static float vz_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Content check: "VZF0" or "VZF1" magic
    if (data && file_size >= VZ_HEADER_SIZE) {
        if (data[0] == 'V' && data[1] == 'Z' && data[2] == 'F' &&
            (data[3] == '0' || data[3] == '1')) {
            return 0.95f;
        }
    }

    if (extension && format_ext_match(extension, ".vz")) return 0.8f;

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool vz_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < VZ_HEADER_SIZE) {
        snprintf(out->error_msg, sizeof(out->error_msg), "VZ: File too small");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Parse header into a tape block
    tape::block_t block{};

    // Filename: bytes 4-20, NUL-padded
    std::memcpy(block.name, data + VZ_NAME_OFFSET, VZ_NAME_LEN);
    block.name[VZ_NAME_LEN] = '\0';
    // Trim trailing spaces/NULs
    for (int j = VZ_NAME_LEN - 1; j >= 0 && (block.name[j] == ' ' || block.name[j] == '\0'); --j)
        block.name[j] = '\0';

    // File type
    uint8_t raw_type = data[VZ_TYPE_OFFSET];
    block.type = (raw_type == VZ_TYPE_CODE) ? tape::TAPE_BLOCK_CODE : tape::TAPE_BLOCK_BASIC;
    block.autorun = (block.type == tape::TAPE_BLOCK_BASIC);

    // Load address
    block.load_addr = format_read_le16(data + VZ_ADDR_OFFSET);
    block.exec_addr = (block.type == tape::TAPE_BLOCK_CODE) ? block.load_addr : 0;

    // Data follows the header
    const uint8_t* payload = data + VZ_HEADER_SIZE;
    size_t payload_size = size - VZ_HEADER_SIZE;

    return tape::load_best_entry(&block, 1, &payload, &payload_size, "VZ", out);
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* vz_extensions[] = { ".vz", nullptr };

const format_descriptor_t VZ_FORMAT_DESCRIPTOR = {
    "VZ",
    "VTech VZ200/VZ300 Tape File",
    vz_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    vz_identify,
    vz_load,
    nullptr,    // list_entries — single file per tape
    nullptr     // extract_entry
};

REGISTER_FORMAT(VZ, &VZ_FORMAT_DESCRIPTOR)
