#ifndef FAM65XX_CONSTANTS_H
#define FAM65XX_CONSTANTS_H

#include <stdint.h>

// ============================================================================
// MOS 6502 FAMILY SHARED CONSTANTS
// ============================================================================

// Status Register Flags (shared by all 6502 family members)
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero  
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

// Status register combined masks for efficiency
#define FLAGS_NZ        (FLAG_N | FLAG_Z)
#define FLAGS_NZC       (FLAG_N | FLAG_Z | FLAG_C)
#define FLAGS_NZVC      (FLAG_N | FLAG_Z | FLAG_V | FLAG_C)

// Memory layout constants (shared by family)
#define STACK_BASE      0x0100
#define STACK_SIZE      0x0100
#define ZERO_PAGE_SIZE  0x0100

// Interrupt vectors (shared by family)
#define VECTOR_NMI      0xFFFA
#define VECTOR_RESET    0xFFFC  
#define VECTOR_IRQ      0xFFFE
#define VECTOR_BRK      VECTOR_IRQ  // BRK uses same vector as IRQ

// Default register values for reset
#define RESET_SP        0xFF
#define RESET_P         (FLAG_U | FLAG_I)  // Unused=1, Interrupt Disable=1
#define RESET_A         0x00
#define RESET_X         0x00
#define RESET_Y         0x00

// Pin mappings (shared by family - individual CPUs can override)
#include "../../../core/system_lines.h"

#define FAM65XX_PIN_IRQ         SYS_LINE_IRQ
#define FAM65XX_PIN_NMI         SYS_LINE_NMI  
#define FAM65XX_PIN_RDY         SYS_LINE_RDY
#define FAM65XX_PIN_AEC         SYS_LINE_AEC    // Address Enable Control
#define FAM65XX_PIN_BA          SYS_LINE_BA     // Bus Available

// Pin masks for efficient operations
#define FAM65XX_MASK_IRQ        SYS_MASK_IRQ
#define FAM65XX_MASK_NMI        SYS_MASK_NMI
#define FAM65XX_MASK_RDY        SYS_MASK_RDY
#define FAM65XX_MASK_AEC        SYS_MASK_AEC
#define FAM65XX_MASK_BA         SYS_MASK_BA

// Combined masks for efficiency
#define FAM65XX_MASK_INPUTS     (FAM65XX_MASK_IRQ | FAM65XX_MASK_NMI | FAM65XX_MASK_RDY)
#define FAM65XX_MASK_OUTPUTS    (FAM65XX_MASK_AEC | FAM65XX_MASK_BA)

#endif // FAM65XX_CONSTANTS_H
