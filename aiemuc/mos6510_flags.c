#include "mos6510.h"

// Macro to define flag clear/set handlers
#define DEFINE_FLAG_CLEAR(fn, flag) void fn##_func(mos6510_t* cpu_dev) { mos6510_flag_clear_helper(cpu_dev, flag); }
#define DEFINE_FLAG_SET(fn, flag)   void fn##_func(mos6510_t* cpu_dev) { mos6510_flag_set_helper(cpu_dev, flag); }

// ============================================================================
// MOS 6510 FLAG INSTRUCTIONS
// ============================================================================
// Clear and Set flag instructions (alphabetical order)

// Clear and set flag instructions via macros
DEFINE_FLAG_CLEAR(clc, FLAG_C) // CLC - Clear Carry Flag (0x18)
DEFINE_FLAG_CLEAR(cld, FLAG_D) // CLD - Clear Decimal Mode Flag (0xD8)
DEFINE_FLAG_CLEAR(cli, FLAG_I) // CLI - Clear Interrupt Disable Flag (0x58)
DEFINE_FLAG_CLEAR(clv, FLAG_V) // CLV - Clear Overflow Flag (0xB8)
DEFINE_FLAG_SET(sec, FLAG_C) // SEC - Set Carry Flag (0x38)
DEFINE_FLAG_SET(sed, FLAG_D) // SED - Set Decimal Mode Flag (0xF8)
DEFINE_FLAG_SET(sei, FLAG_I) // SEI - Set Interrupt Disable Flag (0x78)
