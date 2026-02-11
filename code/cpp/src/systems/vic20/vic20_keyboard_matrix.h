#ifndef VIC20_KEYBOARD_MATRIX_H
#define VIC20_KEYBOARD_MATRIX_H

#include "commodore_keyboard.h"

// VIC-20 Keyboard Matrix
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
// Transform from C64: swap array rows 0↔4, swap columns 0↔7 within each row.

extern const uint32_t keyboard_matrix_unshifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS];

#endif // VIC20_KEYBOARD_MATRIX_H
