#ifndef COMMODORE_KEYBOARD_H
#define COMMODORE_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <unordered_map>

#include "emu_keys.h"
#include "../../core/chip.h"
#include "../../core/system_lines.h"

// ============================================================================
// Commodore Keyboard Matrix Emulation — Multi-System Architecture
// ============================================================================
//
// Supports the full range of Commodore 8-bit keyboard matrices:
//
//   Model         Matrix    Scan Chip    Notes
//   ─────────     ──────    ─────────    ─────
//   PET/CBM       10×8      PIA/VIA      Business/chiclet keyboard variants
//   VIC-20        8×8       VIA 6522     Simplified PET layout
//   C64/C64C      8×8       CIA 6526     Different key mapping than VIC-20
//   C128/128D     11×8      CIA 6526     C64 subset (rows 0-7) + 3 extra rows
//   Plus/4, C16   8×8       TED 7360     Integrated keyboard scanning in TED
//   CBM-II        10×8      TPI 6525     B-series business machines
//
// All models use an N×8 matrix: N scan lines (rows) × 8 data lines (columns).
//
// Uses EmuKey codes (emu_keys.h) for key representation, fully decoupled
// from any host input library.  SDL conversion is handled at the boundary
// by EmuKeySDLMap (emu_key_sdl_map.h).

// ============================================================================
// Legacy CbmKeys namespace — compatibility shim
// ============================================================================
// Maps old CbmKeys::* names to the new EmuKey constants.
// Allows existing code that hasn't been fully ported to keep compiling.
namespace CbmKeys {
    constexpr uint32_t SAME          = EMUKEY_SAME;
    constexpr uint32_t ARROW_LEFT    = EMUKEY_CBM_ARROW_LEFT;
    constexpr uint32_t ARROW_UP      = EMUKEY_CBM_ARROW_UP;
    constexpr uint32_t COMMODORE     = EMUKEY_LGUI;
    constexpr uint32_t CTRL          = EMUKEY_LCTRL;
    constexpr uint32_t CURSOR_DOWN   = EMUKEY_DOWN;
    constexpr uint32_t CURSOR_UP     = EMUKEY_UP;
    constexpr uint32_t CURSOR_LEFT   = EMUKEY_LEFT;
    constexpr uint32_t CURSOR_RIGHT  = EMUKEY_RIGHT;
    constexpr uint32_t DEL           = EMUKEY_BACKSPACE;
    constexpr uint32_t F1            = EMUKEY_F1;
    constexpr uint32_t F2            = EMUKEY_F2;
    constexpr uint32_t F3            = EMUKEY_F3;
    constexpr uint32_t F4            = EMUKEY_F4;
    constexpr uint32_t F5            = EMUKEY_F5;
    constexpr uint32_t F6            = EMUKEY_F6;
    constexpr uint32_t F7            = EMUKEY_F7;
    constexpr uint32_t F8            = EMUKEY_F8;
    constexpr uint32_t HOME          = EMUKEY_HOME;
    constexpr uint32_t INST          = EMUKEY_INSERT;
    constexpr uint32_t PI            = EMUKEY_CBM_PI;
    constexpr uint32_t POUND         = EMUKEY_CBM_POUND;
    constexpr uint32_t RESTORE       = EMUKEY_CBM_RESTORE;
    constexpr uint32_t RETURN        = EMUKEY_RETURN;
    constexpr uint32_t RUN_STOP      = EMUKEY_TAB;
    constexpr uint32_t SHIFT_LEFT    = EMUKEY_LSHIFT;
    constexpr uint32_t SHIFT_RIGHT   = EMUKEY_RSHIFT;
    constexpr uint32_t SPACE         = EMUKEY_SPACE;
    // C128-specific
    constexpr uint32_t HELP          = EMUKEY_F9;
    constexpr uint32_t ALT           = EMUKEY_RALT;
    constexpr uint32_t ESC           = EMUKEY_ESCAPE;
    constexpr uint32_t TAB           = EMUKEY_TAB;
    constexpr uint32_t CAPS_LOCK     = EMUKEY_CAPSLOCK;
    constexpr uint32_t FORTY_EIGHTY  = EMUKEY_F10;
    constexpr uint32_t LINE_FEED     = EMUKEY_KP_ENTER;
    // Keypad (C128)
    constexpr uint32_t KP_0          = EMUKEY_KP_0;
    constexpr uint32_t KP_1          = EMUKEY_KP_1;
    constexpr uint32_t KP_2          = EMUKEY_KP_2;
    constexpr uint32_t KP_3          = EMUKEY_KP_3;
    constexpr uint32_t KP_4          = EMUKEY_KP_4;
    constexpr uint32_t KP_5          = EMUKEY_KP_5;
    constexpr uint32_t KP_6          = EMUKEY_KP_6;
    constexpr uint32_t KP_7          = EMUKEY_KP_7;
    constexpr uint32_t KP_8          = EMUKEY_KP_8;
    constexpr uint32_t KP_9          = EMUKEY_KP_9;
    constexpr uint32_t KP_PLUS       = EMUKEY_KP_PLUS;
    constexpr uint32_t KP_MINUS      = EMUKEY_KP_MINUS;
    constexpr uint32_t KP_PERIOD     = EMUKEY_KP_PERIOD;
    constexpr uint32_t KP_ENTER      = EMUKEY_KP_ENTER;
}

// ============================================================================
// Matrix dimension limits
// ============================================================================
#define MAX_KEYBOARD_ROWS 16   // Generous max (C128 needs 11, PET/CBM-II need 10)
#define MAX_KEYBOARD_COLS  8   // All Commodore models use 8-bit data lines

// ============================================================================
// Keyboard model and scanning chip identification
// ============================================================================

