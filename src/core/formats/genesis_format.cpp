/*
 * genesis_format.cpp — Sega Genesis / Mega Drive ROM format
 *
 * Raw Genesis ROMs have a header at offset $100:
 *   $100–$10F: "SEGA MEGA DRIVE" or "SEGA GENESIS" (16 bytes)
 *   $110–$11F: Copyright / date
 *   $120–$14F: Domestic name (48 bytes)
 *   $150–$17F: International name (48 bytes)
 *   $180–$18D: Serial number
 *   $18E–$18F: Checksum
 *   $1A0–$1A3: ROM start address
 *   $1A4–$1A7: ROM end address
 *   $1F0–$1FF: Region codes ("JUE")
 *
 * SMD-format ROMs have a 512-byte header followed by 16KB interleaved
 * blocks (even bytes, then odd bytes).
 */

#include "core/formats/genesis_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

// ── Identify ─────────────────────────────────────────────────────────────────

static float genesis_identify(const uint8_t* data, size_t size, const char* ext) {
    // Check for "SEGA" string at $100 (raw ROM)
    if (size >= 0x110) {
        if (std::memcmp(&data[0x100], "SEGA", 4) == 0)
            return 0.95f;
    }
    // Check for SMD header (512 bytes, first two bytes = block count + flags)
    if (size >= 0x310 && (size % 16384) == 512) {
        if (std::memcmp(&data[0x300], "SEGA", 4) == 0)
            return 0.85f;  // SMD with header
    }
    // Extension-based
    if (ext) {
        if (format_ext_match(ext, "md") || format_ext_match(ext, "gen"))
            return 0.80f;
        if (format_ext_match(ext, "smd"))
            return 0.75f;
    }
    return 0.0f;
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool genesis_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 512 || !out) return false;

    out->type = FORMAT_LOAD_PROGRAM;

    // Detect and skip SMD header (512 bytes, non-power-of-2 aligned)
    size_t offset = 0;
    if ((size % 16384) == 512 && size > 512) {
        offset = 512;  // Skip SMD header
    }

    size_t rom_size = size - offset;
    out->program.data = new uint8_t[rom_size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data + offset, rom_size);
    out->program.data_size = rom_size;
    out->program.load_addr = 0;

    // Extract title from header
    if (rom_size >= 0x180) {
        const uint8_t* rom = out->program.data;
        char title[49] = {};
        std::memcpy(title, &rom[0x150], 48);
        for (int i = 47; i >= 0 && (title[i] == ' ' || title[i] == '\0'); i--)
            title[i] = '\0';
        if (title[0] >= 0x20) {
            size_t len = strlen(title);
            if (len < sizeof(out->metadata)) {
                std::memcpy(out->metadata, title, len + 1);
                out->metadata_size = len + 1;
            }
        }
    }

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* genesis_extensions[] = {"md", "gen", "smd", "bin", nullptr};

const format_descriptor_t GENESIS_FORMAT_DESCRIPTOR = {
    "Genesis ROM",
    "Sega Genesis / Mega Drive ROM image",
    genesis_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    genesis_identify,
    genesis_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(GENESIS, &GENESIS_FORMAT_DESCRIPTOR);
