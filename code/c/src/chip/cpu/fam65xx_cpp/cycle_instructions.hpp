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
    
    // ADC immediate - 2 cycles total: opcode fetch (cycle 0), then operand fetch + execute (cycle 1)
    static constexpr cycle_desc_t make_adc_imm(uint8_t cycle = 1) {
        // Cycle 1: Read immediate operand from PC, increment PC, execute ADC
        // This gives us the correct PC advancement (+1 from opcode fetch, +1 from operand fetch = +2 total)
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

    // Inc/Dec operations - fixed to use NOP memory operation like accumulator operations
    static constexpr cycle_desc_t make_inx(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::INX) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_iny(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::INY) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dex(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::DEX) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_dey(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::NOP, AluOp::DEY) : addr::make_empty_cycle();
    }

    // Shift/Rotate operations - direct SYNC implementation
    static constexpr cycle_desc_t make_asl_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::ALU, AluOp::ASL) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_lsr_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::ALU, AluOp::LSR) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_rol_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::ALU, AluOp::ROL) : addr::make_empty_cycle();
    }
    static constexpr cycle_desc_t make_ror_acc(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::NOP, DataOp::ALU, AluOp::ROR) : addr::make_empty_cycle();
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

    static constexpr cycle_desc_t make_sbc_imm(uint8_t cycle = 1) {
        return (cycle == 1) ? CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::ALU, AluOp::SBC) : addr::make_empty_cycle();
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
};

} // namespace fam65xx_cpp

#endif // CYCLE_INSTRUCTIONS_HPP