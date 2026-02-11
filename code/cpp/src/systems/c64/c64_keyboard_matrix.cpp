#include "c64_keyboard_matrix.h"

// ============================================================================
// C64 Keyboard Matrix — 8×8
// ============================================================================
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0
// Note: CRSR→/← key position stores CURSOR_RIGHT (unshifted function)
//       CRSR↓/↑ key position stores CURSOR_DOWN (unshifted function)
//       Host LEFT/UP arrows are handled via auto-shift in key_down/key_up.

// Unshifted keys - using SDL keycodes
const uint32_t keyboard_matrix_unshifted_c64[C64_KEYBOARD_ROWS][C64_KEYBOARD_COLS] = {
    {CbmKeys::RUN_STOP, '/', ',', 'N', 'V', 'X', CbmKeys::SHIFT_LEFT, CbmKeys::CURSOR_DOWN}, // row 7 (CRSR↓ key)
    {'Q', CbmKeys::ARROW_UP, '@', 'O', 'U', 'T', 'E', CbmKeys::F5}, // row 6 (↑ char)
    {CbmKeys::COMMODORE, '=', ':', 'K', 'H', 'F', 'S', CbmKeys::F3}, // row 5
    {CbmKeys::SPACE, CbmKeys::SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', CbmKeys::F1}, // row 4
    {'2', CbmKeys::HOME, '-', '0', '8', '6', '4', CbmKeys::F7}, // row 3
    {CbmKeys::CTRL, ';', 'L', 'J', 'G', 'D', 'A', CbmKeys::CURSOR_RIGHT}, // row 2 (CRSR→ key)
    {CbmKeys::ARROW_LEFT, '*', 'P', 'I', 'Y', 'R', 'W', CbmKeys::RETURN}, // row 1 (← char)
    {'1', CbmKeys::POUND, '+', '9', '7', '5', '3', CbmKeys::DEL}, // row 0
};

// Shifted keys - using SDL keycodes
// Note: Cursor/HOME shifted functions are handled by KERNAL when SHIFT is held.
//       SAME is used for cursor positions since auto-shift handles host LEFT/UP.
const uint32_t keyboard_matrix_shifted_c64[C64_KEYBOARD_ROWS][C64_KEYBOARD_COLS] = {
    {CbmKeys::SAME, '?', '<', 'n', 'v', 'x', CbmKeys::SAME, CbmKeys::SAME}, // row 7 (CRSR↓ shifted=cursor up, handled by auto-shift)
    {'q', CbmKeys::PI, CbmKeys::SAME, 'o', 'u', 't', 'e', CbmKeys::F6}, // row 6
    {CbmKeys::SAME, CbmKeys::SAME, '[', 'k', 'h', 'f', 's', CbmKeys::F4}, // row 5
    {CbmKeys::SAME, CbmKeys::SAME, '>', 'm', 'b', 'c', 'z', CbmKeys::F2}, // row 4
    {'"', CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, '(', '&', '$', CbmKeys::F8}, // row 3 (HOME shifted=CLR, KERNAL handles it)
    {CbmKeys::SAME, ']', 'l', 'j', 'g', 'd', 'a', CbmKeys::SAME}, // row 2 (CRSR→ shifted=cursor left, handled by auto-shift)
    {CbmKeys::SAME, CbmKeys::SAME, 'p', 'i', 'y', 'r', 'w', CbmKeys::SAME}, // row 1
    {'!', CbmKeys::SAME, CbmKeys::SAME, ')', '\'', '%', '#', CbmKeys::INST}, // row 0
};

// Pre-built configuration for commodore_keyboard_create()
const keyboard_matrix_config_t c64_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C64_KEYBOARD_ROWS,
    .cols = C64_KEYBOARD_COLS,
    .description = "C64 8x8 keyboard matrix",
    .unshifted = (const uint32_t*)keyboard_matrix_unshifted_c64,
    .shifted = (const uint32_t*)keyboard_matrix_shifted_c64,
};
