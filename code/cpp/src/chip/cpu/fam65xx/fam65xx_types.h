#pragma once
/*
 * fam65xx_types.h - C Interface Types and Compatibility Layer
 *
 * This file contains all C-compatible type definitions, macros, and function
 * prototypes for the MOS 65xx family CPU emulator. It provides the C interface
 * layer that bridges between C code and the C++ template implementation.
 */

#include <cstdint>
#include <type_traits>

// Include system-wide bus definitions
#include "../../../core/aiemuc.h"
#include "../../../core/system_lines.h"
#include "../../../core/chip.h"


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

// Modern C++ function types - preferred for new code
#include <functional>
using fam65xx_mem_read_func = std::function<uint8_t(void* user_data, uint16_t addr, uint8_t bus_state)>;
using fam65xx_mem_write_func = std::function<void(void* user_data, uint16_t addr, uint8_t data)>;

// Legacy C function pointer types - kept for C compatibility
using fam65xx_mem_read_t = uint8_t (*)(void* user_data, uint16_t addr, uint8_t bus_state);
using fam65xx_mem_write_t = void (*)(void* user_data, uint16_t addr, uint8_t data);

// ============================================================================
// Enhanced Chip Descriptor (extends chip_descriptor_t for fam65xx CPUs)
// ============================================================================


typedef struct {
    chip_descriptor_t base;         // Base chip descriptor
    fam65xx_mem_read_t mem_read;    // Memory read callback
    fam65xx_mem_write_t mem_write;  // Memory write callback
    void* mem_user_data;            // User data for memory callbacks
} fam65xx_chip_descriptor_t;


// ============================================================================
// CPU Flags
// ============================================================================

// CPU Flags - Modern C++ class enum
enum class cpu_flags : uint16_t {
    CARRY = 0x01,              // Carry
    ZERO = 0x02,               // Zero
    INTERRUPT_DISABLE = 0x04,  // Interrupt Disable
    DECIMAL_MODE = 0x08,       // Decimal Mode
    BREAK_FLAG = 0x10,         // Break (renamed to avoid conflicts)
    UNUSED = 0x20,             // Unused (always 1) - 6502/6510/65C02
    OVERFLOW_FLAG = 0x40,      // Overflow (renamed to avoid conflicts)
    NEGATIVE = 0x80,           // Negative
    
    // 65C816-specific flags (redefine bit meanings in native mode)
    INDEX_SELECT = 0x10,       // Index Register Select (0 = 16-bit, 1 = 8-bit) - 65C816
    MEMORY_SELECT = 0x20,      // Memory/Accumulator Select (0 = 16-bit, 1 = 8-bit) - 65C816
    EMULATION_MODE = 0x100     // Emulation mode (not in P register, separate)
};

// Legacy macro compatibility - use static_cast for type-safe access
#define FLAG_C  static_cast<uint8_t>(cpu_flags::CARRY)
#define FLAG_Z  static_cast<uint8_t>(cpu_flags::ZERO)
#define FLAG_I  static_cast<uint8_t>(cpu_flags::INTERRUPT_DISABLE)
#define FLAG_D  static_cast<uint8_t>(cpu_flags::DECIMAL_MODE)
#define FLAG_B  static_cast<uint8_t>(cpu_flags::BREAK_FLAG)
#define FLAG_U  static_cast<uint8_t>(cpu_flags::UNUSED)
#define FLAG_V  static_cast<uint8_t>(cpu_flags::OVERFLOW_FLAG)
#define FLAG_N  static_cast<uint8_t>(cpu_flags::NEGATIVE)
#define FLAG_X  static_cast<uint8_t>(cpu_flags::INDEX_SELECT)   // INDEX_SELECT (65C816)
#define FLAG_M  static_cast<uint8_t>(cpu_flags::MEMORY_SELECT)  // MEMORY_SELECT (65C816)
#define FLAG_E  static_cast<uint16_t>(cpu_flags::EMULATION_MODE) // EMULATION_MODE (65C816)

// Interrupt types - Modern C++ scoped enum (ordered by priority: higher value = higher priority)
enum class InterruptType : uint8_t {
    NONE = 0,    // No interrupt active
    BRK,         // Software interrupt (BRK instruction) and default/fallback
    IRQ,         // Maskable interrupt
    COP,         // CoProcessor instruction (65C816)
    NMI,         // Non-maskable interrupt
    ABORT,       // Abort interrupt (65C816)
    RESET        // Reset interrupt (highest priority)
};

