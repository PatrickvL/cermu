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
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // Should never reach here
    }
    
    // Memory modify operations absolute (6 cycles)
    static constexpr cycle_desc_t make_memory_modify_abs(uint8_t cycle, AluOp modify_op) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::TEMP_STORE, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::WRITE_ABS, DataOp::TEMP_STORE, AluOp::NOP); // Write old value back
        if (cycle == 5) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::TEMP_MODIFY, modify_op); // Write modified value
        return CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::NOP); // Should never reach here
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
                        // Handle regular opcodes (0-255)
                        cycle_desc = get_regular_opcode_cycle(static_cast<uint8_t>(op), cyc);
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
    
    // Helper function to get cycle for regular opcodes (0-255)
    static constexpr cycle_desc_t get_regular_opcode_cycle(uint8_t opcode, uint8_t cycle) {
        switch (opcode) {
            case 0xA9: return make_lda_imm();                         // LDA #$nn
            case 0xA5: return make_lda_zp(cycle);                     // LDA $nn
            case 0xAD: return make_lda_abs(cycle);                    // LDA $nnnn
            case 0xA2: return make_ldx_imm();                         // LDX #$nn
            case 0xA6: return make_ldx_zp(cycle);                     // LDX $nn
            case 0xA0: return make_ldy_imm();                         // LDY #$nn
            case 0xA4: return make_ldy_zp(cycle);                     // LDY $nn
            case 0x85: return make_sta_zp(cycle);                     // STA $nn
            case 0x8D: return make_sta_abs(cycle);                    // STA $nnnn
            case 0x69: return make_adc_imm();                         // ADC #$nn
            case 0x29: return make_and_imm();                         // AND #$nn
            case 0x09: return make_ora_imm();                         // ORA #$nn
            case 0x49: return make_eor_imm();                         // EOR #$nn
            case 0xC9: return make_cmp_imm();                         // CMP #$nn
            case 0xE0: return make_cpx_imm();                         // CPX #$nn
            case 0xC0: return make_cpy_imm();                         // CPY #$nn
            case 0x8A: return make_txa();                             // TXA
            case 0xAA: return make_tax();                             // TAX
            case 0x98: return make_tya();                             // TYA
            case 0xA8: return make_tay();                             // TAY
            case 0xBA: return make_tsx();                             // TSX
            case 0x9A: return make_txs();                             // TXS
            case 0x18: return make_clc();                             // CLC
            case 0x38: return make_sec();                             // SEC
            case 0x58: return make_cli();                             // CLI
            case 0x78: return make_sei();                             // SEI
            case 0xB8: return make_clv();                             // CLV
            case 0xD8: return make_cld();                             // CLD
            case 0xF8: return make_sed();                             // SED
            case 0xE8: return make_inx();                             // INX
            case 0xC8: return make_iny();                             // INY
            case 0xCA: return make_dex();                             // DEX
            case 0x88: return make_dey();                             // DEY
            case 0xEA: return make_nop();                             // NOP
            case 0x00: return make_brk(cycle);                        // BRK
            case 0xA7: return make_lax_zp(cycle);                     // LAX $nn (illegal)
            
            // Branch instructions
            case 0x90: return make_bcc(cycle);                        // BCC
            case 0xB0: return make_bcs(cycle);                        // BCS
            case 0xF0: return make_beq(cycle);                        // BEQ
            case 0xD0: return make_bne(cycle);                        // BNE
            case 0x10: return make_bpl(cycle);                        // BPL
            case 0x30: return make_bmi(cycle);                        // BMI
            case 0x50: return make_bvc(cycle);                        // BVC
            case 0x70: return make_bvs(cycle);                        // BVS
            
            // Stack operations
            case 0x48: return make_pha(cycle);                        // PHA
            case 0x68: return make_pla(cycle);                        // PLA
            case 0x08: return make_php(cycle);                        // PHP
            case 0x28: return make_plp(cycle);                        // PLP
            
            // Jump operations
            case 0x4C: return make_jmp_abs(cycle);                    // JMP $nnnn
            case 0x20: return make_jsr(cycle);                        // JSR $nnnn
            case 0x60: return make_rts(cycle);                        // RTS
            case 0x40: return make_rti(cycle);                        // RTI
            
            // Shift/Rotate accumulator
            case 0x0A: return make_asl_acc();                         // ASL A
            case 0x4A: return make_lsr_acc();                         // LSR A
            case 0x2A: return make_rol_acc();                         // ROL A
            case 0x6A: return make_ror_acc();                         // ROR A
            
            // Memory increment/decrement
            case 0xE6: return make_inc_zp(cycle);                     // INC $nn
            case 0xC6: return make_dec_zp(cycle);                     // DEC $nn
            
            // Memory shift/rotate
            case 0x06: return make_asl_zp(cycle);                     // ASL $nn
            case 0x46: return make_lsr_zp(cycle);                     // LSR $nn
            case 0x26: return make_rol_zp(cycle);                     // ROL $nn
            case 0x66: return make_ror_zp(cycle);                     // ROR $nn
            
            // Indexed addressing modes
            case 0xB5: return make_lda_zpx(cycle);                    // LDA $nn,X
            case 0xB6: return make_ldx_zpy(cycle);                    // LDX $nn,Y
            case 0xBD: return make_lda_absx(cycle);                   // LDA $nnnn,X
            case 0xB9: return make_lda_absy(cycle);                   // LDA $nnnn,Y
            
            // Indirect addressing modes
            case 0xA1: return make_lda_indx(cycle);                   // LDA ($nn,X)
            case 0xB1: return make_lda_indy(cycle);                   // LDA ($nn),Y
            
            // Complete STA addressing modes
            case 0x95: return make_sta_zpx(cycle);                    // STA $nn,X
            case 0x9D: return make_sta_absx(cycle);                   // STA $nnnn,X
            case 0x99: return make_sta_absy(cycle);                   // STA $nnnn,Y
            case 0x81: return make_sta_indx(cycle);                   // STA ($nn,X)
            case 0x91: return make_sta_indy(cycle);                   // STA ($nn),Y
            
            // STX/STY addressing modes
            case 0x86: return make_stx_zp(cycle);                     // STX $nn
            case 0x96: return make_stx_zpy(cycle);                    // STX $nn,Y
            case 0x8E: return make_stx_abs(cycle);                    // STX $nnnn
            case 0x84: return make_sty_zp(cycle);                     // STY $nn
            case 0x94: return make_sty_zpx(cycle);                    // STY $nn,X
            case 0x8C: return make_sty_abs(cycle);                    // STY $nnnn
            
            // Complete ADC/SBC addressing modes
            case 0x65: return make_adc_zp(cycle);                     // ADC $nn
            case 0x6D: return make_adc_abs(cycle);                    // ADC $nnnn
            case 0xE9: return make_sbc_imm();                         // SBC #$nn
            case 0xE5: return make_sbc_zp(cycle);                     // SBC $nn
            case 0xED: return make_sbc_abs(cycle);                    // SBC $nnnn
            
            // Indirect JMP
            case 0x6C: return make_jmp_ind(cycle);                    // JMP ($nnnn)
            
            // BIT instruction
            case 0x24: return make_bit_zp(cycle);                     // BIT $nn
            case 0x2C: return make_bit_abs(cycle);                    // BIT $nnnn
            
            // Major illegal opcodes (NMOS 6502)
            case 0x87: return make_sax_zp(cycle);                     // SAX $nn
            case 0xC7: return make_dcp_zp(cycle);                     // DCP $nn
            case 0xE7: return make_isc_zp(cycle);                     // ISC $nn
            case 0x07: return make_slo_zp(cycle);                     // SLO $nn
            case 0x27: return make_rla_zp(cycle);                     // RLA $nn
            case 0x47: return make_sre_zp(cycle);                     // SRE $nn
            case 0x67: return make_rra_zp(cycle);                     // RRA $nn
            
            // 65C02 specific instructions (will be NOP on NMOS)
            case 0x80: return make_bra(cycle);                        // BRA (65C02)
            case 0xDA: return make_phx(cycle);                        // PHX (65C02)
            case 0x5A: return make_phy(cycle);                        // PHY (65C02)
            case 0xFA: return make_plx(cycle);                        // PLX (65C02)
            case 0x7A: return make_ply(cycle);                        // PLY (65C02)
            case 0x64: return make_stz_zp(cycle);                     // STZ $nn (65C02)
            case 0x9C: return make_stz_abs(cycle);                    // STZ $nnnn (65C02)
            case 0x04: return make_tsb_zp(cycle);                     // TSB $nn (65C02)
            case 0x14: return make_trb_zp(cycle);                     // TRB $nn (65C02)
                    
            default: return make_nop();  // Default to NOP for undefined opcodes
        }
    }
    
};

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_HPP