#include "vic20_keyboard_matrix.h"

// ============================================================================
// VIC-20 Keyboard Matrix
// ============================================================================
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
// Transform from C64: swap array rows 0↔4, swap columns 0↔7 within each row.

// Unshifted keys (VIC-20 layout)
const uint32_t keyboard_matrix_unshifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::F7, CbmKeys::HOME, '-', '0', '8', '6', '4', '2'},             // PB7
    {CbmKeys::F5, CbmKeys::ARROW_UP, '@', 'O', 'U', 'T', 'E', 'Q'},        // PB6 (↑ char)
    {CbmKeys::F3, '=', ':', 'K', 'H', 'F', 'S', CbmKeys::COMMODORE},       // PB5
    {CbmKeys::F1, CbmKeys::SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', CbmKeys::SPACE}, // PB4
    {CbmKeys::CURSOR_DOWN, '/', ',', 'N', 'V', 'X', CbmKeys::SHIFT_LEFT, CbmKeys::RUN_STOP}, // PB3 (CRSR↓ key)
    {CbmKeys::CURSOR_RIGHT, ';', 'L', 'J', 'G', 'D', 'A', CbmKeys::CTRL},  // PB2 (CRSR→ key)
    {CbmKeys::RETURN, '*', 'P', 'I', 'Y', 'R', 'W', CbmKeys::ARROW_LEFT},  // PB1 (← char)
    {CbmKeys::DEL, CbmKeys::POUND, '+', '9', '7', '5', '3', '1'},           // PB0
};

// Shifted keys (VIC-20 layout)
const uint32_t keyboard_matrix_shifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::F8, CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, '(', '&', '$', '"'},  // PB7 (HOME shifted=CLR, KERNAL handles it)
    {CbmKeys::F6, CbmKeys::PI, CbmKeys::SAME, 'o', 'u', 't', 'e', 'q'},             // PB6
    {CbmKeys::F4, CbmKeys::SAME, '[', 'k', 'h', 'f', 's', CbmKeys::SAME},           // PB5
    {CbmKeys::F2, CbmKeys::SAME, '>', 'm', 'b', 'c', 'z', CbmKeys::SAME},           // PB4
    {CbmKeys::SAME, '?', '<', 'n', 'v', 'x', CbmKeys::SAME, CbmKeys::SAME},         // PB3 (CRSR↓ shifted=cursor up, handled by auto-shift)
    {CbmKeys::SAME, ']', 'l', 'j', 'g', 'd', 'a', CbmKeys::SAME},                   // PB2 (CRSR→ shifted=cursor left, handled by auto-shift)
    {CbmKeys::SAME, CbmKeys::SAME, 'p', 'i', 'y', 'r', 'w', CbmKeys::SAME},         // PB1
    {CbmKeys::INST, CbmKeys::SAME, CbmKeys::SAME, ')', '\'', '%', '#', '!'},         // PB0
};
