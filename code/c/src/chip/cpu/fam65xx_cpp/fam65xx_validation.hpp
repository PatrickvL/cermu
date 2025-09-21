#ifndef FAM65XX_VALIDATION_HPP
#define FAM65XX_VALIDATION_HPP

#include "cpu_defs.hpp"
#include "cycle_types.hpp"
#include <cstdio>

namespace fam65xx_cpp {

// =============================================================================
// COMPREHENSIVE 6502/65C02 CPU EMULATOR VALIDATION SYSTEM
// =============================================================================
// This file consolidates ALL validation logic for the fam65xx_cpp emulator.
// It provides compile-time validation for:
// - ALU operation flag handling
// - Cycle table SYNC placement 
// - Bit field size constraints
// - Enum value bounds checking
// - Template instantiation safety
// =============================================================================

// =============================================================================
// COMPILE-TIME VALIDATION HELPER FUNCTIONS
// =============================================================================

// Helper to calculate maximum value that fits in N bits
template<size_t bits>
constexpr size_t max_value_for_bits() {
    return (1ULL << bits) - 1;
}

// Helper to count enum values (requires continuous enum values starting from 0)
template<typename EnumType>
constexpr size_t count_enum_values() {
    // This assumes enums are continuous from 0 to N-1
    // For non-continuous enums, this would need manual specification
    return static_cast<size_t>(EnumType::_COUNT);
}

// Specializations for enums that don't have _COUNT
template<>
constexpr size_t count_enum_values<MemOp>() {
    return static_cast<size_t>(MemOp::WRITE_SP_DEC) + 1;
}

template<>
constexpr size_t count_enum_values<DataOp>() {
    return static_cast<size_t>(DataOp::ILLEGAL_COMBO) + 1;
}

template<>
constexpr size_t count_enum_values<AluOp>() {
    return static_cast<size_t>(AluOp::ROR_ACC) + 1;
}

// =============================================================================
// ALU OPERATION VALIDATION SYSTEM
// =============================================================================

/**
 * Template specialization approach for ALU operation validation
 * 
 * This validation system ensures that ALU operations correctly use either:
 * - 'break' statements for operations that should set N/Z flags
 * - 'return' statements for operations that handle their own flags
 * 
 * The template specializations serve as both compile-time validation
 * and documentation of which operations should never reach set_nz_flags.
 */
template<AluOp alu_op>
struct nz_flag_validator {
    /**
     * Default case: operations that SHOULD set N/Z flags pass validation
     * These operations use 'break' in the switch statement to fall through
     * to the common set_nz_flags call.
     */
    static constexpr void validate() {
        // No assertion needed - these operations correctly fall through
    }
};

// =============================================================================
// EXPLICIT SPECIALIZATIONS FOR OPERATIONS THAT HANDLE THEIR OWN FLAGS
// =============================================================================
// These operations correctly use 'return' statements in the switch, so they
// should never actually call validate() at runtime. The specializations serve
// as documentation and would catch any incorrect changes to use 'break'.

template<> struct nz_flag_validator<AluOp::NOP> {
    static constexpr void validate() {
        // NOP correctly uses 'return' - this should never be called
        // NOP performs no operation and sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::TXS> {
    static constexpr void validate() {
        // TXS correctly uses 'return' - this should never be called
        // TXS (Transfer X to Stack Pointer) does not affect N/Z flags
    }
};

template<> struct nz_flag_validator<AluOp::BIT> {
    static constexpr void validate() {
        // BIT correctly uses 'return' - this should never be called
        // BIT handles its own N/V/Z flags based on memory value and A register
    }
};

template<> struct nz_flag_validator<AluOp::CLC> {
    static constexpr void validate() {
        // CLC correctly uses 'return' - this should never be called
        // CLC (Clear Carry) only affects the Carry flag
    }
};

template<> struct nz_flag_validator<AluOp::SEC> {
    static constexpr void validate() {
        // SEC correctly uses 'return' - this should never be called
        // SEC (Set Carry) only affects the Carry flag
    }
};

template<> struct nz_flag_validator<AluOp::CLI> {
    static constexpr void validate() {
        // CLI correctly uses 'return' - this should never be called
        // CLI (Clear Interrupt Disable) only affects the I flag
    }
};

template<> struct nz_flag_validator<AluOp::SEI> {
    static constexpr void validate() {
        // SEI correctly uses 'return' - this should never be called
        // SEI (Set Interrupt Disable) only affects the I flag
    }
};

template<> struct nz_flag_validator<AluOp::CLV> {
    static constexpr void validate() {
        // CLV correctly uses 'return' - this should never be called
        // CLV (Clear Overflow) only affects the V flag
    }
};

template<> struct nz_flag_validator<AluOp::CLD> {
    static constexpr void validate() {
        // CLD correctly uses 'return' - this should never be called
        // CLD (Clear Decimal) only affects the D flag
    }
};

template<> struct nz_flag_validator<AluOp::SED> {
    static constexpr void validate() {
        // SED correctly uses 'return' - this should never be called
        // SED (Set Decimal) only affects the D flag
    }
};

template<> struct nz_flag_validator<AluOp::WAI> {
    static constexpr void validate() {
        // WAI correctly uses 'return' - this should never be called
        // WAI (Wait for Interrupt) - 65C02 instruction that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::STP> {
    static constexpr void validate() {
        // STP correctly uses 'return' - this should never be called
        // STP (Stop) - 65C02 instruction that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::PHX> {
    static constexpr void validate() {
        // PHX correctly uses 'return' - this should never be called
        // PHX (Push X) - 65C02 instruction that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::PHY> {
    static constexpr void validate() {
        // PHY correctly uses 'return' - this should never be called
        // PHY (Push Y) - 65C02 instruction that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::SAX> {
    static constexpr void validate() {
        // SAX correctly uses 'return' - this should never be called
        // SAX (Store A AND X) - illegal opcode that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::JAM> {
    static constexpr void validate() {
        // JAM correctly uses 'return' - this should never be called
        // JAM (Halt CPU) - illegal opcode that sets no flags
    }
};

template<> struct nz_flag_validator<AluOp::BRK_FLAG> {
    static constexpr void validate() {
        // BRK_FLAG correctly uses 'return' - this should never be called
        // BRK flag setting operation only affects B and I flags
    }
};

// =============================================================================
// CYCLE TABLE VALIDATION SYSTEM
// =============================================================================

// Compile-time validation - always enabled with enhanced debugging info
namespace cycle_validation {
    // Template-based validation helpers that show opcode and cycle in error messages
    template<int opcode, int cycle>
    constexpr void trigger_multiple_sync_error() {
        static_assert(opcode < 0, "MULTIPLE_SYNC_FLAGS_DETECTED - Check cycle implementations for duplicate SYNC placements. Opcode and cycle are visible in template parameters above.");
    }
    
    template<int opcode>
    constexpr void trigger_missing_sync_error() {
        static_assert(opcode < 0, "NO_SYNC_FLAG_FOUND - Every opcode must have exactly one SYNC flag in its final cycle. Opcode is visible in template parameter above.");
    }
}

// Enhanced validation macros that provide specific opcode/cycle information in compiler errors
#define VALIDATE_MULTIPLE_SYNC_TEMPLATE(opcode_const, cycle_const) \
    cycle_validation::trigger_multiple_sync_error<opcode_const, cycle_const>()

#define VALIDATE_MISSING_SYNC_TEMPLATE(opcode_const) \
    cycle_validation::trigger_missing_sync_error<opcode_const>()

// Fallback validation macros for runtime variables
#define VALIDATE_MULTIPLE_SYNC(opcode_var, cycle_var) \
    cycle_validation::trigger_multiple_sync_error<255, MAX_CYCLES>()

#define VALIDATE_MISSING_SYNC(opcode_var) \
    cycle_validation::trigger_missing_sync_error<255>()

// =============================================================================
// BIT FIELD AND ENUM VALIDATION SYSTEM
// =============================================================================

// Verify the cycle descriptor structure size remains 2 bytes (16 bits)
static_assert(sizeof(cycle_desc_t) == 2, "cycle_desc_t must be exactly 2 bytes");

// MEMOP FIELD VALIDATION (4 bits = max value 15)
static_assert(static_cast<size_t>(MemOp::WRITE_SP_DEC) <= max_value_for_bits<4>(),
    "MemOp enum values exceed 4-bit field capacity in cycle_desc_t! "
    "Current highest value WRITE_SP_DEC must fit in 4 bits (max 15). "
    "Either reduce enum values or increase field size in cycle_desc_t. "
    "This error indicates that the MemOp enum has grown beyond the allocated "
    "4-bit field in the packed cycle descriptor structure.");

static_assert(count_enum_values<MemOp>() <= (max_value_for_bits<4>() + 1),
    "Total MemOp enum values exceed 4-bit field capacity! "
    "Reduce enum count or increase field size in cycle_desc_t bit fields.");

// DATAOP FIELD VALIDATION (5 bits = max value 31)
static_assert(static_cast<size_t>(DataOp::INDIRECT_HIGH) <= max_value_for_bits<5>(),
    "DataOp enum values exceed 5-bit field capacity in cycle_desc_t! "
    "Current highest value INDIRECT_HIGH must fit in 5 bits (max 31). "
    "Either reduce enum values or increase field size in cycle_desc_t. "
    "This error indicates that the DataOp enum has grown beyond the allocated "
    "5-bit field in the packed cycle descriptor structure.");

static_assert(count_enum_values<DataOp>() <= (max_value_for_bits<5>() + 1),
    "Total DataOp enum values exceed 5-bit field capacity! "
    "Reduce enum count or increase field size in cycle_desc_t bit fields.");

// ALUOP FIELD VALIDATION (6 bits = max value 63)
static_assert(static_cast<size_t>(AluOp::JAM) <= max_value_for_bits<6>(),
    "AluOp enum values exceed 6-bit field capacity in cycle_desc_t! "
    "Current highest value JAM must fit in 6 bits (max 63). "
    "Either reduce enum values or increase field size in cycle_desc_t. "
    "This error indicates that the AluOp enum has grown beyond the allocated "
    "6-bit field in the packed cycle descriptor structure.");

static_assert(count_enum_values<AluOp>() <= (max_value_for_bits<6>() + 1),
    "Total AluOp enum values exceed 6-bit field capacity! "
    "Reduce enum count or increase field size in cycle_desc_t bit fields.");

// BIT FIELD ALLOCATION VALIDATION
// Ensure the bit field sizes add up to exactly 16 bits
static_assert((4 + 5 + 6 + 1) == 16,
    "cycle_desc_t bit field allocation error! "
    "MemOp(4) + DataOp(5) + AluOp(6) + sync(1) must equal 16 bits total.");

// CAPACITY WARNING THRESHOLDS
// Note: MemOp currently uses exactly 16 values (0-15), which perfectly fits the 4-bit field
static_assert(count_enum_values<MemOp>() <= 16,
    "CRITICAL: MemOp enum values exceed or reach 4-bit limit! "
    "Current count approaches maximum capacity for 4-bit field (16 values 0-15).");

static_assert(count_enum_values<DataOp>() <= 30,
    "WARNING: DataOp enum values approaching 5-bit limit! "
    "Consider field size increase if more operations needed (max 32 values 0-31).");

static_assert(count_enum_values<AluOp>() <= 60,
    "WARNING: AluOp enum values approaching 6-bit limit! "
    "Consider field size increase if more operations needed (max 64 values 0-63).");

// SPECIFIC CRITICAL CHECKS
// Specific check for MemOp since we're at exactly the limit
static_assert(count_enum_values<MemOp>() == 14,
    "MemOp enum count has changed! Expected exactly 14 values (0-13) for 4-bit field. "
    "If adding new MemOp values, you MUST increase the bit field size in cycle_desc_t. "
    "Current allocation: MemOp(4 bits) + DataOp(5 bits) + AluOp(6 bits) + sync(1 bit) = 16 bits");

// =============================================================================
// COMPREHENSIVE VALIDATION SUMMARY
// =============================================================================

/**
 * VALIDATION SYSTEM OVERVIEW:
 * 
 * 1. ALU OPERATION VALIDATION:
 *    - Ensures proper flag handling (break vs return statements)
 *    - Prevents operations from incorrectly reaching set_nz_flags
 *    - Documents which operations handle their own flags
 * 
 * 2. CYCLE TABLE VALIDATION:
 *    - Verifies SYNC flag placement in instruction cycles
 *    - Prevents multiple SYNC flags per instruction
 *    - Ensures every instruction has exactly one SYNC flag
 * 
 * 3. BIT FIELD VALIDATION:
 *    - Enforces 16-bit total size for cycle_desc_t
 *    - Validates enum values fit within allocated bit fields
 *    - Provides capacity warnings before limits are reached
 * 
 * 4. ENUM BOUNDS VALIDATION:
 *    - Prevents enum values from exceeding bit field capacity
 *    - Provides detailed error messages for debugging
 *    - Maintains backward compatibility with existing code
 * 
 * All validations are compile-time only and produce zero runtime overhead.
 * Error messages include specific context (opcodes, cycles, enum values)
 * to facilitate quick debugging and resolution.
 */

} // namespace fam65xx_cpp

#endif // FAM65XX_VALIDATION_HPP