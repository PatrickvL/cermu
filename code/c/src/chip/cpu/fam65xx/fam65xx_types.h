#pragma once
/*
 * fam65xx_types.h - C Interface Types and Compatibility Layer
 *
 * This file contains all C-compatible type definitions, macros, and function
 * prototypes for the MOS 65xx family CPU emulator. It provides the C interface
 * layer that bridges between C code and the C++ template implementation.
 */

#include <cstdint>

// Include system-wide bus definitions
#include "../../../core/aiemuc.h"
#include "../../../core/system_lines.h"
#include "../../../core/chip.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Bus State and Pin Definitions
// ============================================================================

// Legacy pin compatibility - map to system_lines.h definitions
#define FAM65XX_RW      BUS_BIT(BUS_RW_BIT)
#define FAM65XX_SYNC    BUS_BIT(BUS_SYNC_BIT)
#define FAM65XX_IRQ     BUS_BIT(BUS_IRQ_BIT)
#define FAM65XX_NMI     BUS_BIT(BUS_NMI_BIT)
#define FAM65XX_RDY     BUS_BIT(BUS_RDY_BIT)
#define FAM65XX_RES     BUS_BIT(BUS_RES_BIT)

// Macros for FAM65XX bus access
#define FAM65XX_GET_ADDR(p) BUS_GET_ADDR(p)
#define FAM65XX_SET_ADDR(p, d) BUS_SET_ADDR(p, d)
#define FAM65XX_GET_DATA(p) BUS_GET_DATA(p)
#define FAM65XX_SET_DATA(p, d) BUS_SET_DATA(p, d)

// CPU pin access using project definitions
#define FAM65XX_GET_RDY(pins)      ((pins) & FAM65XX_RDY)
#define FAM65XX_SET_SYNC(pins, v)  ((pins) = ((v) ? ((pins) | FAM65XX_SYNC) : ((pins) & ~FAM65XX_SYNC)))
#define FAM65XX_GET_SYNC(pins)     ((pins) & FAM65XX_SYNC)
#define FAM65XX_GET_IRQ(pins)      ((pins) & FAM65XX_IRQ)
#define FAM65XX_GET_NMI(pins)      ((pins) & FAM65XX_NMI)
#define FAM65XX_GET_RW(pins)       ((pins) & FAM65XX_RW)

// ============================================================================
// Memory Callback Types (for compatibility)
// ============================================================================

typedef uint8_t (*fam65xx_mem_read_t)(void* user_data, uint16_t addr, uint8_t bus_state);
typedef void (*fam65xx_mem_write_t)(void* user_data, uint16_t addr, uint8_t data);

// ============================================================================
// Enhanced Chip Descriptor (extends chip_descriptor_t for fam65xx CPUs)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    chip_descriptor_t base;         // Base chip descriptor
    fam65xx_mem_read_t mem_read;    // Memory read callback
    fam65xx_mem_write_t mem_write;  // Memory write callback
    void* mem_user_data;            // User data for memory callbacks
} fam65xx_chip_descriptor_t;

#ifdef __cplusplus
}
#endif

// ============================================================================
// CPU Flags
// ============================================================================

#define FLAG_C  0x01  // Carry
#define FLAG_Z  0x02  // Zero
#define FLAG_I  0x04  // Interrupt Disable
#define FLAG_D  0x08  // Decimal Mode
#define FLAG_B  0x10  // Break
#define FLAG_U  0x20  // Unused (always 1) - 6502/6510/65C02
#define FLAG_V  0x40  // Overflow
#define FLAG_N  0x80  // Negative

// 65C816-specific flags (redefine bit meanings in native mode)
#define FLAG_X  0x10  // Index Register Select (0 = 16-bit, 1 = 8-bit) - 65C816
#define FLAG_M  0x20  // Memory/Accumulator Select (0 = 16-bit, 1 = 8-bit) - 65C816
#define FLAG_E 0x100  // Emulation mode (not in P register, separate)

