#ifndef MOS6510_PINS_H
#define MOS6510_PINS_H

#include "../../../core/system_lines.h"

/**
 * MOS6510 CPU pin mapping to system bus lines.
 * Maps the physical pins of the MOS6510 to system-wide bit positions.
 */

// MOS6510 control pins -> system lines
#define MOS6510_PIN_IRQ         SYS_LINE_IRQ
#define MOS6510_PIN_NMI         SYS_LINE_NMI  
#define MOS6510_PIN_RDY         SYS_LINE_RDY
#define MOS6510_PIN_AEC         SYS_LINE_AEC
#define MOS6510_PIN_BA          SYS_LINE_BA

// MOS6510 control pin masks (for lightweight operations)
#define MOS6510_MASK_IRQ        SYS_MASK_IRQ
#define MOS6510_MASK_NMI        SYS_MASK_NMI
#define MOS6510_MASK_RDY        SYS_MASK_RDY
#define MOS6510_MASK_AEC        SYS_MASK_AEC
#define MOS6510_MASK_BA         SYS_MASK_BA

// Combined masks for efficiency
#define MOS6510_MASK_INPUTS     (MOS6510_MASK_IRQ | MOS6510_MASK_NMI | MOS6510_MASK_RDY)
#define MOS6510_MASK_OUTPUTS    (MOS6510_MASK_AEC | MOS6510_MASK_BA)

// MOS6510-specific lightweight pin operations
// These take a system_lines_t* parameter
#define MOS6510_PIN_TEST_IRQ(sys_lines)     SYS_LINES_TEST(sys_lines, MOS6510_MASK_IRQ)
#define MOS6510_PIN_TEST_NMI(sys_lines)     SYS_LINES_TEST(sys_lines, MOS6510_MASK_NMI)
#define MOS6510_PIN_TEST_RDY(sys_lines)     SYS_LINES_TEST(sys_lines, MOS6510_MASK_RDY)

#define MOS6510_PIN_SET_AEC(sys_lines)      SYS_LINES_SET(sys_lines, MOS6510_MASK_AEC)
#define MOS6510_PIN_CLEAR_AEC(sys_lines)    SYS_LINES_CLEAR(sys_lines, MOS6510_MASK_AEC)
#define MOS6510_PIN_SET_BA(sys_lines)       SYS_LINES_SET(sys_lines, MOS6510_MASK_BA)
#define MOS6510_PIN_CLEAR_BA(sys_lines)     SYS_LINES_CLEAR(sys_lines, MOS6510_MASK_BA)

// For the I/O port pins (P0-P7), these are MOS6510-specific
#define MOS6510_IO_PORT_MASK    0xFF

#endif // MOS6510_PINS_H
