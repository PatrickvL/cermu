#ifndef CYCLE_INSTRUCTIONS_HPP
#define CYCLE_INSTRUCTIONS_HPP

#include "cycle_types.hpp"
#include "cycle_addressing.hpp"
#include "cycle_interrupts.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleInstructions : public CycleAddressing<BusConfig>, public CycleInterrupts<BusConfig> {
public:
    // Import base class methods
    using CycleAddressing<BusConfig>::make_empty_cycle;
    using CycleAddressing<BusConfig>::make_immediate;
    using CycleAddressing<BusConfig>::make_zeropage;
    using CycleAddressing<BusConfig>::make_zeropage_write;
    using CycleAddressing<BusConfig>::make_absolute;
    using CycleAddressing<BusConfig>::make_absolute_write;
    using CycleAddressing<BusConfig>::make_zeropage_indexed;
    using CycleAddressing<BusConfig>::make_zeropage_indexed_write;
    using CycleAddressing<BusConfig>::make_absolute_indexed;
    using CycleAddressing<BusConfig>::make_absolute_indexed_write;
    using CycleAddressing<BusConfig>::make_indexed_indirect;
    using CycleAddressing<BusConfig>::make_indexed_indirect_write;
    using CycleAddressing<BusConfig>::make_indirect_indexed;
    using CycleAddressing<BusConfig>::make_indirect_indexed_write;
    using CycleAddressing<BusConfig>::make_memory_modify_zp;
    using CycleAddressing<BusConfig>::make_memory_modify_abs;
    using CycleAddressing<BusConfig>::make_stack_push;
    using CycleAddressing<BusConfig>::make_stack_pull;
    using CycleAddressing<BusConfig>::make_branch;
    using CycleAddressing<BusConfig>::make_jump_absolute;
    using CycleAddressing<BusConfig>::make_jump_indirect;
    using CycleAddressing<BusConfig>::make_jsr;
    using CycleAddressing<BusConfig>::make_rts;
    using CycleAddressing<BusConfig>::make_rti;
    using CycleInterrupts<BusConfig>::make_brk;

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

    // 65C02 accumulator increment/decrement operations
    static constexpr cycle_desc_t make_inc_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_dec_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : make_empty_cycle();
    }

    // 65C02 immediate BIT operation - 2 cycles
    static constexpr cycle_desc_t make_bit_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::BIT) : make_empty_cycle();
    }

    // 65C02 zero page indirect addressing modes - ($zp)
    static constexpr cycle_desc_t make_ora_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::ORA);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_and_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::AND);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_eor_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::EOR);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_adc_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::ADC);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_sta_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_lda_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_cmp_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::CMP);
        return make_empty_cycle();
    }

    static constexpr cycle_desc_t make_sbc_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::SBC);
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
};

} // namespace fam65xx_cpp

#endif // CYCLE_INSTRUCTIONS_HPP