// BRK flags for interrupt handling
#define FAM65XX_BRK_IRQ     (1<<0)
#define FAM65XX_BRK_NMI     (1<<1)
#define FAM65XX_BRK_RESET   (1<<2)

// Interrupt shift register bit layout - merged system (3 bits per interrupt + separators)
#define INT_IRQ_START_BIT   0   // IRQ uses bits 0-2 (3 bits)
#define INT_IRQ_SEP_BIT     3   // Separator bit after IRQ (bit 3)
#define INT_NMI_START_BIT   4   // NMI uses bits 4-6 (3 bits)
#define INT_NMI_SEP_BIT     7   // Separator bit after NMI (bit 7)
#define INT_RESET_START_BIT 8   // RESET uses bits 8-10 (3 bits)
#define INT_RESET_SEP_BIT   11  // Separator bit after RESET (bit 11)
#define INT_IRQ_MASK        (0x7 << INT_IRQ_START_BIT)     // 3 bits: 0b111
#define INT_NMI_MASK        (0x7 << INT_NMI_START_BIT)     // 3 bits: 0b111
#define INT_RESET_MASK      (0x7 << INT_RESET_START_BIT)   // 3 bits: 0b111
#define INT_SEPARATOR_MASK  ((1 << INT_IRQ_SEP_BIT) | (1 << INT_NMI_SEP_BIT) | (1 << INT_RESET_SEP_BIT))

// ============================================================================
// ADDRESSING MODE AND OPERATION ENUMS
// ============================================================================
// Addressing modes describe how operands are fetched.
// Operations describe what the CPU does with those operands.
// These are combined in the opcode table to minimize redundancy.
//
// AM_NON (0): No addressing mode handler needed
//   - Used by: Implicit, Immediate, Accumulator, and Relative modes
//   - These modes either have no operand, operand in next byte, or
//     operate directly on registers without memory access

typedef enum {
    AM_NON = 0, /* No addressing handler (Implicit/Accumulator/Relative) */
    AM_IMM,     /* Immediate - operand is next byte */
    AM_ZER,     /* Zero Page - operand at $00nn */
    AM_ZPX,     /* Zero Page,X - operand at ($00nn + X) & 0xFF */
    AM_ZPY,     /* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
    AM_ABS,     /* Absolute - operand at $nnnn */
    AM_ABX,     /* Absolute,X - operand at $nnnn + X */
    AM_ABY,     /* Absolute,Y - operand at $nnnn + Y */
    AM_IND,     /* Indirect - jump target at ($nnnn) */
    AM_INX,     /* Indexed Indirect - operand at (($nn + X) & 0xFF) */
    AM_INY,     /* Indirect Indexed - operand at ($nn) + Y */
    // Enhanced addressing modes for all family members
    AM_ZPR,     /* Zero Page Relative for BBR/BBS - nn,label - Rockwell */
    AM_ZPI,     /* Zero Page Indirect - ($nn) - 65C02 */
    AM_ABI,     /* Absolute Indexed Indirect - ($nnnn,X) - 65C816 */
    AM_SR,      /* Stack Relative - n,S - 65C816 */
    AM_SRI,     /* Stack Relative Indirect Indexed - (n,S),Y - 65C816 */
    AM_COUNT
} addressing_mode_t;

/* Aliases for documentation/clarity (all map to AM_NON) */
#define AM_IMP  AM_NON  /* Implied/Implicit - no operand */
#define AM_ACC  AM_NON  /* Accumulator - operate on A register */
#define AM_REL  AM_NON  /* Relative - branch offset */

