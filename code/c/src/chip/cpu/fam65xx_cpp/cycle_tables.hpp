#ifndef CYCLE_TABLES_HPP
#define CYCLE_TABLES_HPP

#include "cpu_defs.hpp"
#include <array>

namespace fam65xx_cpp {

// Cycle descriptor structure - still uses bit fields for packing
struct cycle_desc_t {
    uint8_t mem_op : 4;   // Packed as uint8_t for efficiency
    uint8_t data_op : 4;  // Packed as uint8_t for efficiency
    uint8_t alu_op : 8;   // Packed as uint8_t for efficiency
    
    // Type-safe accessors that return proper enum types
    constexpr MemOp get_mem_op() const noexcept { return static_cast<MemOp>(mem_op); }
    constexpr DataOp get_data_op() const noexcept { return static_cast<DataOp>(data_op); }
    constexpr AluOp get_alu_op() const noexcept { return static_cast<AluOp>(alu_op); }
};

// Cycle descriptor creation macros
#define CD_MAKE_SYNC(mem, data, alu) \
    cycle_desc_t{static_cast<uint8_t>(mem), static_cast<uint8_t>(data), static_cast<uint8_t>(alu)}

#define CD_MAKE(mem, data, alu) \
    cycle_desc_t{static_cast<uint8_t>(mem), static_cast<uint8_t>(data), static_cast<uint8_t>(alu)}

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
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bcs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_beq(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bne(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bpl(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bmi(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bvc(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
    }
    
    static constexpr cycle_desc_t make_bvs(uint8_t cycle) {
        if (cycle == 1) return CD_MAKE_SYNC(MemOp::READ_PC_INC, DataOp::BRANCH, AluOp::NOP);
        return CD_MAKE(MemOp::READ_PC, DataOp::NOP, AluOp::NOP);
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
                return 2;
                
            // Zero page instructions - 3 cycles
            case 0xA5:   // LDA $nn
            case 0xA6:   // LDX $nn
            case 0xA4:   // LDY $nn
            case 0x85:   // STA $nn
            case 0xA7:   // LAX $nn (illegal)
                return 3;
                
            // Absolute instructions - 4 cycles
            case 0xAD:   // LDA $nnnn
            case 0x8D:   // STA $nnnn
                return 4;
                
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