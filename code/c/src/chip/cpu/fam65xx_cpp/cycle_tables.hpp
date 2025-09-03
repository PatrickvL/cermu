#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

#include "cpu_defs.hpp"
#include <array>

namespace fam65xx_cpp {

// Cycle descriptor structure - uses bit fields for packing (16 bits total)
struct cycle_desc_t {
    uint16_t mem_op : 4;      // 4 bits for MemOp (max 15)
    uint16_t data_op : 5;     // 5 bits for DataOp (max 31, need for new values)
    uint16_t alu_op : 5;      // 5 bits for AluOp (max 31, reduced from 7)
    uint16_t sync : 1;        // 1 bit - marks final cycle of instruction (SYNC)
    uint16_t conditional : 1; // 1 bit - marks cycles that might be skipped (page crossing, branch taken)
    
    // Type-safe accessors that return proper enum types
    constexpr MemOp get_mem_op() const noexcept { return static_cast<MemOp>(mem_op); }
    constexpr DataOp get_data_op() const noexcept { return static_cast<DataOp>(data_op); }
    constexpr AluOp get_alu_op() const noexcept { return static_cast<AluOp>(alu_op); }
    constexpr bool is_sync() const noexcept { return sync != 0; }
    constexpr bool is_conditional() const noexcept { return conditional != 0; }
};

// Verify the structure size remains 2 bytes (16 bits)
static_assert(sizeof(cycle_desc_t) == 2, "cycle_desc_t must be exactly 2 bytes");

// Cycle descriptor creation macros
#define CD_MAKE_SYNC(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 1, 0}

#define CD_MAKE(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 0, 0}

#define CD_MAKE_CONDITIONAL(mem, data, alu) \
    cycle_desc_t{static_cast<uint16_t>(mem), static_cast<uint16_t>(data), static_cast<uint16_t>(alu), 0, 1}

template<typename BusConfig>
class CycleTables {
public:
    // LDA immediate - single cycle
    static constexpr cycle_desc_t make_lda_imm() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // LDA zero page - 2 cycles
    static constexpr cycle_desc_t make_lda_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // LDA absolute - 3 cycles
    static constexpr cycle_desc_t make_lda_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP);
    }
    
    // ADC immediate - single cycle
    static constexpr cycle_desc_t make_adc_imm() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ADC);
    }
    
    // NOP - single cycle
    static constexpr cycle_desc_t make_nop() {
        return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
    }
    
    // BRK - 7 cycles
    static constexpr cycle_desc_t make_brk(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 5: return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);
            case 6: return CD_MAKE(MemOp::READ_ABS, DataOp::INTERRUPT_VEC, AluOp::NOP);
            default: return make_nop();
        }
    }
    
    // LAX zero page - illegal opcode, 2 cycles
    static constexpr cycle_desc_t make_lax_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::ILLEGAL_COMBO, AluOp::NOP);
    }
    
    // STA operations - store A register
    static constexpr cycle_desc_t make_sta_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_A, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_sta_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
    }
    
    // Transfer operations - single cycle
    static constexpr cycle_desc_t make_txa() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXA); }
    static constexpr cycle_desc_t make_tax() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAX); }
    static constexpr cycle_desc_t make_tya() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TYA); }
    static constexpr cycle_desc_t make_tay() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAY); }
    static constexpr cycle_desc_t make_tsx() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TSX); }
    static constexpr cycle_desc_t make_txs() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXS); }
    
    // Flag operations - single cycle
    static constexpr cycle_desc_t make_clc() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLC); }
    static constexpr cycle_desc_t make_sec() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEC); }
    static constexpr cycle_desc_t make_cli() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLI); }
    static constexpr cycle_desc_t make_sei() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEI); }
    static constexpr cycle_desc_t make_clv() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLV); }
    static constexpr cycle_desc_t make_cld() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLD); }
    static constexpr cycle_desc_t make_sed() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SED); }
    
    // Logical operations
    static constexpr cycle_desc_t make_and_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::AND); }
    static constexpr cycle_desc_t make_ora_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ORA); }
    static constexpr cycle_desc_t make_eor_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::EOR); }
    
    // Compare operations
    static constexpr cycle_desc_t make_cmp_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CMP); }
    static constexpr cycle_desc_t make_cpx_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPX); }
    static constexpr cycle_desc_t make_cpy_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPY); }
    
    // Load operations
    static constexpr cycle_desc_t make_ldx_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_X, AluOp::NOP); }
    static constexpr cycle_desc_t make_ldy_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_Y, AluOp::NOP); }
    
    static constexpr cycle_desc_t make_ldx_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_X, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_ldy_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_Y, AluOp::NOP);
    }
    
    // Inc/Dec operations
    static constexpr cycle_desc_t make_inx() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC); }
    static constexpr cycle_desc_t make_iny() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC); }
    static constexpr cycle_desc_t make_dex() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC); }
    static constexpr cycle_desc_t make_dey() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC); }
    
    // Stack operations
    static constexpr cycle_desc_t make_pha(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STORE_A, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_pla(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::READ_SP, DataOp::LOAD_A, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_php(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_plp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::READ_SP, DataOp::STACK_PULL, AluOp::NOP);
    }
    
    // Branch instructions - 2 cycles base, +1 if taken, +1 if page crossed
    static constexpr cycle_desc_t make_bcc(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bcs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_beq(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bne(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bpl(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bmi(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bvc(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    static constexpr cycle_desc_t make_bvs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE_CONDITIONAL(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Conditional: branch taken
    }
    
    // Jump instructions
    static constexpr cycle_desc_t make_jmp_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::NOP, DataOp::JMP, AluOp::NOP);
    }
    
    // JSR - 6 cycles
    static constexpr cycle_desc_t make_jsr(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
            case 3: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STACK_PUSH, AluOp::NOP);
            case 5: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            default: return CD_MAKE(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        }
    }
    
    // RTS - 6 cycles
    static constexpr cycle_desc_t make_rts(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
            case 3: return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP);
            case 5: return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP); // Internal operation
            default: return make_nop();
        }
    }

    // Shift/Rotate operations - ASL, LSR, ROL, ROR
    static constexpr cycle_desc_t make_asl_acc() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ASL); }
    static constexpr cycle_desc_t make_lsr_acc() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::LSR); }
    static constexpr cycle_desc_t make_rol_acc() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROL); }
    static constexpr cycle_desc_t make_ror_acc() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROR); }
    
    // Memory INC/DEC operations - 5 cycles for zero page
    static constexpr cycle_desc_t make_inc_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::INC); // Write new value
            default: return make_nop();
        }
    }
    
    static constexpr cycle_desc_t make_dec_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::DEC); // Write new value
            default: return make_nop();
        }
    }

    // Memory shift operations - 5 cycles for zero page
    static constexpr cycle_desc_t make_asl_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::ASL); // Write new value
            default: return make_nop();
        }
    }
    
    static constexpr cycle_desc_t make_lsr_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::LSR); // Write new value
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_rol_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::ROL); // Write new value
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_ror_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP); // Write old value
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::ROR); // Write new value
            default: return make_nop();
        }
    }

    // Zero page indexed addressing modes (zp,x and zp,y)
    static constexpr cycle_desc_t make_lda_zpx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X to zero page address
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_ldx_zpy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y to zero page address
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::LOAD_X, AluOp::NOP);
            default: return make_nop();
        }
    }

    // Absolute indexed addressing modes (abs,x and abs,y)
    static constexpr cycle_desc_t make_lda_absx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X, may cross page
            case 4: return CD_MAKE_CONDITIONAL(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP); // Conditional: page crossed
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_lda_absy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y, may cross page
            case 4: return CD_MAKE_CONDITIONAL(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP); // Conditional: page crossed
            default: return make_nop();
        }
    }

    // Indirect indexed addressing modes - (zp,x) and (zp),y
    static constexpr cycle_desc_t make_lda_indx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP); // Add X to zero page pointer
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read low byte of target
            case 4: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read high byte of target
            case 5: return CD_MAKE(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP); // Read from target address
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_lda_indy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP); // Read low byte of base
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP); // Read high byte of base
            case 4: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Add Y, may cross page
            case 5: return CD_MAKE_CONDITIONAL(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP); // Conditional: page crossed
            default: return make_nop();
        }
    }

    // 65C02 specific instructions
    static constexpr cycle_desc_t make_bra(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_phx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STORE_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_phy(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STORE_Y, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_plx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::READ_SP, DataOp::LOAD_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_ply(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        return CD_MAKE(MemOp::READ_SP, DataOp::LOAD_Y, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_stz_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_ZERO, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_stz_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_ZERO, AluOp::NOP);
    }

    // RTI - Return from interrupt, 6 cycles
    static constexpr cycle_desc_t make_rti(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_SP, DataOp::NOP, AluOp::NOP); // Internal operation
            case 3: return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull status
            case 4: return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCL
            case 5: return CD_MAKE(MemOp::READ_SP_INC, DataOp::STACK_PULL, AluOp::NOP); // Pull PCH
            default: return make_nop();
        }
    }

    // Complete addressing modes for STA operations
    static constexpr cycle_desc_t make_sta_zpx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sta_absx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_X, AluOp::NOP); // Always read first (6502 quirk)
            case 4: return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sta_absy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Always read first (6502 quirk)
            case 4: return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sta_indx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
            case 5: return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sta_indy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_Y, AluOp::NOP); // Always read first (6502 quirk)
            case 5: return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
            default: return make_nop();
        }
    }

    // Complete addressing modes for STX/STY
    static constexpr cycle_desc_t make_stx_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_stx_zpy(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_Y, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_X, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_stx_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_X, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_sty_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_Y, AluOp::NOP);
    }

    static constexpr cycle_desc_t make_sty_zpx(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::ADDR_ADD_X, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::STORE_Y, AluOp::NOP);
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sty_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ABS, DataOp::STORE_Y, AluOp::NOP);
    }

    // Complete addressing modes for ALU operations
    static constexpr cycle_desc_t make_adc_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_adc_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ABS, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_sbc_imm() { return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::SBC); }
    static constexpr cycle_desc_t make_sbc_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::ALU, AluOp::SBC);
    }

    static constexpr cycle_desc_t make_sbc_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ABS, DataOp::ALU, AluOp::SBC);
    }

    // Indirect JMP - 5 cycles
    static constexpr cycle_desc_t make_jmp_ind(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP); // Note: 6502 bug with page boundary
            default: return CD_MAKE(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        }
    }

    // Major illegal opcodes (NMOS 6502)
    static constexpr cycle_desc_t make_sax_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::WRITE_ZP, DataOp::ILLEGAL_COMBO, AluOp::SAX);
    }

    static constexpr cycle_desc_t make_dcp_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::DCP); // DEC then CMP
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_isc_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::ISC); // INC then SBC
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_slo_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::SLO); // ASL then ORA
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_rla_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::RLA); // ROL then AND
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_sre_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::SRE); // LSR then EOR
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_rra_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::RRA); // ROR then ADC
            default: return make_nop();
        }
    }

    // 65C02 additional instructions
    static constexpr cycle_desc_t make_tsb_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::TSB); // Test and set bits
            default: return make_nop();
        }
    }

    static constexpr cycle_desc_t make_trb_zp(uint8_t cycle) {
        switch (cycle) {
            case 1: return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
            case 2: return CD_MAKE(MemOp::READ_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 3: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_STORE, AluOp::NOP);
            case 4: return CD_MAKE(MemOp::WRITE_ZP, DataOp::TEMP_MODIFY, AluOp::TRB); // Test and reset bits
            default: return make_nop();
        }
    }

    // BIT instruction variations
    static constexpr cycle_desc_t make_bit_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ZP, DataOp::ALU, AluOp::BIT);
    }

    static constexpr cycle_desc_t make_bit_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_ABS, DataOp::ALU, AluOp::BIT);
    }

    // Main cycle table lookup
    static constexpr cycle_desc_t get_cycle(uint8_t opcode, uint8_t cycle) {
        constexpr auto cycle_table = []() {
            std::array<cycle_desc_t, 2048> table{};
            for (uint16_t i = 0; i < 2048; ++i) {
                uint8_t opcode = i / 8;
                uint8_t cycle = i % 8;
                switch (opcode) {
                    case 0xA9: table[i] = make_lda_imm(); break;          // LDA #$nn
                    case 0xA5: table[i] = make_lda_zp(cycle); break;      // LDA $nn
                    case 0xAD: table[i] = make_lda_abs(cycle); break;     // LDA $nnnn
                    case 0xA2: table[i] = make_ldx_imm(); break;          // LDX #$nn
                    case 0xA6: table[i] = make_ldx_zp(cycle); break;      // LDX $nn
                    case 0xA0: table[i] = make_ldy_imm(); break;          // LDY #$nn
                    case 0xA4: table[i] = make_ldy_zp(cycle); break;      // LDY $nn
                    case 0x85: table[i] = make_sta_zp(cycle); break;      // STA $nn
                    case 0x8D: table[i] = make_sta_abs(cycle); break;     // STA $nnnn
                    case 0x69: table[i] = make_adc_imm(); break;          // ADC #$nn
                    case 0x29: table[i] = make_and_imm(); break;          // AND #$nn
                    case 0x09: table[i] = make_ora_imm(); break;          // ORA #$nn
                    case 0x49: table[i] = make_eor_imm(); break;          // EOR #$nn
                    case 0xC9: table[i] = make_cmp_imm(); break;          // CMP #$nn
                    case 0xE0: table[i] = make_cpx_imm(); break;          // CPX #$nn
                    case 0xC0: table[i] = make_cpy_imm(); break;          // CPY #$nn
                    case 0x8A: table[i] = make_txa(); break;              // TXA
                    case 0xAA: table[i] = make_tax(); break;              // TAX
                    case 0x98: table[i] = make_tya(); break;              // TYA
                    case 0xA8: table[i] = make_tay(); break;              // TAY
                    case 0xBA: table[i] = make_tsx(); break;              // TSX
                    case 0x9A: table[i] = make_txs(); break;              // TXS
                    case 0x18: table[i] = make_clc(); break;              // CLC
                    case 0x38: table[i] = make_sec(); break;              // SEC
                    case 0x58: table[i] = make_cli(); break;              // CLI
                    case 0x78: table[i] = make_sei(); break;              // SEI
                    case 0xB8: table[i] = make_clv(); break;              // CLV
                    case 0xD8: table[i] = make_cld(); break;              // CLD
                    case 0xF8: table[i] = make_sed(); break;              // SED
                    case 0xE8: table[i] = make_inx(); break;              // INX
                    case 0xC8: table[i] = make_iny(); break;              // INY
                    case 0xCA: table[i] = make_dex(); break;              // DEX
                    case 0x88: table[i] = make_dey(); break;              // DEY
                    case 0xEA: table[i] = make_nop(); break;              // NOP
                    case 0x00: table[i] = make_brk(cycle); break;         // BRK
                    case 0xA7: table[i] = make_lax_zp(cycle); break;      // LAX $nn (illegal)
                    
                    // Branch instructions
                    case 0x90: table[i] = make_bcc(cycle); break;         // BCC
                    case 0xB0: table[i] = make_bcs(cycle); break;         // BCS
                    case 0xF0: table[i] = make_beq(cycle); break;         // BEQ
                    case 0xD0: table[i] = make_bne(cycle); break;         // BNE
                    case 0x10: table[i] = make_bpl(cycle); break;         // BPL
                    case 0x30: table[i] = make_bmi(cycle); break;         // BMI
                    case 0x50: table[i] = make_bvc(cycle); break;         // BVC
                    case 0x70: table[i] = make_bvs(cycle); break;         // BVS
                    
                    // Stack operations
                    case 0x48: table[i] = make_pha(cycle); break;         // PHA
                    case 0x68: table[i] = make_pla(cycle); break;         // PLA
                    case 0x08: table[i] = make_php(cycle); break;         // PHP
                    case 0x28: table[i] = make_plp(cycle); break;         // PLP
                    
                    // Jump operations
                    case 0x4C: table[i] = make_jmp_abs(cycle); break;     // JMP $nnnn
                    case 0x20: table[i] = make_jsr(cycle); break;         // JSR $nnnn
                    case 0x60: table[i] = make_rts(cycle); break;         // RTS
                    case 0x40: table[i] = make_rti(cycle); break;         // RTI
                    
                    // Shift/Rotate accumulator
                    case 0x0A: table[i] = make_asl_acc(); break;          // ASL A
                    case 0x4A: table[i] = make_lsr_acc(); break;          // LSR A
                    case 0x2A: table[i] = make_rol_acc(); break;          // ROL A
                    case 0x6A: table[i] = make_ror_acc(); break;          // ROR A
                    
                    // Memory increment/decrement
                    case 0xE6: table[i] = make_inc_zp(cycle); break;      // INC $nn
                    case 0xC6: table[i] = make_dec_zp(cycle); break;      // DEC $nn
                    
                    // Memory shift/rotate
                    case 0x06: table[i] = make_asl_zp(cycle); break;      // ASL $nn
                    case 0x46: table[i] = make_lsr_zp(cycle); break;      // LSR $nn
                    case 0x26: table[i] = make_rol_zp(cycle); break;      // ROL $nn
                    case 0x66: table[i] = make_ror_zp(cycle); break;      // ROR $nn
                    
                    // Indexed addressing modes
                    case 0xB5: table[i] = make_lda_zpx(cycle); break;     // LDA $nn,X
                    case 0xB6: table[i] = make_ldx_zpy(cycle); break;     // LDX $nn,Y
                    case 0xBD: table[i] = make_lda_absx(cycle); break;    // LDA $nnnn,X
                    case 0xB9: table[i] = make_lda_absy(cycle); break;    // LDA $nnnn,Y
                    
                    // Indirect addressing modes
                    case 0xA1: table[i] = make_lda_indx(cycle); break;    // LDA ($nn,X)
                    case 0xB1: table[i] = make_lda_indy(cycle); break;    // LDA ($nn),Y
                    
                    // Complete STA addressing modes
                    case 0x95: table[i] = make_sta_zpx(cycle); break;     // STA $nn,X
                    case 0x9D: table[i] = make_sta_absx(cycle); break;    // STA $nnnn,X
                    case 0x99: table[i] = make_sta_absy(cycle); break;    // STA $nnnn,Y
                    case 0x81: table[i] = make_sta_indx(cycle); break;    // STA ($nn,X)
                    case 0x91: table[i] = make_sta_indy(cycle); break;    // STA ($nn),Y
                    
                    // STX/STY addressing modes
                    case 0x86: table[i] = make_stx_zp(cycle); break;      // STX $nn
                    case 0x96: table[i] = make_stx_zpy(cycle); break;     // STX $nn,Y
                    case 0x8E: table[i] = make_stx_abs(cycle); break;     // STX $nnnn
                    case 0x84: table[i] = make_sty_zp(cycle); break;      // STY $nn
                    case 0x94: table[i] = make_sty_zpx(cycle); break;     // STY $nn,X
                    case 0x8C: table[i] = make_sty_abs(cycle); break;     // STY $nnnn
                    
                    // Complete ADC/SBC addressing modes
                    case 0x65: table[i] = make_adc_zp(cycle); break;      // ADC $nn
                    case 0x6D: table[i] = make_adc_abs(cycle); break;     // ADC $nnnn
                    case 0xE9: table[i] = make_sbc_imm(); break;          // SBC #$nn
                    case 0xE5: table[i] = make_sbc_zp(cycle); break;      // SBC $nn
                    case 0xED: table[i] = make_sbc_abs(cycle); break;     // SBC $nnnn
                    
                    // Indirect JMP
                    case 0x6C: table[i] = make_jmp_ind(cycle); break;     // JMP ($nnnn)
                    
                    // BIT instruction
                    case 0x24: table[i] = make_bit_zp(cycle); break;      // BIT $nn
                    case 0x2C: table[i] = make_bit_abs(cycle); break;     // BIT $nnnn
                    
                    // Major illegal opcodes (NMOS 6502)
                    case 0x87: table[i] = make_sax_zp(cycle); break;      // SAX $nn
                    case 0xC7: table[i] = make_dcp_zp(cycle); break;      // DCP $nn
                    case 0xE7: table[i] = make_isc_zp(cycle); break;      // ISC $nn
                    case 0x07: table[i] = make_slo_zp(cycle); break;      // SLO $nn
                    case 0x27: table[i] = make_rla_zp(cycle); break;      // RLA $nn
                    case 0x47: table[i] = make_sre_zp(cycle); break;      // SRE $nn
                    case 0x67: table[i] = make_rra_zp(cycle); break;      // RRA $nn
                    
                    // 65C02 specific instructions (will be NOP on NMOS)
                    case 0x80: table[i] = make_bra(cycle); break;         // BRA (65C02)
                    case 0xDA: table[i] = make_phx(cycle); break;         // PHX (65C02)
                    case 0x5A: table[i] = make_phy(cycle); break;         // PHY (65C02)
                    case 0xFA: table[i] = make_plx(cycle); break;         // PLX (65C02)
                    case 0x7A: table[i] = make_ply(cycle); break;         // PLY (65C02)
                    case 0x64: table[i] = make_stz_zp(cycle); break;      // STZ $nn (65C02)
                    case 0x9C: table[i] = make_stz_abs(cycle); break;     // STZ $nnnn (65C02)
                    case 0x04: table[i] = make_tsb_zp(cycle); break;      // TSB $nn (65C02)
                    case 0x14: table[i] = make_trb_zp(cycle); break;      // TRB $nn (65C02)
                    
                    default: table[i] = make_nop(); break;                // Default to NOP
                }
            }
            return table;
        }();
        return cycle_table[(opcode * 8) + cycle];
    }
    
    // Get number of cycles for an instruction
    static constexpr uint8_t get_cycle_count(uint8_t opcode) {
        switch (opcode) {
            // Single-byte immediate and implied instructions - 2 cycles
            case 0xA9:   // LDA #$nn
            case 0xA2:   // LDX #$nn
            case 0xA0:   // LDY #$nn
            case 0x69:   // ADC #$nn
            case 0x29:   // AND #$nn
            case 0x09:   // ORA #$nn
            case 0x49:   // EOR #$nn
            case 0xC9:   // CMP #$nn
            case 0xE0:   // CPX #$nn
            case 0xC0:   // CPY #$nn
            case 0x8A:   // TXA
            case 0xAA:   // TAX
            case 0x98:   // TYA
            case 0xA8:   // TAY
            case 0xBA:   // TSX
            case 0x9A:   // TXS
            case 0x18:   // CLC
            case 0x38:   // SEC
            case 0x58:   // CLI
            case 0x78:   // SEI
            case 0xB8:   // CLV
            case 0xD8:   // CLD
            case 0xF8:   // SED
            case 0xE8:   // INX
            case 0xC8:   // INY
            case 0xCA:   // DEX
            case 0x88:   // DEY
            case 0xEA:   // NOP
            case 0x0A:   // ASL A
            case 0x4A:   // LSR A
            case 0x2A:   // ROL A
            case 0x6A:   // ROR A
                return 2;
                
            // Branch instructions - 2+ cycles (base case, +1 if taken, +1 if page crossed)
            case 0x90:   // BCC
            case 0xB0:   // BCS
            case 0xF0:   // BEQ
            case 0xD0:   // BNE
            case 0x10:   // BPL
            case 0x30:   // BMI
            case 0x50:   // BVC
            case 0x70:   // BVS
            case 0x80:   // BRA (65C02)
                return 2;
                
            // Zero page instructions - 3 cycles
            case 0xA5:   // LDA $nn
            case 0xA6:   // LDX $nn
            case 0xA4:   // LDY $nn
            case 0x85:   // STA $nn
            case 0x86:   // STX $nn
            case 0x84:   // STY $nn
            case 0x65:   // ADC $nn
            case 0xE5:   // SBC $nn
            case 0x24:   // BIT $nn
            case 0xA7:   // LAX $nn (illegal)
            case 0x87:   // SAX $nn (illegal)
            case 0x48:   // PHA
            case 0x68:   // PLA
            case 0x08:   // PHP
            case 0x28:   // PLP
            case 0xDA:   // PHX (65C02)
            case 0x5A:   // PHY (65C02)
            case 0xFA:   // PLX (65C02)
            case 0x7A:   // PLY (65C02)
            case 0x64:   // STZ $nn (65C02)
                return 3;
                
            // Zero page indexed instructions - 4 cycles
            case 0xB5:   // LDA $nn,X
            case 0xB6:   // LDX $nn,Y
            case 0x95:   // STA $nn,X
            case 0x96:   // STX $nn,Y
            case 0x94:   // STY $nn,X
                return 4;
                
            // Absolute instructions - 4 cycles
            case 0xAD:   // LDA $nnnn
            case 0x8D:   // STA $nnnn
            case 0x8E:   // STX $nnnn
            case 0x8C:   // STY $nnnn
            case 0x6D:   // ADC $nnnn
            case 0xED:   // SBC $nnnn
            case 0x2C:   // BIT $nnnn
            case 0x4C:   // JMP $nnnn
            case 0x9C:   // STZ $nnnn (65C02)
                return 4;
                
            // Absolute indexed instructions - 4+ cycles (base case, +1 if page crossed)
            case 0xBD:   // LDA $nnnn,X
            case 0xB9:   // LDA $nnnn,Y
            case 0x9D:   // STA $nnnn,X (always 5 cycles)
            case 0x99:   // STA $nnnn,Y (always 5 cycles)
                return 5;
                
            // Indirect JMP - 5 cycles
            case 0x6C:   // JMP ($nnnn)
                return 5;
                
            // Zero page memory operations - 5 cycles
            case 0xE6:   // INC $nn
            case 0xC6:   // DEC $nn
            case 0x06:   // ASL $nn
            case 0x46:   // LSR $nn
            case 0x26:   // ROL $nn
            case 0x66:   // ROR $nn
            case 0xC7:   // DCP $nn (illegal)
            case 0xE7:   // ISC $nn (illegal)
            case 0x07:   // SLO $nn (illegal)
            case 0x27:   // RLA $nn (illegal)
            case 0x47:   // SRE $nn (illegal)
            case 0x67:   // RRA $nn (illegal)
            case 0x04:   // TSB $nn (65C02)
            case 0x14:   // TRB $nn (65C02)
                return 5;
                
            // Indirect indexed instructions - 6 cycles
            case 0xA1:   // LDA ($nn,X)
            case 0xB1:   // LDA ($nn),Y
            case 0x81:   // STA ($nn,X)
            case 0x91:   // STA ($nn),Y
                return 6;
                
            // JSR and RTS - 6 cycles
            case 0x20:   // JSR $nnnn
            case 0x60:   // RTS
            case 0x40:   // RTI
                return 6;
                
            // BRK instruction - 7 cycles
            case 0x00:   // BRK
                return 7;
                
            default:
                return 2; // Default to 2 cycles
        }
    }
};

} // namespace fam65xx_cpp

#endif // CYCLE_TABLES_HPP