#pragma once
/*
 * gb_format.hpp — Nintendo Game Boy / Game Boy Color ROM format
 *
 * Game Boy ROMs (.gb, .gbc) are raw binary images with a header at
 * $100–$14F containing the Nintendo logo, title, CGB flag, cartridge
 * type (MBC), ROM/RAM sizes, and checksums.
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t GB_FORMAT_DESCRIPTOR;
