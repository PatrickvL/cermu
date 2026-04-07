#pragma once
// =============================================================================
// C64 KERNAL ROM Patches — Shared utilities for boot acceleration
// =============================================================================
//
// These patches modify the KERNAL ROM in-place to bypass time-consuming
// routines during boot.  They are safe to apply after ROMs are loaded
// (i.e. after initialize() / bus.init_flat_mem_pointers()).
//
// The skip-memtest patch is applied automatically whenever software is
// loaded via on_file_parsed(), and can also be triggered explicitly with
// the --skip-memtest CLI flag.
// =============================================================================

#include "systems/commodore/c64/c64_system.hpp"

/**
 * Patch KERNAL RAMTAS routine at $FD5F to skip the memory-test loop.
 *
 * The original RAMTAS code writes/reads every byte from $0300 to $9FFF,
 * consuming ~1.5 million CPU cycles.  This patch replaces the test loop
 * with a direct jump to SETTOP ($FD88) after setting the top-of-memory
 * pointer, reducing boot time to ~8K cycles for the page-clear portion.
 *
 * Safe to call multiple times — checks for original byte signature before
 * patching.  Returns true if the patch was applied, false if already
 * patched or the KERNAL ROM is not loaded.
 *
 * @param c64  Initialized C64 system with KERNAL ROM loaded.
 * @return     true if patch was applied, false otherwise.
 */
bool c64_patch_skip_memtest(C64System* c64);
