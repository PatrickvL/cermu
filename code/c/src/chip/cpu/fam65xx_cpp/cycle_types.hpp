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
static constexpr uint16_t VIRTUAL_OPCODE_ABORT = 260;  // 65C816 ABORT interrupt
static constexpr uint16_t VIRTUAL_OPCODE_COP   = 261;  // 65C816 COP instruction

// Constants for 1D array layout
static constexpr uint8_t MAX_CYCLES = 8;
static constexpr uint16_t TOTAL_OPCODES = 262;  // 256 regular + 6 virtual opcodes (RESET, NMI, IRQ, BRK, ABORT, COP)
static constexpr size_t CYCLE_TABLE_SIZE = TOTAL_OPCODES * MAX_CYCLES;  // 262 * 8 = 2096

// Cycle descriptor structure - uses bit fields for packing (16 bits total)
struct cycle_desc_t {
    uint16_t mem_op : 4;      // 4 bits for MemOp (max 15)
    uint16_t data_op : 5;     // 5 bits for DataOp (max 31, need for new values)
    uint16_t alu_op : 6;      // 6 bits for AluOp (max 63, RESTORED to support 59 AluOp elements)
    uint16_t sync : 1;        // 1 bit - marks final cycle of instruction (SYNC)
    // Total: 16 bits exactly (4+5+6+1=16)
    
    // Type-safe accessors that return proper enum types
    constexpr MemOp get_mem_op() const noexcept { return static_cast<MemOp>(mem_op); }
    constexpr DataOp get_data_op() const noexcept { return static_cast<DataOp>(data_op); }
    constexpr AluOp get_alu_op() const noexcept { return static_cast<AluOp>(alu_op); }
    constexpr bool is_sync() const noexcept { return sync != 0; }
    
    // φ1/φ2 phase inference methods - derive phase from operation characteristics
    constexpr bool is_address_setup() const noexcept {
        auto mem = get_mem_op();
        auto data = get_data_op();
        // Address setup operations typically occur in φ2
        return (mem == MemOp::READ_PC_INC || mem == MemOp::READ_PC) ||
               (data == DataOp::ADDR_CALC_LOW || data == DataOp::ADDR_CALC_HIGH ||
                data == DataOp::ADDR_ADD_X || data == DataOp::ADDR_ADD_Y);
    }
    
    constexpr bool is_data_processing() const noexcept {
        auto alu = get_alu_op();
        auto data = get_data_op();
        // Data processing operations typically occur in φ1
        return (alu != AluOp::NOP) ||
               (data == DataOp::LOAD_A || data == DataOp::LOAD_X || data == DataOp::LOAD_Y) ||
               (data == DataOp::TEMP_STORE || data == DataOp::TEMP_MODIFY);
    }
    
    constexpr bool is_bus_control() const noexcept {
        auto mem = get_mem_op();
        // Bus control (writes) typically occur in φ2
        return (mem == MemOp::WRITE_ABS || mem == MemOp::WRITE_ZP ||
                mem == MemOp::WRITE_SP_DEC || mem == MemOp::WRITE_ZPX);
    }
    
    constexpr bool occurs_in_phi1() const noexcept {
        // φ1: Data sampling, internal processing
        return is_data_processing() || (!is_address_setup() && !is_bus_control());
    }
    
    constexpr bool occurs_in_phi2() const noexcept {
        // φ2: Address setup, bus control
        return is_address_setup() || is_bus_control();
    }
    
    constexpr bool spans_both_phases() const noexcept {
        // Complex operations that span both phases
        return occurs_in_phi1() && occurs_in_phi2();
    }
};

// Verify the structure size remains 2 bytes (16 bits)
static_assert(sizeof(cycle_desc_t) == 2, "cycle_desc_t must be exactly 2 bytes");

// Note: Validation for cycle_desc_t bit fields is now in fam65xx_validation.hpp

// Cycle descriptor creation macros (restored to original structure)
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