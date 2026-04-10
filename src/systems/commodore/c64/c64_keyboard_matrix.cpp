#include "systems/commodore/c64/c64_keyboard_matrix.hpp"

// ============================================================================
// C64 Keyboard Matrix — 8×8
// ============================================================================
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7 - row_bit][7 - col_bit]
//   row_bit = PB bit position, col_bit = PA bit position
//
// The keys[] table stores SDL_Keycode values identifying each physical key.
// The decode tables store the PETSCII character produced when the key is
// pressed with a specific modifier (SHIFT, C=, CTRL), or 0 if no
// distinct character is produced (the KERNAL ROM handles it at runtime).
//
// NOTE on cursor keys: CRSR→ and CRSR↓ are the only physical cursor keys.
//   Host LEFT/UP are handled via auto-shift in key_down/key_up.

static const SDL_Keycode c64_keys[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: RUN/STOP, /, ,, N, V, X, LSHIFT, CRSR↓
    CERMU_KEY_CBM_RUN_STOP,  SDLK_SLASH,  SDLK_COMMA,  SDLK_n,  SDLK_v,  SDLK_x,  SDLK_LSHIFT,  SDLK_DOWN,
    // row 6: Q, ↑(char), @, O, U, T, E, F5
    SDLK_q,  CERMU_KEY_CBM_ARROW_UP,  SDLK_LEFTBRACKET,  SDLK_o,  SDLK_u,  SDLK_t,  SDLK_e,  SDLK_F5,
    // row 5: C=, =, :, K, H, F, S, F3
    CERMU_KEY_CBM_COMMODORE,  SDLK_EQUALS,  SDLK_SEMICOLON,  SDLK_k,  SDLK_h,  SDLK_f,  SDLK_s,  SDLK_F3,
    // row 4: SPACE, RSHIFT, ., M, B, C, Z, F1
    SDLK_SPACE,  SDLK_RSHIFT,  SDLK_PERIOD,  SDLK_m,  SDLK_b,  SDLK_c,  SDLK_z,  SDLK_F1,
    // row 3: 2, HOME, -, 0, 8, 6, 4, F7
    SDLK_2,  SDLK_HOME,  SDLK_MINUS,  SDLK_0,  SDLK_8,  SDLK_6,  SDLK_4,  SDLK_F7,
    // row 2: CTRL, ;, L, J, G, D, A, CRSR→
    SDLK_LCTRL,  SDLK_QUOTE,  SDLK_l,  SDLK_j,  SDLK_g,  SDLK_d,  SDLK_a,  SDLK_RIGHT,
    // row 1: ←(char), *, P, I, Y, R, W, RETURN
    CERMU_KEY_CBM_ARROW_LEFT,  SDLK_RIGHTBRACKET,  SDLK_p,  SDLK_i,  SDLK_y,  SDLK_r,  SDLK_w,  SDLK_RETURN,
    // row 0: 1, £, +, 9, 7, 5, 3, DEL
    SDLK_1,  CERMU_KEY_CBM_POUND,  SDLK_BACKSLASH,  SDLK_9,  SDLK_7,  SDLK_5,  SDLK_3,  CERMU_KEY_CBM_DEL,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// Mirrors the C64 KERNAL's normal (unshifted) decode table at $EB81.
// Values match the PETSCII code the KERNAL puts in the keyboard buffer
// when a key is pressed with no modifiers.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
//
// Commodore-specific characters use their PETSCII codes:
//   ← (left arrow) = $5F,  ↑ (up arrow) = $5E,  £ (pound) = $5C
// These diverge from ASCII ($5F=_, $5E=^, $5C=\) — petscii_to_host_char()
// bridges the mapping.
static const petscii_t c64_unshifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: (RUN/STOP), /, ,, N, V, X, (LSHIFT), (CRSR↓)
    0, '/', ',', 'N', 'V', 'X', 0, 0,
    // row 6: Q, ↑($5E), @, O, U, T, E, (F5)
    'Q', 0x5E, '@', 'O', 'U', 'T', 'E', 0,
    // row 5: (C=), =, :, K, H, F, S, (F3)
    0, '=', ':', 'K', 'H', 'F', 'S', 0,
    // row 4: SPACE, (RSHIFT), ., M, B, C, Z, (F1)
    ' ', 0, '.', 'M', 'B', 'C', 'Z', 0,
    // row 3: 2, (HOME), -, 0, 8, 6, 4, (F7)
    '2', 0, '-', '0', '8', '6', '4', 0,
    // row 2: (CTRL), ;, L, J, G, D, A, (CRSR→)
    0, ';', 'L', 'J', 'G', 'D', 'A', 0,
    // row 1: ←($5F), *, P, I, Y, R, W, (RETURN)
    0x5F, '*', 'P', 'I', 'Y', 'R', 'W', 0,
    // row 0: 1, £($5C), +, 9, 7, 5, 3, (DEL)
    '1', 0x5C, '+', '9', '7', '5', '3', 0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Mirrors the C64 KERNAL's shifted decode table at $EBC2.
// Letters use PETSCII lowercase ($C1–$DA), enabling character-accurate
// mapping: host lowercase 'a' → guest Shift+A → C64 lowercase 'a'.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL ROM at runtime).
static const petscii_t c64_shifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: (RUN/STOP), ?, <, n($CE), v($D6), x($D8), (LSHIFT), (CRSR↑ via auto-shift)
    0, '?', '<', 0xCE, 0xD6, 0xD8, 0, 0,
    // row 6: q($D1), π($DE), (@), o($CF), u($D5), t($D4), e($C5), (F6)
    0xD1, 0xDE, 0, 0xCF, 0xD5, 0xD4, 0xC5, 0,
    // row 5: (C=), (=), [, k($CB), h($C8), f($C6), s($D3), (F4)
    0, 0, '[', 0xCB, 0xC8, 0xC6, 0xD3, 0,
    // row 4: (SPACE), (RSHIFT), >, m($CD), b($C2), c($C3), z($DA), (F2)
    0, 0, '>', 0xCD, 0xC2, 0xC3, 0xDA, 0,
    // row 3: ", (CLR), (-), (0), (, &, $, (F8)
    '"', 0, 0, 0, '(', '&', '$', 0,
    // row 2: (CTRL), ], l($CC), j($CA), g($C7), d($C4), a($C1), (CRSR← via auto-shift)
    0, ']', 0xCC, 0xCA, 0xC7, 0xC4, 0xC1, 0,
    // row 1: (←), (*), p($D0), i($C9), y($D9), r($D2), w($D7), (RETURN)
    0, 0, 0xD0, 0xC9, 0xD9, 0xD2, 0xD7, 0,
    // row 0: !, (£), (+), ), ', %, #, (INST)
    '!', 0, 0, ')', '\'', '%', '#', 0,
};

static const keyboard_decode_table_t c64_decode_tables[] = {
    { KEYMOD_NONE,  c64_unshifted_chars },
    { KEYMOD_SHIFT, c64_shifted_chars },
    // Future: { KEYMOD_CBM,  c64_cbm_chars },   — C= key graphics characters
    // Future: { KEYMOD_CTRL, c64_ctrl_chars },   — CTRL key colour codes
};

const keyboard_matrix_config_t c64_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C64_KEYBOARD_ROWS,
    .cols = C64_KEYBOARD_COLS,
    .description = "C64 8x8 keyboard matrix",
    .keys = c64_keys,
    .num_decode_tables = sizeof(c64_decode_tables) / sizeof(c64_decode_tables[0]),
    .decode_tables = c64_decode_tables,
};
