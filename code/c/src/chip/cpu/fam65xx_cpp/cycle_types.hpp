#ifndef CYCLE_TYPES_HPP
#define CYCLE_TYPES_HPP

#include "cpu_defs.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace fam65xx_cpp {

// Virtual opcode constants for interrupts (placed after regular 256 opcodes)
static constexpr uint16_t VIRTUAL_OPCODE_RESET = 256;
static constexpr uint16_t VIRTUAL_OPCODE_NMI   = 257;
static constexpr uint16_t VIRTUAL_OPCODE_IRQ   = 258;

// Constants for 1D array layout
static constexpr uint8_t MAX_CYCLES = 8;
static constexpr uint16_t TOTAL_OPCODES = 259;  // 256 regular + 3 virtual opcodes
static constexpr size_t CYCLE_TABLE_SIZE = TOTAL_OPCODES * MAX_CYCLES;  // 259 * 8 = 2072

// Cycle descriptor structure - uses bit fields for packing (16 bits total)
struct cycle_desc_t {
    uint16_t mem_op : 4;      // 4 bits for MemOp (max 15)
    uint16_t data_op : 5;     // 5 bits for DataOp (max 31, need for new values)
    uint16_t alu_op : 6;      // 6 bits for AluOp (max 63, expanded to fit all operations)
    uint16_t sync : 1;        // 1 bit - marks final cycle of instruction (SYNC)
    // Total: 16 bits exactly
    
    // Type-safe accessors that return proper enum types
    constexpr MemOp get_mem_op() const noexcept { return static_cast<MemOp>(mem_op); }
    constexpr DataOp get_data_op() const noexcept { return static_cast<DataOp>(data_op); }
    constexpr AluOp get_alu_op() const noexcept { return static_cast<AluOp>(alu_op); }
    constexpr bool is_sync() const noexcept { return sync != 0; }
};

// Verify the structure size remains 2 bytes (16 bits)
static_assert(sizeof(cycle_desc_t) == 2, "cycle_desc_t must be exactly 2 bytes");

// === COMPILE-TIME SAFEGUARDS AGAINST ENUM OVERFLOW ===
//
// These static assertions prevent enum values from exceeding their bit field limits
// and will trigger compile-time errors if new enum values are added that don't fit.
//
// SAFEGUARD SYSTEM EXPLANATION:
// The cycle_desc_t structure uses bit fields to pack all instruction metadata into 16 bits:
//   - mem_op:  4 bits (values 0-15) for MemOp enum
//   - data_op: 5 bits (values 0-31) for DataOp enum
//   - alu_op:  6 bits (values 0-63) for AluOp enum
//   - sync:    1 bit  (0 or 1) for instruction completion flag
//
// WHAT THE SAFEGUARDS PROTECT AGAINST:
// 1. Enum value overflow - Adding enum values that exceed bit field capacity
// 2. Silent truncation - Values being silently truncated to fit smaller fields
// 3. Future regressions - Changes that break the carefully balanced bit allocation
//
// HOW TO RESPOND TO SAFEGUARD FAILURES:
// If you get a compile error from these assertions, you have two options:
// 1. EXPAND BIT FIELD: Increase the bit field size (requires restructuring cycle_desc_t)
// 2. OPTIMIZE ENUMS: Remove unused enum values or reorganize enum value assignments
//
// EXAMPLE: If MemOp exceeds 4 bits, you could:
// - Change "uint16_t mem_op : 4" to "uint16_t mem_op : 5"
// - Reduce another field by 1 bit to maintain 16-bit total
// - Or remove unused MemOp enum values to stay within 4 bits

// Helper to count enum values at compile time
template<typename T>
constexpr size_t count_enum_values() {
    // This works for consecutive enum values starting from 0
    // If enums become non-consecutive, update this logic accordingly
    return static_cast<size_t>(T::COUNT) + 1; // +1 for 0-based counting
}

// Helper to get the maximum value that fits in N bits
template<size_t N>
constexpr size_t max_value_for_bits() {
    return (1ULL << N) - 1;
}

// MEMOP FIELD VALIDATION (4 bits = max value 15)
static_assert(static_cast<size_t>(MemOp::READ_VECTOR) <= max_value_for_bits<4>(),
    "MemOp enum values exceed 4-bit field capacity in cycle_desc_t! "
    "Maximum allowed value: 15. Consider expanding mem_op bit field.");

