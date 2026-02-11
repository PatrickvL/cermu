#ifndef VIC20_KEYBOARD_MATRIX_H
#define VIC20_KEYBOARD_MATRIX_H

#include "commodore_keyboard.h"

// VIC-20 Keyboard Matrix — 8×8
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
// Transform from C64: swap array rows 0↔4, swap columns 0↔7 within each row.

#define VIC20_KEYBOARD_ROWS 8
#define VIC20_KEYBOARD_COLS 8

extern const uint32_t keyboard_matrix_unshifted_vic20[VIC20_KEYBOARD_ROWS][VIC20_KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted_vic20[VIC20_KEYBOARD_ROWS][VIC20_KEYBOARD_COLS];

// Pre-built configuration struct for commodore_keyboard_create()
extern const keyboard_matrix_config_t vic20_keyboard_config;

#endif // VIC20_KEYBOARD_MATRIX_H