// Legacy C-style enum compatibility
using interrupt_t = InterruptType;
inline constexpr auto FAM65XX_INT_NONE = InterruptType::NONE;
inline constexpr auto FAM65XX_INT_BRK = InterruptType::BRK;
inline constexpr auto FAM65XX_INT_IRQ = InterruptType::IRQ;
inline constexpr auto FAM65XX_INT_COP = InterruptType::COP;
inline constexpr auto FAM65XX_INT_NMI = InterruptType::NMI;
inline constexpr auto FAM65XX_INT_ABORT = InterruptType::ABORT;
inline constexpr auto FAM65XX_INT_RESET = InterruptType::RESET;

// Interrupt shift register bit layout - merged system (3 bits per interrupt + separators)
// Ordered by priority: RESET > ABORT > NMI > COP > IRQ > BRK
#define INT_BRK_START_BIT   0   // BRK uses bits 0-2 (3 bits)
#define INT_BRK_SEP_BIT     3   // Separator bit after BRK (bit 3)
#define INT_IRQ_START_BIT   4   // IRQ uses bits 4-6 (3 bits)
#define INT_IRQ_SEP_BIT     7   // Separator bit after IRQ (bit 7)
#define INT_COP_START_BIT   8   // COP uses bits 8-10 (3 bits)
#define INT_COP_SEP_BIT     11  // Separator bit after COP (bit 11)
#define INT_NMI_START_BIT   12  // NMI uses bits 12-14 (3 bits)
#define INT_NMI_SEP_BIT     15  // Separator bit after NMI (bit 15)
#define INT_ABORT_START_BIT 16  // ABORT uses bits 16-18 (3 bits)
#define INT_ABORT_SEP_BIT   19  // Separator bit after ABORT (bit 19)
#define INT_RESET_START_BIT 20  // RESET uses bits 20-22 (3 bits)
#define INT_RESET_SEP_BIT   23  // Separator bit after RESET (bit 23)

// Interrupt masks (3 bits each)
#define INT_BRK_MASK        (0x7 << INT_BRK_START_BIT)     // 3 bits: 0b111
#define INT_IRQ_MASK        (0x7 << INT_IRQ_START_BIT)     // 3 bits: 0b111
#define INT_COP_MASK        (0x7 << INT_COP_START_BIT)     // 3 bits: 0b111
#define INT_NMI_MASK        (0x7 << INT_NMI_START_BIT)     // 3 bits: 0b111
#define INT_ABORT_MASK      (0x7 << INT_ABORT_START_BIT)   // 3 bits: 0b111
#define INT_RESET_MASK      (0x7 << INT_RESET_START_BIT)   // 3 bits: 0b111

// Separator bits mask
#define INT_SEPARATOR_MASK  ((1 << INT_BRK_SEP_BIT) | (1 << INT_IRQ_SEP_BIT) | (1 << INT_COP_SEP_BIT) | \
                            (1 << INT_NMI_SEP_BIT) | (1 << INT_ABORT_SEP_BIT) | (1 << INT_RESET_SEP_BIT))

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

enum class AddressingMode : uint8_t {
    NON = 0, /* No addressing handler (Implicit/Accumulator/Relative) */
    IMM,     /* Immediate - operand is next byte */
    ZER,     /* Zero Page - operand at $00nn */
    ZPX,     /* Zero Page,X - operand at ($00nn + X) & 0xFF */
    ZPY,     /* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
    ABS,     /* Absolute - operand at $nnnn */
    ABX,     /* Absolute,X - operand at $nnnn + X */
    ABY,     /* Absolute,Y - operand at $nnnn + Y */
    IND,     /* Indirect - jump target at ($nnnn) */
    INX,     /* Indexed Indirect - operand at (($nn + X) & 0xFF) */
    INY,     /* Indirect Indexed - operand at ($nn) + Y */
    // Enhanced addressing modes for all family members
    ZPR,     /* Zero Page Relative for BBR/BBS - nn,label - Rockwell */
    ZPI,     /* Zero Page Indirect - ($nn) - 65C02 */
    ABI,     /* Absolute Indexed Indirect - ($nnnn,X) - 65C816 */
    SR,      /* Stack Relative - n,S - 65C816 */
    SRI,     /* Stack Relative Indirect Indexed - (n,S),Y - 65C816 */
    COUNT,
    
    // Legacy aliases for documentation/clarity (all map to NON)
    IMP = NON, /* Implied/Implicit - no operand */
    ACC = NON, /* Accumulator - operate on A register */
    REL = NON  /* Relative - branch offset */
};


