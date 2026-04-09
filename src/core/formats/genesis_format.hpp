#pragma once
/*
 * genesis_format.hpp — Sega Genesis / Mega Drive ROM format
 *
 * Genesis ROMs are raw binary images (.md, .gen, .bin) or interleaved
 * Super Magic Drive format (.smd).  ROMs range from 256KB to 4MB.
 * ROM header at $100–$1FF contains metadata: system type, copyright,
 * domestic/international titles, serial, checksum, I/O support, region.
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t GENESIS_FORMAT_DESCRIPTOR;
