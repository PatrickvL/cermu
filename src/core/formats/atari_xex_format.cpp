/*
 * atari_xex_format.cpp — Atari 8-bit XEX/COM executable format
 *
 * Atari DOS executable format:
 *   - $FFFF header (marks start-of-file, or segment boundary)
 *   - 2 bytes: segment start address (little-endian)
 *   - 2 bytes: segment end address (little-endian)
 *   - Data bytes (end - start + 1)
 *   - Repeat for additional segments
 *
 * Special vectors set by segments:
 *   $02E0–$02E1: RUNAD (run address — CPU starts here)
 *   $02E2–$02E3: INITAD (init address — called after each segment load)
 */

#include "core/formats/atari_xex_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

static constexpr uint16_t XEX_HEADER = 0xFFFF;

// ── Identify ─────────────────────────────────────────────────────────────────

static float atari_xex_identify(const uint8_t* data, size_t size, const char* ext) {
    // Check for $FFFF header
    if (size >= 6 && data[0] == 0xFF && data[1] == 0xFF) {
        uint16_t start = data[2] | (data[3] << 8);
        uint16_t end   = data[4] | (data[5] << 8);
        if (end >= start && end < 0xD000)
            return 0.90f;
    }
    if (ext && (format_ext_match(ext, "xex") || format_ext_match(ext, "com")))
        return 0.70f;
    return 0.0f;
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool atari_xex_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 6 || !out) return false;
    if (data[0] != 0xFF || data[1] != 0xFF) return false;

    out->type = FORMAT_LOAD_PROGRAM;

    // Provide raw XEX data — the system loader handles segment parsing
    out->program.data = new uint8_t[size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* atari_xex_extensions[] = {"xex", "com", "exe", "atr", nullptr};

const format_descriptor_t ATARI_XEX_FORMAT_DESCRIPTOR = {
    "Atari XEX",
    "Atari 8-bit executable (segmented load)",
    atari_xex_extensions,
    FORMAT_CAP_LOADABLE,
    atari_xex_identify,
    atari_xex_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(ATARI_XEX, &ATARI_XEX_FORMAT_DESCRIPTOR);
