#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

#include "cpu_defs.hpp"
#include <array>

namespace fam65xx_cpp {

// Virtual opcode constants for interrupts (placed after regular 256 opcodes)
static constexpr uint16_t VIRTUAL_OPCODE_RESET = 256;
static constexpr uint16_t VIRTUAL_OPCODE_NMI   = 257;
static constexpr uint16_t VIRTUAL_OPCODE_IRQ   = 258;

// Cycle validation - constexpr functions that can trigger compilation failures
namespace cycle_validation {
    // Validation state tracking structure
    struct validation_result {
        bool valid;
        const char* error_message;
    };
    
    // Function to validate cycle table generation
    constexpr validation_result validate_cycle_table() {
        // We'll implement the validation logic here during table generation
        // For now, assume valid - the actual validation happens in the table generation loop
        return {true, nullptr};
    }
}

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
    
    // Single-cycle immediate operations (1 cycle total)
    static constexpr cycle_desc_t make_immediate(DataOp data_op, AluOp alu_op = AluOp::NOP) {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, data_op, alu_op);
    }
    
    // Zero page addressing helpers (2 cycles total)
    static constexpr cycle_desc_t make_zeropage(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
    }
    
    // Zero page write operations (2 cycles total)
    static constexpr cycle_desc_t make_zeropage_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
    }
    
    // Absolute addressing helpers (3 cycles total)
    static constexpr cycle_desc_t make_absolute(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op);
    }
    
    // Absolute write operations (3 cycles total)
    static constexpr cycle_desc_t make_absolute_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
    }
    
    // Zero page indexed (zp,X or zp,Y) operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        return CD_MAKE_SYNC(MemOp::READ_ZP, final_op, alu_op);
    }
    
    // Zero page indexed write operations (3 cycles total)
    static constexpr cycle_desc_t make_zeropage_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y
        return CD_MAKE_SYNC(MemOp::WRITE_ZP, store_op, AluOp::NOP);
    }
    
    // Absolute indexed (abs,X or abs,Y) operations (4+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_absolute_indexed(uint8_t cycle, DataOp index_op, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // ADDR_ADD_X or ADDR_ADD_Y, may cross page
        return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 4, always executes
    }
    
    // Absolute indexed write operations (always 4 cycles - writes always do extra cycle)
    static constexpr cycle_desc_t make_absolute_indexed_write(uint8_t cycle, DataOp index_op, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, index_op, AluOp::NOP); // Read dummy byte first (6502 quirk)
        return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
    }
    
    // Indirect indexed (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X to zero page pointer
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read target address low
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read target address high
        return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // Read from target
    }
    
    // Indirect indexed write (zp,X) operations (5 cycles total)
    static constexpr cycle_desc_t make_indexed_indirect_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
    }
    
    // Indirect indexed (zp),Y operations (5+ cycles, page crossing adds 1)
    static constexpr cycle_desc_t make_indirect_indexed(uint8_t cycle, DataOp final_op, AluOp alu_op = AluOp::NOP) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read base address low
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read base address high
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y, may cross page
        return CD_MAKE_SYNC(MemOp::READ_ABS, final_op, alu_op); // cycle 5, always executes
    }
    
    // Indirect indexed write (zp),Y operations (always 5 cycles)
    static constexpr cycle_desc_t make_indirect_indexed_write(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Read dummy first
        return CD_MAKE_SYNC(MemOp::WRITE_ABS, store_op, AluOp::NOP);
    }
    
    // Memory modify operations (5 cycles: read address, read data, write old, write new)
    static constexpr cycle_desc_t make_memory_modify_zp(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, modify_op); // Write modified value
        return CD_MAKE(MemOp::NOP, DataOp::NOP, AluOp::NOP); // Should never reach here
    }
    
    // Memory modify operations absolute (6 cycles)
    static constexpr cycle_desc_t make_memory_modify_abs(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::TEMP_MODIFY, modify_op); // Write modified value
        return CD_MAKE(MemOp::NOP, DataOp::NOP, AluOp::NOP); // Should never reach here
    }
    
    // Stack operations
    static constexpr cycle_desc_t make_stack_push(uint8_t cycle, DataOp store_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, store_op, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_stack_pull(uint8_t cycle, DataOp load_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_SP, load_op, AluOp::NOP);
    }
    
    // Branch operations (2+ cycles base, +1 if taken, +1 more if page crossed)
    static constexpr cycle_desc_t make_branch(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Cycle 2+, execution decides how many
    }
    
    // Jump operations
    static constexpr cycle_desc_t make_jump_absolute(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_jump_indirect(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP); // 6502 page boundary bug
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
    }
    
    // Subroutine operations
    static constexpr cycle_desc_t make_jsr(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP); // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_rts(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        if (cycle == 5) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Internal - increment PC
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
    }
    
    // Interrupt sequences
    static constexpr cycle_desc_t make_interrupt_sequence(uint8_t cycle, AluOp flag_op = AluOp::SEI) {
        if (cycle == 3) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCH
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);  // Push PCL
        if (cycle == 5) return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, flag_op);     // Push P, set flag
        if (cycle == 6) return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);    // Read vector low
        return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP); // Read vector high, sync
    }
    
    static constexpr cycle_desc_t make_rti(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal
        if (cycle == 3) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull status
        if (cycle == 4) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
        if (cycle == 5) return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP);
    }
    
    // === INSTRUCTION IMPLEMENTATIONS (using helpers) ===
    
    // LDA immediate - single cycle
    static constexpr cycle_desc_t make_lda_imm() {
        return make_immediate(DataOp::LOAD_A);
    }
    
    // LDA zero page - 2 cycles
    static constexpr cycle_desc_t make_lda_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_A);
    }
    
    // LDA absolute - 3 cycles
    static constexpr cycle_desc_t make_lda_abs(uint8_t cycle) {
        return make_absolute(cycle, DataOp::LOAD_A);
    }
    
    // ADC immediate - single cycle
    static constexpr cycle_desc_t make_adc_imm() {
        return make_immediate(DataOp::ALU, AluOp::ADC);
    }
    
    // NOP - single cycle
    static constexpr cycle_desc_t make_nop() {
        return make_immediate(DataOp::NOP);
    }
    
    // BRK - 7 cycles (using shared interrupt sequence for cycles 3-7)
    static constexpr cycle_desc_t make_brk(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Read next byte (dummy)
        return make_interrupt_sequence(cycle, AluOp::SEI); // Cycles 3-7 identical to interrupts
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
    
    // Transfer operations - single cycle
    static constexpr cycle_desc_t make_txa() { return make_immediate(DataOp::NOP, AluOp::TXA); }
    static constexpr cycle_desc_t make_tax() { return make_immediate(DataOp::NOP, AluOp::TAX); }
    static constexpr cycle_desc_t make_tya() { return make_immediate(DataOp::NOP, AluOp::TYA); }
    static constexpr cycle_desc_t make_tay() { return make_immediate(DataOp::NOP, AluOp::TAY); }
    static constexpr cycle_desc_t make_tsx() { return make_immediate(DataOp::NOP, AluOp::TSX); }
    static constexpr cycle_desc_t make_txs() { return make_immediate(DataOp::NOP, AluOp::TXS); }
    
    // Flag operations - single cycle
    static constexpr cycle_desc_t make_clc() { return make_immediate(DataOp::NOP, AluOp::CLC); }
    static constexpr cycle_desc_t make_sec() { return make_immediate(DataOp::NOP, AluOp::SEC); }
    static constexpr cycle_desc_t make_cli() { return make_immediate(DataOp::NOP, AluOp::CLI); }
    static constexpr cycle_desc_t make_sei() { return make_immediate(DataOp::NOP, AluOp::SEI); }
    static constexpr cycle_desc_t make_clv() { return make_immediate(DataOp::NOP, AluOp::CLV); }
    static constexpr cycle_desc_t make_cld() { return make_immediate(DataOp::NOP, AluOp::CLD); }
    static constexpr cycle_desc_t make_sed() { return make_immediate(DataOp::NOP, AluOp::SED); }
    
    // Logical operations
    static constexpr cycle_desc_t make_and_imm() { return make_immediate(DataOp::ALU, AluOp::AND); }
    static constexpr cycle_desc_t make_ora_imm() { return make_immediate(DataOp::ALU, AluOp::ORA); }
    static constexpr cycle_desc_t make_eor_imm() { return make_immediate(DataOp::ALU, AluOp::EOR); }
    
    // Compare operations
    static constexpr cycle_desc_t make_cmp_imm() { return make_immediate(DataOp::ALU, AluOp::CMP); }
    static constexpr cycle_desc_t make_cpx_imm() { return make_immediate(DataOp::ALU, AluOp::CPX); }
    static constexpr cycle_desc_t make_cpy_imm() { return make_immediate(DataOp::ALU, AluOp::CPY); }
    
    // Load operations
    static constexpr cycle_desc_t make_ldx_imm() { return make_immediate(DataOp::LOAD_X); }
    static constexpr cycle_desc_t make_ldy_imm() { return make_immediate(DataOp::LOAD_Y); }
    
    static constexpr cycle_desc_t make_ldx_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_X);
    }
    
    static constexpr cycle_desc_t make_ldy_zp(uint8_t cycle) {
        return make_zeropage(cycle, DataOp::LOAD_Y);
    }
    
    // Inc/Dec operations
    static constexpr cycle_desc_t make_inx() { return make_immediate(DataOp::NOP, AluOp::INC); }
    static constexpr cycle_desc_t make_iny() { return make_immediate(DataOp::NOP, AluOp::INC); }
    static constexpr cycle_desc_t make_dex() { return make_immediate(DataOp::NOP, AluOp::DEC); }
    static constexpr cycle_desc_t make_dey() { return make_immediate(DataOp::NOP, AluOp::DEC); }
    
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
    
    // Shift/Rotate operations - ASL, LSR, ROL, ROR
    static constexpr cycle_desc_t make_asl_acc() { return make_immediate(DataOp::NOP, AluOp::ASL); }
    static constexpr cycle_desc_t make_lsr_acc() { return make_immediate(DataOp::NOP, AluOp::LSR); }
    static constexpr cycle_desc_t make_rol_acc() { return make_immediate(DataOp::NOP, AluOp::ROL); }
    static constexpr cycle_desc_t make_ror_acc() { return make_immediate(DataOp::NOP, AluOp::ROR); }
    
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
        return CD_MAKE_SYNC(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_phx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_phy(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_Y, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_plx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_ply(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_Y, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_stz_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_ZP, DataOp::STORE_ZERO, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_stz_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::STORE_ZERO, AluOp::NOP);
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

    static constexpr cycle_desc_t make_sbc_imm() { return make_immediate(DataOp::ALU, AluOp::SBC); }
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
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
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
        switch (cycle) {
            case 1: return CD_MAKE(MemOp::DUMMY_READ, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
            case 2: return CD_MAKE(MemOp::DUMMY_READ, DataOp::NOP, AluOp::NOP);           // Dummy read (RESET specific)
            default: return make_interrupt_sequence(cycle, AluOp::SEI); // Cycles 3-7 use shared pattern
        }
    }
    
    // NMI sequence - uses shared interrupt pattern with different initial cycles
    static constexpr cycle_desc_t get_nmi_cycle(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);     // Read next instruction byte
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);         // Read next instruction byte (dummy)
        return make_interrupt_sequence(cycle, AluOp::SEI); // Cycles 3-7 use shared pattern
    }
    
    // IRQ sequence - identical to NMI (different vector handled by DataOp::INTERRUPT_VEC)
    static constexpr cycle_desc_t get_irq_cycle(uint8_t cycle) {
        return get_nmi_cycle(cycle); // Completely identical to NMI
    }

    // Main cycle table lookup (supports virtual opcodes at 256+)
    static constexpr cycle_desc_t get_cycle(uint16_t opcode, uint8_t cycle) {
        constexpr auto cycle_table = []() {
            // Expanded table: 259 opcodes (256 regular + 3 virtual) * 8 cycles each
            std::array<cycle_desc_t, 2072> table{};  // (256+3) * 8 = 2072
            
            // Generate cycle table with proper nested loops and SYNC validation
            // Loop over 256 regular opcodes + 3 virtual opcodes (259 total)
            for (uint16_t op = 0; op < 259; ++op) {
                bool sync_found = false;
                
                // Each opcode has at most 8 cycles (cycles 1-8, cycle 0 is opcode fetch)
                for (uint8_t cyc = 1; cyc <= 8; ++cyc) {
                    const uint16_t table_index = (op * 8) + (cyc - 1);  // Convert to 0-based table index
                    cycle_desc_t cycle_desc;
                    
                    // Handle virtual opcodes (256, 257, 258)
                    if (op >= 256) {
                        switch (op) {
                            case VIRTUAL_OPCODE_RESET: cycle_desc = get_reset_cycle(cyc); break;
                            case VIRTUAL_OPCODE_NMI:   cycle_desc = get_nmi_cycle(cyc); break;
                            case VIRTUAL_OPCODE_IRQ:   cycle_desc = get_irq_cycle(cyc); break;
                            default: cycle_desc = make_nop(); break; // Should not happen with only 3 virtual opcodes
                        }
                    } else {
                        // Handle regular opcodes (0-255) - COMPLETE 6502 INSTRUCTION SET
                        const uint8_t opcode = static_cast<uint8_t>(op);
                        switch (opcode) {
                            // === COLUMN 0: Control Instructions ===
                            case 0x00: cycle_desc = make_brk(cyc); break;                        // BRK impl
                            case 0x10: cycle_desc = make_bpl(cyc); break;                        // BPL rel
                            case 0x20: cycle_desc = make_jsr(cyc); break;                        // JSR abs
                            case 0x30: cycle_desc = make_bmi(cyc); break;                        // BMI rel
                            case 0x40: cycle_desc = make_rti(cyc); break;                        // RTI impl
                            case 0x50: cycle_desc = make_bvc(cyc); break;                        // BVC rel
                            case 0x60: cycle_desc = make_rts(cyc); break;                        // RTS impl
                            case 0x70: cycle_desc = make_bvs(cyc); break;                        // BVS rel
                            case 0x80: cycle_desc = make_bra(cyc); break;                        // BRA rel (65C02) / NOP (NMOS)
                            case 0x90: cycle_desc = make_bcc(cyc); break;                        // BCC rel
                            case 0xA0: cycle_desc = make_ldy_imm(); break;                       // LDY #imm
                            case 0xB0: cycle_desc = make_bcs(cyc); break;                        // BCS rel
                            case 0xC0: cycle_desc = make_cpy_imm(); break;                       // CPY #imm
                            case 0xD0: cycle_desc = make_bne(cyc); break;                        // BNE rel
                            case 0xE0: cycle_desc = make_cpx_imm(); break;                       // CPX #imm
                            case 0xF0: cycle_desc = make_beq(cyc); break;                        // BEQ rel

                            // === COLUMN 1: Indexed Indirect (zp,X) ===
                            case 0x01: cycle_desc = make_ora_indx(cyc); break;                   // ORA (zp,X)
                            case 0x11: cycle_desc = make_ora_indy(cyc); break;                   // ORA (zp),Y
                            case 0x21: cycle_desc = make_and_indx(cyc); break;                   // AND (zp,X)
                            case 0x31: cycle_desc = make_and_indy(cyc); break;                   // AND (zp),Y
                            case 0x41: cycle_desc = make_eor_indx(cyc); break;                   // EOR (zp,X)
                            case 0x51: cycle_desc = make_eor_indy(cyc); break;                   // EOR (zp),Y
                            case 0x61: cycle_desc = make_adc_indx(cyc); break;                   // ADC (zp,X)
                            case 0x71: cycle_desc = make_adc_indy(cyc); break;                   // ADC (zp),Y
                            case 0x81: cycle_desc = make_sta_indx(cyc); break;                   // STA (zp,X)
                            case 0x91: cycle_desc = make_sta_indy(cyc); break;                   // STA (zp),Y
                            case 0xA1: cycle_desc = make_lda_indx(cyc); break;                   // LDA (zp,X)
                            case 0xB1: cycle_desc = make_lda_indy(cyc); break;                   // LDA (zp),Y
                            case 0xC1: cycle_desc = make_cmp_indx(cyc); break;                   // CMP (zp,X)
                            case 0xD1: cycle_desc = make_cmp_indy(cyc); break;                   // CMP (zp),Y
                            case 0xE1: cycle_desc = make_sbc_indx(cyc); break;                   // SBC (zp,X)
                            case 0xF1: cycle_desc = make_sbc_indy(cyc); break;                   // SBC (zp),Y

                            // === COLUMN 2: Illegal/Undocumented/65C02 ===
                            case 0x02: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x12: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x22: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x32: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x42: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x52: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x62: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x72: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0x82: cycle_desc = make_nop(); break;                           // NOP #imm (illegal)
                            case 0x92: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0xA2: cycle_desc = make_ldx_imm(); break;                       // LDX #imm
                            case 0xB2: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0xC2: cycle_desc = make_nop(); break;                           // NOP #imm (illegal)
                            case 0xD2: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)
                            case 0xE2: cycle_desc = make_nop(); break;                           // NOP #imm (illegal)
                            case 0xF2: cycle_desc = make_nop(); break;                           // HLT/JAM (illegal)

                            // === COLUMN 3: Illegal opcodes (mostly SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC) ===
                            case 0x03: cycle_desc = make_slo_indx(cyc); break;                   // SLO (zp,X) (illegal)
                            case 0x13: cycle_desc = make_slo_indy(cyc); break;                   // SLO (zp),Y (illegal)
                            case 0x23: cycle_desc = make_rla_indx(cyc); break;                   // RLA (zp,X) (illegal)
                            case 0x33: cycle_desc = make_rla_indy(cyc); break;                   // RLA (zp),Y (illegal)
                            case 0x43: cycle_desc = make_sre_indx(cyc); break;                   // SRE (zp,X) (illegal)
                            case 0x53: cycle_desc = make_sre_indy(cyc); break;                   // SRE (zp),Y (illegal)
                            case 0x63: cycle_desc = make_rra_indx(cyc); break;                   // RRA (zp,X) (illegal)
                            case 0x73: cycle_desc = make_rra_indy(cyc); break;                   // RRA (zp),Y (illegal)
                            case 0x83: cycle_desc = make_sax_indx(cyc); break;                   // SAX (zp,X) (illegal)
                            case 0x93: cycle_desc = make_nop(); break;                           // AHX (zp),Y (illegal, unstable)
                            case 0xA3: cycle_desc = make_lax_indx(cyc); break;                   // LAX (zp,X) (illegal)
                            case 0xB3: cycle_desc = make_lax_indy(cyc); break;                   // LAX (zp),Y (illegal)
                            case 0xC3: cycle_desc = make_dcp_indx(cyc); break;                   // DCP (zp,X) (illegal)
                            case 0xD3: cycle_desc = make_dcp_indy(cyc); break;                   // DCP (zp),Y (illegal)
                            case 0xE3: cycle_desc = make_isc_indx(cyc); break;                   // ISC (zp,X) (illegal)
                            case 0xF3: cycle_desc = make_isc_indy(cyc); break;                   // ISC (zp),Y (illegal)

                            // === COLUMN 4: Bit test/set/clear and NOP variants ===
                            case 0x04: cycle_desc = make_tsb_zp(cyc); break;                     // TSB zp (65C02) / NOP zp (illegal on NMOS)
                            case 0x14: cycle_desc = make_trb_zp(cyc); break;                     // TRB zp (65C02) / NOP zp,X (illegal on NMOS)
                            case 0x24: cycle_desc = make_bit_zp(cyc); break;                     // BIT zp
                            case 0x34: cycle_desc = make_bit_zpx(cyc); break;                    // BIT zp,X (65C02) / NOP zp,X (illegal on NMOS)
                            case 0x44: cycle_desc = make_nop_zp(cyc); break;                     // NOP zp (illegal)
                            case 0x54: cycle_desc = make_nop_zpx(cyc); break;                    // NOP zp,X (illegal)
                            case 0x64: cycle_desc = make_stz_zp(cyc); break;                     // STZ zp (65C02) / NOP zp (illegal on NMOS)
                            case 0x74: cycle_desc = make_stz_zpx(cyc); break;                    // STZ zp,X (65C02) / NOP zp,X (illegal on NMOS)
                            case 0x84: cycle_desc = make_sty_zp(cyc); break;                     // STY zp
                            case 0x94: cycle_desc = make_sty_zpx(cyc); break;                    // STY zp,X
                            case 0xA4: cycle_desc = make_ldy_zp(cyc); break;                     // LDY zp
                            case 0xB4: cycle_desc = make_ldy_zpx(cyc); break;                    // LDY zp,X
                            case 0xC4: cycle_desc = make_cpy_zp(cyc); break;                     // CPY zp
                            case 0xD4: cycle_desc = make_nop_zpx(cyc); break;                    // NOP zp,X (illegal)
                            case 0xE4: cycle_desc = make_cpx_zp(cyc); break;                     // CPX zp
                            case 0xF4: cycle_desc = make_nop_zpx(cyc); break;                    // NOP zp,X (illegal)

                            // === COLUMN 5: Zero Page ===
                            case 0x05: cycle_desc = make_ora_zp(cyc); break;                     // ORA zp
                            case 0x15: cycle_desc = make_ora_zpx(cyc); break;                    // ORA zp,X
                            case 0x25: cycle_desc = make_and_zp(cyc); break;                     // AND zp
                            case 0x35: cycle_desc = make_and_zpx(cyc); break;                    // AND zp,X
                            case 0x45: cycle_desc = make_eor_zp(cyc); break;                     // EOR zp
                            case 0x55: cycle_desc = make_eor_zpx(cyc); break;                    // EOR zp,X
                            case 0x65: cycle_desc = make_adc_zp(cyc); break;                     // ADC zp
                            case 0x75: cycle_desc = make_adc_zpx(cyc); break;                    // ADC zp,X
                            case 0x85: cycle_desc = make_sta_zp(cyc); break;                     // STA zp
                            case 0x95: cycle_desc = make_sta_zpx(cyc); break;                    // STA zp,X
                            case 0xA5: cycle_desc = make_lda_zp(cyc); break;                     // LDA zp
                            case 0xB5: cycle_desc = make_lda_zpx(cyc); break;                    // LDA zp,X
                            case 0xC5: cycle_desc = make_cmp_zp(cyc); break;                     // CMP zp
                            case 0xD5: cycle_desc = make_cmp_zpx(cyc); break;                    // CMP zp,X
                            case 0xE5: cycle_desc = make_sbc_zp(cyc); break;                     // SBC zp
                            case 0xF5: cycle_desc = make_sbc_zpx(cyc); break;                    // SBC zp,X

                            // === COLUMN 6: Arithmetic Shift & Rotate ===
                            case 0x06: cycle_desc = make_asl_zp(cyc); break;                     // ASL zp
                            case 0x16: cycle_desc = make_asl_zpx(cyc); break;                    // ASL zp,X
                            case 0x26: cycle_desc = make_rol_zp(cyc); break;                     // ROL zp
                            case 0x36: cycle_desc = make_rol_zpx(cyc); break;                    // ROL zp,X
                            case 0x46: cycle_desc = make_lsr_zp(cyc); break;                     // LSR zp
                            case 0x56: cycle_desc = make_lsr_zpx(cyc); break;                    // LSR zp,X
                            case 0x66: cycle_desc = make_ror_zp(cyc); break;                     // ROR zp
                            case 0x76: cycle_desc = make_ror_zpx(cyc); break;                    // ROR zp,X
                            case 0x86: cycle_desc = make_stx_zp(cyc); break;                     // STX zp
                            case 0x96: cycle_desc = make_stx_zpy(cyc); break;                    // STX zp,Y
                            case 0xA6: cycle_desc = make_ldx_zp(cyc); break;                     // LDX zp
                            case 0xB6: cycle_desc = make_ldx_zpy(cyc); break;                    // LDX zp,Y
                            case 0xC6: cycle_desc = make_dec_zp(cyc); break;                     // DEC zp
                            case 0xD6: cycle_desc = make_dec_zpx(cyc); break;                    // DEC zp,X
                            case 0xE6: cycle_desc = make_inc_zp(cyc); break;                     // INC zp
                            case 0xF6: cycle_desc = make_inc_zpx(cyc); break;                    // INC zp,X

                            // === COLUMN 7: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC) ===
                            case 0x07: cycle_desc = make_slo_zp(cyc); break;                     // SLO zp (illegal)
                            case 0x17: cycle_desc = make_slo_zpx(cyc); break;                    // SLO zp,X (illegal)
                            case 0x27: cycle_desc = make_rla_zp(cyc); break;                     // RLA zp (illegal)
                            case 0x37: cycle_desc = make_rla_zpx(cyc); break;                    // RLA zp,X (illegal)
                            case 0x47: cycle_desc = make_sre_zp(cyc); break;                     // SRE zp (illegal)
                            case 0x57: cycle_desc = make_sre_zpx(cyc); break;                    // SRE zp,X (illegal)
                            case 0x67: cycle_desc = make_rra_zp(cyc); break;                     // RRA zp (illegal)
                            case 0x77: cycle_desc = make_rra_zpx(cyc); break;                    // RRA zp,X (illegal)
                            case 0x87: cycle_desc = make_sax_zp(cyc); break;                     // SAX zp (illegal)
                            case 0x97: cycle_desc = make_sax_zpy(cyc); break;                    // SAX zp,Y (illegal)
                            case 0xA7: cycle_desc = make_lax_zp(cyc); break;                     // LAX zp (illegal)
                            case 0xB7: cycle_desc = make_lax_zpy(cyc); break;                    // LAX zp,Y (illegal)
                            case 0xC7: cycle_desc = make_dcp_zp(cyc); break;                     // DCP zp (illegal)
                            case 0xD7: cycle_desc = make_dcp_zpx(cyc); break;                    // DCP zp,X (illegal)
                            case 0xE7: cycle_desc = make_isc_zp(cyc); break;                     // ISC zp (illegal)
                            case 0xF7: cycle_desc = make_isc_zpx(cyc); break;                    // ISC zp,X (illegal)

                            // === COLUMN 8: Stack/Status Operations ===
                            case 0x08: cycle_desc = make_php(cyc); break;                        // PHP impl
                            case 0x18: cycle_desc = make_clc(); break;                           // CLC impl
                            case 0x28: cycle_desc = make_plp(cyc); break;                        // PLP impl
                            case 0x38: cycle_desc = make_sec(); break;                           // SEC impl
                            case 0x48: cycle_desc = make_pha(cyc); break;                        // PHA impl
                            case 0x58: cycle_desc = make_cli(); break;                           // CLI impl
                            case 0x68: cycle_desc = make_pla(cyc); break;                        // PLA impl
                            case 0x78: cycle_desc = make_sei(); break;                           // SEI impl
                            case 0x88: cycle_desc = make_dey(); break;                           // DEY impl
                            case 0x98: cycle_desc = make_tya(); break;                           // TYA impl
                            case 0xA8: cycle_desc = make_tay(); break;                           // TAY impl
                            case 0xB8: cycle_desc = make_clv(); break;                           // CLV impl
                            case 0xC8: cycle_desc = make_iny(); break;                           // INY impl
                            case 0xD8: cycle_desc = make_cld(); break;                           // CLD impl
                            case 0xE8: cycle_desc = make_inx(); break;                           // INX impl
                            case 0xF8: cycle_desc = make_sed(); break;                           // SED impl

                            // === COLUMN 9: Immediate ===
                            case 0x09: cycle_desc = make_ora_imm(); break;                       // ORA #imm
                            case 0x19: cycle_desc = make_ora_absy(cyc); break;                   // ORA abs,Y
                            case 0x29: cycle_desc = make_and_imm(); break;                       // AND #imm
                            case 0x39: cycle_desc = make_and_absy(cyc); break;                   // AND abs,Y
                            case 0x49: cycle_desc = make_eor_imm(); break;                       // EOR #imm
                            case 0x59: cycle_desc = make_eor_absy(cyc); break;                   // EOR abs,Y
                            case 0x69: cycle_desc = make_adc_imm(); break;                       // ADC #imm
                            case 0x79: cycle_desc = make_adc_absy(cyc); break;                   // ADC abs,Y
                            case 0x89: cycle_desc = make_nop(); break;                           // NOP #imm (illegal)
                            case 0x99: cycle_desc = make_sta_absy(cyc); break;                   // STA abs,Y
                            case 0xA9: cycle_desc = make_lda_imm(); break;                       // LDA #imm
                            case 0xB9: cycle_desc = make_lda_absy(cyc); break;                   // LDA abs,Y
                            case 0xC9: cycle_desc = make_cmp_imm(); break;                       // CMP #imm
                            case 0xD9: cycle_desc = make_cmp_absy(cyc); break;                   // CMP abs,Y
                            case 0xE9: cycle_desc = make_sbc_imm(); break;                       // SBC #imm
                            case 0xF9: cycle_desc = make_sbc_absy(cyc); break;                   // SBC abs,Y

                            // === COLUMN A: Accumulator & Implied ===
                            case 0x0A: cycle_desc = make_asl_acc(); break;                       // ASL A
                            case 0x1A: cycle_desc = make_nop(); break;                           // NOP impl (illegal) / INC A (65C02)
                            case 0x2A: cycle_desc = make_rol_acc(); break;                       // ROL A
                            case 0x3A: cycle_desc = make_nop(); break;                           // NOP impl (illegal) / DEC A (65C02)
                            case 0x4A: cycle_desc = make_lsr_acc(); break;                       // LSR A
                            case 0x5A: cycle_desc = make_phy(cyc); break;                        // PHY impl (65C02) / NOP (illegal on NMOS)
                            case 0x6A: cycle_desc = make_ror_acc(); break;                       // ROR A
                            case 0x7A: cycle_desc = make_ply(cyc); break;                        // PLY impl (65C02) / NOP (illegal on NMOS)
                            case 0x8A: cycle_desc = make_txa(); break;                           // TXA impl
                            case 0x9A: cycle_desc = make_txs(); break;                           // TXS impl
                            case 0xAA: cycle_desc = make_tax(); break;                           // TAX impl
                            case 0xBA: cycle_desc = make_tsx(); break;                           // TSX impl
                            case 0xCA: cycle_desc = make_dex(); break;                           // DEX impl
                            case 0xDA: cycle_desc = make_phx(cyc); break;                        // PHX impl (65C02) / NOP (illegal on NMOS)
                            case 0xEA: cycle_desc = make_nop(); break;                           // NOP impl
                            case 0xFA: cycle_desc = make_plx(cyc); break;                        // PLX impl (65C02) / NOP (illegal on NMOS)

                            // === COLUMN B: Illegal opcodes (mostly unstable) ===
                            case 0x0B: cycle_desc = make_nop(); break;                           // ANC #imm (illegal, unstable)
                            case 0x1B: cycle_desc = make_nop(); break;                           // SLO abs,Y (illegal)
                            case 0x2B: cycle_desc = make_nop(); break;                           // ANC #imm (illegal, unstable)
                            case 0x3B: cycle_desc = make_nop(); break;                           // RLA abs,Y (illegal)
                            case 0x4B: cycle_desc = make_nop(); break;                           // ALR #imm (illegal, unstable)
                            case 0x5B: cycle_desc = make_nop(); break;                           // SRE abs,Y (illegal)
                            case 0x6B: cycle_desc = make_nop(); break;                           // ARR #imm (illegal, unstable)
                            case 0x7B: cycle_desc = make_nop(); break;                           // RRA abs,Y (illegal)
                            case 0x8B: cycle_desc = make_nop(); break;                           // XAA #imm (illegal, highly unstable)
                            case 0x9B: cycle_desc = make_nop(); break;                           // TAS abs,Y (illegal, unstable)
                            case 0xAB: cycle_desc = make_nop(); break;                           // LAX #imm (illegal, unstable)
                            case 0xBB: cycle_desc = make_nop(); break;                           // LAS abs,Y (illegal, unstable)
                            case 0xCB: cycle_desc = make_nop(); break;                           // AXS #imm (illegal, unstable)
                            case 0xDB: cycle_desc = make_nop(); break;                           // DCP abs,Y (illegal)
                            case 0xEB: cycle_desc = make_nop(); break;                           // SBC #imm (illegal, same as legal E9)
                            case 0xFB: cycle_desc = make_nop(); break;                           // ISC abs,Y (illegal)

                            // === COLUMN C: Absolute addressing & Jump ===
                            case 0x0C: cycle_desc = make_tsb_abs(cyc); break;                    // TSB abs (65C02) / NOP abs (illegal on NMOS)
                            case 0x1C: cycle_desc = make_trb_abs(cyc); break;                    // TRB abs (65C02) / NOP abs,X (illegal on NMOS)
                            case 0x2C: cycle_desc = make_bit_abs(cyc); break;                    // BIT abs
                            case 0x3C: cycle_desc = make_bit_absx(cyc); break;                   // BIT abs,X (65C02) / NOP abs,X (illegal on NMOS)
                            case 0x4C: cycle_desc = make_jmp_abs(cyc); break;                    // JMP abs
                            case 0x5C: cycle_desc = make_nop_absx(cyc); break;                   // NOP abs,X (illegal)
                            case 0x6C: cycle_desc = make_jmp_ind(cyc); break;                    // JMP (abs)
                            case 0x7C: cycle_desc = make_jmp_absx_ind(cyc); break;               // JMP (abs,X) (65C02) / NOP abs,X (illegal on NMOS)
                            case 0x8C: cycle_desc = make_sty_abs(cyc); break;                    // STY abs
                            case 0x9C: cycle_desc = make_stz_abs(cyc); break;                    // STZ abs (65C02) / SHY abs,X (illegal on NMOS)
                            case 0xAC: cycle_desc = make_ldy_abs(cyc); break;                    // LDY abs
                            case 0xBC: cycle_desc = make_ldy_absx(cyc); break;                   // LDY abs,X
                            case 0xCC: cycle_desc = make_cpy_abs(cyc); break;                    // CPY abs
                            case 0xDC: cycle_desc = make_nop_absx(cyc); break;                   // NOP abs,X (illegal)
                            case 0xEC: cycle_desc = make_cpx_abs(cyc); break;                    // CPX abs
                            case 0xFC: cycle_desc = make_nop_absx(cyc); break;                   // NOP abs,X (illegal)

                            // === COLUMN D: Absolute ===
                            case 0x0D: cycle_desc = make_ora_abs(cyc); break;                    // ORA abs
                            case 0x1D: cycle_desc = make_ora_absx(cyc); break;                   // ORA abs,X
                            case 0x2D: cycle_desc = make_and_abs(cyc); break;                    // AND abs
                            case 0x3D: cycle_desc = make_and_absx(cyc); break;                   // AND abs,X
                            case 0x4D: cycle_desc = make_eor_abs(cyc); break;                    // EOR abs
                            case 0x5D: cycle_desc = make_eor_absx(cyc); break;                   // EOR abs,X
                            case 0x6D: cycle_desc = make_adc_abs(cyc); break;                    // ADC abs
                            case 0x7D: cycle_desc = make_adc_absx(cyc); break;                   // ADC abs,X
                            case 0x8D: cycle_desc = make_sta_abs(cyc); break;                    // STA abs
                            case 0x9D: cycle_desc = make_sta_absx(cyc); break;                   // STA abs,X
                            case 0xAD: cycle_desc = make_lda_abs(cyc); break;                    // LDA abs
                            case 0xBD: cycle_desc = make_lda_absx(cyc); break;                   // LDA abs,X
                            case 0xCD: cycle_desc = make_cmp_abs(cyc); break;                    // CMP abs
                            case 0xDD: cycle_desc = make_cmp_absx(cyc); break;                   // CMP abs,X
                            case 0xED: cycle_desc = make_sbc_abs(cyc); break;                    // SBC abs
                            case 0xFD: cycle_desc = make_sbc_absx(cyc); break;                   // SBC abs,X

                            // === COLUMN E: Absolute with shifts ===
                            case 0x0E: cycle_desc = make_asl_abs(cyc); break;                    // ASL abs
                            case 0x1E: cycle_desc = make_asl_absx(cyc); break;                   // ASL abs,X
                            case 0x2E: cycle_desc = make_rol_abs(cyc); break;                    // ROL abs
                            case 0x3E: cycle_desc = make_rol_absx(cyc); break;                   // ROL abs,X
                            case 0x4E: cycle_desc = make_lsr_abs(cyc); break;                    // LSR abs
                            case 0x5E: cycle_desc = make_lsr_absx(cyc); break;                   // LSR abs,X
                            case 0x6E: cycle_desc = make_ror_abs(cyc); break;                    // ROR abs
                            case 0x7E: cycle_desc = make_ror_absx(cyc); break;                   // ROR abs,X
                            case 0x8E: cycle_desc = make_stx_abs(cyc); break;                    // STX abs
                            case 0x9E: cycle_desc = make_nop(); break;                           // SHX abs,Y (illegal, unstable)
                            case 0xAE: cycle_desc = make_ldx_abs(cyc); break;                    // LDX abs
                            case 0xBE: cycle_desc = make_ldx_absy(cyc); break;                   // LDX abs,Y
                            case 0xCE: cycle_desc = make_dec_abs(cyc); break;                    // DEC abs
                            case 0xDE: cycle_desc = make_dec_absx(cyc); break;                   // DEC abs,X
                            case 0xEE: cycle_desc = make_inc_abs(cyc); break;                    // INC abs
                            case 0xFE: cycle_desc = make_inc_absx(cyc); break;                   // INC abs,X

                            // === COLUMN F: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC) ===
                            case 0x0F: cycle_desc = make_slo_abs(cyc); break;                    // SLO abs (illegal)
                            case 0x1F: cycle_desc = make_slo_absx(cyc); break;                   // SLO abs,X (illegal)
                            case 0x2F: cycle_desc = make_rla_abs(cyc); break;                    // RLA abs (illegal)
                            case 0x3F: cycle_desc = make_rla_absx(cyc); break;                   // RLA abs,X (illegal)
                            case 0x4F: cycle_desc = make_sre_abs(cyc); break;                    // SRE abs (illegal)
                            case 0x5F: cycle_desc = make_sre_absx(cyc); break;                   // SRE abs,X (illegal)
                            case 0x6F: cycle_desc = make_rra_abs(cyc); break;                    // RRA abs (illegal)
                            case 0x7F: cycle_desc = make_rra_absx(cyc); break;                   // RRA abs,X (illegal)
                            case 0x8F: cycle_desc = make_sax_abs(cyc); break;                    // SAX abs (illegal)
                            case 0x9F: cycle_desc = make_nop(); break;                           // AHX abs,Y (illegal, unstable)
                            case 0xAF: cycle_desc = make_lax_abs(cyc); break;                    // LAX abs (illegal)
                            case 0xBF: cycle_desc = make_lax_absy(cyc); break;                   // LAX abs,Y (illegal)
                            case 0xCF: cycle_desc = make_dcp_abs(cyc); break;                    // DCP abs (illegal)
                            case 0xDF: cycle_desc = make_dcp_absx(cyc); break;                   // DCP abs,X (illegal)
                            case 0xEF: cycle_desc = make_isc_abs(cyc); break;                    // ISC abs (illegal)
                            case 0xFF: cycle_desc = make_isc_absx(cyc); break;                   // ISC abs,X (illegal)

                            default: cycle_desc = make_nop(); break;  // Fallback - should never happen
                        }
                    }
                    
                    table[table_index] = cycle_desc;
                    
                    // Validate SYNC bit constraints: exactly one SYNC per opcode
                    if (cycle_desc.is_sync()) {
                        if (sync_found) {
                            // COMPILE-TIME ERROR: Multiple SYNC bits in same opcode
                            // This validation ensures proper SYNC placement at compile-time
                            volatile int force_compilation_error[-1];  // Negative array size = compilation error
                            (void)force_compilation_error;  // Suppress unused variable warning
                        } else {
                            sync_found = true;
                            // SYNC marks end of opcode - fill remaining cycles with NOPs
                            // No functional cycles are allowed after SYNC
                            for (uint8_t remaining = cyc + 1; remaining <= 8; ++remaining) {
                                const uint16_t remaining_index = (op * 8) + (remaining - 1);
                                table[remaining_index] = make_nop();
                            }
                            break;  // End cycle generation for this opcode
                        }
                    }
                }
                
                // Ensure every opcode has exactly one SYNC cycle
                if (!sync_found) {
                    // COMPILE-TIME ERROR: No SYNC found for opcode - this indicates a bug
                    // This validation ensures every opcode has exactly one SYNC flag
                    volatile int force_compilation_error[-1];  // Negative array size = compilation error
                    (void)force_compilation_error;  // Suppress unused variable warning
                }
            }
            
            return table;
        }();
        
        // Convert 1-based cycle to 0-based table index
        return cycle_table[(opcode * 8) + (cycle - 1)];
    }
    
    
};

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_HPP