enum class Operation : uint8_t {
    // Core 6502 operations (0-45) - these are used in all processors
    LDA, LDX, LDY,                                    // 0-2: Load operations
    STA, STX, STY,                                    // 3-5: Store operations
    ADC, SBC,                                         // 6-7: Arithmetic
    AND, ORA, EOR,                                    // 8-10: Logic operations
    CMP, CPX, CPY,                                    // 11-13: Compare operations
    ASL, LSR, ROL, ROR,                               // 14-17: Shift/rotate
    INC, DEC,                                         // 18-19: Increment/decrement
    INX, INY, DEX, DEY,                               // 20-23: Register inc/dec
    TAX, TAY, TXA, TYA, TSX, TXS,                     // 24-29: Transfer operations
    PHA, PHP, PLA, PLP,                               // 30-33: Stack operations
    BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS,           // 34-41: Branches
    CLC, SEC, CLI, SEI, CLD, SED, CLV,                // 42-48: Flag operations
    JMP, JSR, RTS, RTI, BRK,                          // 49-53: Control flow
    BIT, NOP, JAM,                                    // 54-56: Test/misc
    
    // Illegal opcodes - most commonly used in 6502/6510 (57-74)
    LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA,           // 57-64: Combo ops
    ANC, ASR, ARR, SBX,                               // 65-68: Special accumulator
    SHA, SHS, SHX, SHY, LAS,                          // 69-73: Store with AND
    XAA,                                              // 74: Special operation
    
    // 65C02 enhancements (75-84)
    BRA, STZ, TRB, TSB, PHX, PHY, PLX, PLY, WAI, STP, // 75-84

    // Extended operations (85-116) - these will map to NOP in 7-bit tables
    // but can be handled via processor-specific logic
    RMB0, RMB1, RMB2, RMB3, RMB4, RMB5, RMB6, RMB7,
    SMB0, SMB1, SMB2, SMB3, SMB4, SMB5, SMB6, SMB7,
    BBR0, BBR1, BBR2, BBR3, BBR4, BBR5, BBR6, BBR7,
    BBS0, BBS1, BBS2, BBS3, BBS4, BBS5, BBS6, BBS7,
    // 65C816 16-bit operations (117-135)
    REP, SEP, XBA, XCE, COP, WDM,
    PEA, PER, PEI, PHB, PHD, PHK, PLB, PLD,
    RTL, JSL, JML, MVN, MVP,
    
    COUNT
};

// ============================================================================
// GLOBAL REGISTER CONSTANTS (optimized layout for both narrow and wide CPUs)
// ============================================================================

// 8-bit register constants - optimized layout with no gaps for 8-bit CPUs
// Type-safe enum typedefs for register access
typedef enum : uint8_t {
    // Core registers (0-11) - used by both 8-bit and 16-bit CPUs
    // 16-bit aligned register pairs (endian-aware) for memory addresses
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    REG_SPL = 0,       // Stack pointer (low byte)
    REG_SPH = 1,       // Stack pointer (high byte)
    REG_ABL = 2,       // Address Bus (low byte)
    REG_ABH = 3,       // Address Bus (high byte)
    REG_PCL = 4,       // Program Counter (low byte)
    REG_PCH = 5,       // Program Counter (high byte)
    REG_AL = 6,        // Accumulator (low byte for 65C816)
    REG_AH = 7,        // Accumulator high byte (65C816) / unused (8-bit CPUs)
    REG_XL = 8,        // X index (low byte for 65C816)
    REG_XH = 9,        // X index high byte (65C816) / unused (8-bit CPUs)
    REG_YL = 10,       // Y index (low byte for 65C816)
    REG_YH = 11,       // Y index high byte (65C816) / unused (8-bit CPUs)
#else
    REG_SPH = 0,       // Stack pointer (high byte)
    REG_SPL = 1,       // Stack pointer (low byte)
    REG_ABH = 2,       // Address Bus (high byte)
    REG_ABL = 3,       // Address Bus (low byte)
    REG_PCH = 4,       // Program Counter (high byte)
    REG_PCL = 5,       // Program Counter (low byte)
    REG_AH = 6,        // Accumulator high byte (65C816) / unused (8-bit CPUs)
    REG_AL = 7,        // Accumulator (low byte for 65C816)
    REG_XH = 8,        // X index high byte (65C816) / unused (8-bit CPUs)
    REG_XL = 9,        // X index (low byte for 65C816)
    REG_YH = 10,       // Y index high byte (65C816) / unused (8-bit CPUs)
    REG_YL = 11,       // Y index (low byte for 65C816)
#endif

    // Common registers (continue from 12) - used by both CPU types
    REG_P = 12,        // Processor status
    REG_IR = 13,       // Instruction Register (current opcode)
    REG_DL = 14,       // Data latch (internal)
    
    // Extended registers (15-18) - only used by 65C816
    REG_DBR = 15,      // Data Bank register (65C816 only)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    REG_DLow = 16,     // Direct Page low byte (65C816 only)
    REG_DH = 17,       // Direct Page high byte (65C816 only)
#else
    REG_DH = 16,       // Direct Page high byte (65C816 only)
    REG_DLow = 17,     // Direct Page low byte (65C816 only)
#endif
    REG_PBR = 18,      // Program Bank register (65C816 only)

    REG_COUNT_16BIT,  // Number of 8-bit registers for 65C816
    REG_COUNT_8BIT = REG_DL + 1,  // Core registers 0-14 (high bytes unused for 8-bit CPUs)
    
    // Compatibility mapping
    REG_A = REG_AL,    // Map legacy A register to AL for compatibility
    REG_X = REG_XL,    // Map legacy X register to XL for compatibility
    REG_Y = REG_YL,    // Map legacy Y register to YL for compatibility
    REG_S = REG_SPL    // Map legacy S register to SPL for compatibility
} reg8_t;