typedef enum {
    // Core 6502 operations (0-45) - these are used in all processors
    OP_LDA, OP_LDX, OP_LDY,                                    // 0-2: Load operations
    OP_STA, OP_STX, OP_STY,                                    // 3-5: Store operations
    OP_ADC, OP_SBC,                                            // 6-7: Arithmetic
    OP_AND, OP_ORA, OP_EOR,                                    // 8-10: Logic operations
    OP_CMP, OP_CPX, OP_CPY,                                    // 11-13: Compare operations
    OP_ASL, OP_LSR, OP_ROL, OP_ROR,                            // 14-17: Shift/rotate
    OP_INC, OP_DEC,                                            // 18-19: Increment/decrement
    OP_INX, OP_INY, OP_DEX, OP_DEY,                            // 20-23: Register inc/dec
    OP_TAX, OP_TAY, OP_TXA, OP_TYA, OP_TSX, OP_TXS,            // 24-29: Transfer operations
    OP_PHA, OP_PHP, OP_PLA, OP_PLP,                            // 30-33: Stack operations
    OP_BCC, OP_BCS, OP_BEQ, OP_BNE, OP_BMI, OP_BPL, OP_BVC, OP_BVS, // 34-41: Branches
    OP_CLC, OP_SEC, OP_CLI, OP_SEI, OP_CLD, OP_SED, OP_CLV,    // 42-48: Flag operations
    OP_JMP, OP_JSR, OP_RTS, OP_RTI, OP_BRK,                    // 49-53: Control flow
    OP_BIT, OP_NOP, OP_JAM,                                    // 54-56: Test/misc
    
    // Illegal opcodes - most commonly used in 6502/6510 (57-74)
    OP_LAX, OP_SAX, OP_DCP, OP_ISC, OP_SLO, OP_RLA, OP_SRE, OP_RRA, // 57-64: Combo ops
    OP_ANC, OP_ASR, OP_ARR, OP_SBX,                            // 65-68: Special accumulator
    OP_SHA, OP_SHS, OP_SHX, OP_SHY, OP_LAS,                    // 69-73: Store with AND
    OP_XAA,                                                    // 74: Special operation
    
    // 65C02 enhancements (75-84)
    OP_BRA, OP_STZ, OP_TRB, OP_TSB, OP_PHX, OP_PHY, OP_PLX, OP_PLY, OP_WAI, OP_STP, // 75-84

    // Extended operations (85-116) - these will map to OP_NOP in 7-bit tables
    // but can be handled via processor-specific logic
    OP_RMB0, OP_RMB1, OP_RMB2, OP_RMB3, OP_RMB4, OP_RMB5, OP_RMB6, OP_RMB7,
    OP_SMB0, OP_SMB1, OP_SMB2, OP_SMB3, OP_SMB4, OP_SMB5, OP_SMB6, OP_SMB7,
    OP_BBR0, OP_BBR1, OP_BBR2, OP_BBR3, OP_BBR4, OP_BBR5, OP_BBR6, OP_BBR7,
    OP_BBS0, OP_BBS1, OP_BBS2, OP_BBS3, OP_BBS4, OP_BBS5, OP_BBS6, OP_BBS7,
    // 65C816 16-bit operations (117-135)
    OP_REP, OP_SEP, OP_XBA, OP_XCE, OP_COP, OP_WDM,
    OP_PEA, OP_PER, OP_PEI, OP_PHB, OP_PHD, OP_PHK, OP_PLB, OP_PLD,
    OP_RTL, OP_JSL, OP_JML, OP_MVN, OP_MVP,
    
    OP_COUNT
} operation_t;

// ============================================================================
// 8-bit Register indices with endian-aware 16-bit pairs
// ============================================================================

