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
 * Reference: http://www.6502.org/users/andre/petindex/keyboards.html
 *            Commodore PET Service Manual keyboard matrix schematic
 */

#include "systems/commodore/pet/pet_keyboard_matrix.hpp"

// ============================================================================
// PET Keyboard Matrix — 10×8
// ============================================================================
// PIA1 Port A selects the row (active-low, bits 3:0 decoded)
// PIA1 Port B reads the column (PB0–PB7, active-low)
//
// Array convention: array[9 - hw_row][7 - PB_bit]
//   Row index 0 = hw row 9, Row index 9 = hw row 0
//   Col index 0 = PB7,      Col index 7 = PB0
//   Same bit-reversed layout as C64/VIC-20/C16/C128 matrices.
//
// PET "graphics keyboard" matrix (PET 2001-N, 3032, 4032):

static const SDL_Keycode pet_keys[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // row 9: REPEAT key (directly connected to CA1 on PIA1)
    CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,  CERMU_KEY_NONE,
    // row 8 reversed: —(none), [, SPACE, ?, >, +, \, *
    CERMU_KEY_NONE,  SDLK_LEFTBRACKET,  SDLK_SPACE,  SDLK_SLASH,  SDLK_PERIOD,  SDLK_BACKSLASH,  SDLK_BACKSLASH,  SDLK_RIGHTBRACKET,
    // row 7 reversed: —(none), REVERSE, RSHIFT, @, ,, n, v, x
    CERMU_KEY_NONE,  SDLK_LCTRL,  SDLK_RSHIFT,  SDLK_BACKQUOTE,  SDLK_COMMA,  SDLK_n,  SDLK_v,  SDLK_x,
    // row 6 reversed: —(none), —(none), LSHIFT, ., m, b, c, z
    CERMU_KEY_NONE,  CERMU_KEY_NONE,  SDLK_LSHIFT,  SDLK_PERIOD,  SDLK_m,  SDLK_b,  SDLK_c,  SDLK_z,
    // row 5 reversed: —(none), —(none), ], :, k, h, f, s
    CERMU_KEY_NONE,  CERMU_KEY_NONE,  SDLK_RIGHTBRACKET,  SDLK_LEFTBRACKET,  SDLK_k,  SDLK_h,  SDLK_f,  SDLK_s,
    // row 4 reversed: —(none), RETURN, ;, l, j, g, d, a
    CERMU_KEY_NONE,  SDLK_RETURN,  SDLK_SEMICOLON,  SDLK_l,  SDLK_j,  SDLK_g,  SDLK_d,  SDLK_a,
    // row 3 reversed: —(none), /, =, p, i, y, r, w
    CERMU_KEY_NONE,  SDLK_SLASH,  SDLK_EQUALS,  SDLK_p,  SDLK_i,  SDLK_y,  SDLK_r,  SDLK_w,
    // row 2 reversed: —(none), STOP, ↑, o, u, t, e, q
    CERMU_KEY_NONE,  CERMU_KEY_CBM_RUN_STOP,  CERMU_KEY_CBM_ARROW_UP,  SDLK_o,  SDLK_u,  SDLK_t,  SDLK_e,  SDLK_q,
    // row 1 reversed: CRSR↓, CRSR→, —, ), \, ', $, "
    SDLK_DOWN,  SDLK_RIGHT,  SDLK_MINUS,  SDLK_0,  SDLK_8,  SDLK_6,  SDLK_4,  SDLK_2,
    // row 0 reversed: DEL, HOME, ←, (, &, %, #, !
    CERMU_KEY_CBM_DEL,  SDLK_HOME,  CERMU_KEY_CBM_ARROW_LEFT,  SDLK_9,  SDLK_7,  SDLK_5,  SDLK_3,  SDLK_1,
};

// ============================================================================
// PETSCII Decode Tables
// ============================================================================

// Unshifted character decode table — PETSCII codes per matrix position.
// Same bit-reversed layout as pet_keys[] above.
// 0 = non-character key (modifier, function key, cursor key).
static const petscii_t pet_unshifted_chars[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // row 9: (REPEAT row — no characters)
    0, 0, 0, 0, 0, 0, 0, 0,
    // row 8 reversed: —, [, SPACE, ?, >, +, \, *
    0, '[', ' ', '?', '>', '+', '\\', '*',
    // row 7 reversed: —, RVS, RSHIFT, @, ,, N, V, X
    0, 0, 0, '@', ',', 'N', 'V', 'X',
    // row 6 reversed: —, —, LSHIFT, ., M, B, C, Z
    0, 0, 0, '.', 'M', 'B', 'C', 'Z',
    // row 5 reversed: —, —, ], :, K, H, F, S
    0, 0, ']', ':', 'K', 'H', 'F', 'S',
    // row 4 reversed: —, RETURN, ;, L, J, G, D, A
    0, 0, ';', 'L', 'J', 'G', 'D', 'A',
    // row 3 reversed: —, /, =, P, I, Y, R, W
    0, '/', '=', 'P', 'I', 'Y', 'R', 'W',
    // row 2 reversed: —, STOP, ↑($5E), O, U, T, E, Q
    0, 0, 0x5E, 'O', 'U', 'T', 'E', 'Q',
    // row 1 reversed: ↓, →, -, ), \, ', $, "
    0, 0, '-', ')', '\\', '\'', '$', '"',
    // row 0 reversed: DEL, HOME, ←($5F), (, &, %, #, !
    0, 0, 0x5F, '(', '&', '%', '#', '!',
};

// Shifted character decode table — PETSCII codes per matrix position.
// Letters produce PETSCII lowercase ($C1–$DA).
static const petscii_t pet_shifted_chars[PET_KEYBOARD_ROWS * PET_KEYBOARD_COLS] = {
    // row 9: (REPEAT row)
    0, 0, 0, 0, 0, 0, 0, 0,
    // row 8 shifted reversed: (all 0 — no distinct shifted output)
    0, 0, 0, 0, 0, 0, 0, 0,
    // row 7 shifted reversed: —, RVS, RSHIFT, @, <, n($CE), v($D6), x($D8)
    0, 0, 0, 0, '<', 0xCE, 0xD6, 0xD8,
    // row 6 shifted reversed: —, —, LSHIFT, ., m($CD), b($C2), c($C3), z($DA)
    0, 0, 0, 0, 0xCD, 0xC2, 0xC3, 0xDA,
    // row 5 shifted reversed: —, —, [, :, k($CB), h($C8), f($C6), s($D3)
    0, 0, 0, '[', 0xCB, 0xC8, 0xC6, 0xD3,
    // row 4 shifted reversed: —, RETURN, ;, l($CC), j($CA), g($C7), d($C4), a($C1)
    0, 0, 0, 0xCC, 0xCA, 0xC7, 0xC4, 0xC1,
    // row 3 shifted reversed: —, /, =, p($D0), i($C9), y($D9), r($D2), w($D7)
    0, 0, 0, 0xD0, 0xC9, 0xD9, 0xD2, 0xD7,
    // row 2 shifted reversed: —, STOP, π($DE), o($CF), u($D5), t($D4), e($C5), q($D1)
    0, 0, 0xDE, 0xCF, 0xD5, 0xD4, 0xC5, 0xD1,
    // row 1 shifted reversed: ↑, ←, —, 0, 8, 6, 4, 2
    0, 0, 0, '0', '8', '6', '4', '2',
    // row 0 shifted reversed: INST, CLR, ←, 9, 7, 5, 3, 1
    0, 0, 0, '9', '7', '5', '3', '1',
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
