#pragma once
// =============================================================================
// C64 SID Player — SID file loading, info display, and 6502 playback stub
// =============================================================================
//
// This module handles everything specific to playing SID music files on the
// emulated C64:
//
//   1. Writing the SID tune payload into C64 RAM
//   2. Setting the SID chip revision to match the file's metadata
//   3. Composing a full-screen info page (title, author, technical details)
//   4. Injecting a 6502 player stub that sets up CIA Timer A IRQ playback
//
// The caller is responsible for:
//   - Ensuring the C64 system is initialized and in a compatible configuration
//     (correct PAL/NTSC region, correct SID revision)
//   - Patching the KERNAL if fast boot is desired (c64_kernal_patches.h)
//   - Setting the CPU program counter after this function returns
//
// The player stub lives at $0340–$03FF (the cassette buffer area), which is
// safe because SID tunes never use tape I/O.
// =============================================================================

#include "c64.h"
#include "../../core/formats/sid_format.h"
#include "../../core/formats/format_handler.h"

/**
 * Write the SID header info page to C64 screen RAM ($0400) and colour RAM.
 *
 * Produces a nicely formatted 40×25 display showing:
 *   - Title, author, and release info
 *   - SID model, video standard, number of songs
 *   - Load/init/play addresses and playback speed
 *   - "NOW PLAYING" status line
 *
 * @param screen     Pointer to screen RAM (1000 bytes at $0400)
 * @param color      Pointer to colour RAM (1000 nybbles)
 * @param sid        Parsed SID header
 * @param subtune    0-based subtune index being played
 * @param use_cia    true if using CIA timer rate, false for VBI
 */
void c64_write_sid_info_page(uint8_t* screen, uint8_t* color,
                             const sid_header_t* sid,
                             uint16_t subtune, bool use_cia);

/**
 * Apply a SID file load to an initialized C64 system.
 *
 * This performs the complete SID injection sequence:
 *   1. Writes tune payload to RAM at sid->load_addr
 *   2. Sets SID chip revision from file metadata (v2+)
 *   3. Writes SID info page to screen/colour RAM
 *   4. Builds 6502 player stub at $0340 with CIA Timer A IRQ
 *   5. Sets CPU registers and PC to start the stub
 *
 * For PSID files with play_addr != 0: sets up Timer A IRQ to call play()
 * For PSID with play_addr == 0 or RSID: calls init() only (tune manages IRQ)
 *
 * @param c64   Initialized C64 system (RAM, SID, CPU must be valid)
 * @param sid   Parsed SID header with resolved addresses
 * @param prog  Program data from format_load_result_t (payload bytes)
 */
void c64_apply_sid_load(c64_t* c64, const sid_header_t* sid,
                        const program_data_t* prog);