typedef enum {
    // 16-bit aligned register pairs (endian-aware) for memory addresses
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    REG_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    REG_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    REG_ABL,       // Address Bus (low byte, even index for little endian)
    REG_ABH,       // Address Bus (high byte)
    REG_PCL,       // Program Counter (low byte, even index for little endian)
    REG_PCH,       // Program Counter (high byte)
#else
    REG_SPH,       // Stack pointer (high byte) - always 0x01 for 6502/6510
    REG_SPL,       // Stack pointer (low byte) - full 16-bit stack register
    REG_ABH,       // Address Bus (high byte, even index for big endian)
    REG_ABL,       // Address Bus (low byte)
    REG_PCH,       // Program Counter (high byte, even index for big endian)
    REG_PCL,       // Program Counter (low byte)
#endif
    // Public registers
    REG_A,         // Accumulator
    REG_X,         // X index
    REG_Y,         // Y index
    REG_P,         // Processor status
    // Internal registers
    REG_IR,        // Instruction Register (current opcode)
    REG_DL,        // Data latch
    
    REG_COUNT,
    
    // Compatibility mapping for 8-bit stack pointer
    REG_S = REG_SPL  // Map legacy S register to SPL for compatibility
} reg8_t;

// 16-bit register indices (native endian compatible)
typedef enum {
    REG_SP = REG_SPL / 2,  // Stack pointer as 16-bit (SPL in low, 0x01 in high)
    REG_AB = REG_ABL / 2,  // Address Bus Latch as 16-bit (ADL/ADH pair)
    REG_PC = REG_PCL / 2,  // Program counter / PC as 16-bit (PCL/PCH pair)
} reg16_t;

// ============================================================================
// Opcode Encoding
// ============================================================================

// Opcode bit flags
typedef enum {
    OF_NONE          = 0x0,  // No special flags
    OF_ILLEGAL_STORE = 0x1,  // Illegal store quirk - uses wrong address on page cross
    OF_SKIP_PAGE     = 0x2,  // Can skip page cross penalty cycle (read operations only)
    OF_RMW           = 0x4,  // Read-Modify-Write operation
    OF_RESERVED      = 0x8   // Reserved for future use
} opcode_flags_t;

typedef struct {
    uint16_t op_index : 8;  // Operation index (0-255, bits 0-7) [type operation_t]
    uint16_t am_index : 4;  // Addressing mode index (0-15, bits 8-11) [type addr_mode_t]
    uint16_t flags    : 4;  // Opcode flags (bits 12-15) [type opcode_flags_t]
} opcode_info_t;

// ============================================================================
// Accessor Macros
// ============================================================================

/* Accessor macros for cleaner code */
#define CPU_SP(cpu)    ((cpu)->reg16[REG_SP])   /* Stack pointer (0x0100 | SPL) */
#define CPU_AB(cpu)    ((cpu)->reg16[REG_AB])   /* Address Bus Latch (ABH/ABL) */
#define CPU_PC(cpu)    ((cpu)->reg16[REG_PC])   /* Program Counter (PCH/PCL) */

/* Individual byte access - using the new register layout */
#define CPU_S(cpu)     ((cpu)->reg8[REG_SPL])   /* Stack Pointer Low (High is always 0x01)*/
#define CPU_ABH(cpu)   ((cpu)->reg8[REG_ABH])   /* Address Bus Latch High */
#define CPU_ABL(cpu)   ((cpu)->reg8[REG_ABL])   /* Address Bus Latch Low */
#define CPU_PCH(cpu)   ((cpu)->reg8[REG_PCH])   /* Program Counter High */
#define CPU_PCL(cpu)   ((cpu)->reg8[REG_PCL])   /* Program Counter Low */

#define CPU_A(cpu)     ((cpu)->reg8[REG_A])
#define CPU_X(cpu)     ((cpu)->reg8[REG_X])
#define CPU_Y(cpu)     ((cpu)->reg8[REG_Y])
#define CPU_P(cpu)     ((cpu)->reg8[REG_P])
#define CPU_IR(cpu)    ((cpu)->reg8[REG_IR])
#define CPU_DL(cpu)    ((cpu)->reg8[REG_DL])

/* Legacy aliases for compatibility */
#define CPU_AD(cpu)    CPU_AB(cpu)  /* Address latch as 16-bit - now maps to AB */

#ifdef __cplusplus
} // extern "C"
#endif