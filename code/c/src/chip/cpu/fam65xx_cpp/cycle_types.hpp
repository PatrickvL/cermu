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
static constexpr uint16_t VIRTUAL_OPCODE_BRK   = 259;

// Constants for 1D array layout
static constexpr uint8_t MAX_CYCLES = 8;
static constexpr uint16_t TOTAL_OPCODES = 260;  // 256 regular + 4 virtual opcodes
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

// Note: Validation for cycle_desc_t bit fields is now in fam65xx_validation.hpp

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