#pragma once
/*
 * cermu_keys.hpp — Emulator-specific key codes
 *
 * Defines key codes for keys that exist on emulated systems but have
 * no equivalent on modern host keyboards.  These live in a value range
 * that doesn't collide with SDL_Keycode values.
 *
 * Standard keyboard keys use SDL_Keycode (SDLK_*) directly — no
 * abstraction layer.  Only keys unique to emulated hardware need
 * custom defines here.
 */

#include <SDL_keycode.h>
#include <cstdint>

// ============================================================================
// Missing SDL_Keycode's
// ============================================================================

inline constexpr SDL_Keycode SDLK_PIPE = (SDL_Keycode)'|';  // Vertical Pipe, absent in some SDL versions

// ============================================================================
// Marker values
// ============================================================================

#define CERMU_KEY_NONE     ((SDL_Keycode)0x3FFFFFFF)  // No key / invalid
#define CERMU_KEY_SAME     ((SDL_Keycode)0x3FFFFFFE)  // Shifted output = same as unshifted

// ============================================================================
// Emulator-specific keys  (0x20000000+ range — below SDLK_SCANCODE_MASK)
// ============================================================================
// Keys that exist on emulated systems but have no standard host equivalent.
// Value range chosen to avoid collision with both:
//   - Bare ASCII keycodes (0–127)
//   - SDL scancode-masked keycodes (0x40000000+)

#define CERMU_KEY_BASE     0x20000000

// --- Commodore family: positional aliases ---
// Commodore keys that map to standard host key positions.  These compile
// to the same SDL_Keycode value — no redirect needed — but give
// Commodore-accurate names for use in matrix tables and keyboard code.
// The Commodore keyboard does NOT have TAB / LGUI / BACKSPACE keys;
// these aliases express the positional mapping to the host key that
// physically occupies the equivalent location.

constexpr SDL_Keycode CERMU_KEY_CBM_RUN_STOP  = SDLK_TAB;        // RUN/STOP  (host TAB position)
constexpr SDL_Keycode CERMU_KEY_CBM_COMMODORE = SDLK_LGUI;       // C= key    (host Super/LGUI position)
constexpr SDL_Keycode CERMU_KEY_CBM_DEL       = SDLK_BACKSPACE;  // INST/DEL  (host Backspace position)

// --- Commodore family: custom key codes ---
// Keys with no standard host equivalent.  Systems register host key
// redirects at runtime so the host can reach these keys.

#define CERMU_KEY_CBM_ARROW_LEFT    (CERMU_KEY_BASE + 0)   // ← character key (C64, VIC-20)
#define CERMU_KEY_CBM_ARROW_UP      (CERMU_KEY_BASE + 1)   // ↑ character key (C64, VIC-20)
#define CERMU_KEY_CBM_POUND         (CERMU_KEY_BASE + 2)   // £ key (C64, VIC-20, C16)
#define CERMU_KEY_CBM_RESTORE       (CERMU_KEY_BASE + 3)   // RESTORE (NMI trigger, not in matrix)
#define CERMU_KEY_CBM_PI            (CERMU_KEY_BASE + 4)   // π (shifted ↑ on C64/VIC-20)

// C128-specific keys
#define CERMU_KEY_CBM_HELP          (CERMU_KEY_BASE + 8)   // HELP key (C128)
#define CERMU_KEY_CBM_LINE_FEED     (CERMU_KEY_BASE + 9)   // LINE FEED key (C128)
#define CERMU_KEY_CBM_40_80_DISPLAY (CERMU_KEY_BASE + 10)  // 40/80 DISPLAY key (C128)
#define CERMU_KEY_CBM_NO_SCROLL     (CERMU_KEY_BASE + 11)  // NO SCROLL key (C128)
#define CERMU_KEY_CBM_ALT           (CERMU_KEY_BASE + 12)  // ALT key (C128)
    // NOTE: C128 ALT cannot simply map to SDLK_RALT because KeyboardMapper
    // reserves Right Alt as the "emulator modifier" key — a namespace prefix
    // for synthetic combos (RAlt+Esc → RUN/STOP, RAlt+R → RESTORE, etc.).
    // RAlt key-down is consumed by the mapper and never reaches the guest.
    // The emulator modifier is configurable via set_emulator_modifier(), but
    // no system currently changes it.  C128 ALT is therefore a custom key
    // code, reachable via the Virtual Keys menu or future key remapping.

// ============================================================================
// Convenience test
// ============================================================================

static inline bool cermu_key_is_custom(SDL_Keycode key) {
    return key >= CERMU_KEY_BASE && key < (CERMU_KEY_BASE + 0x1000);
}

static inline bool cermu_key_is_marker(SDL_Keycode key) {
    return key == CERMU_KEY_NONE || key == CERMU_KEY_SAME;
}