// 16-bit register constants - these work for both narrow and wide CPUs
typedef enum : uint8_t {
    REG_SP = REG_SPL / 2,   // Stack pointer (16-bit)
    REG_AB = REG_ABL / 2,   // Address Bus (16-bit)
    REG_PC = REG_PCL / 2,   // Program Counter (16-bit)
    
    // 65C816 extended 16-bit registers (only meaningful for wide CPUs)
    // Now properly aligned with endian-aware register pairs
    REG_A_FULL = REG_AL / 2, // Full accumulator (65C816 only)
    REG_X_FULL = REG_XL / 2, // Full X register (65C816 only)
    REG_Y_FULL = REG_YL / 2, // Full Y register (65C816 only)
    REG_D = REG_DLow / 2,    // Direct Page register (65C816 only)
} reg16_t;

// ============================================================================
// REGISTER TYPE WRAPPERS FOR TEMPLATE-DEPENDENT TYPES
// ============================================================================

namespace fam65xx {

// Simple wrapper types that provide implicit conversion from register constants
// These allow template-dependent types while maintaining compatibility with REG_* constants

struct narrow_reg8_t {
    uint8_t value;
    
    // Allow implicit conversion from register constants
    constexpr narrow_reg8_t(uint8_t reg) : value(reg) {}
    
    // Allow implicit conversion to uint8_t for array indexing
    constexpr operator uint8_t() const { return value; }
};

struct narrow_reg16_t {
    uint8_t value;
    
    // Allow implicit conversion from register constants
    constexpr narrow_reg16_t(uint8_t reg) : value(reg) {}
    
    // Allow implicit conversion to uint8_t for array indexing
    constexpr operator uint8_t() const { return value; }
};

struct wide_reg8_t {
    uint8_t value;
    
    // Allow implicit conversion from register constants
    constexpr wide_reg8_t(uint8_t reg) : value(reg) {}
    
    // Allow implicit conversion to uint8_t for array indexing
    constexpr operator uint8_t() const { return value; }
};

struct wide_reg16_t {
    uint8_t value;
    
    // Allow implicit conversion from register constants
    constexpr wide_reg16_t(uint8_t reg) : value(reg) {}
    
    // Allow implicit conversion to uint8_t for array indexing
    constexpr operator uint8_t() const { return value; }
};

} // namespace fam65xx

// ============================================================================
// Opcode Encoding
// ============================================================================

enum class OpcodeFlags : uint8_t {
    NONE          = 0x0,  // No special flags
    ILLEGAL_STORE = 0x1,  // Illegal store quirk - uses wrong address on page cross
    SKIP_PAGE     = 0x2,  // Can skip page cross penalty cycle (read operations only)
    RMW           = 0x4,  // Read-Modify-Write operation
    RESERVED      = 0x8   // Reserved for future use
};

// ============================================================================
// MODERN C++ HELPER FUNCTIONS
// ============================================================================

// Helper function to convert scoped enums to indices for array access
// This eliminates the need for static_cast<size_t>() everywhere
template<typename E>
constexpr inline auto to_index(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}

struct opcode_info_t {
    uint16_t op_index : 8;  // Operation index (0-255, bits 0-7) [type Operation]
    uint16_t am_index : 4;  // Addressing mode index (0-15, bits 8-11) [type AddressingMode]
    uint16_t flags    : 4;  // Opcode flags (bits 12-15) [type OpcodeFlags]
    
    // Constructor to handle scoped enum conversion
    constexpr opcode_info_t(Operation op, AddressingMode am, OpcodeFlags fl)
        : op_index(to_index(op))
        , am_index(to_index(am))
        , flags(to_index(fl))
    {}
    
    // Default constructor for aggregate initialization
    constexpr opcode_info_t() : op_index(0), am_index(0), flags(0) {}
};

// ============================================================================
// CONVENIENT TYPE ALIASES FOR CLEAN SYNTAX
// ============================================================================

// Type aliases for cleaner opcode table syntax
using OP = Operation;
using AM = AddressingMode;
using OF = OpcodeFlags;
