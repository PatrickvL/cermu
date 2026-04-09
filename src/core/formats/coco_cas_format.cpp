/*
 * coco_cas_format.cpp — TRS-80 / CoCo CAS cassette tape format
 *
 * CAS files wrap raw cassette data.  The format uses a leader (0x55 bytes),
 * a sync (0x3C), then blocks: type, length, data, checksum.
 * Block $00 = filename header, $01 = data, $FF = end-of-file.
 */

#include "core/formats/coco_cas_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

static constexpr uint8_t CAS_LEADER   = 0x55;
static constexpr uint8_t CAS_SYNC     = 0x3C;
static constexpr uint8_t BLOCK_NAME   = 0x00;
static constexpr uint8_t BLOCK_DATA   = 0x01;
static constexpr uint8_t BLOCK_EOF    = 0xFF;

// ── Identify ─────────────────────────────────────────────────────────────────

static float coco_cas_identify(const uint8_t* data, size_t size, const char* ext) {
    if (size >= 2 && data[0] == CAS_LEADER && data[1] == CAS_LEADER)
        return 0.90f;  // Starts with leader bytes
    if (ext && format_ext_match(ext, "cas"))
        return 0.70f;
    return 0.0f;
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool coco_cas_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 16 || !out) return false;

    // Find first sync byte after leader
    size_t pos = 0;
    while (pos < size && data[pos] == CAS_LEADER) pos++;
    if (pos >= size || data[pos] != CAS_SYNC) return false;
    pos++;  // skip sync

    // Read filename block
    if (pos >= size || data[pos] != BLOCK_NAME) return false;
    pos++;  // block type
    if (pos >= size) return false;
    uint8_t name_len = data[pos++];
    if (pos + name_len >= size) return false;

    // Extract metadata from filename block
    // Byte 0-7: filename, 8: file type, 9: data type, 10: gap flag, 11-12: exec addr
    char filename[9] = {};
    size_t copy_len = (name_len > 8) ? 8 : name_len;
    std::memcpy(filename, &data[pos], copy_len);

    // Skip to data blocks and concatenate
    // This is a simplified loader — full implementation would parse all blocks
    out->type = FORMAT_LOAD_RAW;

    // For now: provide the entire raw CAS as program data
    // The system's load_file() handles the actual parsing
    out->program.data = new uint8_t[size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* coco_cas_extensions[] = {"cas", nullptr};

const format_descriptor_t COCO_CAS_FORMAT_DESCRIPTOR = {
    "CoCo CAS",
    "TRS-80 Color Computer cassette tape image",
    coco_cas_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    coco_cas_identify,
    coco_cas_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

REGISTER_FORMAT(COCO_CAS, &COCO_CAS_FORMAT_DESCRIPTOR);
