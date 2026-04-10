#pragma once

#include <cstdint>

#include <unordered_map>

#include "core/input/cermu_keys.hpp"
#include "core/chip.hpp"
#include "core/system_lines.hpp"
#include "utils/keyboard_matrix.hpp"

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
// Uses SDL_Keycode (SDLK_*) for standard keys and CERMU_KEY_* defines
// (cermu_keys.hpp) for emulator-specific keys with no host equivalent.

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
//   PETSCII $5C → '^'   (host caret → guest £ key)
//   PETSCII $5E → '|'   (host pipe → guest ↑ key)
//   PETSCII $5F → '\\'  (host backslash → guest ← key)
//   PETSCII $C1–$DA → 'a'–'z'  (PETSCII lowercase = $C1+, not $61+)
//   PETSCII $DE → '~'   (host tilde → guest π, shifted ↑ key)
//   PETSCII $FF → '~'   (alternate: π as its own code)
//
// The host character choices for £/↑/← are deliberate:
//   £ → '^' : Shift+6 on the host — both are currency/symbol characters
//   ↑ → '|' : Shift+backslash on the host, so the \\ key pair covers both arrows
//   ← → '\\': Unshifted backslash, positional match on US keyboards
//
// Note: host '_' (Shift+minus) is mapped separately to PETSCII $A4 (▁),
// the Commodore graphics character that most closely resembles an underscore.
// This mapping is done as a post-loop fixup in build_character_map_from_matrix
// because $A4 is a C= key combination (KEYMOD_CBM) not in the standard
// KEYMOD_NONE/KEYMOD_SHIFT decode tables.

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
        case 0x5C: return '^';   // £ (pound sign) → host caret
        case 0x5E: return '|';   // ↑ (up arrow) → host pipe
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

struct keyboard_decode_table_t {
    uint8_t modifiers;               // Modifier bitmask (KEYMOD_SHIFT, etc.)
    const petscii_t* petscii;        // PETSCII codes, rows × cols entries
};

// ============================================================================
// Matrix dimension limits
// ============================================================================
#define MAX_KEYBOARD_ROWS 16   // Generous max (C128 needs 11, PET/CBM-II need 10)
#define MAX_KEYBOARD_COLS 16   // C128 needs 11 columns; padded to 16 for alignment

// ============================================================================
// Keyboard model and scanning chip identification
// ============================================================================

// Identifies which Commodore keyboard layout is active.
// Each model has a unique matrix mapping even if the dimensions match.
enum keyboard_model_t {
    KEYBOARD_MODEL_UNKNOWN = 0,
    KEYBOARD_MODEL_C64,         //  8×8, CIA 6526 — the "standard" home computer keyboard
    KEYBOARD_MODEL_VIC20,       //  8×8, VIA 6522 — simplified PET-derived layout
    KEYBOARD_MODEL_C128,        // 11×8, CIA 6526 — C64 subset (rows 0-7) + 3 extra rows
    KEYBOARD_MODEL_PLUS4_C16,   //  8×8, TED 7360 — different key mapping, TED-integrated scan
    KEYBOARD_MODEL_PET,         // 10×8, PIA/VIA  — business/chiclet keyboard variants
    KEYBOARD_MODEL_CBM_II,      // 10×8, TPI 6525 — B-series (B128, B256, 710, 720)
    KEYBOARD_MODEL_COUNT
};

// Identifies which I/O chip handles the keyboard matrix scanning.
// Determines how the system layer wires up port read callbacks.
enum keyboard_scan_chip_t {
    KEYBOARD_SCAN_UNKNOWN = 0,
    KEYBOARD_SCAN_CIA,    // MOS 6526 CIA — C64, C128 (Port A = col select, Port B = row read)
    KEYBOARD_SCAN_VIA,    // MOS 6522 VIA — VIC-20 (Port B = col select, Port A = row read)
    KEYBOARD_SCAN_TED,    // MOS 7360 TED — Plus/4, C16 (integrated scan, no separate chip)
    KEYBOARD_SCAN_PIA,    // MOS 6520 PIA — early PET models
    KEYBOARD_SCAN_TPI,    // MOS 6525 TPI — CBM-II series
};

// ============================================================================
// Matrix configuration — provided by system layer at creation time
// ============================================================================

struct keyboard_matrix_config_t {
    keyboard_model_t model;
    keyboard_scan_chip_t scan_chip;
    uint8_t rows;
    uint8_t cols;
    const char* description;

    // Guest keyboard matrix — Unicode codepoints with hardware positions.
    // Row/col values are hardware bit positions used directly by the
    // scan callbacks (no bit-reversal needed).
    const KeyMatrixEntry* entries;
    int num_entries;

    // Character override table — sparse entries for host characters
    // that don't exist on the guest (e.g., '{' → '(' fallback) or
    // host-specific mappings for Commodore-unique characters
    // (e.g., host '\' → guest ← position, host '|' → guest ↑).
    const KeyCharOverride* char_overrides;
    int num_char_overrides;

