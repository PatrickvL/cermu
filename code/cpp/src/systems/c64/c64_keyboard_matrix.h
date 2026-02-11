#ifndef C64_KEYBOARD_MATRIX_H
#define C64_KEYBOARD_MATRIX_H

#include "commodore_keyboard.h"

// C64 Keyboard Matrix
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0

extern const uint32_t keyboard_matrix_unshifted_c64[KEYBOARD_ROWS][KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted_c64[KEYBOARD_ROWS][KEYBOARD_COLS];

#endif // C64_KEYBOARD_MATRIX_H
