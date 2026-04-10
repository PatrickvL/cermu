#include "systems/commodore/vic20/vic20_keyboard_matrix.hpp"

// ============================================================================
// VIC-20 Keyboard Matrix — 8×8
// ============================================================================
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
//
// keys[] stores SDL_Keycode values.  Shifted characters stored in decode tables.

// Key positions note:
//   '@' key → SDLK_LEFTBRACKET  (physical position of @ on Commodore)
//   '*' key → SDLK_RIGHTBRACKET (physical position of * on Commodore)
//   ':' key → SDLK_SEMICOLON    (Commodore : is on the ; key position)
//   ';' key → SDLK_QUOTE        (Commodore ; is on the ' key position)
//   '+' key → SDLK_BACKSLASH    (Commodore + key, no direct ASCII match)

static const SDL_Keycode vic20_keys[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: F7, HOME, -, 0, 8, 6, 4, 2
    SDLK_F7,  SDLK_HOME,  SDLK_MINUS,  SDLK_0,  SDLK_8,  SDLK_6,  SDLK_4,  SDLK_2,
    // PB6: F5, ↑(char), @, O, U, T, E, Q
    SDLK_F5,  CERMU_KEY_CBM_ARROW_UP,  SDLK_LEFTBRACKET,  SDLK_o,  SDLK_u,  SDLK_t,  SDLK_e,  SDLK_q,
    // PB5: F3, =, :, K, H, F, S, C=
    SDLK_F3,  SDLK_EQUALS,  SDLK_SEMICOLON,  SDLK_k,  SDLK_h,  SDLK_f,  SDLK_s,  CERMU_KEY_CBM_COMMODORE,
    // PB4: F1, RSHIFT, ., M, B, C, Z, SPACE
    SDLK_F1,  SDLK_RSHIFT,  SDLK_PERIOD,  SDLK_m,  SDLK_b,  SDLK_c,  SDLK_z,  SDLK_SPACE,
    // PB3: CRSR↓, /, ,, N, V, X, LSHIFT, RUN/STOP
    SDLK_DOWN,  SDLK_SLASH,  SDLK_COMMA,  SDLK_n,  SDLK_v,  SDLK_x,  SDLK_LSHIFT,  CERMU_KEY_CBM_RUN_STOP,
    // PB2: CRSR→, ;, L, J, G, D, A, CTRL
    SDLK_RIGHT,  SDLK_QUOTE,  SDLK_l,  SDLK_j,  SDLK_g,  SDLK_d,  SDLK_a,  SDLK_LCTRL,
    // PB1: RETURN, *, P, I, Y, R, W, ←(char)
    SDLK_RETURN,  SDLK_RIGHTBRACKET,  SDLK_p,  SDLK_i,  SDLK_y,  SDLK_r,  SDLK_w,  CERMU_KEY_CBM_ARROW_LEFT,
    // PB0: DEL, £, +, 9, 7, 5, 3, 1
    CERMU_KEY_CBM_DEL,  CERMU_KEY_CBM_POUND,  SDLK_BACKSLASH,  SDLK_9,  SDLK_7,  SDLK_5,  SDLK_3,  SDLK_1,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// Mirrors the VIC-20 KERNAL's normal (unshifted) decode table at $EC5E.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
// Commodore-specific: ← = $5F, ↑ = $5E, £ = $5C.
static const petscii_t vic20_unshifted_chars[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: (F7), (HOME), -, 0, 8, 6, 4, 2
    0, 0, '-', '0', '8', '6', '4', '2',
    // PB6: (F5), ↑($5E), @, O, U, T, E, Q
    0, 0x5E, '@', 'O', 'U', 'T', 'E', 'Q',
    // PB5: (F3), =, :, K, H, F, S, (C=)
    0, '=', ':', 'K', 'H', 'F', 'S', 0,
    // PB4: (F1), (RSHIFT), ., M, B, C, Z, SPACE
    0, 0, '.', 'M', 'B', 'C', 'Z', ' ',
    // PB3: (CRSR↓), /, ,, N, V, X, (LSHIFT), (RUN/STOP)
    0, '/', ',', 'N', 'V', 'X', 0, 0,
    // PB2: (CRSR→), ;, L, J, G, D, A, (CTRL)
    0, ';', 'L', 'J', 'G', 'D', 'A', 0,
    // PB1: (RETURN), *, P, I, Y, R, W, ←($5F)
    0, '*', 'P', 'I', 'Y', 'R', 'W', 0x5F,
    // PB0: (DEL), £($5C), +, 9, 7, 5, 3, 1
    0, 0x5C, '+', '9', '7', '5', '3', '1',
};

// Shifted character decode table — PETSCII codes per matrix position.
// Mirrors the VIC-20 KERNAL's shifted decode table at $EC9F.
// Letters use PETSCII lowercase ($C1–$DA) for character-accurate mapping.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL ROM at runtime).
static const petscii_t vic20_shifted_chars[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: (F8), (CLR), (-), (0), (, &, $, "
    0, 0, 0, 0, '(', '&', '$', '"',
    // PB6: (F6), π($DE), (@), o($CF), u($D5), t($D4), e($C5), q($D1)
    0, 0xDE, 0, 0xCF, 0xD5, 0xD4, 0xC5, 0xD1,
    // PB5: (F4), (=), [, k($CB), h($C8), f($C6), s($D3), (C=)
    0, 0, '[', 0xCB, 0xC8, 0xC6, 0xD3, 0,
    // PB4: (F2), (RSHIFT), >, m($CD), b($C2), c($C3), z($DA), (SPACE)
    0, 0, '>', 0xCD, 0xC2, 0xC3, 0xDA, 0,
    // PB3: (CRSR↑), ?, <, n($CE), v($D6), x($D8), (LSHIFT), (RUN/STOP)
    0, '?', '<', 0xCE, 0xD6, 0xD8, 0, 0,
    // PB2: (CRSR←), ], l($CC), j($CA), g($C7), d($C4), a($C1), (CTRL)
    0, ']', 0xCC, 0xCA, 0xC7, 0xC4, 0xC1, 0,
    // PB1: (RETURN), (*), p($D0), i($C9), y($D9), r($D2), w($D7), (←)
    0, 0, 0xD0, 0xC9, 0xD9, 0xD2, 0xD7, 0,
    // PB0: (INST), (£), (+), ), ', %, #, !
    0, 0, 0, ')', '\'', '%', '#', '!',
};

static const keyboard_decode_table_t vic20_decode_tables[] = {
    { KEYMOD_NONE,  vic20_unshifted_chars },
    { KEYMOD_SHIFT, vic20_shifted_chars },
    // Future: { KEYMOD_CBM,  vic20_cbm_chars },
};

const keyboard_matrix_config_t vic20_keyboard_config = {
    .model = KEYBOARD_MODEL_VIC20,
    .scan_chip = KEYBOARD_SCAN_VIA,
    .rows = VIC20_KEYBOARD_ROWS,
    .cols = VIC20_KEYBOARD_COLS,
    .description = "VIC-20 8x8 keyboard matrix",
    .keys = vic20_keys,
    .num_decode_tables = sizeof(vic20_decode_tables) / sizeof(vic20_decode_tables[0]),
    .decode_tables = vic20_decode_tables,
};
