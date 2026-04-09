#pragma once

/**
 * CPR Format Handler — Amstrad CPC Plus Cartridge
 *
 * The .cpr format is a RIFF container for Amstrad CPC Plus cartridges.
 *
 * File structure:
 *   "RIFF" + total_size(LE32) + "AMS!" form type
 *   Chunks:
 *     "cb00" + size(LE32) + data  (ROM bank 0, 16 KB)
 *     "cb01" + size(LE32) + data  (ROM bank 1, 16 KB)
 *     ...up to "cb31" (32 banks max, 512 KB total)
 *
 * Each bank is 16 KB.  Banks 0-1 are the cartridge system ROM overlay.
 * The CPC Plus boots from bank 0 at power-on when a cartridge is present.
 */

#include "core/formats/format_handler.hpp"

/// Parsed CPR cartridge header (stored in result metadata)
struct cpr_header_t {
    uint8_t  num_banks;         // Number of 16 KB banks (1-32)
    uint32_t total_rom_size;    // Total ROM data in bytes
};

extern const format_descriptor_t CPR_FORMAT_DESCRIPTOR;
