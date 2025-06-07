#ifndef SYSTEM_LINES_H
#define SYSTEM_LINES_H

#include <stdint.h>

/**
 * System-wide bus line bit positions.
 * These define the canonical bit positions for all shared signals.
 * All chips map their pins to these positions.
 */

// Control lines (shared across most chips)
#define SYS_LINE_IRQ        0   // IRQ line (bit 0)
#define SYS_LINE_NMI        1   // NMI line (bit 1)
#define SYS_LINE_RESET      2   // RESET line (bit 2)
#define SYS_LINE_BA         3   // Bus Available (bit 3)
#define SYS_LINE_AEC        4   // Address Enable Control (bit 4)
#define SYS_LINE_RDY        5   // Ready (bit 5)

// Data bus (8 bits starting at bit 8)
#define SYS_LINE_D0         8
#define SYS_LINE_D1         9
#define SYS_LINE_D2         10
#define SYS_LINE_D3         11
#define SYS_LINE_D4         12
#define SYS_LINE_D5         13
#define SYS_LINE_D6         14
#define SYS_LINE_D7         15

// Address bus (16 bits starting at bit 16)
#define SYS_LINE_A0         16
#define SYS_LINE_A1         17
// ... up to A15 at bit 31

// Bit masks for easy access
#define SYS_MASK_IRQ        (1U << SYS_LINE_IRQ)
#define SYS_MASK_NMI        (1U << SYS_LINE_NMI)
#define SYS_MASK_RESET      (1U << SYS_LINE_RESET)
#define SYS_MASK_BA         (1U << SYS_LINE_BA)
#define SYS_MASK_AEC        (1U << SYS_LINE_AEC)
#define SYS_MASK_RDY        (1U << SYS_LINE_RDY)

#define SYS_MASK_DATA_BUS   (0xFFU << SYS_LINE_D0)    // 8-bit data bus mask
#define SYS_MASK_ADDR_BUS   (0xFFFFU << SYS_LINE_A0)  // 16-bit address bus mask

/**
 * System bus line state structure.
 * This should be owned by the system and passed to chips as needed.
 */
typedef struct {
    uint32_t lines;                     // Current state of all system lines
    uint32_t previous_lines;            // Previous state for change detection
} system_lines_t;

/**
 * Helper macros for lightweight bit operations.
 * These operate on a system_lines_t structure.
 */
#define SYS_LINES_GET(sys_lines, mask)      ((sys_lines)->lines & (mask))
#define SYS_LINES_SET(sys_lines, mask)      ((sys_lines)->lines |= (mask))
#define SYS_LINES_CLEAR(sys_lines, mask)    ((sys_lines)->lines &= ~(mask))
#define SYS_LINES_TOGGLE(sys_lines, mask)   ((sys_lines)->lines ^= (mask))
#define SYS_LINES_TEST(sys_lines, mask)     (((sys_lines)->lines & (mask)) != 0)

// For read-only access (chips can read the raw value directly)
#define SYS_LINES_RAW(sys_lines)            ((sys_lines)->lines)

// Extract data bus value
#define SYS_DATA_GET(sys_lines)             (((sys_lines)->lines >> SYS_LINE_D0) & 0xFF)
#define SYS_DATA_SET(sys_lines, value)      ((sys_lines)->lines = ((sys_lines)->lines & ~SYS_MASK_DATA_BUS) | (((uint32_t)(value)) << SYS_LINE_D0))

// Extract address bus value 
#define SYS_ADDR_GET(sys_lines)             (((sys_lines)->lines >> SYS_LINE_A0) & 0xFFFF)
#define SYS_ADDR_SET(sys_lines, value)      ((sys_lines)->lines = ((sys_lines)->lines & ~SYS_MASK_ADDR_BUS) | (((uint32_t)(value)) << SYS_LINE_A0))

// Initialize system lines
static inline void system_lines_init(system_lines_t* sys_lines) {
    sys_lines->lines = 0;
    sys_lines->previous_lines = 0;
}

#endif // SYSTEM_LINES_H
