#ifndef CYCLE_TABLE_GEN_HPP
#define CYCLE_TABLE_GEN_HPP

#include "cycle_types.hpp"
#include "cycle_validation.hpp"
#include "cycle_instructions.hpp"
#include <array>

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleTables : public CycleInstructions<BusConfig> {
public:
    using inst = CycleInstructions<BusConfig>;
    using addr = CycleAddressing<BusConfig>;

    // === COMPILE-TIME 1D CYCLE TABLE IMPLEMENTATION ===
    
    // Template-based get_cycle method that adapts using template arguments for compile-time generation
    template<uint16_t OpCode, uint8_t Cycle>
    // Use if constexpr for compile-time dispatch
    static constexpr cycle_desc_t get_cycle() {
        switch (OpCode) {
            // For all other opcodes, return empty cycle
            default: return addr::make_empty_cycle();  // Fallback for any missing opcodes
            // Handle virtual opcodes (256, 257, 258)
            case VIRTUAL_OPCODE_RESET: return inst::get_reset_cycle(Cycle);
            case VIRTUAL_OPCODE_NMI: return inst::get_nmi_cycle(Cycle);
            case VIRTUAL_OPCODE_IRQ: return inst::get_irq_cycle(Cycle);
            // Handle regular opcodes (0-255) - COMPLETE 6502 INSTRUCTION SET
            case 0x00: return inst::make_brk(Cycle);                        // BRK impl
            case 0x10: return addr::make_branch(Cycle);                // BPL rel
            case 0x20: return addr::make_jsr(Cycle);                   // JSR abs
            case 0x30: return addr::make_branch(Cycle);                // BMI rel
            case 0x40: return addr::make_rti(Cycle);                   // RTI impl
            case 0x50: return addr::make_branch(Cycle);                // BVC rel
            case 0x60: return addr::make_rts(Cycle);                   // RTS impl
            case 0x70: return addr::make_branch(Cycle);                // BVS rel
            case 0x80:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_branch(Cycle);                // BRA rel (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP #imm (illegal on NMOS)
                }
            case 0x90: return addr::make_branch(Cycle);                // BCC rel
            case 0xA0: return inst::make_ldy_imm(Cycle);               // LDY #imm
            case 0xB0: return addr::make_branch(Cycle);                // BCS rel
            case 0xC0: return inst::make_cpy_imm(Cycle);               // CPY #imm
            case 0xD0: return addr::make_branch(Cycle);                // BNE rel
            case 0xE0: return inst::make_cpx_imm(Cycle);               // CPX #imm
            case 0xF0: return addr::make_branch(Cycle);                // BEQ rel

            // Column 1: Indexed Indirect (zp,X)
            case 0x01: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::ORA);     // ORA (zp,X)
            case 0x11: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::ORA);     // ORA (zp),Y
            case 0x21: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::AND);     // AND (zp,X)
            case 0x31: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::AND);     // AND (zp),Y
            case 0x41: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::EOR);     // EOR (zp,X)
            case 0x51: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::EOR);     // EOR (zp),Y
            case 0x61: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::ADC);     // ADC (zp,X)
            case 0x71: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::ADC);     // ADC (zp),Y
            case 0x81: return addr::make_indexed_indirect_write(Cycle, DataOp::STORE_A);       // STA (zp,X)
            case 0x91: return addr::make_indirect_indexed_write(Cycle, DataOp::STORE_A);       // STA (zp),Y
            case 0xA1: return addr::make_indexed_indirect(Cycle, DataOp::LOAD_A);              // LDA (zp,X)
            case 0xB1: return addr::make_indirect_indexed(Cycle, DataOp::LOAD_A);              // LDA (zp),Y
            case 0xC1: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::CMP);     // CMP (zp,X)
            case 0xD1: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::CMP);     // CMP (zp),Y
            case 0xE1: return addr::make_indexed_indirect(Cycle, DataOp::ALU, AluOp::SBC);     // SBC (zp,X)
            case 0xF1: return addr::make_indirect_indexed(Cycle, DataOp::ALU, AluOp::SBC);     // SBC (zp),Y

            // Column 2: Illegal/Undocumented/65C02 - variant-specific behavior
            case 0x02:
                if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x12:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_ora_zp_ind(Cycle);            // ORA ($zp) (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x22:
                if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x32:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_and_zp_ind(Cycle);            // AND ($zp) (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x42:
            case 0x62:
            case 0x92:
            case 0xD2:
            case 0xF2:
                if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x52:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_eor_zp_ind(Cycle);            // EOR ($zp) (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x72:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_adc_zp_ind(Cycle);            // ADC ($zp) (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0xB2:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_lda_zp_ind(Cycle);            // LDA ($zp) (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // HLT/JAM (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0x82:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_nop(Cycle);                   // Reserved for future use (65C02)
                } else if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // NOP #imm (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0xC2:
            case 0xE2:
                if constexpr (BusConfig::has_illegal_opcodes) {
                    return inst::make_nop(Cycle);                   // NOP #imm (NMOS illegal)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (CMOS)
                }
            case 0xA2: return inst::make_ldx_imm(Cycle);               // LDX #imm (all variants)

            // Column 3: Illegal opcodes (mostly SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x03: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::SLO); // SLO (zp,X) (illegal)
            case 0x13: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::SLO); // SLO (zp),Y (illegal)
            case 0x23: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::RLA); // RLA (zp,X) (illegal)
            case 0x33: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::RLA); // RLA (zp),Y (illegal)
            case 0x43: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::SRE); // SRE (zp,X) (illegal)
            case 0x53: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::SRE); // SRE (zp),Y (illegal)
            case 0x63: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::RRA); // RRA (zp,X) (illegal)
            case 0x73: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::RRA); // RRA (zp),Y (illegal)
            case 0x83: return addr::make_indexed_indirect_write(Cycle, DataOp::ILLEGAL_COMBO);       // SAX (zp,X) (illegal)
            case 0x93: return inst::make_nop(Cycle);                   // AHX (zp),Y (illegal, unstable)
            case 0xA3: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO);             // LAX (zp,X) (illegal)
            case 0xB3: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO);             // LAX (zp),Y (illegal)
            case 0xC3: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::DCP); // DCP (zp,X) (illegal)
            case 0xD3: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::DCP); // DCP (zp),Y (illegal)
            case 0xE3: return addr::make_indexed_indirect(Cycle, DataOp::ILLEGAL_COMBO, AluOp::ISC); // ISC (zp,X) (illegal)
            case 0xF3: return addr::make_indirect_indexed(Cycle, DataOp::ILLEGAL_COMBO, AluOp::ISC); // ISC (zp),Y (illegal)

            // Column 4: Bit test/set/clear and NOP variants
            case 0x04:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_memory_modify_zp(Cycle, AluOp::TSB);                // TSB zp (65C02)
                } else {
                    return addr::make_zeropage(Cycle, DataOp::NOP);                // NOP zp (illegal on NMOS)
                }
            case 0x14:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_memory_modify_zp(Cycle, AluOp::TRB);                // TRB zp (65C02)
                } else {
                    return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP);               // NOP zp,X (illegal on NMOS)
                }
            case 0x24: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::BIT);                // BIT zp
            case 0x34:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT);               // BIT zp,X (65C02)
                } else {
                    return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP);               // NOP zp,X (illegal on NMOS)
                }
            case 0x44: return addr::make_zeropage(Cycle, DataOp::NOP);                // NOP zp (illegal)
            case 0x54: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP);               // NOP zp,X (illegal)
            case 0x64:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_zeropage_write(Cycle, DataOp::STORE_ZERO);                // STZ zp (65C02)
                } else {
                    return addr::make_zeropage(Cycle, DataOp::NOP);                // NOP zp (illegal on NMOS)
                }
            case 0x74:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_zeropage_indexed_write(Cycle, DataOp::ADDR_ADD_X, DataOp::STORE_ZERO);               // STZ zp,X (65C02)
                } else {
                    return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP);               // NOP zp,X (illegal on NMOS)
                }
            case 0x84: return addr::make_zeropage_write(Cycle, DataOp::STORE_Y);              // STY zp
            case 0x94: return addr::make_zeropage_indexed_write(Cycle, DataOp::ADDR_ADD_X, DataOp::STORE_Y); // STY zp,X
            case 0xA4: return addr::make_zeropage(Cycle, DataOp::LOAD_Y);                     // LDY zp
            case 0xB4: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y); // LDY zp,X
            case 0xC4: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::CPY);            // CPY zp
            case 0xD4: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP zp,X (illegal)
            case 0xE4: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::CPX);            // CPX zp
            case 0xF4: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP zp,X (illegal)

            // Column 5: Zero Page
            case 0x05: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::ORA);            // ORA zp
            case 0x15: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA); // ORA zp,X
            case 0x25: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::AND);            // AND zp
            case 0x35: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND); // AND zp,X
            case 0x45: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::EOR);            // EOR zp
            case 0x55: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR); // EOR zp,X
            case 0x65: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::ADC);            // ADC zp
            case 0x75: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC); // ADC zp,X
            case 0x85: return addr::make_zeropage_write(Cycle, DataOp::STORE_A);              // STA zp
            case 0x95: return addr::make_zeropage_indexed_write(Cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A); // STA zp,X
            case 0xA5: return addr::make_zeropage(Cycle, DataOp::LOAD_A);                     // LDA zp
            case 0xB5: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A); // LDA zp,X
            case 0xC5: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::CMP);            // CMP zp
            case 0xD5: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP); // CMP zp,X
            case 0xE5: return addr::make_zeropage(Cycle, DataOp::ALU, AluOp::SBC);            // SBC zp
            case 0xF5: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC); // SBC zp,X
            
            // Column 6: Arithmetic Shift & Rotate
            case 0x06: return addr::make_memory_modify_zp(Cycle, AluOp::ASL);                 // ASL zp
            case 0x16: return addr::make_memory_modify_zp(Cycle, AluOp::ASL);                 // ASL zp,X
            case 0x26: return addr::make_memory_modify_zp(Cycle, AluOp::ROL);                 // ROL zp
            case 0x36: return addr::make_memory_modify_zp(Cycle, AluOp::ROL);                 // ROL zp,X
            case 0x46: return addr::make_memory_modify_zp(Cycle, AluOp::LSR);                 // LSR zp
            case 0x56: return addr::make_memory_modify_zp(Cycle, AluOp::LSR);                 // LSR zp,X
            case 0x66: return addr::make_memory_modify_zp(Cycle, AluOp::ROR);                 // ROR zp
            case 0x76: return addr::make_memory_modify_zp(Cycle, AluOp::ROR);                 // ROR zp,X
            case 0x86: return addr::make_zeropage_write(Cycle, DataOp::STORE_X);              // STX zp
            case 0x96: return addr::make_zeropage_indexed_write(Cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_X); // STX zp,Y
            case 0xA6: return addr::make_zeropage(Cycle, DataOp::LOAD_X);                     // LDX zp
            case 0xB6: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X); // LDX zp,Y
            case 0xC6: return addr::make_memory_modify_zp(Cycle, AluOp::DEC);                 // DEC zp
            case 0xD6: return addr::make_memory_modify_zp(Cycle, AluOp::DEC);                 // DEC zp,X
            case 0xE6: return addr::make_memory_modify_zp(Cycle, AluOp::INC);                 // INC zp
            case 0xF6: return addr::make_memory_modify_zp(Cycle, AluOp::INC);                 // INC zp,X
            
            // Column 7: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x07: return addr::make_memory_modify_zp(Cycle, AluOp::SLO);               // SLO zp (illegal)
            case 0x17: return addr::make_memory_modify_zp(Cycle, AluOp::SLO);               // SLO zp,X (illegal)
            case 0x27: return addr::make_memory_modify_zp(Cycle, AluOp::RLA);               // RLA zp (illegal)
            case 0x37: return addr::make_memory_modify_zp(Cycle, AluOp::RLA);               // RLA zp,X (illegal)
            case 0x47: return addr::make_memory_modify_zp(Cycle, AluOp::SRE);               // SRE zp (illegal)
            case 0x57: return addr::make_memory_modify_zp(Cycle, AluOp::SRE);               // SRE zp,X (illegal)
            case 0x67: return addr::make_memory_modify_zp(Cycle, AluOp::RRA);               // RRA zp (illegal)
            case 0x77: return addr::make_memory_modify_zp(Cycle, AluOp::RRA);               // RRA zp,X (illegal)
            case 0x87: return addr::make_zeropage_write(Cycle, DataOp::ILLEGAL_COMBO);      // SAX zp (illegal)
            case 0x97: return addr::make_zeropage_indexed_write(Cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO); // SAX zp,Y (illegal)
            case 0xA7: return addr::make_zeropage(Cycle, DataOp::ILLEGAL_COMBO);            // LAX zp (illegal)
            case 0xB7: return addr::make_zeropage_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO); // LAX zp,Y (illegal)
            case 0xC7: return addr::make_memory_modify_zp(Cycle, AluOp::DCP);               // DCP zp (illegal)
            case 0xD7: return addr::make_memory_modify_zp(Cycle, AluOp::DCP);               // DCP zp,X (illegal)
            case 0xE7: return addr::make_memory_modify_zp(Cycle, AluOp::ISC);               // ISC zp (illegal)
            case 0xF7: return addr::make_memory_modify_zp(Cycle, AluOp::ISC);               // ISC zp,X (illegal)
            
            // Column 8: Stack/Status Operations
            case 0x08: return addr::make_stack_push(Cycle, DataOp::STACK_PUSH);              // PHP impl
            case 0x18: return inst::make_clc(Cycle);                   // CLC impl
            case 0x28: return addr::make_stack_pull(Cycle, DataOp::STACK_PULL);              // PLP impl
            case 0x38: return inst::make_sec(Cycle);                   // SEC impl
            case 0x48: return addr::make_stack_push(Cycle, DataOp::STORE_A);                 // PHA impl
            case 0x58: return inst::make_cli(Cycle);                   // CLI impl
            case 0x68: return addr::make_stack_pull(Cycle, DataOp::LOAD_A);                  // PLA impl
            case 0x78: return inst::make_sei(Cycle);                   // SEI impl
            case 0x88: return inst::make_dey(Cycle);                   // DEY impl
            case 0x98: return inst::make_tya(Cycle);                   // TYA impl
            case 0xA8: return inst::make_tay(Cycle);                   // TAY impl
            case 0xB8: return inst::make_clv(Cycle);                   // CLV impl
            case 0xC8: return inst::make_iny(Cycle);                   // INY impl
            case 0xD8: return inst::make_cld(Cycle);                   // CLD impl
            case 0xE8: return inst::make_inx(Cycle);                   // INX impl
            case 0xF8: return inst::make_sed(Cycle);                   // SED impl
            
            // Column 9: Immediate
            case 0x09: return inst::make_ora_imm(Cycle);               // ORA #imm
            case 0x19: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ORA); // ORA abs,Y
            case 0x29: return inst::make_and_imm(Cycle);               // AND #imm
            case 0x39: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::AND); // AND abs,Y
            case 0x49: return inst::make_eor_imm(Cycle);               // EOR #imm
            case 0x59: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::EOR); // EOR abs,Y
            case 0x69: return inst::make_adc_imm(Cycle);               // ADC #imm
            case 0x79: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::ADC); // ADC abs,Y
            case 0x89:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_bit_imm(Cycle);               // BIT #imm (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP #imm (illegal)
                }
            case 0x99: return addr::make_absolute_indexed_write(Cycle, DataOp::ADDR_ADD_Y, DataOp::STORE_A); // STA abs,Y
            case 0xA9: return inst::make_lda_imm(Cycle);               // LDA #imm
            case 0xB9: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_A); // LDA abs,Y
            case 0xC9: return inst::make_cmp_imm(Cycle);               // CMP #imm
            case 0xD9: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::CMP); // CMP abs,Y
            case 0xE9: return inst::make_sbc_imm(Cycle);               // SBC #imm
            case 0xF9: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ALU, AluOp::SBC); // SBC abs,Y
            
            // Column A: Accumulator & Implied
            case 0x0A: return inst::make_asl_acc(Cycle);               // ASL A
            case 0x1A:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_inc_acc(Cycle);               // INC A (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP impl (illegal)
                }
            case 0x2A: return inst::make_rol_acc(Cycle);               // ROL A
            case 0x3A:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_dec_acc(Cycle);               // DEC A (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP impl (illegal)
                }
            case 0x4A: return inst::make_lsr_acc(Cycle);               // LSR A
            case 0x5A:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_phy(Cycle);                   // PHY impl (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (illegal on NMOS)
                }
            case 0x6A: return inst::make_ror_acc(Cycle);               // ROR A
            case 0x7A:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_ply(Cycle);                   // PLY impl (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (illegal on NMOS)
                }
            case 0x8A: return inst::make_txa(Cycle);                   // TXA impl
            case 0x9A: return inst::make_txs(Cycle);                   // TXS impl
            case 0xAA: return inst::make_tax(Cycle);                   // TAX impl
            case 0xBA: return inst::make_tsx(Cycle);                   // TSX impl
            case 0xCA: return inst::make_dex(Cycle);                   // DEX impl
            case 0xDA:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_phx(Cycle);                   // PHX impl (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (illegal on NMOS)
                }
            case 0xEA: return inst::make_nop(Cycle);                   // NOP impl
            case 0xFA:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_plx(Cycle);                   // PLX impl (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // NOP (illegal on NMOS)
                }
            
            // Column B: Illegal opcodes (mostly unstable)
            case 0x0B: return inst::make_nop(Cycle);                   // ANC #imm (illegal, unstable)
            case 0x1B: return inst::make_nop(Cycle);                   // SLO abs,Y (illegal)
            case 0x2B: return inst::make_nop(Cycle);                   // ANC #imm (illegal, unstable)
            case 0x3B: return inst::make_nop(Cycle);                   // RLA abs,Y (illegal)
            case 0x4B: return inst::make_nop(Cycle);                   // ALR #imm (illegal, unstable)
            case 0x5B: return inst::make_nop(Cycle);                   // SRE abs,Y (illegal)
            case 0x6B: return inst::make_nop(Cycle);                   // ARR #imm (illegal, unstable)
            case 0x7B: return inst::make_nop(Cycle);                   // RRA abs,Y (illegal)
            case 0x8B: return inst::make_nop(Cycle);                   // XAA #imm (illegal, highly unstable)
            case 0x9B: return inst::make_nop(Cycle);                   // TAS abs,Y (illegal, unstable)
            case 0xAB: return inst::make_nop(Cycle);                   // LAX #imm (illegal, unstable)
            case 0xBB: return inst::make_nop(Cycle);                   // LAS abs,Y (illegal, unstable)
            case 0xCB: return inst::make_nop(Cycle);                   // AXS #imm (illegal, unstable)
            case 0xDB: return inst::make_nop(Cycle);                   // DCP abs,Y (illegal)
            case 0xEB: return inst::make_nop(Cycle);                   // SBC #imm (illegal, same as legal E9)
            case 0xFB: return inst::make_nop(Cycle);                   // ISC abs,Y (illegal)
            
            // Column C: Absolute addressing & Jump
            case 0x0C:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_memory_modify_abs(Cycle, AluOp::TSB);               // TSB abs (65C02)
                } else {
                    return addr::make_absolute(Cycle, DataOp::NOP);               // NOP abs (illegal on NMOS)
                }
            case 0x1C:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_memory_modify_abs(Cycle, AluOp::TRB);               // TRB abs (65C02)
                } else {
                    return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP);              // NOP abs,X (illegal on NMOS)
                }
            case 0x2C: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::BIT);           // BIT abs
            case 0x3C:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::BIT); // BIT abs,X (65C02)
                } else {
                    return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP abs,X (illegal on NMOS)
                }
            case 0x4C: return addr::make_jump_absolute(Cycle);          // JMP abs
            case 0x5C: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP abs,X (illegal)
            case 0x6C: return addr::make_jump_indirect(Cycle);          // JMP (abs)
            case 0x7C:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return inst::make_jmp_absx_ind(Cycle);          // JMP (abs,X) (65C02)
                } else {
                    return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP abs,X (illegal on NMOS)
                }
            case 0x8C: return addr::make_absolute_write(Cycle, DataOp::STORE_Y);             // STY abs
            case 0x9C:
                if constexpr (BusConfig::has_cmos_fixes) {
                    return addr::make_absolute_write(Cycle, DataOp::STORE_ZERO);               // STZ abs (65C02)
                } else {
                    return inst::make_nop(Cycle);                   // SHY abs,X (illegal on NMOS, unstable)
                }
            case 0xAC: return addr::make_absolute(Cycle, DataOp::LOAD_Y);                    // LDY abs
            case 0xBC: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_Y); // LDY abs,X
            case 0xCC: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::CPY);           // CPY abs
            case 0xDC: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP abs,X (illegal)
            case 0xEC: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::CPX);           // CPX abs
            case 0xFC: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::NOP); // NOP abs,X (illegal)
            
            // Column D: Absolute
            case 0x0D: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::ORA);          // ORA abs
            case 0x1D: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ORA); // ORA abs,X
            case 0x2D: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::AND);          // AND abs
            case 0x3D: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::AND); // AND abs,X
            case 0x4D: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::EOR);          // EOR abs
            case 0x5D: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::EOR); // EOR abs,X
            case 0x6D: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::ADC);          // ADC abs
            case 0x7D: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::ADC); // ADC abs,X
            case 0x8D: return addr::make_absolute_write(Cycle, DataOp::STORE_A);            // STA abs
            case 0x9D: return addr::make_absolute_indexed_write(Cycle, DataOp::ADDR_ADD_X, DataOp::STORE_A); // STA abs,X
            case 0xAD: return addr::make_absolute(Cycle, DataOp::LOAD_A);                   // LDA abs
            case 0xBD: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::LOAD_A); // LDA abs,X
            case 0xCD: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::CMP);          // CMP abs
            case 0xDD: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::CMP); // CMP abs,X
            case 0xED: return addr::make_absolute(Cycle, DataOp::ALU, AluOp::SBC);          // SBC abs
            case 0xFD: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_X, DataOp::ALU, AluOp::SBC); // SBC abs,X
            
            // Column E: Absolute with shifts
            case 0x0E: return addr::make_memory_modify_abs(Cycle, AluOp::ASL);              // ASL abs
            case 0x1E: return addr::make_memory_modify_abs(Cycle, AluOp::ASL);              // ASL abs,X
            case 0x2E: return addr::make_memory_modify_abs(Cycle, AluOp::ROL);              // ROL abs
            case 0x3E: return addr::make_memory_modify_abs(Cycle, AluOp::ROL);              // ROL abs,X
            case 0x4E: return addr::make_memory_modify_abs(Cycle, AluOp::LSR);              // LSR abs
            case 0x5E: return addr::make_memory_modify_abs(Cycle, AluOp::LSR);              // LSR abs,X
            case 0x6E: return addr::make_memory_modify_abs(Cycle, AluOp::ROR);              // ROR abs
            case 0x7E: return addr::make_memory_modify_abs(Cycle, AluOp::ROR);              // ROR abs,X
            case 0x8E: return addr::make_absolute_write(Cycle, DataOp::STORE_X);            // STX abs
            case 0x9E: return inst::make_nop(Cycle);                   // SHX abs,Y (illegal, unstable)
            case 0xAE: return addr::make_absolute(Cycle, DataOp::LOAD_X);                   // LDX abs
            case 0xBE: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::LOAD_X); // LDX abs,Y
            case 0xCE: return addr::make_memory_modify_abs(Cycle, AluOp::DEC);              // DEC abs
            case 0xDE: return addr::make_memory_modify_abs(Cycle, AluOp::DEC);              // DEC abs,X
            case 0xEE: return addr::make_memory_modify_abs(Cycle, AluOp::INC);              // INC abs
            case 0xFE: return addr::make_memory_modify_abs(Cycle, AluOp::INC);              // INC abs,X
            
            // Column F: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x0F: return addr::make_memory_modify_abs(Cycle, AluOp::SLO);               // SLO abs (illegal)
            case 0x1F: return addr::make_memory_modify_abs(Cycle, AluOp::SLO);              // SLO abs,X (illegal)
            case 0x2F: return addr::make_memory_modify_abs(Cycle, AluOp::RLA);               // RLA abs (illegal)
            case 0x3F: return addr::make_memory_modify_abs(Cycle, AluOp::RLA);              // RLA abs,X (illegal)
            case 0x4F: return addr::make_memory_modify_abs(Cycle, AluOp::SRE);               // SRE abs (illegal)
            case 0x5F: return addr::make_memory_modify_abs(Cycle, AluOp::SRE);              // SRE abs,X (illegal)
            case 0x6F: return addr::make_memory_modify_abs(Cycle, AluOp::RRA);               // RRA abs (illegal)
            case 0x7F: return addr::make_memory_modify_abs(Cycle, AluOp::RRA);              // RRA abs,X (illegal)
            case 0x8F: return addr::make_absolute_write(Cycle, DataOp::ILLEGAL_COMBO);               // SAX abs (illegal)
            case 0x9F: return inst::make_nop(Cycle);                   // AHX abs,Y (illegal, unstable)
            case 0xAF: return addr::make_absolute(Cycle, DataOp::ILLEGAL_COMBO);               // LAX abs (illegal)
            case 0xBF: return addr::make_absolute_indexed(Cycle, DataOp::ADDR_ADD_Y, DataOp::ILLEGAL_COMBO);              // LAX abs,Y (illegal)
            case 0xCF: return addr::make_memory_modify_abs(Cycle, AluOp::DCP);               // DCP abs (illegal)
            case 0xDF: return addr::make_memory_modify_abs(Cycle, AluOp::DCP);              // DCP abs,X (illegal)
            case 0xEF: return addr::make_memory_modify_abs(Cycle, AluOp::ISC);               // ISC abs (illegal)
            case 0xFF: return addr::make_memory_modify_abs(Cycle, AluOp::ISC);              // ISC abs,X (illegal)
       }
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
            return addr::make_empty_cycle();
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

#endif // CYCLE_TABLE_GEN_HPP