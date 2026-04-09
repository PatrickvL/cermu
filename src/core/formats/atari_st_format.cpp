/*
 * atari_st_format.cpp — Atari ST disk and program format
 *
 * GEMDOS PRG/TOS header (28 bytes):
 *   $00–$01: Magic ($601A)
 *   $02–$05: TEXT segment size
 *   $06–$09: DATA segment size
 *   $0A–$0D: BSS segment size
 *   $0E–$11: Symbol table size
 *   $12–$15: Reserved
 *   $16–$19: Reserved (no reloc if nonzero)
 *   $1A:     Relocation flag (0=relocatable)
 *
 * .st disk images: raw sector dump, 512 bytes/sector,
 *   typically 9 sectors × 80 tracks × 2 sides = 720KB.
 */

#include "core/formats/atari_st_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

static constexpr uint16_t PRG_MAGIC = 0x601A;

// ── Identify ─────────────────────────────────────────────────────────────────

static float atari_st_identify(const uint8_t* data, size_t size, const char* ext) {
    // Check for GEMDOS PRG/TOS header magic
    if (size >= 28 && data[0] == 0x60 && data[1] == 0x1A) {
        return 0.92f;
    }
    // Raw .st disk image: must be 720KB or 360KB
    if (size == 737280 || size == 368640) {
        if (ext && format_ext_match(ext, "st"))
            return 0.90f;
    }
    // Extension-based
    if (ext) {
        if (format_ext_match(ext, "prg") || format_ext_match(ext, "tos"))
            return 0.75f;
        if (format_ext_match(ext, "st"))
            return 0.70f;
    }
    return 0.0f;
}

// ── Load ─────────────────────────────────────────────────────────────────────

static bool atari_st_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size < 28 || !out) return false;

    out->type = FORMAT_LOAD_PROGRAM;

    // Detect PRG/TOS executable
    if (data[0] == 0x60 && data[1] == 0x1A) {
        // Parse GEMDOS header
        uint32_t text_size = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint32_t data_size = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
        (void)text_size;
        (void)data_size;

        out->program.data = new uint8_t[size];
        std::memcpy(const_cast<uint8_t*>(out->program.data), data, size);
        out->program.data_size = size;
        out->program.load_addr = 0;  // Relocated by GEMDOS
        return true;
    }

    // Raw disk image
    out->type = FORMAT_LOAD_RAW;
    out->program.data = new uint8_t[size];
    std::memcpy(const_cast<uint8_t*>(out->program.data), data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;

    return true;
}

// ── Descriptor ───────────────────────────────────────────────────────────────

static const char* atari_st_extensions[] = {"prg", "tos", "st", nullptr};

const format_descriptor_t ATARI_ST_FORMAT_DESCRIPTOR = {
    "Atari ST",
    "Atari ST executable or disk image",
    atari_st_extensions,
    FORMAT_CAP_LOADABLE,
    atari_st_identify,
    atari_st_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(ATARI_ST, &ATARI_ST_FORMAT_DESCRIPTOR);
