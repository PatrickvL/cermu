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

// Macro to create a ChipPin with simplified structure (no derivable fields)
#define PIN(num, lbl_enum) \
    {num, PinLabel::lbl_enum, nullptr, false, false}

// Macro to define left and right pins simultaneously for DIP packages
// Simplified for new ChipPin structure
#define PIN_LR(left_num, left_lbl_enum, right_num, right_lbl_enum) \
    layout.left_pins.push_back(PIN(left_num, left_lbl_enum)); \
    layout.right_pins.push_back(PIN(right_num, right_lbl_enum));

// ============================================================================
// PIN CREATION HELPER FUNCTIONS
// ============================================================================

// Unified pin creation function (replaces all make_*_pin functions)
inline ChipPin make_pin(uint8_t num, PinLabel label, const char* alt_function = nullptr) {
    return ChipPin{num, label, alt_function, false, false};
}

#endif // PIN_MACROS_H