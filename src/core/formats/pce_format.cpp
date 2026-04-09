/*
 * pce_format.cpp — NEC PC Engine / TurboGrafx-16 HuCard format
 *
 * HuCard ROMs are headerless raw dumps.  Common sizes are powers-of-two
 * multiples of 8KB (256KB, 384KB, 512KB, 768KB, 1MB).
 *
 * Some dumps have a 512-byte copier header prepended — detected when
 * filesize % 8192 == 512.
 */

#include "core/formats/pce_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

// ── Identify ─────────────────────────────────────────────────────────────────

static float pce_identify(const uint8_t* data, size_t size, const char* ext) {
    (void)data;

    // Size heuristic: must be a multiple of 8KB (with optional 512-byte header)
    size_t rom_size = size;
    if ((size % 8192) == 512 && size > 512)
        rom_size = size - 512;

    bool valid_size = (rom_size >= 32768) && (rom_size % 8192 == 0) && (rom_size <= 1048576);

    if (ext && format_ext_match(ext, "pce")) {
        return valid_size ? 0.90f : 0.70f;
    }
    return 0.0f;  // No magic bytes — extension required
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool pce_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 8192 || !out) return false;

    // Skip copier header if present
    size_t offset = 0;
    if ((size % 8192) == 512 && size > 512)
        offset = 512;

    size_t rom_size = size - offset;

    out->type = FORMAT_LOAD_PROGRAM;

    out->program.data = new uint8_t[rom_size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data + offset, rom_size);
    out->program.data_size = rom_size;
    out->program.load_addr = 0;

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* pce_extensions[] = {"pce", nullptr};

const format_descriptor_t PCE_FORMAT_DESCRIPTOR = {
    "PC Engine HuCard",
    "NEC PC Engine / TurboGrafx-16 HuCard ROM",
    pce_extensions,
    FORMAT_CAP_LOADABLE,
    pce_identify,
    pce_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(PCE, &PCE_FORMAT_DESCRIPTOR);