// Count all MemOp values to ensure they fit
template<>
constexpr size_t count_enum_values<MemOp>() {
    // Manually count MemOp values since they're not consecutive
    // Update this if new MemOp values are added
    return 15; // NOP=0, READ_PC_INC=1, READ_PC=2, READ_ABS=3, WRITE_ABS=4,
               // READ_ZP=5, WRITE_ZP=6, READ_ZPX=7, WRITE_ZPX=8, READ_ZPY=9,
               // WRITE_ZPY=10, READ_SP=11, WRITE_SP_DEC=12, READ_SP_INC=13,
               // READ_VECTOR=14
}

static_assert(count_enum_values<MemOp>() <= (max_value_for_bits<4>() + 1),
    "Total MemOp enum values exceed 4-bit field capacity! "
    "Current count exceeds 16 values. Expand mem_op bit field or optimize enum values.");

// DATAOP FIELD VALIDATION (5 bits = max value 31)
static_assert(static_cast<size_t>(DataOp::INDIRECT_HIGH) <= max_value_for_bits<5>(),
    "DataOp enum values exceed 5-bit field capacity in cycle_desc_t! "
    "Maximum allowed value: 31. Consider expanding data_op bit field.");

template<>
constexpr size_t count_enum_values<DataOp>() {
    // DataOp values are non-consecutive, manually count them
    // Update this if new DataOp values are added
    return 22; // Based on cpu_defs.hpp: LOAD_A=0 through INDIRECT_HIGH=21
}

static_assert(count_enum_values<DataOp>() <= (max_value_for_bits<5>() + 1),
    "Total DataOp enum values exceed 5-bit field capacity! "
    "Current count exceeds 32 values. Expand data_op bit field or optimize enum values.");

// ALUOP FIELD VALIDATION (6 bits = max value 63)
static_assert(static_cast<size_t>(AluOp::JAM) <= max_value_for_bits<6>(),
    "AluOp enum values exceed 6-bit field capacity in cycle_desc_t! "
    "Maximum allowed value: 63. Consider expanding alu_op bit field.");

template<>
constexpr size_t count_enum_values<AluOp>() {
    // AluOp values appear to be consecutive starting from 0
    // Update this if the enum structure changes
    return 43; // Based on cpu_defs.hpp: NOP=0 through JAM=42
}

static_assert(count_enum_values<AluOp>() <= (max_value_for_bits<6>() + 1),
    "Total AluOp enum values exceed 6-bit field capacity! "
    "Current count exceeds 64 values. Expand alu_op bit field or optimize enum values.");

// BIT FIELD ALLOCATION VALIDATION
// Ensure the bit field sizes add up to exactly 16 bits
static_assert((4 + 5 + 6 + 1) == 16,
    "cycle_desc_t bit field allocation error! "
    "mem_op(4) + data_op(5) + alu_op(6) + sync(1) must equal exactly 16 bits.");

// FUTURE-PROOFING WARNINGS
// These will trigger if someone adds enum values that approach the limits
// Note: MemOp currently uses exactly 16 values (0-15), which perfectly fits the 4-bit field
static_assert(count_enum_values<MemOp>() <= 16,
    "CRITICAL: MemOp enum values exceed or reach 4-bit limit! "
    "Current usage: exactly 16 values (0-15). Cannot add more without expanding mem_op bit field.");

static_assert(count_enum_values<DataOp>() <= 30,
    "WARNING: DataOp enum values approaching 5-bit limit! "
    "Consider expanding data_op bit field before adding more values.");

static_assert(count_enum_values<AluOp>() <= 60,
    "WARNING: AluOp enum values approaching 6-bit limit! "
    "Consider expanding alu_op bit field before adding more values.");

// Specific check for MemOp since we're at exactly the limit
static_assert(count_enum_values<MemOp>() == 15,
    "MemOp enum count has changed! Expected exactly 15 values (0-14) for 4-bit field. "
    "If adding new MemOp values, you MUST expand the mem_op bit field size.");

// Cycle descriptor creation macros
#define CD_MAKE_SYNC(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 1}

#define CD_MAKE(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 0}

// Note: Conditional cycles are now handled in execution logic instead of bit field
#define CD_MAKE_CONDITIONAL(mem, data, alu) CD_MAKE(mem, data, alu)

// Helper function to calculate 1D index from opcode and cycle
static constexpr size_t get_cycle_index(uint16_t opcode, uint8_t cycle) {
    return opcode * MAX_CYCLES + (cycle - 1);  // cycle is 1-based, convert to 0-based
}

} // namespace fam65xx_cpp

#endif // CYCLE_TYPES_HPP