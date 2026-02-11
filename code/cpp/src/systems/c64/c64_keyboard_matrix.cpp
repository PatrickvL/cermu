#include "c64_keyboard_matrix.h"

// ============================================================================
// C64 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//
// The keys[] table stores EmuKey values identifying each physical key.
// The decode tables store the ASCII character produced when the key is
// pressed with a specific modifier (SHIFT, C=, CTRL), or 0 if no
// distinct character is produced (the KERNAL ROM handles it at runtime).
//
// Unshifted characters are derived from emu_key_to_char(key).
// NOTE on cursor keys: CRSR→ and CRSR↓ are the only physical cursor keys.
//   Host LEFT/UP are handled via auto-shift in key_down/key_up.

static const emu_key_t c64_keys[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: RUN/STOP, /, ,, N, V, X, LSHIFT, CRSR↓
    EMUKEY_TAB,  EMUKEY_SLASH,  EMUKEY_COMMA,  EMUKEY_N,  EMUKEY_V,  EMUKEY_X,  EMUKEY_LSHIFT,  EMUKEY_DOWN,
    // row 6: Q, ↑(char), @, O, U, T, E, F5
    EMUKEY_Q,  EMUKEY_CBM_ARROW_UP,  EMUKEY_LEFTBRACKET,  EMUKEY_O,  EMUKEY_U,  EMUKEY_T,  EMUKEY_E,  EMUKEY_F5,
    // row 5: C=, =, :, K, H, F, S, F3
    EMUKEY_LGUI,  EMUKEY_EQUALS,  EMUKEY_SEMICOLON,  EMUKEY_K,  EMUKEY_H,  EMUKEY_F,  EMUKEY_S,  EMUKEY_F3,
    // row 4: SPACE, RSHIFT, ., M, B, C, Z, F1
    EMUKEY_SPACE,  EMUKEY_RSHIFT,  EMUKEY_PERIOD,  EMUKEY_M,  EMUKEY_B,  EMUKEY_C,  EMUKEY_Z,  EMUKEY_F1,
    // row 3: 2, HOME, -, 0, 8, 6, 4, F7
    EMUKEY_2,  EMUKEY_HOME,  EMUKEY_MINUS,  EMUKEY_0,  EMUKEY_8,  EMUKEY_6,  EMUKEY_4,  EMUKEY_F7,
    // row 2: CTRL, ;, L, J, G, D, A, CRSR→
    EMUKEY_LCTRL,  EMUKEY_APOSTROPHE,  EMUKEY_L,  EMUKEY_J,  EMUKEY_G,  EMUKEY_D,  EMUKEY_A,  EMUKEY_RIGHT,
    // row 1: ←(char), *, P, I, Y, R, W, RETURN
    EMUKEY_CBM_ARROW_LEFT,  EMUKEY_RIGHTBRACKET,  EMUKEY_P,  EMUKEY_I,  EMUKEY_Y,  EMUKEY_R,  EMUKEY_W,  EMUKEY_RETURN,
    // row 0: 1, £, +, 9, 7, 5, 3, DEL
    EMUKEY_1,  EMUKEY_CBM_POUND,  EMUKEY_BACKSLASH,  EMUKEY_9,  EMUKEY_7,  EMUKEY_5,  EMUKEY_3,  EMUKEY_BACKSPACE,
};

// Shifted character decode table — ASCII characters per matrix position.
// Mirrors the C64 KERNAL's shifted decode table at $EBC2.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL ROM at runtime).
static const uint8_t c64_shifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: (RUN/STOP), ?, <, N, V, X, (LSHIFT), (CRSR↑ via auto-shift)
    0, '?', '<', 'N', 'V', 'X', 0, 0,
    // row 6: Q, (π), (@), O, U, T, E, (F6)
    'Q', 0, 0, 'O', 'U', 'T', 'E', 0,
    // row 5: (C=), (=), [, K, H, F, S, (F4)
    0, 0, '[', 'K', 'H', 'F', 'S', 0,
    // row 4: (SPACE), (RSHIFT), >, M, B, C, Z, (F2)
    0, 0, '>', 'M', 'B', 'C', 'Z', 0,
    // row 3: ", (CLR), (-), (0), (, &, $, (F8)
    '"', 0, 0, 0, '(', '&', '$', 0,
    // row 2: (CTRL), ], L, J, G, D, A, (CRSR← via auto-shift)
    0, ']', 'L', 'J', 'G', 'D', 'A', 0,
    // row 1: (←), (*), P, I, Y, R, W, (RETURN)
    0, 0, 'P', 'I', 'Y', 'R', 'W', 0,
    // row 0: !, (£), (+), ), ', %, #, (INST)
    '!', 0, 0, ')', '\'', '%', '#', 0,
};

static const keyboard_decode_table_t c64_decode_tables[] = {
    { KEYMOD_SHIFT, c64_shifted_chars },
    // Future: { KEYMOD_CBM,  c64_cbm_chars },   — C= key character decode
    // Future: { KEYMOD_CTRL, c64_ctrl_chars },  — CTRL key character decode
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
