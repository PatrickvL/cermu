/*
 * pet_keyboard_matrix.cpp — Commodore PET Keyboard Matrix Data
 *
 * PET 10×8 matrix via PIA1:
 *   PIA Port A bits[3:0] select keyboard row (active-low, directly decoded)
 *   PIA Port B reads the column lines for the selected row
 *
 * Matrix layout based on the standard PET/CBM "graphics" keyboard
 * (Business keyboard variant has the same matrix, just different
 * physical key labelling).
 *
 * Row/column numbering follows the PET schematics where:
 *   Row = keyboard connector pin (active when selected via Port A)
 *   Col = Port B bit (PB0–PB7)
 *
 * Reference: http://www.6502.org/users/andre/petindex/keyboards.html
 *            Commodore PET Service Manual keyboard matrix schematic
 */

#include "systems/commodore/pet/pet_keyboard_matrix.hpp"

// ============================================================================
// PET Keyboard Matrix — 10×8
// ============================================================================
// PIA1 Port A selects the row (active-low, bits 3:0)
// PIA1 Port B reads the column (PB0–PB7, active-low)
// keys[row * PET_KEYBOARD_COLS + col]
//
// PET "graphics keyboard" matrix (PET 2001-N, 3032, 4032):

static const SDL_Keycode pet_keys[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // Row 0:  !   #   %   &   (   ←   HOME  DEL
    SDLK_1,  SDLK_3,  SDLK_5,  SDLK_7,  SDLK_9,  CERMU_KEY_CBM_ARROW_LEFT,  SDLK_HOME,  CERMU_KEY_CBM_DEL,
    // Row 1:  "   $   '   \   )   —   CRSR→  CRSR↓
    SDLK_2,  SDLK_4,  SDLK_6,  SDLK_8,  SDLK_0,  SDLK_MINUS,  SDLK_RIGHT,  SDLK_DOWN,
    // Row 2:  q   e   t   u   o   ↑   STOP   —(none)
    SDLK_q,  SDLK_e,  SDLK_t,  SDLK_u,  SDLK_o,  CERMU_KEY_CBM_ARROW_UP,  CERMU_KEY_CBM_RUN_STOP,  CERMU_KEY_NONE,
    // Row 3:  w   r   y   i   p   =   /   —(none)
    SDLK_w,  SDLK_r,  SDLK_y,  SDLK_i,  SDLK_p,  SDLK_EQUALS,  SDLK_SLASH,  CERMU_KEY_NONE,
    // Row 4:  a   d   g   j   l   ;   RETURN  —(none)
    SDLK_a,  SDLK_d,  SDLK_g,  SDLK_j,  SDLK_l,  SDLK_SEMICOLON,  SDLK_RETURN,  CERMU_KEY_NONE,
    // Row 5:  s   f   h   k   :   ]   —(none)  —(none)
    SDLK_s,  SDLK_f,  SDLK_h,  SDLK_k,  SDLK_LEFTBRACKET,  SDLK_RIGHTBRACKET,  CERMU_KEY_NONE,  CERMU_KEY_NONE,
    // Row 6:  z   c   b   m   .   LSHIFT  —(none)  —(none)
    SDLK_z,  SDLK_c,  SDLK_b,  SDLK_m,  SDLK_PERIOD,  SDLK_LSHIFT,  CERMU_KEY_NONE,  CERMU_KEY_NONE,
    // Row 7:  x   v   n   ,   @   RSHIFT  REVERSE  —(none)
    SDLK_x,  SDLK_v,  SDLK_n,  SDLK_COMMA,  SDLK_BACKQUOTE,  SDLK_RSHIFT,  SDLK_LCTRL,  CERMU_KEY_NONE,
    // Row 8:  *   \   +   >   ?   SPACE   [   —(none)
    SDLK_RIGHTBRACKET,  SDLK_BACKSLASH,  SDLK_BACKSLASH,  SDLK_PERIOD,  SDLK_SLASH,  SDLK_SPACE,  SDLK_LEFTBRACKET,  CERMU_KEY_NONE,
    // Row 9: REPEAT key (directly connected to CA1 on PIA1)
    CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,
};

