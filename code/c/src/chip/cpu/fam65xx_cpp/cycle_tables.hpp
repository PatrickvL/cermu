#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

#include "cpu_defs.hpp"
#include <array>
#include <cstdio>

namespace fam65xx_cpp {

// Virtual opcode constants for interrupts (placed after regular 256 opcodes)
static constexpr uint16_t VIRTUAL_OPCODE_RESET = 256;
static constexpr uint16_t VIRTUAL_OPCODE_NMI   = 257;
static constexpr uint16_t VIRTUAL_OPCODE_IRQ   = 258;

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
    // Remove is_conditional() since we removed the field for space
};

// Verify the structure size remains 2 bytes (16 bits)
static_assert(sizeof(cycle_desc_t) == 2, "cycle_desc_t must be exactly 2 bytes");

// Cycle descriptor creation macros
#define CD_MAKE_SYNC(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 1}

#define CD_MAKE(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 0}

// Note: Conditional cycles are now handled in execution logic instead of bit field
#define CD_MAKE_CONDITIONAL(mem, data, alu) CD_MAKE(mem, data, alu)

template<typename BusConfig>
class CycleTables {
public:
    // === COMPREHENSIVE ADDRESSING MODE HELPERS ===
    
    // Single-cycle immediate operations (1 cycle total) - VALIDATED SYNC PLACEMENT
    static constexpr cycle_desc_t make_immediate(DataOp data_op, AluOp alu_op = AluOp::NOP) {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, data_op, alu_op);
    }
    
    // Zero page addressing helpers (2 cycles total)
    static constexpr cycle_desc_t make_zeropage(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Zero page write operations (2 cycles total)
    static constexpr cycle_desc_t make_zeropage_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Absolute addressing helpers (3 cycles total)
    static constexpr cycle_desc_t make_absolute(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Absolute write operations (3 cycles total)
    static constexpr cycle_desc_t make_absolute_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Zero page indexed (zp,X or zp,Y) operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
        return make_empty_cycle();
    }
    
    // Zero page indexed write operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Absolute indexed (abs,X or abs,Y) operations (4+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_absolute_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y, may cross page
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 4, always executes
        return make_empty_cycle();
    }
    
    // Absolute indexed write operations (always 4 cycles - writes always do extra cycle)
    static constexpr cycle_desc_t make_absolute_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // Read dummy byte first (6502 quirk)
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Indirect indexed (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X to zero page pointer
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read target address low
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read target address high
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // Read from target
        return make_empty_cycle();
    }
    
    // Indirect indexed write (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Indirect indexed (zp),Y operations (5+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_indirect_indexed(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read base address low
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read base address high
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y, may cross page
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 5, always executes
        return make_empty_cycle();
    }
    
    // Indirect indexed write (zp),Y operations (always 5 cycles)
    static constexpr cycle_desc_t make_indirect_indexed_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Read dummy first
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Memory modify operations (5 cycles: read address, read data, write old, write new)
    static constexpr cycle_desc_t make_memory_modify_zp(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, modify_op); // Write modified value WITHOUT SYNC
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // SYNC on cycle 5
        return make_empty_cycle(); // Return empty cycle for invalid cycles
    }
    
    // Memory modify operations absolute (6 cycles)
    static constexpr cycle_desc_t make_memory_modify_abs(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_MODIFY, modify_op); // Write modified value WITHOUT SYNC
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // SYNC on cycle 6
        return make_empty_cycle(); // Return empty cycle for invalid cycles
    }
    
    // Stack operations
    static constexpr cycle_desc_t make_stack_push(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, store_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_stack_pull(uint8_t cycle, DataOp load_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, load_op, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Branch operations (2+ cycles base, +1 if taken, +1 more if page crossed)
    static constexpr cycle_desc_t make_branch(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Cycle 2+, execution decides how many
        return make_empty_cycle();
    }
    
    // Jump operations
    static constexpr cycle_desc_t make_jump_absolute(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_jump_indirect(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP); // 6502 page boundary bug
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Subroutine operations
    static constexpr cycle_desc_t make_jsr(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_rts(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Internal - increment PC
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // Interrupt sequences
    static constexpr cycle_desc_t make_interrupt_sequence(uint8_t cycle, AluOp flag_op = AluOp::SEI) {
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, flag_op);     // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);    // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return make_empty_cycle(); // Invalid cycle
    }
    
    static constexpr cycle_desc_t make_rti(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull status
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
        return make_empty_cycle();
    }
    
    // === INSTRUCTION IMPLEMENTATIONS (using helpers) ===
    
    // LDA immediate - direct SYNC implementation
    static constexpr cycle_desc_t make_lda_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_A, AluOp::NOP) : make_empty_cycle();
    }
    
    // LDA zero page - 2 cycles
    static constexpr cycle_desc_t make_lda_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_A);
    }
    
    // LDA absolute - 3 cycles
    static constexpr cycle_desc_t make_lda_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::LOAD_A);
    }
    
    // ADC immediate - direct SYNC implementation
    static constexpr cycle_desc_t make_adc_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ADC) : make_empty_cycle();
    }
    
    // Real NOP opcode (0xEA) - 1 cycle instruction
    static constexpr cycle_desc_t make_nop(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP) : make_empty_cycle();
    }
    
    // Empty cycle slot - used to fill unused table entries after SYNC
    static constexpr cycle_desc_t make_empty_cycle() {
        return CD_MAKE(MemOp::NOP, DataOp::NOP, AluOp::NOP);  // No SYNC
    }
    
    // BRK - 7 cycles (explicit implementation)
    static constexpr cycle_desc_t make_brk(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Read next byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return make_empty_cycle();
    }
    
    // LAX zero page - illegal opcode, 2 cycles
    static constexpr cycle_desc_t make_lax_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ILLEGAL_COMBO);
    }
    
    // STA operations - store A register
    static constexpr cycle_desc_t make_sta_zp(uint8_t cycle) {
        return make_zeropage_write(cycle, DataOp::STORE_A);
    }
    
    static constexpr cycle_desc_t make_sta_abs(uint8_t cycle) {
        return make_absolute_write(cycle, DataOp::STORE_A);
    }
    
    // Transfer operations - direct SYNC implementation to avoid helper issues
    static constexpr cycle_desc_t make_txa(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXA) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tax(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAX) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tya(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TYA) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tay(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAY) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tsx(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TSX) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_txs(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXS) : make_empty_cycle();
    }
    
    // Flag operations - direct SYNC implementation to avoid helper issues
    static constexpr cycle_desc_t make_clc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sec(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cli(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLI) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sei(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEI) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_clv(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLV) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cld(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLD) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sed(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SED) : make_empty_cycle();
    }
    
    // Logical operations - direct SYNC implementation
    static constexpr cycle_desc_t make_and_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::AND) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ora_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ORA) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_eor_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::EOR) : make_empty_cycle();
    }
    
    // Compare operations - direct SYNC implementation
    static constexpr cycle_desc_t make_cmp_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CMP) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cpx_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPX) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cpy_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPY) : make_empty_cycle();
    }
    
    // Load operations - direct SYNC implementation
    static constexpr cycle_desc_t make_ldx_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_X, AluOp::NOP) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ldy_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_Y, AluOp::NOP) : make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_ldx_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_X);
    }
    
    static constexpr cycle_desc_t make_ldy_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_Y);
    }
    
    // Inc/Dec operations - direct SYNC implementation
    static constexpr cycle_desc_t make_inx(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_iny(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dex(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dey(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : make_empty_cycle();
    }
    
    // Stack operations
    static constexpr cycle_desc_t make_pha(uint8_t cycle) { return make_stack_push(cycle, DataOp::STORE_A); }
    static constexpr cycle_desc_t make_pla(uint8_t cycle) { return make_stack_pull(cycle, DataOp::LOAD_A); }
    static constexpr cycle_desc_t make_php(uint8_t cycle) { return make_stack_push(cycle, DataOp::STACK_PUSH); }
    static constexpr cycle_desc_t make_plp(uint8_t cycle) { return make_stack_pull(cycle, DataOp::STACK_PULL); }
    
    // Branch instructions - 2 cycles base, +1 if taken, +1 if page crossed (all identical)
    static constexpr cycle_desc_t make_bcc(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bcs(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_beq(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bne(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bpl(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bmi(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bvc(uint8_t cycle) { return make_branch(cycle); }
    static constexpr cycle_desc_t make_bvs(uint8_t cycle) { return make_branch(cycle); }
    
    // Jump instructions - JMP abs is 3 cycles
    static constexpr cycle_desc_t make_jmp_abs(uint8_t cycle) {
        return make_jump_absolute(cycle);
    }
    
    // Shift/Rotate operations - direct SYNC implementation
    static constexpr cycle_desc_t make_asl_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ASL) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_lsr_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::LSR) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_rol_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROL) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ror_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROR) : make_empty_cycle();
    }
    
    // Memory INC/DEC operations - 5 cycles for zero page
    static constexpr cycle_desc_t make_inc_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::INC); }
    static constexpr cycle_desc_t make_dec_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::DEC); }

    // Memory shift operations - 5 cycles for zero page
    static constexpr cycle_desc_t make_asl_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::ASL); }
    static constexpr cycle_desc_t make_lsr_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::LSR); }
    static constexpr cycle_desc_t make_rol_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::ROL); }
    static constexpr cycle_desc_t make_ror_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::ROR); }

    // Zero page indexed addressing modes (zp,x and zp,y) - 3 cycles
    static constexpr cycle_desc_t make_lda_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_ldx_zpy(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X);
    }

    // Absolute indexed addressing modes (abs,x and abs,y) - 4+ cycles
    static constexpr cycle_desc_t make_lda_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_lda_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_A);
    }

    // Indirect indexed addressing modes - (zp,x) and (zp),y - 5+ cycles
    static constexpr cycle_desc_t make_lda_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_lda_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::LOAD_A);
    }

    // 65C02 specific instructions - BRA is 2 cycles
    static constexpr cycle_desc_t make_bra(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_phx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_X, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_phy(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_Y, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_plx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_X, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_ply(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_Y, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_stz_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_ZP, DataOp::STORE_ZERO, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_stz_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::STORE_ZERO, AluOp::NOP);
        return make_empty_cycle();
    }


    // Complete addressing modes for STA operations - 3 cycles
    static constexpr cycle_desc_t make_sta_zpx(uint8_t cycle) {
        return make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_absx(uint8_t cycle) {
        return make_absolute_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_absy(uint8_t cycle) {
        return make_absolute_indexed_write(cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_indx(uint8_t cycle) {
        return make_indexed_indirect_write(cycle, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_indy(uint8_t cycle) {
        return make_indirect_indexed_write(cycle, DataOp::STORE_A);
    }

    // Complete addressing modes for STX/STY
    static constexpr cycle_desc_t make_stx_zp(uint8_t cycle) {
        return make_zeropage_write(cycle, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_stx_zpy(uint8_t cycle) {
        return make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_stx_abs(uint8_t cycle) {
        return make_absolute_write(cycle, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_sty_zp(uint8_t cycle) {
        return make_zeropage_write(cycle, DataOp::STORE_Y);
    }

    static constexpr cycle_desc_t make_sty_zpx(uint8_t cycle) {
        return make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_Y);
    }

    static constexpr cycle_desc_t make_sty_abs(uint8_t cycle) {
        return make_absolute_write(cycle, DataOp::STORE_Y);
    }

    // Complete addressing modes for ALU operations
    static constexpr cycle_desc_t make_adc_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_adc_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_sbc_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::SBC) : make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sbc_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::SBC);
    }

    static constexpr cycle_desc_t make_sbc_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::SBC);
    }

    // Indirect JMP - 5 cycles
    static constexpr cycle_desc_t make_jmp_ind(uint8_t cycle) {
        return make_jump_indirect(cycle);
    }

    // Major illegal opcodes (NMOS 6502) - special case for SAX (2-cycle)
    static constexpr cycle_desc_t make_sax_zp(uint8_t cycle) {
        return make_zeropage_write(cycle, DataOp::ILLEGAL_COMBO);
    }

    // All other illegal opcodes use the standard memory modification pattern
    static constexpr cycle_desc_t make_dcp_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::DCP); }
    static constexpr cycle_desc_t make_isc_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::ISC); }
    static constexpr cycle_desc_t make_slo_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::SLO); }
    static constexpr cycle_desc_t make_rla_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::RLA); }
    static constexpr cycle_desc_t make_sre_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::SRE); }
    static constexpr cycle_desc_t make_rra_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::RRA); }

    // 65C02 additional instructions - also use memory modification pattern
    static constexpr cycle_desc_t make_tsb_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::TSB); }
    static constexpr cycle_desc_t make_trb_zp(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::TRB); }

    // BIT instruction variations
    static constexpr cycle_desc_t make_bit_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::BIT);
    }

    static constexpr cycle_desc_t make_bit_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::BIT);
    }

    // === ADDITIONAL INSTRUCTION IMPLEMENTATIONS FOR COMPLETE 6502 SET ===
    
    // Complete ORA addressing modes
    static constexpr cycle_desc_t make_ora_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::ORA);
    }

    // Complete AND addressing modes
    static constexpr cycle_desc_t make_and_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::AND);
    }

    // Complete EOR addressing modes
    static constexpr cycle_desc_t make_eor_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::EOR);
    }

    // Complete ADC addressing modes
    static constexpr cycle_desc_t make_adc_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::ADC);
    }

    // Complete SBC addressing modes
    static constexpr cycle_desc_t make_sbc_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::SBC);
    }

    // Complete CMP addressing modes
    static constexpr cycle_desc_t make_cmp_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_indx(uint8_t cycle) {
        return make_indexed_indirect(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_indy(uint8_t cycle) {
        return make_indirect_indexed(cycle, DataOp::ALU, AluOp::CMP);
    }

    // Complete CPX/CPY addressing modes
    static constexpr cycle_desc_t make_cpx_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::CPX);
    }
    static constexpr cycle_desc_t make_cpx_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::CPX);
    }
    static constexpr cycle_desc_t make_cpy_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::ALU, AluOp::CPY);
    }
    static constexpr cycle_desc_t make_cpy_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::ALU, AluOp::CPY);
    }

    // Complete LDX/LDY addressing modes
    static constexpr cycle_desc_t make_ldx_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::LOAD_X);
    }
    static constexpr cycle_desc_t make_ldx_absy(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X);
    }
    static constexpr cycle_desc_t make_ldy_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y);
    }
    static constexpr cycle_desc_t make_ldy_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::LOAD_Y);
    }
    static constexpr cycle_desc_t make_ldy_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y);
    }

    // Complete memory modification operations (INC/DEC/ASL/LSR/ROL/ROR)
    static constexpr cycle_desc_t make_inc_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::INC);  // Note: indexed versions use same helper
    }
    static constexpr cycle_desc_t make_dec_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::DEC);
    }
    static constexpr cycle_desc_t make_inc_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::INC);
    }
    static constexpr cycle_desc_t make_dec_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::DEC);
    }
    static constexpr cycle_desc_t make_inc_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::INC);  // Note: indexed versions use same helper
    }
    static constexpr cycle_desc_t make_dec_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::DEC);
    }

    // Complete shift/rotate operations
    static constexpr cycle_desc_t make_asl_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_zpx(uint8_t cycle) {
        return make_memory_modify_zp(cycle, AluOp::ROR);
    }
    static constexpr cycle_desc_t make_asl_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ROR);
    }
    static constexpr cycle_desc_t make_asl_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_absx(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::ROR);
    }

    // NOP variants (illegal opcodes with different addressing modes)
    static constexpr cycle_desc_t make_nop_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::NOP);
    }

    // 65C02 extensions
    static constexpr cycle_desc_t make_bit_zpx(uint8_t cycle) {
        return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT);
    }
    static constexpr cycle_desc_t make_bit_absx(uint8_t cycle) {
        return make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT);
    }
    static constexpr cycle_desc_t make_stz_zpx(uint8_t cycle) {
        return make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_ZERO);
    }
    static constexpr cycle_desc_t make_tsb_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::TSB);
    }
    static constexpr cycle_desc_t make_trb_abs(uint8_t cycle) {
        return make_memory_modify_abs(cycle, AluOp::TRB);
    }
    static constexpr cycle_desc_t make_jmp_absx_ind(uint8_t cycle) {
        // JMP (abs,X) - 65C02 only, 6 cycles
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_X, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 5) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return make_empty_cycle();
    }

    // Complete illegal opcode implementations
    static constexpr cycle_desc_t make_slo_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::SLO); }
    static constexpr cycle_desc_t make_slo_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::SLO); }
    static constexpr cycle_desc_t make_slo_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::SLO); }
    static constexpr cycle_desc_t make_slo_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::SLO); }
    static constexpr cycle_desc_t make_slo_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::SLO); }

    static constexpr cycle_desc_t make_rla_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::RLA); }
    static constexpr cycle_desc_t make_rla_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::RLA); }
    static constexpr cycle_desc_t make_rla_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::RLA); }
    static constexpr cycle_desc_t make_rla_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::RLA); }
    static constexpr cycle_desc_t make_rla_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::RLA); }

    static constexpr cycle_desc_t make_sre_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::SRE); }
    static constexpr cycle_desc_t make_sre_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::SRE); }
    static constexpr cycle_desc_t make_sre_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::SRE); }
    static constexpr cycle_desc_t make_sre_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::SRE); }
    static constexpr cycle_desc_t make_sre_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::SRE); }

    static constexpr cycle_desc_t make_rra_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::RRA); }
    static constexpr cycle_desc_t make_rra_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::RRA); }
    static constexpr cycle_desc_t make_rra_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::RRA); }
    static constexpr cycle_desc_t make_rra_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::RRA); }
    static constexpr cycle_desc_t make_rra_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::RRA); }

    static constexpr cycle_desc_t make_sax_abs(uint8_t cycle) { return make_absolute_write(cycle, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_sax_zpy(uint8_t cycle) { return make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_sax_indx(uint8_t cycle) { return make_indexed_indirect_write(cycle, DataOp::ILLEGAL_COMBO); }

    static constexpr cycle_desc_t make_lax_abs(uint8_t cycle) { return make_absolute(cycle, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_lax_absy(uint8_t cycle) { return make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_lax_zpy(uint8_t cycle) { return make_zeropage_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_lax_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO); }
    static constexpr cycle_desc_t make_lax_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO); }

    static constexpr cycle_desc_t make_dcp_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::DCP); }
    static constexpr cycle_desc_t make_dcp_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::DCP); }
    static constexpr cycle_desc_t make_dcp_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::DCP); }
    static constexpr cycle_desc_t make_dcp_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::DCP); }
    static constexpr cycle_desc_t make_dcp_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::DCP); }

    static constexpr cycle_desc_t make_isc_zpx(uint8_t cycle) { return make_memory_modify_zp(cycle, AluOp::ISC); }
    static constexpr cycle_desc_t make_isc_abs(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::ISC); }
    static constexpr cycle_desc_t make_isc_absx(uint8_t cycle) { return make_memory_modify_abs(cycle, AluOp::ISC); }
    static constexpr cycle_desc_t make_isc_indx(uint8_t cycle) { return make_indexed_indirect(cycle, DataOp::ILLEGAL_COMBO, AluOp::ISC); }
    static constexpr cycle_desc_t make_isc_indy(uint8_t cycle) { return make_indirect_indexed(cycle, DataOp::ILLEGAL_COMBO, AluOp::ISC); }

    // === INTERRUPT CYCLE SEQUENCES (unified using shared patterns) ===
    
    // Base interrupt sequence generator - used by BRK, NMI, IRQ, and RESET
    static constexpr cycle_desc_t get_reset_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::DUMMY_READ, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
        if (cycle == 2) return CD_MAKE(MemOp::DUMMY_READ, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return make_empty_cycle();
    }
    
    // NMI sequence - uses shared interrupt pattern with different initial cycles
    static constexpr cycle_desc_t get_nmi_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);     // Read next instruction byte
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);         // Read next instruction byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return make_empty_cycle();
    }
    
    // IRQ sequence - identical to NMI (different vector handled by DataOp::INTERRUPT_VEC)
    static constexpr cycle_desc_t get_irq_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);     // Read next instruction byte
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);         // Read next instruction byte (dummy)
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::SEI);  // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);   // Read vector low
        if (cycle == 7) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
        return make_empty_cycle();
    }

    // === COMPILE-TIME 1D CYCLE TABLE IMPLEMENTATION ===
    
    // Constants for 1D array layout
    static constexpr uint8_t MAX_CYCLES = 8;
    static constexpr uint16_t TOTAL_OPCODES = 259;  // 256 regular + 3 virtual opcodes
    static constexpr size_t CYCLE_TABLE_SIZE = TOTAL_OPCODES * MAX_CYCLES;  // 259 * 8 = 2072
    
    // Helper function to calculate 1D index from opcode and cycle
    static constexpr size_t get_cycle_index(uint16_t opcode, uint8_t cycle) {
        return opcode * MAX_CYCLES + (cycle - 1);  // cycle is 1-based, convert to 0-based
    }
    
    // Template-based get_cycle method that adapts using template arguments for compile-time generation
    template<uint16_t OpCode, uint8_t Cycle>
    static constexpr cycle_desc_t get_cycle() {
        // Handle virtual opcodes (256, 257, 258)
        if constexpr (OpCode >= 256) {
            if constexpr (OpCode == VIRTUAL_OPCODE_RESET) return get_reset_cycle(Cycle);
            else if constexpr (OpCode == VIRTUAL_OPCODE_NMI) return get_nmi_cycle(Cycle);
            else if constexpr (OpCode == VIRTUAL_OPCODE_IRQ) return get_irq_cycle(Cycle);
            else return make_empty_cycle(); // Should not happen with only 3 virtual opcodes
        }
        
        // Handle regular opcodes (0-255) - COMPLETE 6502 INSTRUCTION SET
        constexpr uint8_t op = static_cast<uint8_t>(OpCode);
        
        // Use if constexpr for compile-time dispatch
        if constexpr (op == 0x00) return make_brk(Cycle);                        // BRK impl
        else if constexpr (op == 0x10) return make_bpl(Cycle);                   // BPL rel
        else if constexpr (op == 0x20) return make_jsr(Cycle);                   // JSR abs
        else if constexpr (op == 0x30) return make_bmi(Cycle);                   // BMI rel
        else if constexpr (op == 0x40) return make_rti(Cycle);                   // RTI impl
        else if constexpr (op == 0x50) return make_bvc(Cycle);                   // BVC rel
        else if constexpr (op == 0x60) return make_rts(Cycle);                   // RTS impl
        else if constexpr (op == 0x70) return make_bvs(Cycle);                   // BVS rel
        else if constexpr (op == 0x80) return make_nop(Cycle);                   // NOP #imm (illegal on NMOS) / BRA rel (65C02)
        else if constexpr (op == 0x90) return make_bcc(Cycle);                   // BCC rel
        else if constexpr (op == 0xA0) return make_ldy_imm(Cycle);               // LDY #imm
        else if constexpr (op == 0xB0) return make_bcs(Cycle);                   // BCS rel
        else if constexpr (op == 0xC0) return make_cpy_imm(Cycle);               // CPY #imm
        else if constexpr (op == 0xD0) return make_bne(Cycle);                   // BNE rel
        else if constexpr (op == 0xE0) return make_cpx_imm(Cycle);               // CPX #imm
        else if constexpr (op == 0xF0) return make_beq(Cycle);                   // BEQ rel
        
        // Column 1: Indexed Indirect (zp,X)
        else if constexpr (op == 0x01) return make_ora_indx(Cycle);              // ORA (zp,X)
        else if constexpr (op == 0x11) return make_ora_indy(Cycle);              // ORA (zp),Y
        else if constexpr (op == 0x21) return make_and_indx(Cycle);              // AND (zp,X)
        else if constexpr (op == 0x31) return make_and_indy(Cycle);              // AND (zp),Y
        else if constexpr (op == 0x41) return make_eor_indx(Cycle);              // EOR (zp,X)
        else if constexpr (op == 0x51) return make_eor_indy(Cycle);              // EOR (zp),Y
        else if constexpr (op == 0x61) return make_adc_indx(Cycle);              // ADC (zp,X)
        else if constexpr (op == 0x71) return make_adc_indy(Cycle);              // ADC (zp),Y
        else if constexpr (op == 0x81) return make_sta_indx(Cycle);              // STA (zp,X)
        else if constexpr (op == 0x91) return make_sta_indy(Cycle);              // STA (zp),Y
        else if constexpr (op == 0xA1) return make_lda_indx(Cycle);              // LDA (zp,X)
        else if constexpr (op == 0xB1) return make_lda_indy(Cycle);              // LDA (zp),Y
        else if constexpr (op == 0xC1) return make_cmp_indx(Cycle);              // CMP (zp,X)
        else if constexpr (op == 0xD1) return make_cmp_indy(Cycle);              // CMP (zp),Y
        else if constexpr (op == 0xE1) return make_sbc_indx(Cycle);              // SBC (zp,X)
        else if constexpr (op == 0xF1) return make_sbc_indy(Cycle);              // SBC (zp),Y
        
        // Column 2: Illegal/Undocumented/65C02
        else if constexpr (op == 0x02) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x12) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x22) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x32) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x42) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x52) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x62) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x72) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0x82) return make_nop(Cycle);                   // NOP #imm (illegal)
        else if constexpr (op == 0x92) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0xA2) return make_ldx_imm(Cycle);               // LDX #imm
        else if constexpr (op == 0xB2) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0xC2) return make_nop(Cycle);                   // NOP #imm (illegal)
        else if constexpr (op == 0xD2) return make_nop(Cycle);                   // HLT/JAM (illegal)
        else if constexpr (op == 0xE2) return make_nop(Cycle);                   // NOP #imm (illegal)
        else if constexpr (op == 0xF2) return make_nop(Cycle);                   // HLT/JAM (illegal)
        
        // Column 3: Illegal opcodes (mostly SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
        else if constexpr (op == 0x03) return make_slo_indx(Cycle);              // SLO (zp,X) (illegal)
        else if constexpr (op == 0x13) return make_slo_indy(Cycle);              // SLO (zp),Y (illegal)
        else if constexpr (op == 0x23) return make_rla_indx(Cycle);              // RLA (zp,X) (illegal)
        else if constexpr (op == 0x33) return make_rla_indy(Cycle);              // RLA (zp),Y (illegal)
        else if constexpr (op == 0x43) return make_sre_indx(Cycle);              // SRE (zp,X) (illegal)
        else if constexpr (op == 0x53) return make_sre_indy(Cycle);              // SRE (zp),Y (illegal)
        else if constexpr (op == 0x63) return make_rra_indx(Cycle);              // RRA (zp,X) (illegal)
        else if constexpr (op == 0x73) return make_rra_indy(Cycle);              // RRA (zp),Y (illegal)
        else if constexpr (op == 0x83) return make_sax_indx(Cycle);              // SAX (zp,X) (illegal)
        else if constexpr (op == 0x93) return make_nop(Cycle);                   // AHX (zp),Y (illegal, unstable)
        else if constexpr (op == 0xA3) return make_lax_indx(Cycle);              // LAX (zp,X) (illegal)
        else if constexpr (op == 0xB3) return make_lax_indy(Cycle);              // LAX (zp),Y (illegal)
        else if constexpr (op == 0xC3) return make_dcp_indx(Cycle);              // DCP (zp,X) (illegal)
        else if constexpr (op == 0xD3) return make_dcp_indy(Cycle);              // DCP (zp),Y (illegal)
        else if constexpr (op == 0xE3) return make_isc_indx(Cycle);              // ISC (zp,X) (illegal)
        else if constexpr (op == 0xF3) return make_isc_indy(Cycle);              // ISC (zp),Y (illegal)
        
        // Column 4: Bit test/set/clear and NOP variants
        else if constexpr (op == 0x04) return make_tsb_zp(Cycle);                // TSB zp (65C02) / NOP zp (illegal on NMOS)
        else if constexpr (op == 0x14) return make_trb_zp(Cycle);                // TRB zp (65C02) / NOP zp,X (illegal on NMOS)
        else if constexpr (op == 0x24) return make_bit_zp(Cycle);                // BIT zp
        else if constexpr (op == 0x34) return make_bit_zpx(Cycle);               // BIT zp,X (65C02) / NOP zp,X (illegal on NMOS)
        else if constexpr (op == 0x44) return make_nop_zp(Cycle);                // NOP zp (illegal)
        else if constexpr (op == 0x54) return make_nop_zpx(Cycle);               // NOP zp,X (illegal)
        else if constexpr (op == 0x64) return make_stz_zp(Cycle);                // STZ zp (65C02) / NOP zp (illegal on NMOS)
        else if constexpr (op == 0x74) return make_stz_zpx(Cycle);               // STZ zp,X (65C02) / NOP zp,X (illegal on NMOS)
        else if constexpr (op == 0x84) return make_sty_zp(Cycle);                // STY zp
        else if constexpr (op == 0x94) return make_sty_zpx(Cycle);               // STY zp,X
        else if constexpr (op == 0xA4) return make_ldy_zp(Cycle);                // LDY zp
        else if constexpr (op == 0xB4) return make_ldy_zpx(Cycle);               // LDY zp,X
        else if constexpr (op == 0xC4) return make_cpy_zp(Cycle);                // CPY zp
        else if constexpr (op == 0xD4) return make_nop_zpx(Cycle);               // NOP zp,X (illegal)
        else if constexpr (op == 0xE4) return make_cpx_zp(Cycle);                // CPX zp
        else if constexpr (op == 0xF4) return make_nop_zpx(Cycle);               // NOP zp,X (illegal)
        
        // Column 5: Zero Page
        else if constexpr (op == 0x05) return make_ora_zp(Cycle);                // ORA zp
        else if constexpr (op == 0x15) return make_ora_zpx(Cycle);               // ORA zp,X
        else if constexpr (op == 0x25) return make_and_zp(Cycle);                // AND zp
        else if constexpr (op == 0x35) return make_and_zpx(Cycle);               // AND zp,X
        else if constexpr (op == 0x45) return make_eor_zp(Cycle);                // EOR zp
        else if constexpr (op == 0x55) return make_eor_zpx(Cycle);               // EOR zp,X
        else if constexpr (op == 0x65) return make_adc_zp(Cycle);                // ADC zp
        else if constexpr (op == 0x75) return make_adc_zpx(Cycle);               // ADC zp,X
        else if constexpr (op == 0x85) return make_sta_zp(Cycle);                // STA zp
        else if constexpr (op == 0x95) return make_sta_zpx(Cycle);               // STA zp,X
        else if constexpr (op == 0xA5) return make_lda_zp(Cycle);                // LDA zp
        else if constexpr (op == 0xB5) return make_lda_zpx(Cycle);               // LDA zp,X
        else if constexpr (op == 0xC5) return make_cmp_zp(Cycle);                // CMP zp
        else if constexpr (op == 0xD5) return make_cmp_zpx(Cycle);               // CMP zp,X
        else if constexpr (op == 0xE5) return make_sbc_zp(Cycle);                // SBC zp
        else if constexpr (op == 0xF5) return make_sbc_zpx(Cycle);               // SBC zp,X
        
        // Column 6: Arithmetic Shift & Rotate
        else if constexpr (op == 0x06) return make_asl_zp(Cycle);                // ASL zp
        else if constexpr (op == 0x16) return make_asl_zpx(Cycle);               // ASL zp,X
        else if constexpr (op == 0x26) return make_rol_zp(Cycle);                // ROL zp
        else if constexpr (op == 0x36) return make_rol_zpx(Cycle);               // ROL zp,X
        else if constexpr (op == 0x46) return make_lsr_zp(Cycle);                // LSR zp
        else if constexpr (op == 0x56) return make_lsr_zpx(Cycle);               // LSR zp,X
        else if constexpr (op == 0x66) return make_ror_zp(Cycle);                // ROR zp
        else if constexpr (op == 0x76) return make_ror_zpx(Cycle);               // ROR zp,X
        else if constexpr (op == 0x86) return make_stx_zp(Cycle);                // STX zp
        else if constexpr (op == 0x96) return make_stx_zpy(Cycle);               // STX zp,Y
        else if constexpr (op == 0xA6) return make_ldx_zp(Cycle);                // LDX zp
        else if constexpr (op == 0xB6) return make_ldx_zpy(Cycle);               // LDX zp,Y
        else if constexpr (op == 0xC6) return make_dec_zp(Cycle);                // DEC zp
        else if constexpr (op == 0xD6) return make_dec_zpx(Cycle);               // DEC zp,X
        else if constexpr (op == 0xE6) return make_inc_zp(Cycle);                // INC zp
        else if constexpr (op == 0xF6) return make_inc_zpx(Cycle);               // INC zp,X
        
        // Column 7: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
        else if constexpr (op == 0x07) return make_slo_zp(Cycle);                // SLO zp (illegal)
        else if constexpr (op == 0x17) return make_slo_zpx(Cycle);               // SLO zp,X (illegal)
        else if constexpr (op == 0x27) return make_rla_zp(Cycle);                // RLA zp (illegal)
        else if constexpr (op == 0x37) return make_rla_zpx(Cycle);               // RLA zp,X (illegal)
        else if constexpr (op == 0x47) return make_sre_zp(Cycle);                // SRE zp (illegal)
        else if constexpr (op == 0x57) return make_sre_zpx(Cycle);               // SRE zp,X (illegal)
        else if constexpr (op == 0x67) return make_rra_zp(Cycle);                // RRA zp (illegal)
        else if constexpr (op == 0x77) return make_rra_zpx(Cycle);               // RRA zp,X (illegal)
        else if constexpr (op == 0x87) return make_sax_zp(Cycle);                // SAX zp (illegal)
        else if constexpr (op == 0x97) return make_sax_zpy(Cycle);               // SAX zp,Y (illegal)
        else if constexpr (op == 0xA7) return make_lax_zp(Cycle);                // LAX zp (illegal)
        else if constexpr (op == 0xB7) return make_lax_zpy(Cycle);               // LAX zp,Y (illegal)
        else if constexpr (op == 0xC7) return make_dcp_zp(Cycle);                // DCP zp (illegal)
        else if constexpr (op == 0xD7) return make_dcp_zpx(Cycle);               // DCP zp,X (illegal)
        else if constexpr (op == 0xE7) return make_isc_zp(Cycle);                // ISC zp (illegal)
        else if constexpr (op == 0xF7) return make_isc_zpx(Cycle);               // ISC zp,X (illegal)
        
        // Column 8: Stack/Status Operations
        else if constexpr (op == 0x08) return make_php(Cycle);                   // PHP impl
        else if constexpr (op == 0x18) return make_clc(Cycle);                   // CLC impl
        else if constexpr (op == 0x28) return make_plp(Cycle);                   // PLP impl
        else if constexpr (op == 0x38) return make_sec(Cycle);                   // SEC impl
        else if constexpr (op == 0x48) return make_pha(Cycle);                   // PHA impl
        else if constexpr (op == 0x58) return make_cli(Cycle);                   // CLI impl
        else if constexpr (op == 0x68) return make_pla(Cycle);                   // PLA impl
        else if constexpr (op == 0x78) return make_sei(Cycle);                   // SEI impl
        else if constexpr (op == 0x88) return make_dey(Cycle);                   // DEY impl
        else if constexpr (op == 0x98) return make_tya(Cycle);                   // TYA impl
        else if constexpr (op == 0xA8) return make_tay(Cycle);                   // TAY impl
        else if constexpr (op == 0xB8) return make_clv(Cycle);                   // CLV impl
        else if constexpr (op == 0xC8) return make_iny(Cycle);                   // INY impl
        else if constexpr (op == 0xD8) return make_cld(Cycle);                   // CLD impl
        else if constexpr (op == 0xE8) return make_inx(Cycle);                   // INX impl
        else if constexpr (op == 0xF8) return make_sed(Cycle);                   // SED impl
        
        // Column 9: Immediate
        else if constexpr (op == 0x09) return make_ora_imm(Cycle);               // ORA #imm
        else if constexpr (op == 0x19) return make_ora_absy(Cycle);              // ORA abs,Y
        else if constexpr (op == 0x29) return make_and_imm(Cycle);               // AND #imm
        else if constexpr (op == 0x39) return make_and_absy(Cycle);              // AND abs,Y
        else if constexpr (op == 0x49) return make_eor_imm(Cycle);               // EOR #imm
        else if constexpr (op == 0x59) return make_eor_absy(Cycle);              // EOR abs,Y
        else if constexpr (op == 0x69) return make_adc_imm(Cycle);               // ADC #imm
        else if constexpr (op == 0x79) return make_adc_absy(Cycle);              // ADC abs,Y
        else if constexpr (op == 0x89) return make_nop(Cycle);                   // NOP #imm (illegal)
        else if constexpr (op == 0x99) return make_sta_absy(Cycle);              // STA abs,Y
        else if constexpr (op == 0xA9) return make_lda_imm(Cycle);               // LDA #imm
        else if constexpr (op == 0xB9) return make_lda_absy(Cycle);              // LDA abs,Y
        else if constexpr (op == 0xC9) return make_cmp_imm(Cycle);               // CMP #imm
        else if constexpr (op == 0xD9) return make_cmp_absy(Cycle);              // CMP abs,Y
        else if constexpr (op == 0xE9) return make_sbc_imm(Cycle);               // SBC #imm
        else if constexpr (op == 0xF9) return make_sbc_absy(Cycle);              // SBC abs,Y
        
        // Column A: Accumulator & Implied
        else if constexpr (op == 0x0A) return make_asl_acc(Cycle);               // ASL A
        else if constexpr (op == 0x1A) return make_nop(Cycle);                   // NOP impl (illegal) / INC A (65C02)
        else if constexpr (op == 0x2A) return make_rol_acc(Cycle);               // ROL A
        else if constexpr (op == 0x3A) return make_nop(Cycle);                   // NOP impl (illegal) / DEC A (65C02)
        else if constexpr (op == 0x4A) return make_lsr_acc(Cycle);               // LSR A
        else if constexpr (op == 0x5A) return make_phy(Cycle);                   // PHY impl (65C02) / NOP (illegal on NMOS)
        else if constexpr (op == 0x6A) return make_ror_acc(Cycle);               // ROR A
        else if constexpr (op == 0x7A) return make_ply(Cycle);                   // PLY impl (65C02) / NOP (illegal on NMOS)
        else if constexpr (op == 0x8A) return make_txa(Cycle);                   // TXA impl
        else if constexpr (op == 0x9A) return make_txs(Cycle);                   // TXS impl
        else if constexpr (op == 0xAA) return make_tax(Cycle);                   // TAX impl
        else if constexpr (op == 0xBA) return make_tsx(Cycle);                   // TSX impl
        else if constexpr (op == 0xCA) return make_dex(Cycle);                   // DEX impl
        else if constexpr (op == 0xDA) return make_phx(Cycle);                   // PHX impl (65C02) / NOP (illegal on NMOS)
        else if constexpr (op == 0xEA) return make_nop(Cycle);                   // NOP impl
        else if constexpr (op == 0xFA) return make_plx(Cycle);                   // PLX impl (65C02) / NOP (illegal on NMOS)
        
        // Column B: Illegal opcodes (mostly unstable)
        else if constexpr (op == 0x0B) return make_nop(Cycle);                   // ANC #imm (illegal, unstable)
        else if constexpr (op == 0x1B) return make_nop(Cycle);                   // SLO abs,Y (illegal)
        else if constexpr (op == 0x2B) return make_nop(Cycle);                   // ANC #imm (illegal, unstable)
        else if constexpr (op == 0x3B) return make_nop(Cycle);                   // RLA abs,Y (illegal)
        else if constexpr (op == 0x4B) return make_nop(Cycle);                   // ALR #imm (illegal, unstable)
        else if constexpr (op == 0x5B) return make_nop(Cycle);                   // SRE abs,Y (illegal)
        else if constexpr (op == 0x6B) return make_nop(Cycle);                   // ARR #imm (illegal, unstable)
        else if constexpr (op == 0x7B) return make_nop(Cycle);                   // RRA abs,Y (illegal)
        else if constexpr (op == 0x8B) return make_nop(Cycle);                   // XAA #imm (illegal, highly unstable)
        else if constexpr (op == 0x9B) return make_nop(Cycle);                   // TAS abs,Y (illegal, unstable)
        else if constexpr (op == 0xAB) return make_nop(Cycle);                   // LAX #imm (illegal, unstable)
        else if constexpr (op == 0xBB) return make_nop(Cycle);                   // LAS abs,Y (illegal, unstable)
        else if constexpr (op == 0xCB) return make_nop(Cycle);                   // AXS #imm (illegal, unstable)
        else if constexpr (op == 0xDB) return make_nop(Cycle);                   // DCP abs,Y (illegal)
        else if constexpr (op == 0xEB) return make_nop(Cycle);                   // SBC #imm (illegal, same as legal E9)
        else if constexpr (op == 0xFB) return make_nop(Cycle);                   // ISC abs,Y (illegal)
        
        // Column C: Absolute addressing & Jump
        else if constexpr (op == 0x0C) return make_tsb_abs(Cycle);               // TSB abs (65C02) / NOP abs (illegal on NMOS)
        else if constexpr (op == 0x1C) return make_trb_abs(Cycle);               // TRB abs (65C02) / NOP abs,X (illegal on NMOS)
        else if constexpr (op == 0x2C) return make_bit_abs(Cycle);               // BIT abs
        else if constexpr (op == 0x3C) return make_bit_absx(Cycle);              // BIT abs,X (65C02) / NOP abs,X (illegal on NMOS)
        else if constexpr (op == 0x4C) return make_jmp_abs(Cycle);               // JMP abs
        else if constexpr (op == 0x5C) return make_nop_absx(Cycle);              // NOP abs,X (illegal)
        else if constexpr (op == 0x6C) return make_jmp_ind(Cycle);               // JMP (abs)
        else if constexpr (op == 0x7C) return make_jmp_absx_ind(Cycle);          // JMP (abs,X) (65C02) / NOP abs,X (illegal on NMOS)
        else if constexpr (op == 0x8C) return make_sty_abs(Cycle);               // STY abs
        else if constexpr (op == 0x9C) return make_stz_abs(Cycle);               // STZ abs (65C02) / SHY abs,X (illegal on NMOS)
        else if constexpr (op == 0xAC) return make_ldy_abs(Cycle);               // LDY abs
        else if constexpr (op == 0xBC) return make_ldy_absx(Cycle);              // LDY abs,X
        else if constexpr (op == 0xCC) return make_cpy_abs(Cycle);               // CPY abs
        else if constexpr (op == 0xDC) return make_nop_absx(Cycle);              // NOP abs,X (illegal)
        else if constexpr (op == 0xEC) return make_cpx_abs(Cycle);               // CPX abs
        else if constexpr (op == 0xFC) return make_nop_absx(Cycle);              // NOP abs,X (illegal)
        
        // Column D: Absolute
        else if constexpr (op == 0x0D) return make_ora_abs(Cycle);               // ORA abs
        else if constexpr (op == 0x1D) return make_ora_absx(Cycle);              // ORA abs,X
        else if constexpr (op == 0x2D) return make_and_abs(Cycle);               // AND abs
        else if constexpr (op == 0x3D) return make_and_absx(Cycle);              // AND abs,X
        else if constexpr (op == 0x4D) return make_eor_abs(Cycle);               // EOR abs
        else if constexpr (op == 0x5D) return make_eor_absx(Cycle);              // EOR abs,X
        else if constexpr (op == 0x6D) return make_adc_abs(Cycle);               // ADC abs
        else if constexpr (op == 0x7D) return make_adc_absx(Cycle);              // ADC abs,X
        else if constexpr (op == 0x8D) return make_sta_abs(Cycle);               // STA abs
        else if constexpr (op == 0x9D) return make_sta_absx(Cycle);              // STA abs,X
        else if constexpr (op == 0xAD) return make_lda_abs(Cycle);               // LDA abs
        else if constexpr (op == 0xBD) return make_lda_absx(Cycle);              // LDA abs,X
        else if constexpr (op == 0xCD) return make_cmp_abs(Cycle);               // CMP abs
        else if constexpr (op == 0xDD) return make_cmp_absx(Cycle);              // CMP abs,X
        else if constexpr (op == 0xED) return make_sbc_abs(Cycle);               // SBC abs
        else if constexpr (op == 0xFD) return make_sbc_absx(Cycle);              // SBC abs,X
        
        // Column E: Absolute with shifts
        else if constexpr (op == 0x0E) return make_asl_abs(Cycle);               // ASL abs
        else if constexpr (op == 0x1E) return make_asl_absx(Cycle);              // ASL abs,X
        else if constexpr (op == 0x2E) return make_rol_abs(Cycle);               // ROL abs
        else if constexpr (op == 0x3E) return make_rol_absx(Cycle);              // ROL abs,X
        else if constexpr (op == 0x4E) return make_lsr_abs(Cycle);               // LSR abs
        else if constexpr (op == 0x5E) return make_lsr_absx(Cycle);              // LSR abs,X
        else if constexpr (op == 0x6E) return make_ror_abs(Cycle);               // ROR abs
        else if constexpr (op == 0x7E) return make_ror_absx(Cycle);              // ROR abs,X
        else if constexpr (op == 0x8E) return make_stx_abs(Cycle);               // STX abs
        else if constexpr (op == 0x9E) return make_nop(Cycle);                   // SHX abs,Y (illegal, unstable)
        else if constexpr (op == 0xAE) return make_ldx_abs(Cycle);               // LDX abs
        else if constexpr (op == 0xBE) return make_ldx_absy(Cycle);              // LDX abs,Y
        else if constexpr (op == 0xCE) return make_dec_abs(Cycle);               // DEC abs
        else if constexpr (op == 0xDE) return make_dec_absx(Cycle);              // DEC abs,X
        else if constexpr (op == 0xEE) return make_inc_abs(Cycle);               // INC abs
        else if constexpr (op == 0xFE) return make_inc_absx(Cycle);              // INC abs,X
        
        // Column F: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
        else if constexpr (op == 0x0F) return make_slo_abs(Cycle);               // SLO abs (illegal)
        else if constexpr (op == 0x1F) return make_slo_absx(Cycle);              // SLO abs,X (illegal)
        else if constexpr (op == 0x2F) return make_rla_abs(Cycle);               // RLA abs (illegal)
        else if constexpr (op == 0x3F) return make_rla_absx(Cycle);              // RLA abs,X (illegal)
        else if constexpr (op == 0x4F) return make_sre_abs(Cycle);               // SRE abs (illegal)
        else if constexpr (op == 0x5F) return make_sre_absx(Cycle);              // SRE abs,X (illegal)
        else if constexpr (op == 0x6F) return make_rra_abs(Cycle);               // RRA abs (illegal)
        else if constexpr (op == 0x7F) return make_rra_absx(Cycle);              // RRA abs,X (illegal)
        else if constexpr (op == 0x8F) return make_sax_abs(Cycle);               // SAX abs (illegal)
        else if constexpr (op == 0x9F) return make_nop(Cycle);                   // AHX abs,Y (illegal, unstable)
        else if constexpr (op == 0xAF) return make_lax_abs(Cycle);               // LAX abs (illegal)
        else if constexpr (op == 0xBF) return make_lax_absy(Cycle);              // LAX abs,Y (illegal)
        else if constexpr (op == 0xCF) return make_dcp_abs(Cycle);               // DCP abs (illegal)
        else if constexpr (op == 0xDF) return make_dcp_absx(Cycle);              // DCP abs,X (illegal)
        else if constexpr (op == 0xEF) return make_isc_abs(Cycle);               // ISC abs (illegal)
        else if constexpr (op == 0xFF) return make_isc_absx(Cycle);              // ISC abs,X (illegal)
        
        // For all other opcodes, return empty cycle
        else return make_empty_cycle();  // Fallback for any missing opcodes
    }
    
    // Runtime get_cycle function that uses template dispatch
    static constexpr cycle_desc_t get_cycle(uint16_t opcode, uint8_t cycle) {
        // This would need a large constexpr dispatch table in practice
        // For now, we'll use the traditional switch approach but access from the table
        return get_cycle_from_table(opcode, cycle);
    }
    
    // Generate complete 1D cycle table at compile time
    template<size_t... Indices>
    static constexpr std::array<cycle_desc_t, CYCLE_TABLE_SIZE> generate_cycle_table_impl(std::index_sequence<Indices...>) {
        return {get_cycle<Indices / MAX_CYCLES, (Indices % MAX_CYCLES) + 1>()...};
    }
    
    static constexpr std::array<cycle_desc_t, CYCLE_TABLE_SIZE> generate_cycle_table() {
        return generate_cycle_table_impl(std::make_index_sequence<CYCLE_TABLE_SIZE>{});
    }
    
    // Single pre-generated compile-time 1D cycle table for all 259 opcodes
    static constexpr auto cycle_table = generate_cycle_table();
    
    // Table lookup function
    static constexpr cycle_desc_t get_cycle_from_table(uint16_t opcode, uint8_t cycle) {
        if (opcode >= TOTAL_OPCODES || cycle < 1 || cycle > MAX_CYCLES) {
            return make_empty_cycle();
        }
        return cycle_table[get_cycle_index(opcode, cycle)];
    }
    
    // === COMPILE-TIME VALIDATION SYSTEM ===
    
    // Template function to validate SYNC placement for a specific opcode
    template<uint16_t OpCode>
    static constexpr void validate_sync_placement() {
        bool found_sync = false;
        uint8_t sync_cycle = 0;
        
        // Check all cycles for this opcode
        for (uint8_t cycle = 1; cycle <= MAX_CYCLES; ++cycle) {
            auto desc = get_cycle<OpCode, cycle>();
            
            // If this is an empty cycle, we've reached the end
            if (desc.get_mem_op() == MemOp::NOP &&
                desc.get_data_op() == DataOp::NOP &&
                desc.get_alu_op() == AluOp::NOP &&
                !desc.is_sync()) {
                break;
            }
            
            if (desc.is_sync()) {
                if (found_sync) {
                    // Multiple SYNC flags found - trigger template error with specific opcode/cycle
                    VALIDATE_MULTIPLE_SYNC_TEMPLATE(OpCode, cycle);
                }
                found_sync = true;
                sync_cycle = cycle;
            }
        }
        
        // Every instruction must have exactly one SYNC flag
        if (!found_sync) {
            VALIDATE_MISSING_SYNC_TEMPLATE(OpCode);
        }
        
        // Validate that all cycles after SYNC are empty
        if (found_sync && sync_cycle < MAX_CYCLES) {
            for (uint8_t cycle = sync_cycle + 1; cycle <= MAX_CYCLES; ++cycle) {
                auto desc = get_cycle<OpCode, cycle>();
                if (desc.get_mem_op() != MemOp::NOP ||
                    desc.get_data_op() != DataOp::NOP ||
                    desc.get_alu_op() != AluOp::NOP ||
                    desc.is_sync()) {
                    // Non-empty cycle found after SYNC
                    VALIDATE_MULTIPLE_SYNC_TEMPLATE(OpCode, cycle);
                }
            }
        }
    }
    
    // Force compile-time validation using comma operator to avoid void assignment errors
    static constexpr bool validation_complete = (
        validate_sync_placement<0x00>(),      // BRK
        validate_sync_placement<0xEA>(),      // NOP
        validate_sync_placement<0xA9>(),      // LDA #imm
        validate_sync_placement<0x4C>(),      // JMP abs
        validate_sync_placement<VIRTUAL_OPCODE_RESET>(),  // RESET
        validate_sync_placement<VIRTUAL_OPCODE_NMI>(),    // NMI
        validate_sync_placement<VIRTUAL_OPCODE_IRQ>(),    // IRQ
        true  // Final value for the expression
    );

};

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_HPP