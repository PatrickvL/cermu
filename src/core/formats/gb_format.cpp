/*
 * gb_format.cpp — Nintendo Game Boy / Game Boy Color ROM format
 *
 * GB ROM header at $100–$14F:
 *   $100–$103: Entry point (NOP + JP)
 *   $104–$133: Nintendo logo (48 bytes, verified by boot ROM)
 *   $134–$143: Title (16 bytes, shortened to 11 on CGB)
 *   $143:      CGB flag ($80=GBC compatible, $C0=GBC only)
 *   $147:      Cartridge type (MBC1, MBC2, MBC3, MBC5, etc.)
 *   $148:      ROM size (0=32KB, 1=64KB, 2=128KB, ...)
 *   $149:      RAM size (0=none, 1=2KB, 2=8KB, 3=32KB)
 *   $14D:      Header checksum
 *   $14E–$14F: Global checksum
 */

#include "core/formats/gb_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

// Nintendo logo at $104–$133 (first 8 bytes used for quick check)
static constexpr uint8_t NINTENDO_LOGO_HEAD[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B
};

// ── Identify ─────────────────────────────────────────────────────────────────

static float gb_identify(const uint8_t* data, size_t size, const char* ext) {
    // Check for Nintendo logo at $104
    if (size >= 0x150 && std::memcmp(&data[0x104], NINTENDO_LOGO_HEAD, 8) == 0) {
        // Verify header checksum
        uint8_t cksum = 0;
        for (int i = 0x134; i <= 0x14C; i++)
            cksum = cksum - data[i] - 1;
        if (cksum == data[0x14D])
            return 0.98f;
        return 0.90f;  // Logo match but bad checksum
    }
    if (ext) {
        if (format_ext_match(ext, "gb"))  return 0.85f;
        if (format_ext_match(ext, "gbc")) return 0.85f;
    }
    return 0.0f;
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool gb_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 0x150 || !out) return false;

    out->type = FORMAT_LOAD_PROGRAM;

    out->program.data = new uint8_t[size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;

    // Extract title from header ($134–$143) into program_title
    char title[17] = {};
    std::memcpy(title, &data[0x134], 16);
    for (int i = 15; i >= 0 && (title[i] == '\0' || title[i] == ' '); i--)
        title[i] = '\0';
    if (title[0] >= 0x20) {
        size_t len = strlen(title);
        if (len < sizeof(out->metadata)) {
            std::memcpy(out->metadata, title, len + 1);
            out->metadata_size = len + 1;
        }
    }

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* gb_extensions[] = {"gb", "gbc", nullptr};

const format_descriptor_t GB_FORMAT_DESCRIPTOR = {
    "Game Boy ROM",
    "Nintendo Game Boy / Game Boy Color ROM image",
    gb_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    gb_identify,
    gb_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(GB, &GB_FORMAT_DESCRIPTOR);
