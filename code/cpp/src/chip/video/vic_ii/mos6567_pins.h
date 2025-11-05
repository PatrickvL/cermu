#ifndef MOS6567_PINS_H
#define MOS6567_PINS_H

#include "../../../core/system_lines.h"

/**
 * MOS6567 (VIC-II) pin mapping to system bus lines.
 * Shows how a different chip maps its pins to the same system lines.
 */

// VIC-II control pins -> system lines (same as CPU for shared signals)
#define VIC_PIN_IRQ             SYS_LINE_IRQ    // VIC can assert IRQ
#define VIC_PIN_BA              SYS_LINE_BA     // VIC monitors BA from CPU
#define VIC_PIN_AEC             SYS_LINE_AEC    // VIC monitors AEC from CPU

// VIC-II specific control signals (these might map to different system positions)
#define VIC_PIN_RAS             6               // Row Address Strobe
#define VIC_PIN_CAS             7               // Column Address Strobe

// VIC-II pin masks
#define VIC_MASK_IRQ            SYS_MASK_IRQ
#define VIC_MASK_BA             SYS_MASK_BA
#define VIC_MASK_AEC            SYS_MASK_AEC
#define VIC_MASK_RAS            (1U << VIC_PIN_RAS)
#define VIC_MASK_CAS            (1U << VIC_PIN_CAS)

// VIC-II can both read and drive these lines
#define VIC_MASK_INPUTS         (VIC_MASK_BA | VIC_MASK_AEC)
#define VIC_MASK_OUTPUTS        (VIC_MASK_IRQ | VIC_MASK_RAS | VIC_MASK_CAS)

// VIC-II specific operations
#define VIC_PIN_TEST_BA(sys_lines)      SYS_LINES_TEST(sys_lines, VIC_MASK_BA)
#define VIC_PIN_TEST_AEC(sys_lines)     SYS_LINES_TEST(sys_lines, VIC_MASK_AEC)
#define VIC_PIN_SET_IRQ(sys_lines)      SYS_LINES_SET(sys_lines, VIC_MASK_IRQ)
#define VIC_PIN_CLEAR_IRQ(sys_lines)    SYS_LINES_CLEAR(sys_lines, VIC_MASK_IRQ)

#endif // MOS6567_PINS_H
