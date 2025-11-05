/*
 * pin_macros.h - Pin creation macros and helper functions
 * 
 * Compact macros for defining chip pin layouts with consistent syntax.
 * Includes both modern enum-based and legacy string-based pin creation.
 */

#ifndef PIN_MACROS_H
#define PIN_MACROS_H

#include "pin_types.h"

// ============================================================================
// MODERN PIN DEFINITION MACROS (ENUM-BASED)
// ============================================================================

// Macro to create a ChipPin with all members properly initialized
#define PIN(num, lbl_enum, bit, inv) \
    {num, PinLabel::lbl_enum, bit, inv, nullptr, nullptr, false, false}

// Macro to define left and right pins simultaneously for DIP packages
// Improved argument order: left_bit moved last for natural left-right mirroring
#define PIN_LR(left_num, left_lbl_enum, left_inv, \
               right_num, right_lbl_enum, right_inv, right_bit, left_bit) \
    layout.left_pins.push_back(PIN(left_num, left_lbl_enum, left_bit, left_inv)); \
    layout.right_pins.push_back(PIN(right_num, right_lbl_enum, right_bit, right_inv));

// ============================================================================
// PIN CREATION HELPER FUNCTIONS
// ============================================================================

// Unified pin creation function (replaces all make_*_pin functions)
ChipPin make_pin(uint8_t num, PinLabel label, const char* custom_group = nullptr);

#endif // PIN_MACROS_H