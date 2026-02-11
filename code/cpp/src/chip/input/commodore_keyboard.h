#ifndef COMMODORE_KEYBOARD_H
#define COMMODORE_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL_keycode.h>

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
//   C128/128D     11×8      CIA 6526     C64 subset + 3 extra rows (Help, Alt, etc.)
//   Plus/4, C16   8×8       TED 7360     Integrated keyboard scanning in TED
//   CBM-II        10×8      TPI 6525     B-series business machines
//
// All models use an N×8 matrix: N scan lines (rows) × 8 data lines (columns).
// The scan chip drives N output lines to select rows, then reads 8 input
// lines to detect key closures. This architecture stores both "forward" and
// "reverse" contact arrays to support bidirectional scanning (software can
// scan in either direction on CIA/VIA/TED).
//
// The Amiga keyboard is NOT supported here — it uses a self-contained
// microcontroller that communicates via serial protocol, not a scanned matrix.
//
// Uses SDL keycodes for host-native key information.

// SDL keycode namespace — maps host SDL keys to Commodore key concepts.
// NOTE on cursor keys: The C64/VIC-20 has only CRSR→ and CRSR↓ physical keys.
// Cursor left = SHIFT + CRSR→, cursor up = SHIFT + CRSR↓.
// Host LEFT/UP arrow keys are handled via auto-shift in key_down/key_up.
namespace CbmKeys {
    // Regular ASCII keys (0-127)
    const uint32_t SAME = 0;              // Used when shifted key equals unshifted
    const uint32_t ARROW_LEFT = 0;                 // ← character (not directly mappable from host)
    const uint32_t ARROW_UP = 0;                   // ↑ character (not directly mappable from host)
    const uint32_t COMMODORE = SDLK_LGUI;         // Commodore key (Windows/Command)
    const uint32_t CTRL = SDLK_LCTRL;             // Control key
    const uint32_t CURSOR_DOWN = SDLK_DOWN;       // CRSR↓ key (physical key)
    const uint32_t CURSOR_UP = SDLK_UP;             // CRSR↑ key (Plus/4, C128 — dedicated physical key)
    const uint32_t CURSOR_LEFT = SDLK_LEFT;         // CRSR← key (Plus/4, C128 — dedicated physical key)
    const uint32_t CURSOR_RIGHT = SDLK_RIGHT;     // CRSR→ key (physical key)
    const uint32_t DEL = SDLK_BACKSPACE;          // Delete/Backspace
    const uint32_t F1 = SDLK_F1;                  // Function keys
    const uint32_t F2 = SDLK_F2;
    const uint32_t F3 = SDLK_F3;
    const uint32_t F4 = SDLK_F4;
    const uint32_t F5 = SDLK_F5;
    const uint32_t F6 = SDLK_F6;
    const uint32_t F7 = SDLK_F7;
    const uint32_t F8 = SDLK_F8;
    const uint32_t HOME = SDLK_HOME;              // Home key
    const uint32_t INST = SDLK_INSERT;            // Insert key
    const uint32_t PI = 0;                       // Pi symbol (not directly mappable)
    const uint32_t POUND = 0;                    // Pound symbol (not directly mappable)
    const uint32_t RESTORE = SDLK_BACKQUOTE;     // Restore key (backtick/tilde)
    const uint32_t RETURN = SDLK_RETURN;          // Return/Enter
    const uint32_t RUN_STOP = SDLK_TAB;          // Run/Stop key
    const uint32_t SHIFT_LEFT = SDLK_LSHIFT;     // Left shift
    const uint32_t SHIFT_RIGHT = SDLK_RSHIFT;    // Right shift
    const uint32_t SPACE = SDLK_SPACE;            // Space bar

    // C128-specific keys (active only in C128 mode, not in C64 compatibility mode)
    const uint32_t HELP = SDLK_F9;               // HELP key (mapped to F9)
    const uint32_t ALT = SDLK_RALT;              // ALT key
    const uint32_t ESC = SDLK_ESCAPE;            // ESC key
    const uint32_t TAB = SDLK_TAB;               // TAB key (shared with RUN_STOP on C64)
    const uint32_t CAPS_LOCK = SDLK_CAPSLOCK;    // CAPS LOCK key
    const uint32_t FORTY_EIGHTY = SDLK_F10;      // 40/80 column toggle (mapped to F10)
    const uint32_t LINE_FEED = SDLK_KP_ENTER;    // LINE FEED key

    // Numeric keypad keys (C128)
    const uint32_t KP_0 = SDLK_KP_0;
    const uint32_t KP_1 = SDLK_KP_1;
    const uint32_t KP_2 = SDLK_KP_2;
    const uint32_t KP_3 = SDLK_KP_3;
    const uint32_t KP_4 = SDLK_KP_4;
    const uint32_t KP_5 = SDLK_KP_5;
    const uint32_t KP_6 = SDLK_KP_6;
    const uint32_t KP_7 = SDLK_KP_7;
    const uint32_t KP_8 = SDLK_KP_8;
    const uint32_t KP_9 = SDLK_KP_9;
    const uint32_t KP_PLUS = SDLK_KP_PLUS;
    const uint32_t KP_MINUS = SDLK_KP_MINUS;
    const uint32_t KP_PERIOD = SDLK_KP_PERIOD;
    const uint32_t KP_ENTER = SDLK_KP_ENTER;
}

