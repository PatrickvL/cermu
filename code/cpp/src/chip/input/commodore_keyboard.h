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
// Keyboard modifier flags
// ============================================================================
// Bitmask flags for keyboard modifier keys used in decode tables and
// GuestKeyAction.  These mirror the modifier key positions in the
// physical keyboard matrix.
//
// On real Commodore hardware, the KERNAL ROM selects a character decode
// table based on which modifier key contacts are closed in the matrix.
// These flags identify which modifier combination a decode table (or a
// GuestKeyAction) corresponds to.

#define KEYMOD_NONE      0x00    // No modifier — base key output
#define KEYMOD_SHIFT     0x01    // Shift key (left or right)
#define KEYMOD_CBM       0x02    // Commodore (C=) key
#define KEYMOD_CTRL      0x04    // Control key

// ============================================================================
// PETSCII type alias
// ============================================================================
// PETSCII (PET Standard Code of Information Interchange) is the character
// encoding used by all Commodore 8-bit computers.  It diverges from ASCII
// at several points:
//
//   PETSCII   ASCII     Glyph
//   ───────   ─────     ─────
//   $5C       $5C       £  (not backslash)
//   $5E       $5E       ↑  (not caret)
//   $5F       $5F       ←  (not underscore)
//   $C1–$DA   $61–$7A   a–z (lowercase letters live here, not at $61)
//   $FF       n/a       π  (pi)
//   $A0–$BF   n/a       Graphics characters (C= key combinations)
//   $60–$7F   n/a       Graphics characters (shifted)
//
// The decode tables below store PETSCII codes, not ASCII.  Use
// petscii_to_host_char() to convert for the mapper's character map.
typedef uint8_t petscii_t;

// ============================================================================
// PETSCII → host character conversion
// ============================================================================
// Returns the ASCII character on the host keyboard that best matches a
// PETSCII code, or 0 if no host equivalent exists (graphics chars, control
// codes, etc.).
//
// This is the bridge between the KERNAL ROM's decode tables (which produce
// PETSCII codes) and the mapper's char_map_[] (which is indexed by the
// ASCII character the host user types via SDL_TEXTINPUT).
//
// Divergence points from ASCII that we map:
//   PETSCII $5C → '|'   (host pipe → guest £ key)
//   PETSCII $5E → '^'   (host caret → guest ↑ key)
//   PETSCII $5F → '\\'  (host backslash → guest ← key)
//   PETSCII $C1–$DA → 'a'–'z'  (PETSCII lowercase = $C1+, not $61+)
//   PETSCII $DE → '~'   (host tilde → guest π, shifted ↑ key)
//   PETSCII $FF → '~'   (alternate: π as its own code)
//
// The host character choices for £/↑/← are deliberate: they're the closest
// visual or positional matches on a US keyboard layout.

static inline char petscii_to_host_char(petscii_t p) {
    // PETSCII control codes ($01–$1F, $80–$9F) → no host character
    if (p == 0) return 0;
    if (p < 0x20 && p != 0x0D) return 0;  // $0D = RETURN
    if (p >= 0x80 && p <= 0x9F) return 0;  // Control codes (colours, cursor, etc.)

    // PETSCII graphics characters ($60–$7F, $A0–$BF) → no host equivalent
    if (p >= 0x60 && p <= 0x7F) return 0;
    if (p >= 0xA0 && p <= 0xBF) return 0;

    // PETSCII $C0 = shifted graphics (no character) → skip
    if (p == 0xC0) return 0;

    // PETSCII lowercase letters: $C1–$DA → 'a'–'z'
    if (p >= 0xC1 && p <= 0xDA) return (char)('a' + (p - 0xC1));

    // PETSCII $DB–$FE = shifted graphics → no host character
    // Exception: $DE = π (produced by Shift+↑ in KERNAL decode table)
    if (p == 0xDE) return '~';
    if (p >= 0xDB && p <= 0xFE) return 0;

    // PETSCII $FF = π
    if (p == 0xFF) return '~';

    // PETSCII divergence points from ASCII in the $20–$5F range
    switch (p) {
        case 0x5C: return '|';   // £ (pound sign) → host pipe
        case 0x5E: return '^';   // ↑ (up arrow) → host caret
        case 0x5F: return '\\';  // ← (left arrow) → host backslash
        default:   break;
    }

    // Everything else in $20–$5B, $5D matches ASCII
    if (p >= 0x20 && p <= 0x5D) return (char)p;

    return 0;  // Unmapped
}

// ============================================================================
// Keyboard character decode table
// ============================================================================
// Maps each matrix position to the character it produces when a specific
// modifier combination is active.
//
// On real Commodore hardware, the KERNAL ROM contains byte tables that
// map each matrix position to a PETSCII code, indexed by modifier state:
//
//   C64 KERNAL:  $EB81 (normal), $EBC2 (shift), $EC03 (C=), $EC78 (ctrl)
//   VIC-20:      $EC5E (normal), $EC9F (shift), $ECE0 (C=)
//   C16/Plus4:   TED handles scanning; similar decode structure in KERNAL
//
// This structure mirrors that design.  Each entry is a PETSCII code:
//   0       = no character output (modifier key, function key, cursor key,
//             or same as unmodified — the KERNAL handles it at runtime)
//   1-127   = PETSCII printable character (mostly ASCII-compatible)
//   128-255 = PETSCII extended characters (graphics, colour codes, etc.)

typedef struct {
    uint8_t modifiers;               // Modifier bitmask (KEYMOD_SHIFT, etc.)
    const petscii_t* petscii;        // PETSCII codes, rows × cols entries
} keyboard_decode_table_t;

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

    // Character decode tables — one per modifier combination.
    // Each table maps every matrix position to the PETSCII code it
    // produces when that modifier combination is active.  Mirrors the
    // KERNAL ROM's decode table structure.
    //
    // The tables are searched in order when building the keyboard mapper's
    // character map.  First-write-wins: the first table that maps a
    // PETSCII code to a host character claims that char_map_ entry.
    //
    // Ordering:
    //   [0] = KEYMOD_NONE  (unshifted characters — from KERNAL $EB81 etc.)
    //   [1] = KEYMOD_SHIFT (shifted characters   — from KERNAL $EBC2 etc.)
    //   [2] = KEYMOD_CBM   (Commodore key chars)  — optional
    //   [3] = KEYMOD_CTRL  (control key chars)    — optional
    int num_decode_tables;
    const keyboard_decode_table_t* decode_tables;
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
    // Decode tables from config (for mapper / runtime use)
    int num_decode_tables;
    const keyboard_decode_table_t* decode_tables;

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