    // Host key bindings — maps SDL_Keycode to guest char32_t for keys
    // where the mapping is non-trivial (PUA keys, non-ASCII characters).
    // ASCII-range keys are auto-derived and don't need explicit bindings.
    const HostKeyBinding* host_bindings;
    int num_host_bindings;
};

// ============================================================================
// Key position info — stored in the optimised lookup
// ============================================================================

struct key_position_t {
    uint8_t row;
    uint8_t col;
};

// ============================================================================
// Commodore keyboard state
// ============================================================================

struct commodore_keyboard_t {
    // Model and matrix configuration
    keyboard_model_t model;
    keyboard_scan_chip_t scan_chip;
    uint8_t matrix_rows;
    uint8_t matrix_cols;

    // Keyboard matrix contact state
    // Row/col are hardware bit positions — key_down/key_up use them directly:
    //   row_open_contacts[row] &= ~(1 << col)   (close contact)
    //   row_open_contacts[row] |=  (1 << col)   (open contact)
    //
    // row_open_contacts[hw_row] = bitmask of column contacts for that row
    // col_open_contacts[hw_col] = bitmask of row contacts for that column
    // All bits set = all contacts open (no keys pressed)
    // Bit cleared = contact closed (key pressed)
    //
    // Each system's scan callback reads from the appropriate array:
    //   C64/C128: row_open_contacts[PB_bit] (reverse scan),
    //             col_open_contacts[PA_bit] (forward scan)
    //   VIC-20:   row_open_contacts[PB_bit] (forward scan — PB is column select!),
    //             col_open_contacts[PA_bit] (reverse scan — PA is row read!)
    //   C16:      row_open_contacts[PIO2_bit] (forward scan only)
    //   PET:      row_open_contacts[hw_row] (forward scan only)
    uint16_t row_open_contacts[MAX_KEYBOARD_ROWS];
    uint16_t col_open_contacts[MAX_KEYBOARD_COLS];

    // SDL_Keycode → {row, col} lookup (hash map — matrix is <100 entries)
    std::unordered_map<SDL_Keycode, key_position_t> key_lookup_;

    // Current keyboard state
    bool restore_key_pressed;
    bool caps_lock_active;

    // Active matrix entries (from config, for rebuild on reset)
    const KeyMatrixEntry* active_entries;
    int num_entries;

    // Character override table (from config)
    const KeyCharOverride* char_overrides;
    int num_char_overrides;

    // Host key bindings (from config)
    const HostKeyBinding* host_bindings;
    int num_host_bindings;

    // Auto-shift tracking: cursor left/up require SHIFT + physical cursor key
    bool auto_shift_left_active;
    bool auto_shift_up_active;
    // F-key auto-shift: even F-keys (F2/F4/F6/F8) → SHIFT + physical odd F-key.
    // Counter tracks how many even-F-key auto-shifts are currently active,
    // so SHIFT is only released when ALL auto-shifts (cursor + F-key) are done.
    uint8_t auto_shift_fkey_count;

    // Reference to connected scanning chip ports
    void* scan_port_a_reference;
    void* scan_port_b_reference;

    // ========================================================================
    // Methods
    // ========================================================================

    // Lifecycle
    bool init(const keyboard_matrix_config_t* config);
    void reset();

    // Keyboard input handling (accepts SDL_Keycode or CERMU_KEY_* values)
    void key_down(SDL_Keycode key, bool shifted);
    void key_up(SDL_Keycode key, bool shifted);

    // Key position lookup (O(1) amortized via hash map)
    bool find_key(SDL_Keycode key, uint8_t* out_row, uint8_t* out_col) const;

    // Utility
    static bool is_special_key(SDL_Keycode key);
    void toggle_caps_lock();
    bool any_auto_shift_active() const {
        return auto_shift_left_active || auto_shift_up_active || auto_shift_fkey_count > 0;
    }
    // True when cursor LEFT/UP need auto-shift (C64/VIC-20 only — no dedicated keys)
    bool needs_cursor_auto_shift() const {
        return model != KEYBOARD_MODEL_PLUS4_C16 && model != KEYBOARD_MODEL_C128;
    }
    // Map an even F-key to its physical odd F-key, or CERMU_KEY_NONE if no auto-shift needed
    SDL_Keycode resolve_fkey_physical(SDL_Keycode key) const;

    // Keyboard scanning and I/O chip integration
    void update_matrix();
    void connect_ports(void* port_a, void* port_b);

    // Keyboard matrix state functions for CIA/VIA/TED integration
    bool is_row_closed(uint8_t row, uint8_t col);
    bool is_col_closed(uint8_t row, uint8_t col);

    // Debug functions
    void print_matrix();
    void print_state();
};