// Identifies which Commodore keyboard layout is active.
// Each model has a unique matrix mapping even if the dimensions match.
typedef enum {
    KEYBOARD_MODEL_UNKNOWN = 0,
    KEYBOARD_MODEL_C64,         //  8×8, CIA 6526 — the "standard" home computer keyboard
    KEYBOARD_MODEL_VIC20,       //  8×8, VIA 6522 — simplified PET-derived layout
    KEYBOARD_MODEL_C128,        // 11×8, CIA 6526 — C64 subset (rows 0-7) + 3 extra rows
    KEYBOARD_MODEL_PLUS4_C16,   //  8×8, TED 7360 — different key mapping, TED-integrated scan
    KEYBOARD_MODEL_PET,         // 10×8, PIA/VIA  — business/chiclet keyboard variants
    KEYBOARD_MODEL_CBM_II,      // 10×8, TPI 6525 — B-series (B128, B256, 710, 720)
    KEYBOARD_MODEL_COUNT
} keyboard_model_t;

// Identifies which I/O chip handles the keyboard matrix scanning.
// Determines how the system layer wires up port read callbacks.
typedef enum {
    KEYBOARD_SCAN_UNKNOWN = 0,
    KEYBOARD_SCAN_CIA,    // MOS 6526 CIA — C64, C128 (Port A = col select, Port B = row read)
    KEYBOARD_SCAN_VIA,    // MOS 6522 VIA — VIC-20 (Port B = col select, Port A = row read)
    KEYBOARD_SCAN_TED,    // MOS 7360 TED — Plus/4, C16 (integrated scan, no separate chip)
    KEYBOARD_SCAN_PIA,    // MOS 6520 PIA — early PET models
    KEYBOARD_SCAN_TPI,    // MOS 6525 TPI — CBM-II series
} keyboard_scan_chip_t;

// ============================================================================
// Matrix configuration — provided by system layer at creation time
// ============================================================================

typedef struct {
    keyboard_model_t model;
    keyboard_scan_chip_t scan_chip;
    uint8_t rows;
    uint8_t cols;
    const char* description;

    // Key identity table — row-major flat array of size [rows * cols].
    // Index a key at (row, col) as: keys[row * cols + col]
    // Values are emu_key_t constants (EmuKey codes from emu_keys.h).
    const emu_key_t* keys;

    // Shifted character/key output table — row-major flat array [rows * cols].
    // Stores information about the shifted output for each matrix position:
    //   1–127       = ASCII character produced when shifted
    //   EMUKEY_SAME = shifted output is same as unshifted (handled by KERNAL)
    //   >= 512      = EmuKey of shifted function key (F2, INST, etc.) — informational
    //   0           = no entry / not applicable
    const uint32_t* shifted_chars;
} keyboard_matrix_config_t;

// ============================================================================
// Key position info — stored in the optimised lookup
// ============================================================================

typedef struct {
    uint8_t row;
    uint8_t col;
} key_position_t;

// ============================================================================
// Commodore keyboard state
// ============================================================================

typedef struct {
    chip_descriptor_t descriptor;

    // Model and matrix configuration
    keyboard_model_t model;
    keyboard_scan_chip_t scan_chip;
    uint8_t matrix_rows;
    uint8_t matrix_cols;

    // Keyboard matrix contact state
    // row_open_contacts[row_bit] = bitmask of column contacts
    // col_open_contacts[col_bit] = bitmask of row contacts
    // 0xFF/0xFFFF = all contacts open (no keys pressed)
    // Bit cleared = contact closed (key pressed)
    uint8_t  row_open_contacts[MAX_KEYBOARD_ROWS];
    uint16_t col_open_contacts[MAX_KEYBOARD_COLS];

    // Optimised EmuKey → {row, col} lookup
    // Direct array for identity-mapped keys (0–511): constant-time lookup
    key_position_t key_direct_lookup[512];
    bool           key_direct_valid[512];
    // Hash map for emulator-specific keys (512+): O(1) amortised
    std::unordered_map<emu_key_t, key_position_t> key_ext_lookup;

    // Current keyboard state
    bool restore_key_pressed;
    bool caps_lock_active;

    // Active matrix pointer — keys[] table from the config
    const emu_key_t* active_keys;
    // Shifted character table pointer from config
    const uint32_t* active_shifted_chars;

    // Auto-shift tracking: cursor left/up require SHIFT + physical cursor key
    bool auto_shift_left_active;
    bool auto_shift_up_active;

    // Reference to connected scanning chip ports
    void* scan_port_a_reference;
    void* scan_port_b_reference;

} commodore_keyboard_t;

// ============================================================================
// Function declarations
// ============================================================================

// Lifecycle
commodore_keyboard_t* commodore_keyboard_create(const keyboard_matrix_config_t* config);
void commodore_keyboard_destroy(commodore_keyboard_t* keyboard);
void commodore_keyboard_reset(commodore_keyboard_t* keyboard);

// EmuKey-based keyboard input handling
void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, emu_key_t key, bool shifted);
void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, emu_key_t key, bool shifted);

// Key position lookup (O(1) via the optimised structures)
bool commodore_keyboard_find_key(const commodore_keyboard_t* keyboard,
                                 emu_key_t key, uint8_t* out_row, uint8_t* out_col);

// Utility
bool commodore_keyboard_is_special_key(emu_key_t key);
void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard);

// Keyboard scanning and I/O chip integration
void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard, void* port_a, void* port_b);

// Keyboard matrix state functions for CIA/VIA/TED integration
bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);
bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);

// Debug functions
void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_print_state(commodore_keyboard_t* keyboard);

#endif // COMMODORE_KEYBOARD_H
