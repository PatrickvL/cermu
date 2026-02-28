#pragma once

#include <cstdint>

// ============================================================================
// Generic Emulator Key Codes — IO-Library-Independent Key Representation
// ============================================================================
//
// All emulated keyboard matrices use these key codes instead of SDL keycodes,
// decoupling the emulation core from any specific host input library.
//
// Design:
//   - Values 0–(EMUKEY_EMU_BASE-1) are identity-mapped to SDL scancodes (SDL_Scancode).
//     These cover all standard keyboard keys: letters, digits, punctuation,
//     modifiers, function keys, cursor keys, etc.  Conversion is free: just
//     cast the value. An EMUKEY_EMU_BASE-bit bitset in the SDL mapper marks which values
//     are identity-mapped, so only deviations require a map lookup.
//
//   - Values EMUKEY_EMU_BASE+ are emulator-specific keys with no host equivalent:
//     Commodore ← ↑ £ π, RESTORE, etc.  Future systems add keys here too.
//
//   - Marker values near UINT32_MAX signal "no key" and "same as unshifted."
//
// The EmuKey for a standard key equals its SDL_SCANCODE_* constant (e.g.,
// EMUKEY_A == SDL_SCANCODE_A == 4).  This is a deliberate optimisation;
// there is no semantic dependency on SDL — only a numerical coincidence
// that makes the bidirectional mapping a no-op for the majority of keys.
//
// Character association:
//   emu_key_to_char(key) returns the ASCII character a key typically
//   produces (e.g., EMUKEY_A → 'A', EMUKEY_SEMICOLON → ';').
//   System-specific shifted characters are stored in the matrix config.

typedef uint32_t emu_key_t;

// ============================================================================
// Marker values
// ============================================================================

#define EMUKEY_NONE     ((emu_key_t)0xFFFFFFFF)   // No key / invalid
#define EMUKEY_SAME     ((emu_key_t)0xFFFFFFFE)   // Shifted output = same as unshifted

// ============================================================================
// Identity-mapped keys  (value == SDL_SCANCODE_*)
// ============================================================================
// These constants are numerically identical to SDL_SCANCODE_* values.
// They exist so that matrix tables and emulation code never need to
// #include any SDL header.

// --- Letters (SDL_SCANCODE_A = 4 … SDL_SCANCODE_Z = 29) ---
#define EMUKEY_A         4
#define EMUKEY_B         5
#define EMUKEY_C         6
#define EMUKEY_D         7
#define EMUKEY_E         8
#define EMUKEY_F         9
#define EMUKEY_G        10
#define EMUKEY_H        11
#define EMUKEY_I        12
#define EMUKEY_J        13
#define EMUKEY_K        14
#define EMUKEY_L        15
#define EMUKEY_M        16
#define EMUKEY_N        17
#define EMUKEY_O        18
#define EMUKEY_P        19
#define EMUKEY_Q        20
#define EMUKEY_R        21
#define EMUKEY_S        22
#define EMUKEY_T        23
#define EMUKEY_U        24
#define EMUKEY_V        25
#define EMUKEY_W        26
#define EMUKEY_X        27
#define EMUKEY_Y        28
#define EMUKEY_Z        29

// --- Digits (SDL_SCANCODE_1 = 30 … SDL_SCANCODE_0 = 39) ---
#define EMUKEY_1        30
#define EMUKEY_2        31
#define EMUKEY_3        32
#define EMUKEY_4        33
#define EMUKEY_5        34
#define EMUKEY_6        35
#define EMUKEY_7        36
#define EMUKEY_8        37
#define EMUKEY_9        38
#define EMUKEY_0        39

// --- Common action keys ---
#define EMUKEY_RETURN       40
#define EMUKEY_ESCAPE       41
#define EMUKEY_BACKSPACE    42
#define EMUKEY_TAB          43
#define EMUKEY_SPACE        44

// --- Punctuation / symbols (US layout positions) ---
#define EMUKEY_MINUS        45   // - _
#define EMUKEY_EQUALS       46   // = +
#define EMUKEY_LEFTBRACKET  47   // [ {
#define EMUKEY_RIGHTBRACKET 48   // ] }
#define EMUKEY_BACKSLASH    49   // \ |
#define EMUKEY_NONUSHASH    50   // # ~ (non-US)
#define EMUKEY_SEMICOLON    51   // ; :
#define EMUKEY_APOSTROPHE   52   // ' "
#define EMUKEY_GRAVE        53   // ` ~
#define EMUKEY_COMMA        54   // , <
#define EMUKEY_PERIOD       55   // . >
#define EMUKEY_SLASH        56   // / ?

// --- Caps Lock ---
#define EMUKEY_CAPSLOCK     57

// --- Function keys ---
#define EMUKEY_F1           58
#define EMUKEY_F2           59
#define EMUKEY_F3           60
#define EMUKEY_F4           61
#define EMUKEY_F5           62
#define EMUKEY_F6           63
#define EMUKEY_F7           64
#define EMUKEY_F8           65
#define EMUKEY_F9           66
#define EMUKEY_F10          67
#define EMUKEY_F11          68
#define EMUKEY_F12          69

// --- Print / Scroll / Pause ---
#define EMUKEY_PRINTSCREEN  70
#define EMUKEY_SCROLLLOCK   71
#define EMUKEY_PAUSE        72

// --- Navigation cluster ---
#define EMUKEY_INSERT       73
#define EMUKEY_HOME         74
#define EMUKEY_PAGEUP       75
#define EMUKEY_DELETE       76
#define EMUKEY_END          77
#define EMUKEY_PAGEDOWN     78
#define EMUKEY_RIGHT        79
#define EMUKEY_LEFT         80
#define EMUKEY_DOWN         81
#define EMUKEY_UP           82

