#pragma once

#include "core/input/emu_keys.hpp"
#include <cstdint>
#include <bitset>
#include <initializer_list>
#include <unordered_map>

// Forward-declare SDL types so we don't drag SDL headers into emulation code
// These are the actual SDL types used at the boundary.
typedef int32_t SDL_Keycode;
typedef uint32_t SDL_Scancode_t;  // SDL_Scancode is an enum; we use uint32_t

// ============================================================================
// EmuKeySDLMap — Bidirectional mapping between EmuKey and SDL scancodes
// ============================================================================
//
// Two mapping layers, checked in order:
//
//   1. **System overrides** (sdl_to_emu_ / emu_to_sdl_):
//      Explicit mappings registered at runtime via register_mapping() or
//      register_candidates().  These take priority over identity mappings,
//      allowing a system to redirect host scancodes (e.g. LALT → C128 ALT).
//      Cleared by clear_system_mappings() when switching systems.
//
//   2. **Identity map** (bitset):
//      For the 0–511 range, bit N set means EmuKey N == SDL_Scancode N.
//      This is the default for all standard keyboard keys.
//
// Typical usage:
//   emu_key_t ek = EmuKeySDLMap::instance().scancode_to_emu_key(event.key.keysym.scancode);
//   SDL_Scancode sc = EmuKeySDLMap::instance().emu_key_to_scancode(ek);

class EmuKeySDLMap {
public:
    // Get the global singleton instance (created on first call)
    static EmuKeySDLMap& instance();

    // ========================================================================
    // Conversion functions
    // ========================================================================

    // SDL scancode → EmuKey.  Returns EMUKEY_NONE if no mapping exists.
    // System overrides are checked first, then the identity bitset.
    emu_key_t scancode_to_emu_key(SDL_Scancode_t scancode) const;

    // EmuKey → SDL scancode.  Returns 0xFFFFFFFF if no mapping exists.
    SDL_Scancode_t emu_key_to_scancode(emu_key_t key) const;

    // SDL keycode (SDLK_*) → EmuKey.
    // This handles the SDLK_SCANCODE_MASK:  if the keycode has the mask bit
    // set, the scancode is extracted and looked up.  If it's a bare
    // ASCII value (< 128), it's mapped to the physical key that produces it
    // on a US layout (e.g., SDLK_a → SDL_SCANCODE_A → EMUKEY_A).
    emu_key_t sdl_keycode_to_emu_key(SDL_Keycode sdl_keycode) const;

    // ========================================================================
    // System-specific candidate mapping
    // ========================================================================

    // Register an emu-specific key with an ordered preference list of host
    // scancodes.  The first available scancode becomes the primary mapping
    // (used for emu_key → scancode reverse lookup).
    //
    // map_all:
    //   false (default) — only the chosen primary scancode maps to the key.
    //   true            — ALL available candidates map to the key.
    //                     Use for "both ALTs → guest ALT" style unification.
    //
    // Probes keyboard availability via SDL_GetKeyFromScancode().
    // If no candidate is available, the key is left unmapped.
    void register_candidates(emu_key_t key,
                             std::initializer_list<SDL_Scancode_t> candidates,
                             bool map_all = false);

    // Clear all system-specific (non-identity) mappings.
    // Call when switching between emulated systems so stale overrides
    // from the previous system don't interfere.
    void clear_system_mappings();

    // ========================================================================
    // Low-level mapping (for direct single-key registration)
    // ========================================================================

    // Register a non-identity mapping between an EmuKey and an SDL scancode.
    void register_mapping(emu_key_t key, SDL_Scancode_t scancode);

    // Remove a mapping.
    void remove_mapping(emu_key_t key);

    // ========================================================================
    // Query
    // ========================================================================

    // Probe whether a scancode is usable on the current host keyboard.
    // Uses SDL_GetKeyFromScancode() — returns false if the OS has no
    // mapping for the scancode (SDLK_UNKNOWN).
    static bool is_scancode_available(SDL_Scancode_t scancode);

    // Check whether a given scancode is identity-mapped (bit in the bitset).
    bool is_identity_mapped(SDL_Scancode_t scancode) const;

    // Get count of non-identity mappings (for debug)
    size_t deviation_count() const { return emu_to_sdl_.size(); }

private:
    EmuKeySDLMap();

    // EMUKEY_EMU_BASE-bit bitset: bit N set → EmuKey N == SDL_SCANCODE N (identity)
    std::bitset<EMUKEY_EMU_BASE> identity_;

    // System override mappings (checked before identity).
    // sdl_to_emu_ is many-to-one: multiple scancodes can map to the same emu_key.
    // emu_to_sdl_ stores only the primary scancode for each emu_key.
    std::unordered_map<emu_key_t, SDL_Scancode_t>  emu_to_sdl_;
    std::unordered_map<SDL_Scancode_t, emu_key_t>  sdl_to_emu_;

    // Keycode (bare ASCII) → scancode mapping for sdl_keycode_to_emu_key().
    // Covers SDLK_a..'z' → SDL_SCANCODE_A..Z, SDLK_0..'9' → scancodes, etc.
    SDL_Scancode_t ascii_to_scancode_[128];
};