// ============================================================================
// PETSCII Decode Tables
// ============================================================================

// Unshifted character decode table — PETSCII codes per matrix position.
// 0 = non-character key (modifier, function key, cursor key).
static const petscii_t pet_unshifted_chars[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // Row 0:  !   #   %   &   (   ←   HOME  DEL
    '!', '#', '%', '&', '(', 0x5F, 0, 0,
    // Row 1:  2   4   6   8   0   -   →   ↓
    '"', '$', '\'', '\\', ')', '-', 0, 0,
    // Row 2:  q   e   t   u   o   ↑   STOP  —
    'Q', 'E', 'T', 'U', 'O', 0x5E, 0, 0,
    // Row 3:  w   r   y   i   p   =   /   —
    'W', 'R', 'Y', 'I', 'P', '=', '/', 0,
    // Row 4:  a   d   g   j   l   ;   RETURN —
    'A', 'D', 'G', 'J', 'L', ';', 0, 0,
    // Row 5:  s   f   h   k   :   ]   —   —
    'S', 'F', 'H', 'K', ':', ']', 0, 0,
    // Row 6:  z   c   b   m   .   LSHIFT — —
    'Z', 'C', 'B', 'M', '.', 0, 0, 0,
    // Row 7:  x   v   n   ,   @   RSHIFT  RVS —
    'X', 'V', 'N', ',', '@', 0, 0, 0,
    // Row 8:  *   \   +   >   ?   SPACE [  —
    '*', '\\', '+', '>', '?', ' ', '[', 0,
    // Row 9:  (REPEAT row — no characters)
    0, 0, 0, 0, 0, 0, 0, 0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Letters produce PETSCII lowercase ($C1–$DA).
static const petscii_t pet_shifted_chars[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // Row 0 shifted: 1   3   5   7   9   ←   CLR  INST
    '1', '3', '5', '7', '9', 0, 0, 0,
    // Row 1 shifted: 2   4   6   8   0   —   ←   ↑
    '2', '4', '6', '8', '0', 0, 0, 0,
    // Row 2 shifted: q   e   t   u   o   ↑   STOP —
    0xD1, 0xC5, 0xD4, 0xD5, 0xCF, 0xDE, 0, 0,
    // Row 3 shifted: w   r   y   i   p   =   /   —
    0xD7, 0xD2, 0xD9, 0xC9, 0xD0, 0, 0, 0,
    // Row 4 shifted: a   d   g   j   l   ;   RETURN —
    0xC1, 0xC4, 0xC7, 0xCA, 0xCC, 0, 0, 0,
    // Row 5 shifted: s   f   h   k   :   ]   —   —
    0xD3, 0xC6, 0xC8, 0xCB, '[', 0, 0, 0,
    // Row 6 shifted: z   c   b   m   .   LSHIFT — —
    0xDA, 0xC3, 0xC2, 0xCD, 0, 0, 0, 0,
    // Row 7 shifted: x   v   n   ,   @   RSHIFT  RVS —
    0xD8, 0xD6, 0xCE, '<', 0, 0, 0, 0,
    // Row 8 shifted: *   \   +   >   ?   SPACE  [  —
    0, 0, 0, 0, 0, 0, 0, 0,
    // Row 9 shifted: (REPEAT row)
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const keyboard_decode_table_t pet_decode_tables[] = {
    { KEYMOD_NONE,  pet_unshifted_chars },
    { KEYMOD_SHIFT, pet_shifted_chars },
};

const keyboard_matrix_config_t pet_keyboard_config = {
    .model = KEYBOARD_MODEL_PET,
    .scan_chip = KEYBOARD_SCAN_PIA,
    .rows = PET_KEYBOARD_ROWS,
    .cols = PET_KEYBOARD_COLS,
    .description = "PET 10x8 keyboard matrix (graphics keyboard)",
    .keys = pet_keys,
    .num_decode_tables = sizeof(pet_decode_tables) / sizeof(pet_decode_tables[0]),
    .decode_tables = pet_decode_tables,
};