// --- Numeric keypad ---
#define EMUKEY_NUMLOCK      83
#define EMUKEY_KP_DIVIDE    84
#define EMUKEY_KP_MULTIPLY  85
#define EMUKEY_KP_MINUS     86
#define EMUKEY_KP_PLUS      87
#define EMUKEY_KP_ENTER     88
#define EMUKEY_KP_1         89
#define EMUKEY_KP_2         90
#define EMUKEY_KP_3         91
#define EMUKEY_KP_4         92
#define EMUKEY_KP_5         93
#define EMUKEY_KP_6         94
#define EMUKEY_KP_7         95
#define EMUKEY_KP_8         96
#define EMUKEY_KP_9         97
#define EMUKEY_KP_0         98
#define EMUKEY_KP_PERIOD    99

// --- Modifier keys ---
#define EMUKEY_LCTRL       224
#define EMUKEY_LSHIFT      225
#define EMUKEY_LALT        226
#define EMUKEY_LGUI        227   // Windows / Command / Super
#define EMUKEY_RCTRL       228
#define EMUKEY_RSHIFT      229
#define EMUKEY_RALT        230
#define EMUKEY_RGUI        231

// ============================================================================
// Emulator-specific keys  (value >= 512)
// ============================================================================
// Keys that exist on emulated systems but have no standard host equivalent.
// Each block is reserved for a system family; new systems add blocks below.

#define EMUKEY_EMU_BASE    512

// --- Commodore family (512–575) ---
#define EMUKEY_CBM_ARROW_LEFT    512   // ← character key (C64, VIC-20)
#define EMUKEY_CBM_ARROW_UP      513   // ↑ character key (C64, VIC-20)
#define EMUKEY_CBM_POUND         514   // £ key (C64, VIC-20, C16)
#define EMUKEY_CBM_RESTORE       515   // RESTORE (NMI trigger, not in matrix)
#define EMUKEY_CBM_PI            516   // π (shifted ↑ on C64/VIC-20) — character marker

// Commodore-specific aliases for identity-mapped keys.
// These share the same numeric value as their host equivalent but give
// Commodore-meaningful names for use in Commodore-context code.
#define EMUKEY_CBM_RUN_STOP      EMUKEY_TAB        // RUN/STOP key (VIC-20/C64 — same position as host TAB)
#define EMUKEY_CBM_COMMODORE     EMUKEY_LGUI       // C= key (host: Super/Windows/Command)
#define EMUKEY_CBM_DEL           EMUKEY_BACKSPACE   // INST/DEL key

// --- Reserved for NES/Famicom (576–639) ---
// #define EMUKEY_NES_SELECT      576
// #define EMUKEY_NES_START       577

// --- Reserved for Apple (640–703) ---
// #define EMUKEY_APPLE_RESET     640

// ============================================================================
// Character lookup — unshifted character produced by a key
// ============================================================================
// Returns the ASCII character that a key typically produces in its unshifted
// state on a Commodore keyboard (uppercase letters, digits, symbols).
// Returns 0 for non-character keys (modifiers, function keys, cursor keys).
//
// System-specific modified characters are NOT handled here; they come from
// the per-system decode tables in the matrix config.

static inline char emu_key_to_char(emu_key_t key) {
    // Letters → uppercase (Commodore unshifted = uppercase)
    if (key >= EMUKEY_A && key <= EMUKEY_Z) {
        return (char)('A' + (key - EMUKEY_A));
    }

    // Digits
    if (key >= EMUKEY_1 && key <= EMUKEY_9) {
        return (char)('1' + (key - EMUKEY_1));
    }
    if (key == EMUKEY_0) return '0';

    // Punctuation (US layout position → character on Commodore keyboard)
    switch (key) {
        case EMUKEY_SPACE:        return ' ';
        case EMUKEY_MINUS:        return '-';
        case EMUKEY_EQUALS:       return '=';
        case EMUKEY_SEMICOLON:    return ';';
        case EMUKEY_APOSTROPHE:   return '\'';
        case EMUKEY_COMMA:        return ',';
        case EMUKEY_PERIOD:       return '.';
        case EMUKEY_SLASH:        return '/';
        case EMUKEY_BACKSLASH:    return '\\';
        case EMUKEY_LEFTBRACKET:  return '[';
        case EMUKEY_RIGHTBRACKET: return ']';
        case EMUKEY_GRAVE:        return '`';

        // Commodore special characters
        case EMUKEY_CBM_ARROW_LEFT: return 0;   // ← has no ASCII equivalent
        case EMUKEY_CBM_ARROW_UP:   return 0;   // ↑ has no ASCII equivalent
        case EMUKEY_CBM_POUND:      return 0;   // £ has no ASCII equivalent
        case EMUKEY_CBM_PI:         return 0;   // π has no ASCII equivalent

        default: return 0;  // Non-character key
    }
}

// ============================================================================
// Utility: check whether an EmuKey is identity-mapped (0–511 range)
// ============================================================================

static inline bool emu_key_is_identity(emu_key_t key) {
    return key < EMUKEY_EMU_BASE;
}

static inline bool emu_key_is_emu_specific(emu_key_t key) {
    return key >= EMUKEY_EMU_BASE && key < EMUKEY_SAME;
}

static inline bool emu_key_is_marker(emu_key_t key) {
    return key >= EMUKEY_SAME;
}