// ============================================================================
// Matrix dimension limits
// ============================================================================
// Maximum dimensions across all supported Commodore models.
// C128 has the widest matrix at 11×8. PET/CBM-II have 10×8.
// Column count is always 8 (8-bit I/O port width), but MAX_KEYBOARD_COLS is
// set to 8 since no Commodore model exceeds this for the data lines.
#define MAX_KEYBOARD_ROWS 16   // Generous max (C128 needs 11, PET/CBM-II need 10)
#define MAX_KEYBOARD_COLS  8   // All Commodore models use 8-bit data lines
#define MAX_KEY_LOOKUP   512   // SDL keycode fast-lookup table size

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
// Each system provides a static config describing its keyboard matrix.
// The keyboard chip itself is completely matrix-agnostic; it just needs
// dimensions, model identity, and pointers to the key mapping tables.

typedef struct {
    keyboard_model_t model;           // Which Commodore model this matrix belongs to
    keyboard_scan_chip_t scan_chip;   // Which I/O chip scans this matrix
    uint8_t rows;                     // Number of matrix rows (scan lines: 8, 10, or 11)
    uint8_t cols;                     // Number of matrix columns (data lines: always 8)
    const char* description;          // Human-readable name (e.g., "C64 8×8 keyboard matrix")

    // Key mapping tables — row-major flat arrays of size [rows × cols].
    // Index a key at (row, col) as: table[row * cols + col]
    // Values are SDL keycodes (uint32_t) or CbmKeys:: constants.
    const uint32_t* unshifted;        // Unshifted key mapping [rows * cols]
    const uint32_t* shifted;          // Shifted key mapping [rows * cols]
} keyboard_matrix_config_t;

// ============================================================================
// Key matrix info — per-position metadata
// ============================================================================

typedef struct {
    uint8_t row;
    uint8_t col;
    bool shifted;
    uint32_t key_code;  // SDL keycode (32-bit to handle all SDLK_* values)
} key_matrix_info_t;

// ============================================================================
// Commodore keyboard state
// ============================================================================

typedef struct {
    chip_descriptor_t descriptor;

    // Model and matrix configuration
    keyboard_model_t model;           // Which Commodore model keyboard
    keyboard_scan_chip_t scan_chip;   // Which chip scans this matrix
    uint8_t matrix_rows;              // Actual row count for this model (8, 10, or 11)
    uint8_t matrix_cols;              // Actual column count for this model (always 8)

    // Keyboard matrix contact state
    // row_open_contacts[row_bit] = bitmask of column contacts (uint8_t, cols ≤ 8)
    // col_open_contacts[col_bit] = bitmask of row contacts (uint16_t, rows can exceed 8)
    // 0xFF/0xFFFF = all contacts open (no keys pressed)
    // Bit cleared = contact closed (key pressed at that intersection)
    uint8_t  row_open_contacts[MAX_KEYBOARD_ROWS];   // Indexed by row bit, stores col bitmask
    uint16_t col_open_contacts[MAX_KEYBOARD_COLS];    // Indexed by col bit, stores row bitmask

    // Keyboard matrix lookup
    key_matrix_info_t key_matrix[MAX_KEYBOARD_ROWS][MAX_KEYBOARD_COLS];
    key_matrix_info_t key_lookup[MAX_KEY_LOOKUP];  // SDL keycode fast lookup (ASCII range)

    // Current keyboard state
    bool restore_key_pressed;
    bool caps_lock_active;

    // Active matrix pointers — row-major flat arrays, stride = matrix_cols
    // Access key at (row, col) as: active_unshifted[row * matrix_cols + col]
    const uint32_t* active_unshifted;
    const uint32_t* active_shifted;

    // Auto-shift tracking: cursor left/up require SHIFT + physical cursor key
    bool auto_shift_left_active;   // SHIFT auto-pressed for host LEFT arrow
    bool auto_shift_up_active;     // SHIFT auto-pressed for host UP arrow

    // Reference to connected scanning chip ports
    void* scan_port_a_reference;   // For column scanning (CIA Port A, VIA Port B, etc.)
    void* scan_port_b_reference;   // For row reading (CIA Port B, VIA Port A, etc.)

} commodore_keyboard_t;

// ============================================================================
// Function declarations
// ============================================================================

// System-specific keyboard matrices are provided by the system layer at
// creation time via the config struct. The keyboard itself is completely
// matrix-agnostic — it works with any N×8 Commodore keyboard layout.

// Lifecycle
commodore_keyboard_t* commodore_keyboard_create(const keyboard_matrix_config_t* config);
void commodore_keyboard_destroy(commodore_keyboard_t* keyboard);
void commodore_keyboard_reset(commodore_keyboard_t* keyboard);

// SDL-based keyboard input handling
void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted);
void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted);

// SDL keycode to Commodore key mapping
uint32_t commodore_keyboard_map_host_key(uint32_t sdl_key, bool shifted);
bool commodore_keyboard_is_special_key(uint32_t key_code);

// Keyboard scanning and I/O chip integration
void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard, void* port_a, void* port_b);

// Utility functions
void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard);

// Keyboard matrix state functions for CIA/VIA/TED integration
bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);
bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);

// Debug functions
void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_print_state(commodore_keyboard_t* keyboard);

#endif // COMMODORE_KEYBOARD_H