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

// Pre-built configuration struct for commodore_keyboard_create()
// Contains keys[] (EmuKey values) and shifted_chars[] tables.
extern const keyboard_matrix_config_t vic20_keyboard_config;

#endif // VIC20_KEYBOARD_MATRIX_H
