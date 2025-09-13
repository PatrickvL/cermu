#ifndef CYCLE_INSTRUCTIONS_HPP
#define CYCLE_INSTRUCTIONS_HPP

#include "cycle_types.hpp"
#include "cycle_addressing.hpp"
#include "cycle_interrupts.hpp"

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleInstructions : public CycleAddressing<BusConfig>, public CycleInterrupts<BusConfig> {
    // Short alias to avoid repetitive typing
    using addr = CycleAddressing<BusConfig>;

public:

    // === INSTRUCTION IMPLEMENTATIONS (using helpers) ===
    
    // LDA immediate - direct SYNC implementation
    static constexpr cycle_desc_t make_lda_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_A, AluOp::NOP) : addr::make_empty_cycle();
    }
    
    
    // ADC immediate - direct SYNC implementation
    static constexpr cycle_desc_t make_adc_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ADC) : addr::make_empty_cycle();
    }
    
    // Real NOP opcode (0xEA) - 1 cycle instruction
    static constexpr cycle_desc_t make_nop(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP) : addr::make_empty_cycle();
    }
    
    
    
    // Transfer operations - direct SYNC implementation to avoid helper issues
    static constexpr cycle_desc_t make_txa(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXA) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tax(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAX) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tya(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TYA) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tay(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TAY) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_tsx(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TSX) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_txs(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::TXS) : addr::make_empty_cycle();
    }
    
    // Flag operations - direct SYNC implementation to avoid helper issues
    static constexpr cycle_desc_t make_clc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sec(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cli(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLI) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sei(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SEI) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_clv(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLV) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cld(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::CLD) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sed(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::SED) : addr::make_empty_cycle();
    }
    
    // Logical operations - direct SYNC implementation
    static constexpr cycle_desc_t make_and_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::AND) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ora_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::ORA) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_eor_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::EOR) : addr::make_empty_cycle();
    }
    
    // Compare operations - direct SYNC implementation
    static constexpr cycle_desc_t make_cmp_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CMP) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cpx_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPX) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_cpy_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::CPY) : addr::make_empty_cycle();
    }
    
    // Load operations - direct SYNC implementation
    static constexpr cycle_desc_t make_ldx_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_X, AluOp::NOP) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ldy_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::LOAD_Y, AluOp::NOP) : addr::make_empty_cycle();
    }
    
    
    // Inc/Dec operations - direct SYNC implementation
    static constexpr cycle_desc_t make_inx(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_iny(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dex(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dey(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : addr::make_empty_cycle();
    }
    
    
    
    
    // Shift/Rotate operations - direct SYNC implementation
    static constexpr cycle_desc_t make_asl_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ASL) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_lsr_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::LSR) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_rol_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROL) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ror_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::ROR) : addr::make_empty_cycle();
    }
    

    // Basic zero page addressing modes for loads
    static constexpr cycle_desc_t make_lda_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_ldx_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::LOAD_X);
    }

    static constexpr cycle_desc_t make_ldy_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::LOAD_Y);
    }

    // Absolute addressing modes for loads
    static constexpr cycle_desc_t make_lda_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::LOAD_A);
    }

    // Zero page indexed addressing modes (zp,x and zp,y) - 3 cycles
    static constexpr cycle_desc_t make_lda_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_ldx_zpy(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X);
    }

    // Absolute indexed addressing modes (abs,x and abs,y) - 4+ cycles
    static constexpr cycle_desc_t make_lda_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_lda_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_A);
    }

    // Indirect indexed addressing modes - (zp,x) and (zp),y - 5+ cycles
    static constexpr cycle_desc_t make_lda_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::LOAD_A);
    }

    static constexpr cycle_desc_t make_lda_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::LOAD_A);
    }


    static constexpr cycle_desc_t make_phx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_X, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_phy(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_Y, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_plx(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_X, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_ply(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_SP_INC, DataOp::NOP, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::READ_SP, DataOp::LOAD_Y, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_stz_zp(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE_SYNC(MemOp::WRITE_ZP, DataOp::STORE_ZERO, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_stz_abs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::STORE_ZERO, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    // 65C02 accumulator increment/decrement operations
    static constexpr cycle_desc_t make_inc_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::INC) : addr::make_empty_cycle();
    }
    
    static constexpr cycle_desc_t make_dec_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::NOP, AluOp::DEC) : addr::make_empty_cycle();
    }

    // 65C02 immediate BIT operation - 2 cycles
    static constexpr cycle_desc_t make_bit_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::BIT) : addr::make_empty_cycle();
    }

    // 65C02 zero page indirect addressing modes - ($zp)
    static constexpr cycle_desc_t make_ora_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::ORA);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_and_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::AND);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_eor_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::EOR);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_adc_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::ADC);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_sta_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::WRITE_ABS, DataOp::STORE_A, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_lda_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::LOAD_A, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_cmp_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::CMP);
        return addr::make_empty_cycle();
    }

    static constexpr cycle_desc_t make_sbc_zp_ind(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ZP, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 4) return CD_MAKE_SYNC(MemOp::READ_ABS, DataOp::ALU, AluOp::SBC);
        return addr::make_empty_cycle();
    }

    // Complete addressing modes for STA operations - 3 cycles
    static constexpr cycle_desc_t make_sta_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_absx(uint8_t cycle) {
        return addr::make_absolute_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_absy(uint8_t cycle) {
        return addr::make_absolute_indexed_write(cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_indx(uint8_t cycle) {
        return addr::make_indexed_indirect_write(cycle, DataOp::STORE_A);
    }

    static constexpr cycle_desc_t make_sta_indy(uint8_t cycle) {
        return addr::make_indirect_indexed_write(cycle, DataOp::STORE_A);
    }

    // Complete addressing modes for STX/STY
    static constexpr cycle_desc_t make_stx_zp(uint8_t cycle) {
        return addr::make_zeropage_write(cycle, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_stx_zpy(uint8_t cycle) {
        return addr::make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_stx_abs(uint8_t cycle) {
        return addr::make_absolute_write(cycle, DataOp::STORE_X);
    }

    static constexpr cycle_desc_t make_sty_zp(uint8_t cycle) {
        return addr::make_zeropage_write(cycle, DataOp::STORE_Y);
    }

    static constexpr cycle_desc_t make_sty_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_Y);
    }

    static constexpr cycle_desc_t make_sty_abs(uint8_t cycle) {
        return addr::make_absolute_write(cycle, DataOp::STORE_Y);
    }

    // Complete addressing modes for ALU operations
    static constexpr cycle_desc_t make_adc_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_adc_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::ADC);
    }

    static constexpr cycle_desc_t make_sbc_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::SBC) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_sbc_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::SBC);
    }

    static constexpr cycle_desc_t make_sbc_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::SBC);
    }


    // Major illegal opcodes (NMOS 6502) - special case for SAX (2-cycle)
    static constexpr cycle_desc_t make_sax_zp(uint8_t cycle) {
        return addr::make_zeropage_write(cycle, DataOp::ILLEGAL_COMBO);
    }

    // All illegal opcodes and 65C02 additional instructions removed - they were simple wrappers
    // These have been replaced with direct calls to addr:: functions where used

    // BIT instruction variations
    static constexpr cycle_desc_t make_bit_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::BIT);
    }

    static constexpr cycle_desc_t make_bit_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::BIT);
    }

    // === ADDITIONAL INSTRUCTION IMPLEMENTATIONS FOR COMPLETE 6502 SET ===
    
    // Complete ORA addressing modes
    static constexpr cycle_desc_t make_ora_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::ORA);
    }
    static constexpr cycle_desc_t make_ora_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::ORA);
    }

    // Complete AND addressing modes
    static constexpr cycle_desc_t make_and_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::AND);
    }
    static constexpr cycle_desc_t make_and_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::AND);
    }

    // Complete EOR addressing modes
    static constexpr cycle_desc_t make_eor_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::EOR);
    }
    static constexpr cycle_desc_t make_eor_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::EOR);
    }

    // Complete ADC addressing modes
    static constexpr cycle_desc_t make_adc_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::ADC);
    }
    static constexpr cycle_desc_t make_adc_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::ADC);
    }

    // Complete SBC addressing modes
    static constexpr cycle_desc_t make_sbc_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::SBC);
    }
    static constexpr cycle_desc_t make_sbc_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::SBC);
    }

    // Complete CMP addressing modes
    static constexpr cycle_desc_t make_cmp_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_indx(uint8_t cycle) {
        return addr::make_indexed_indirect(cycle, DataOp::ALU, AluOp::CMP);
    }
    static constexpr cycle_desc_t make_cmp_indy(uint8_t cycle) {
        return addr::make_indirect_indexed(cycle, DataOp::ALU, AluOp::CMP);
    }

    // Complete CPX/CPY addressing modes
    static constexpr cycle_desc_t make_cpx_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::CPX);
    }
    static constexpr cycle_desc_t make_cpx_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::CPX);
    }
    static constexpr cycle_desc_t make_cpy_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::ALU, AluOp::CPY);
    }
    static constexpr cycle_desc_t make_cpy_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::ALU, AluOp::CPY);
    }

    // Complete LDX/LDY addressing modes
    static constexpr cycle_desc_t make_ldx_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::LOAD_X);
    }
    static constexpr cycle_desc_t make_ldx_absy(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X);
    }
    static constexpr cycle_desc_t make_ldy_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y);
    }
    static constexpr cycle_desc_t make_ldy_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::LOAD_Y);
    }
    static constexpr cycle_desc_t make_ldy_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y);
    }

    // Complete memory modification operations (INC/DEC/ASL/LSR/ROL/ROR)
    static constexpr cycle_desc_t make_inc_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::INC);  // Note: indexed versions use same helper
    }
    static constexpr cycle_desc_t make_dec_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::DEC);
    }
    static constexpr cycle_desc_t make_inc_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::INC);
    }
    static constexpr cycle_desc_t make_dec_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::DEC);
    }
    static constexpr cycle_desc_t make_inc_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::INC);  // Note: indexed versions use same helper
    }
    static constexpr cycle_desc_t make_dec_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::DEC);
    }

    // Complete shift/rotate operations
    static constexpr cycle_desc_t make_asl_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_zpx(uint8_t cycle) {
        return addr::make_memory_modify_zp(cycle, AluOp::ROR);
    }
    static constexpr cycle_desc_t make_asl_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ROR);
    }
    static constexpr cycle_desc_t make_asl_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ASL);
    }
    static constexpr cycle_desc_t make_lsr_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::LSR);
    }
    static constexpr cycle_desc_t make_rol_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ROL);
    }
    static constexpr cycle_desc_t make_ror_absx(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::ROR);
    }

    // NOP variants (illegal opcodes with different addressing modes)
    static constexpr cycle_desc_t make_nop_zp(uint8_t cycle) {
        return addr::make_zeropage(cycle, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_abs(uint8_t cycle) {
        return addr::make_absolute(cycle, DataOp::NOP);
    }
    static constexpr cycle_desc_t make_nop_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::NOP);
    }

    // 65C02 extensions
    static constexpr cycle_desc_t make_bit_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT);
    }
    static constexpr cycle_desc_t make_bit_absx(uint8_t cycle) {
        return addr::make_absolute_indexed(cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT);
    }
    static constexpr cycle_desc_t make_stz_zpx(uint8_t cycle) {
        return addr::make_zeropage_indexed_write(cycle, DataOp::ADDR_ADD_X, DataOp::STORE_ZERO);
    }
    static constexpr cycle_desc_t make_tsb_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::TSB);
    }
    static constexpr cycle_desc_t make_trb_abs(uint8_t cycle) {
        return addr::make_memory_modify_abs(cycle, AluOp::TRB);
    }
    static constexpr cycle_desc_t make_jmp_absx_ind(uint8_t cycle) {
        // JMP (abs,X) - 65C02 only, 6 cycles
        if (cycle == 1) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_LOW, AluOp::NOP);
        if (cycle == 2) return CD_MAKE(MemOp::READ_PC_INC, DataOp::ADDR_CALC_HIGH, AluOp::NOP);
        if (cycle == 3) return CD_MAKE(MemOp::READ_ABS, DataOp::ADDR_ADD_X, AluOp::NOP);
        if (cycle == 4) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_LOW, AluOp::NOP);
        if (cycle == 5) return CD_MAKE(MemOp::READ_ABS, DataOp::INDIRECT_HIGH, AluOp::NOP);
        if (cycle == 6) return CD_MAKE_SYNC(MemOp::NOP, DataOp::JMP, AluOp::NOP);
        return addr::make_empty_cycle();
    }

    // All orphaned wrapper functions removed for performance optimization
    // Direct calls to addr:: functions are used where these instructions are needed
};

} // namespace fam65xx_cpp

#endif // CYCLE_INSTRUCTIONS_HPP