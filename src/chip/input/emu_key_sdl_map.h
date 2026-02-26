#pragma once

#include "emu_keys.h"
#include <cstdint>
#include <bitset>
#include <unordered_map>

// Forward-declare SDL types so we don't drag SDL headers into emulation code
// These are the actual SDL types used at the boundary.
typedef int32_t SDL_Keycode;
typedef uint32_t SDL_Scancode_t;  // SDL_Scancode is an enum; we use uint32_t

// ============================================================================
// EmuKeySDLMap — Bidirectional mapping between EmuKey and SDL scancodes
// ============================================================================
//
// For the 0–511 identity range a 512-bit bitset marks which values are
// identity-mapped (EmuKey == SDL_Scancode).  Only deviations (emulator-
// specific keys that have a configured SDL equivalent) use the explicit
// hash maps.
//
// Typical usage:
//   emu_key_t ek = EmuKeySDLMap::instance().scancode_to_emu_key(event.key.keysym.scancode);
//   SDL_Scancode sc = EmuKeySDLMap::instance().emu_key_to_scancode(ek);
//
// The mapping is initialised once (singleton), and the default configuration
// identity-maps every SDL scancode in the 0–511 range.

class EmuKeySDLMap {
public:
    // Get the global singleton instance (created on first call)
    static EmuKeySDLMap& instance();

    // ========================================================================
    // Conversion functions
    // ========================================================================

    // SDL scancode → EmuKey.  Returns EMUKEY_NONE if no mapping exists.
    emu_key_t scancode_to_emu_key(SDL_Scancode_t scancode) const;

    // EmuKey → SDL scancode.  Returns 0xFFFFFFFF if no mapping exists.
    SDL_Scancode_t emu_key_to_scancode(emu_key_t key) const;

    // SDL keycode (SDLK_*) → EmuKey.
    // This handles the SDLK_SCANCODE_MASK:  if the keycode has the mask bit
    // set, the scancode is extracted and identity-mapped.  If it's a bare
    // ASCII value (< 128), it's mapped to the physical key that produces it
    // on a US layout (e.g., SDLK_a → SDL_SCANCODE_A → EMUKEY_A).
    emu_key_t sdl_keycode_to_emu_key(SDL_Keycode sdl_keycode) const;

    // ========================================================================
    // Configuration (call before first use if non-default mapping is needed)
    // ========================================================================

    // Register a non-identity mapping between an EmuKey and an SDL scancode.
    // Used for emulator-specific keys that should respond to a particular
    // host key (e.g., EMUKEY_CBM_RESTORE → SDL_SCANCODE_GRAVE).
    void register_mapping(emu_key_t key, SDL_Scancode_t scancode);

    // Remove a mapping.
    void remove_mapping(emu_key_t key);

    // ========================================================================
    // Query
    // ========================================================================

    // Check whether a given EmuKey is identity-mapped (bit in the bitset).
    bool is_identity_mapped(SDL_Scancode_t scancode) const;

    // Get count of non-identity mappings (for debug)
    size_t deviation_count() const { return emu_to_sdl_.size(); }

private:
    EmuKeySDLMap();

    // 512-bit bitset: bit N set → EmuKey N == SDL_SCANCODE N (identity)
    std::bitset<512> identity_;

    // Non-identity mappings (emulator-specific keys ↔ SDL scancodes)
    std::unordered_map<emu_key_t, SDL_Scancode_t>  emu_to_sdl_;
    std::unordered_map<SDL_Scancode_t, emu_key_t>  sdl_to_emu_;

    // Keycode (bare ASCII) → scancode mapping for sdl_keycode_to_emu_key().
    // Covers SDLK_a..'z' → SDL_SCANCODE_A..Z, SDLK_0..'9' → scancodes, etc.
    SDL_Scancode_t ascii_to_scancode_[128];
};

