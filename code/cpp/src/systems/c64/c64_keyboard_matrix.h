#ifndef C64_KEYBOARD_MATRIX_H
#define C64_KEYBOARD_MATRIX_H

#include "commodore_keyboard.h"

// C64 Keyboard Matrix — 8×8
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0

#define C64_KEYBOARD_ROWS 8
#define C64_KEYBOARD_COLS 8

extern const uint32_t keyboard_matrix_unshifted_c64[C64_KEYBOARD_ROWS][C64_KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted_c64[C64_KEYBOARD_ROWS][C64_KEYBOARD_COLS];

// Pre-built configuration struct for commodore_keyboard_create()
extern const keyboard_matrix_config_t c64_keyboard_config;

#endif // C64_KEYBOARD_MATRIX_H
