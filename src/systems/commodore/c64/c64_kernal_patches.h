#pragma once
// =============================================================================
// C64 KERNAL ROM Patches — Shared utilities for boot acceleration
// =============================================================================
//
// These patches modify the KERNAL ROM in-place to bypass time-consuming
// routines during boot.  They are safe to apply after ROMs are loaded
// (i.e. after initialize() / bus.init_unified_pointers()).
//
// TODO: The skip-memtest flag could also be set automatically when
//       whitelisted software is loaded (SID files, known-safe demos, etc.).
//       A future "patch registry" could manage per-system patches that
//       users can enable/disable via a configuration UI.
// =============================================================================

#include "c64_system.h"

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
