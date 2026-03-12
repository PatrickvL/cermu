#pragma once

/**
 * iNES Format — NES ROM image identification
 *
 * Provides a format descriptor for iNES ROM files (.nes).
 * Identification only — the NES system loads cartridges through its own
 * Cartridge class rather than the generic format_load_result_t path.
 */

#include "core/formats/format_handler.h"

extern const format_descriptor_t INES_FORMAT_DESCRIPTOR;
