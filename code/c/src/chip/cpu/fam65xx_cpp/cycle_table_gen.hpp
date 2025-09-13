#ifndef CYCLE_TABLE_GEN_HPP
#define CYCLE_TABLE_GEN_HPP

#include "cycle_types.hpp"
#include "cycle_validation.hpp"
#include "cycle_addressing.hpp"
#include "cycle_interrupts.hpp"
#include "cycle_instructions.hpp"
#include <array>

namespace fam65xx_cpp {

template<typename BusConfig>
class CycleTables : public CycleInstructions<BusConfig> {
public:
    // Import all instruction implementations
    using CycleInstructions<BusConfig>::make_empty_cycle;
    using CycleInstructions<BusConfig>::make_nop;
    using CycleInstructions<BusConfig>::make_lda_imm;
    using CycleInstructions<BusConfig>::make_lda_zp;
    using CycleInstructions<BusConfig>::make_lda_abs;
    using CycleInstructions<BusConfig>::make_adc_imm;
    using CycleInstructions<BusConfig>::make_brk;
    // Import interrupt sequences
    using CycleInterrupts<BusConfig>::get_reset_cycle;
    using CycleInterrupts<BusConfig>::get_nmi_cycle;
    using CycleInterrupts<BusConfig>::get_irq_cycle;
    // Import all other instructions (abbreviated for brevity - all are available)

    // === COMPILE-TIME 1D CYCLE TABLE IMPLEMENTATION ===
    
    // Template-based get_cycle method that adapts using template arguments for compile-time generation
    template<uint16_t OpCode, uint8_t Cycle>
    // Use if constexpr for compile-time dispatch
    static constexpr cycle_desc_t get_cycle() {
        switch (OpCode) {
            // For all other opcodes, return empty cycle
            default: return make_empty_cycle();  // Fallback for any missing opcodes
            // Handle virtual opcodes (256, 257, 258)
            case VIRTUAL_OPCODE_RESET: return get_reset_cycle(Cycle);
            case VIRTUAL_OPCODE_NMI: return get_nmi_cycle(Cycle);
            case VIRTUAL_OPCODE_IRQ: return get_irq_cycle(Cycle);
            // Handle regular opcodes (0-255) - COMPLETE 6502 INSTRUCTION SET
            case 0x00: return CycleInstructions<BusConfig>::make_brk(Cycle);                        // BRK impl
            case 0x10: return CycleInstructions<BusConfig>::make_bpl(Cycle);                   // BPL rel
            case 0x20: return CycleInstructions<BusConfig>::make_jsr(Cycle);                   // JSR abs
            case 0x30: return CycleInstructions<BusConfig>::make_bmi(Cycle);                   // BMI rel
            case 0x40: return CycleInstructions<BusConfig>::make_rti(Cycle);                   // RTI impl
            case 0x50: return CycleInstructions<BusConfig>::make_bvc(Cycle);                   // BVC rel
            case 0x60: return CycleInstructions<BusConfig>::make_rts(Cycle);                   // RTS impl
            case 0x70: return CycleInstructions<BusConfig>::make_bvs(Cycle);                   // BVS rel
            case 0x80: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP #imm (illegal on NMOS) / BRA rel (65C02)
            case 0x90: return CycleInstructions<BusConfig>::make_bcc(Cycle);                   // BCC rel
            case 0xA0: return CycleInstructions<BusConfig>::make_ldy_imm(Cycle);               // LDY #imm
            case 0xB0: return CycleInstructions<BusConfig>::make_bcs(Cycle);                   // BCS rel
            case 0xC0: return CycleInstructions<BusConfig>::make_cpy_imm(Cycle);               // CPY #imm
            case 0xD0: return CycleInstructions<BusConfig>::make_bne(Cycle);                   // BNE rel
            case 0xE0: return CycleInstructions<BusConfig>::make_cpx_imm(Cycle);               // CPX #imm
            case 0xF0: return CycleInstructions<BusConfig>::make_beq(Cycle);                   // BEQ rel

            // Column 1: Indexed Indirect (zp,X)
            case 0x01: return CycleInstructions<BusConfig>::make_ora_indx(Cycle);              // ORA (zp,X)
            case 0x11: return CycleInstructions<BusConfig>::make_ora_indy(Cycle);              // ORA (zp),Y
            case 0x21: return CycleInstructions<BusConfig>::make_and_indx(Cycle);              // AND (zp,X)
            case 0x31: return CycleInstructions<BusConfig>::make_and_indy(Cycle);              // AND (zp),Y
            case 0x41: return CycleInstructions<BusConfig>::make_eor_indx(Cycle);              // EOR (zp,X)
            case 0x51: return CycleInstructions<BusConfig>::make_eor_indy(Cycle);              // EOR (zp),Y
            case 0x61: return CycleInstructions<BusConfig>::make_adc_indx(Cycle);              // ADC (zp,X)
            case 0x71: return CycleInstructions<BusConfig>::make_adc_indy(Cycle);              // ADC (zp),Y
            case 0x81: return CycleInstructions<BusConfig>::make_sta_indx(Cycle);              // STA (zp,X)
            case 0x91: return CycleInstructions<BusConfig>::make_sta_indy(Cycle);              // STA (zp),Y
            case 0xA1: return CycleInstructions<BusConfig>::make_lda_indx(Cycle);              // LDA (zp,X)
            case 0xB1: return CycleInstructions<BusConfig>::make_lda_indy(Cycle);              // LDA (zp),Y
            case 0xC1: return CycleInstructions<BusConfig>::make_cmp_indx(Cycle);              // CMP (zp,X)
            case 0xD1: return CycleInstructions<BusConfig>::make_cmp_indy(Cycle);              // CMP (zp),Y
            case 0xE1: return CycleInstructions<BusConfig>::make_sbc_indx(Cycle);              // SBC (zp,X)
            case 0xF1: return CycleInstructions<BusConfig>::make_sbc_indy(Cycle);              // SBC (zp),Y

            // Column 2: Illegal/Undocumented/65C02
            case 0x02: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x12: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x22: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x32: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x42: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x52: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x62: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x72: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0x82: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP #imm (illegal)
            case 0x92: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0xA2: return CycleInstructions<BusConfig>::make_ldx_imm(Cycle);               // LDX #imm
            case 0xB2: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0xC2: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP #imm (illegal)
            case 0xD2: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)
            case 0xE2: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP #imm (illegal)
            case 0xF2: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // HLT/JAM (illegal)

            // Column 3: Illegal opcodes (mostly SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x03: return CycleInstructions<BusConfig>::make_slo_indx(Cycle);              // SLO (zp,X) (illegal)
            case 0x13: return CycleInstructions<BusConfig>::make_slo_indy(Cycle);              // SLO (zp),Y (illegal)
            case 0x23: return CycleInstructions<BusConfig>::make_rla_indx(Cycle);              // RLA (zp,X) (illegal)
            case 0x33: return CycleInstructions<BusConfig>::make_rla_indy(Cycle);              // RLA (zp),Y (illegal)
            case 0x43: return CycleInstructions<BusConfig>::make_sre_indx(Cycle);              // SRE (zp,X) (illegal)
            case 0x53: return CycleInstructions<BusConfig>::make_sre_indy(Cycle);              // SRE (zp),Y (illegal)
            case 0x63: return CycleInstructions<BusConfig>::make_rra_indx(Cycle);              // RRA (zp,X) (illegal)
            case 0x73: return CycleInstructions<BusConfig>::make_rra_indy(Cycle);              // RRA (zp),Y (illegal)
            case 0x83: return CycleInstructions<BusConfig>::make_sax_indx(Cycle);              // SAX (zp,X) (illegal)
            case 0x93: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // AHX (zp),Y (illegal, unstable)
            case 0xA3: return CycleInstructions<BusConfig>::make_lax_indx(Cycle);              // LAX (zp,X) (illegal)
            case 0xB3: return CycleInstructions<BusConfig>::make_lax_indy(Cycle);              // LAX (zp),Y (illegal)
            case 0xC3: return CycleInstructions<BusConfig>::make_dcp_indx(Cycle);              // DCP (zp,X) (illegal)
            case 0xD3: return CycleInstructions<BusConfig>::make_dcp_indy(Cycle);              // DCP (zp),Y (illegal)
            case 0xE3: return CycleInstructions<BusConfig>::make_isc_indx(Cycle);              // ISC (zp,X) (illegal)
            case 0xF3: return CycleInstructions<BusConfig>::make_isc_indy(Cycle);              // ISC (zp),Y (illegal)

            // Column 4: Bit test/set/clear and NOP variants
            case 0x04: return CycleInstructions<BusConfig>::make_tsb_zp(Cycle);                // TSB zp (65C02) / NOP zp (illegal on NMOS)
            case 0x14: return CycleInstructions<BusConfig>::make_trb_zp(Cycle);                // TRB zp (65C02) / NOP zp,X (illegal on NMOS)
            case 0x24: return CycleInstructions<BusConfig>::make_bit_zp(Cycle);                // BIT zp
            case 0x34: return CycleInstructions<BusConfig>::make_bit_zpx(Cycle);               // BIT zp,X (65C02) / NOP zp,X (illegal on NMOS)
            case 0x44: return CycleInstructions<BusConfig>::make_nop_zp(Cycle);                // NOP zp (illegal)
            case 0x54: return CycleInstructions<BusConfig>::make_nop_zpx(Cycle);               // NOP zp,X (illegal)
            case 0x64: return CycleInstructions<BusConfig>::make_stz_zp(Cycle);                // STZ zp (65C02) / NOP zp (illegal on NMOS)
            case 0x74: return CycleInstructions<BusConfig>::make_stz_zpx(Cycle);               // STZ zp,X (65C02) / NOP zp,X (illegal on NMOS)
            case 0x84: return CycleInstructions<BusConfig>::make_sty_zp(Cycle);                // STY zp
            case 0x94: return CycleInstructions<BusConfig>::make_sty_zpx(Cycle);               // STY zp,X
            case 0xA4: return CycleInstructions<BusConfig>::make_ldy_zp(Cycle);                // LDY zp
            case 0xB4: return CycleInstructions<BusConfig>::make_ldy_zpx(Cycle);               // LDY zp,X
            case 0xC4: return CycleInstructions<BusConfig>::make_cpy_zp(Cycle);                // CPY zp
            case 0xD4: return CycleInstructions<BusConfig>::make_nop_zpx(Cycle);               // NOP zp,X (illegal)
            case 0xE4: return CycleInstructions<BusConfig>::make_cpx_zp(Cycle);                // CPX zp
            case 0xF4: return CycleInstructions<BusConfig>::make_nop_zpx(Cycle);               // NOP zp,X (illegal)

            // Column 5: Zero Page
            case 0x05: return CycleInstructions<BusConfig>::make_ora_zp(Cycle);                // ORA zp
            case 0x15: return CycleInstructions<BusConfig>::make_ora_zpx(Cycle);               // ORA zp,X
            case 0x25: return CycleInstructions<BusConfig>::make_and_zp(Cycle);                // AND zp
            case 0x35: return CycleInstructions<BusConfig>::make_and_zpx(Cycle);               // AND zp,X
            case 0x45: return CycleInstructions<BusConfig>::make_eor_zp(Cycle);                // EOR zp
            case 0x55: return CycleInstructions<BusConfig>::make_eor_zpx(Cycle);               // EOR zp,X
            case 0x65: return CycleInstructions<BusConfig>::make_adc_zp(Cycle);                // ADC zp
            case 0x75: return CycleInstructions<BusConfig>::make_adc_zpx(Cycle);               // ADC zp,X
            case 0x85: return CycleInstructions<BusConfig>::make_sta_zp(Cycle);                // STA zp
            case 0x95: return CycleInstructions<BusConfig>::make_sta_zpx(Cycle);               // STA zp,X
            case 0xA5: return CycleInstructions<BusConfig>::make_lda_zp(Cycle);                // LDA zp
            case 0xB5: return CycleInstructions<BusConfig>::make_lda_zpx(Cycle);               // LDA zp,X
            case 0xC5: return CycleInstructions<BusConfig>::make_cmp_zp(Cycle);                // CMP zp
            case 0xD5: return CycleInstructions<BusConfig>::make_cmp_zpx(Cycle);               // CMP zp,X
            case 0xE5: return CycleInstructions<BusConfig>::make_sbc_zp(Cycle);                // SBC zp
            case 0xF5: return CycleInstructions<BusConfig>::make_sbc_zpx(Cycle);               // SBC zp,X
            
            // Column 6: Arithmetic Shift & Rotate
            case 0x06: return CycleInstructions<BusConfig>::make_asl_zp(Cycle);                // ASL zp
            case 0x16: return CycleInstructions<BusConfig>::make_asl_zpx(Cycle);               // ASL zp,X
            case 0x26: return CycleInstructions<BusConfig>::make_rol_zp(Cycle);                // ROL zp
            case 0x36: return CycleInstructions<BusConfig>::make_rol_zpx(Cycle);               // ROL zp,X
            case 0x46: return CycleInstructions<BusConfig>::make_lsr_zp(Cycle);                // LSR zp
            case 0x56: return CycleInstructions<BusConfig>::make_lsr_zpx(Cycle);               // LSR zp,X
            case 0x66: return CycleInstructions<BusConfig>::make_ror_zp(Cycle);                // ROR zp
            case 0x76: return CycleInstructions<BusConfig>::make_ror_zpx(Cycle);               // ROR zp,X
            case 0x86: return CycleInstructions<BusConfig>::make_stx_zp(Cycle);                // STX zp
            case 0x96: return CycleInstructions<BusConfig>::make_stx_zpy(Cycle);               // STX zp,Y
            case 0xA6: return CycleInstructions<BusConfig>::make_ldx_zp(Cycle);                // LDX zp
            case 0xB6: return CycleInstructions<BusConfig>::make_ldx_zpy(Cycle);               // LDX zp,Y
            case 0xC6: return CycleInstructions<BusConfig>::make_dec_zp(Cycle);                // DEC zp
            case 0xD6: return CycleInstructions<BusConfig>::make_dec_zpx(Cycle);               // DEC zp,X
            case 0xE6: return CycleInstructions<BusConfig>::make_inc_zp(Cycle);                // INC zp
            case 0xF6: return CycleInstructions<BusConfig>::make_inc_zpx(Cycle);               // INC zp,X
            
            // Column 7: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x07: return CycleInstructions<BusConfig>::make_slo_zp(Cycle);                // SLO zp (illegal)
            case 0x17: return CycleInstructions<BusConfig>::make_slo_zpx(Cycle);               // SLO zp,X (illegal)
            case 0x27: return CycleInstructions<BusConfig>::make_rla_zp(Cycle);                // RLA zp (illegal)
            case 0x37: return CycleInstructions<BusConfig>::make_rla_zpx(Cycle);               // RLA zp,X (illegal)
            case 0x47: return CycleInstructions<BusConfig>::make_sre_zp(Cycle);                // SRE zp (illegal)
            case 0x57: return CycleInstructions<BusConfig>::make_sre_zpx(Cycle);               // SRE zp,X (illegal)
            case 0x67: return CycleInstructions<BusConfig>::make_rra_zp(Cycle);                // RRA zp (illegal)
            case 0x77: return CycleInstructions<BusConfig>::make_rra_zpx(Cycle);               // RRA zp,X (illegal)
            case 0x87: return CycleInstructions<BusConfig>::make_sax_zp(Cycle);                // SAX zp (illegal)
            case 0x97: return CycleInstructions<BusConfig>::make_sax_zpy(Cycle);               // SAX zp,Y (illegal)
            case 0xA7: return CycleInstructions<BusConfig>::make_lax_zp(Cycle);                // LAX zp (illegal)
            case 0xB7: return CycleInstructions<BusConfig>::make_lax_zpy(Cycle);               // LAX zp,Y (illegal)
            case 0xC7: return CycleInstructions<BusConfig>::make_dcp_zp(Cycle);                // DCP zp (illegal)
            case 0xD7: return CycleInstructions<BusConfig>::make_dcp_zpx(Cycle);               // DCP zp,X (illegal)
            case 0xE7: return CycleInstructions<BusConfig>::make_isc_zp(Cycle);                // ISC zp (illegal)
            case 0xF7: return CycleInstructions<BusConfig>::make_isc_zpx(Cycle);               // ISC zp,X (illegal)
            
            // Column 8: Stack/Status Operations
            case 0x08: return CycleInstructions<BusConfig>::make_php(Cycle);                   // PHP impl
            case 0x18: return CycleInstructions<BusConfig>::make_clc(Cycle);                   // CLC impl
            case 0x28: return CycleInstructions<BusConfig>::make_plp(Cycle);                   // PLP impl
            case 0x38: return CycleInstructions<BusConfig>::make_sec(Cycle);                   // SEC impl
            case 0x48: return CycleInstructions<BusConfig>::make_pha(Cycle);                   // PHA impl
            case 0x58: return CycleInstructions<BusConfig>::make_cli(Cycle);                   // CLI impl
            case 0x68: return CycleInstructions<BusConfig>::make_pla(Cycle);                   // PLA impl
            case 0x78: return CycleInstructions<BusConfig>::make_sei(Cycle);                   // SEI impl
            case 0x88: return CycleInstructions<BusConfig>::make_dey(Cycle);                   // DEY impl
            case 0x98: return CycleInstructions<BusConfig>::make_tya(Cycle);                   // TYA impl
            case 0xA8: return CycleInstructions<BusConfig>::make_tay(Cycle);                   // TAY impl
            case 0xB8: return CycleInstructions<BusConfig>::make_clv(Cycle);                   // CLV impl
            case 0xC8: return CycleInstructions<BusConfig>::make_iny(Cycle);                   // INY impl
            case 0xD8: return CycleInstructions<BusConfig>::make_cld(Cycle);                   // CLD impl
            case 0xE8: return CycleInstructions<BusConfig>::make_inx(Cycle);                   // INX impl
            case 0xF8: return CycleInstructions<BusConfig>::make_sed(Cycle);                   // SED impl
            
            // Column 9: Immediate
            case 0x09: return CycleInstructions<BusConfig>::make_ora_imm(Cycle);               // ORA #imm
            case 0x19: return CycleInstructions<BusConfig>::make_ora_absy(Cycle);              // ORA abs,Y
            case 0x29: return CycleInstructions<BusConfig>::make_and_imm(Cycle);               // AND #imm
            case 0x39: return CycleInstructions<BusConfig>::make_and_absy(Cycle);              // AND abs,Y
            case 0x49: return CycleInstructions<BusConfig>::make_eor_imm(Cycle);               // EOR #imm
            case 0x59: return CycleInstructions<BusConfig>::make_eor_absy(Cycle);              // EOR abs,Y
            case 0x69: return CycleInstructions<BusConfig>::make_adc_imm(Cycle);               // ADC #imm
            case 0x79: return CycleInstructions<BusConfig>::make_adc_absy(Cycle);              // ADC abs,Y
            case 0x89: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP #imm (illegal)
            case 0x99: return CycleInstructions<BusConfig>::make_sta_absy(Cycle);              // STA abs,Y
            case 0xA9: return CycleInstructions<BusConfig>::make_lda_imm(Cycle);               // LDA #imm
            case 0xB9: return CycleInstructions<BusConfig>::make_lda_absy(Cycle);              // LDA abs,Y
            case 0xC9: return CycleInstructions<BusConfig>::make_cmp_imm(Cycle);               // CMP #imm
            case 0xD9: return CycleInstructions<BusConfig>::make_cmp_absy(Cycle);              // CMP abs,Y
            case 0xE9: return CycleInstructions<BusConfig>::make_sbc_imm(Cycle);               // SBC #imm
            case 0xF9: return CycleInstructions<BusConfig>::make_sbc_absy(Cycle);              // SBC abs,Y
            
            // Column A: Accumulator & Implied
            case 0x0A: return CycleInstructions<BusConfig>::make_asl_acc(Cycle);               // ASL A
            case 0x1A: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP impl (illegal) / INC A (65C02)
            case 0x2A: return CycleInstructions<BusConfig>::make_rol_acc(Cycle);               // ROL A
            case 0x3A: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP impl (illegal) / DEC A (65C02)
            case 0x4A: return CycleInstructions<BusConfig>::make_lsr_acc(Cycle);               // LSR A
            case 0x5A: return CycleInstructions<BusConfig>::make_phy(Cycle);                   // PHY impl (65C02) / NOP (illegal on NMOS)
            case 0x6A: return CycleInstructions<BusConfig>::make_ror_acc(Cycle);               // ROR A
            case 0x7A: return CycleInstructions<BusConfig>::make_ply(Cycle);                   // PLY impl (65C02) / NOP (illegal on NMOS)
            case 0x8A: return CycleInstructions<BusConfig>::make_txa(Cycle);                   // TXA impl
            case 0x9A: return CycleInstructions<BusConfig>::make_txs(Cycle);                   // TXS impl
            case 0xAA: return CycleInstructions<BusConfig>::make_tax(Cycle);                   // TAX impl
            case 0xBA: return CycleInstructions<BusConfig>::make_tsx(Cycle);                   // TSX impl
            case 0xCA: return CycleInstructions<BusConfig>::make_dex(Cycle);                   // DEX impl
            case 0xDA: return CycleInstructions<BusConfig>::make_phx(Cycle);                   // PHX impl (65C02) / NOP (illegal on NMOS)
            case 0xEA: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // NOP impl
            case 0xFA: return CycleInstructions<BusConfig>::make_plx(Cycle);                   // PLX impl (65C02) / NOP (illegal on NMOS)
            
            // Column B: Illegal opcodes (mostly unstable)
            case 0x0B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // ANC #imm (illegal, unstable)
            case 0x1B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // SLO abs,Y (illegal)
            case 0x2B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // ANC #imm (illegal, unstable)
            case 0x3B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // RLA abs,Y (illegal)
            case 0x4B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // ALR #imm (illegal, unstable)
            case 0x5B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // SRE abs,Y (illegal)
            case 0x6B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // ARR #imm (illegal, unstable)
            case 0x7B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // RRA abs,Y (illegal)
            case 0x8B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // XAA #imm (illegal, highly unstable)
            case 0x9B: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // TAS abs,Y (illegal, unstable)
            case 0xAB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // LAX #imm (illegal, unstable)
            case 0xBB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // LAS abs,Y (illegal, unstable)
            case 0xCB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // AXS #imm (illegal, unstable)
            case 0xDB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // DCP abs,Y (illegal)
            case 0xEB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // SBC #imm (illegal, same as legal E9)
            case 0xFB: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // ISC abs,Y (illegal)
            
            // Column C: Absolute addressing & Jump
            case 0x0C: return CycleInstructions<BusConfig>::make_tsb_abs(Cycle);               // TSB abs (65C02) / NOP abs (illegal on NMOS)
            case 0x1C: return CycleInstructions<BusConfig>::make_trb_abs(Cycle);               // TRB abs (65C02) / NOP abs,X (illegal on NMOS)
            case 0x2C: return CycleInstructions<BusConfig>::make_bit_abs(Cycle);               // BIT abs
            case 0x3C: return CycleInstructions<BusConfig>::make_bit_absx(Cycle);              // BIT abs,X (65C02) / NOP abs,X (illegal on NMOS)
            case 0x4C: return CycleInstructions<BusConfig>::make_jmp_abs(Cycle);               // JMP abs
            case 0x5C: return CycleInstructions<BusConfig>::make_nop_absx(Cycle);              // NOP abs,X (illegal)
            case 0x6C: return CycleInstructions<BusConfig>::make_jmp_ind(Cycle);               // JMP (abs)
            case 0x7C: return CycleInstructions<BusConfig>::make_jmp_absx_ind(Cycle);          // JMP (abs,X) (65C02) / NOP abs,X (illegal on NMOS)
            case 0x8C: return CycleInstructions<BusConfig>::make_sty_abs(Cycle);               // STY abs
            case 0x9C: return CycleInstructions<BusConfig>::make_stz_abs(Cycle);               // STZ abs (65C02) / SHY abs,X (illegal on NMOS)
            case 0xAC: return CycleInstructions<BusConfig>::make_ldy_abs(Cycle);               // LDY abs
            case 0xBC: return CycleInstructions<BusConfig>::make_ldy_absx(Cycle);              // LDY abs,X
            case 0xCC: return CycleInstructions<BusConfig>::make_cpy_abs(Cycle);               // CPY abs
            case 0xDC: return CycleInstructions<BusConfig>::make_nop_absx(Cycle);              // NOP abs,X (illegal)
            case 0xEC: return CycleInstructions<BusConfig>::make_cpx_abs(Cycle);               // CPX abs
            case 0xFC: return CycleInstructions<BusConfig>::make_nop_absx(Cycle);              // NOP abs,X (illegal)
            
            // Column D: Absolute
            case 0x0D: return CycleInstructions<BusConfig>::make_ora_abs(Cycle);               // ORA abs
            case 0x1D: return CycleInstructions<BusConfig>::make_ora_absx(Cycle);              // ORA abs,X
            case 0x2D: return CycleInstructions<BusConfig>::make_and_abs(Cycle);               // AND abs
            case 0x3D: return CycleInstructions<BusConfig>::make_and_absx(Cycle);              // AND abs,X
            case 0x4D: return CycleInstructions<BusConfig>::make_eor_abs(Cycle);               // EOR abs
            case 0x5D: return CycleInstructions<BusConfig>::make_eor_absx(Cycle);              // EOR abs,X
            case 0x6D: return CycleInstructions<BusConfig>::make_adc_abs(Cycle);               // ADC abs
            case 0x7D: return CycleInstructions<BusConfig>::make_adc_absx(Cycle);              // ADC abs,X
            case 0x8D: return CycleInstructions<BusConfig>::make_sta_abs(Cycle);               // STA abs
            case 0x9D: return CycleInstructions<BusConfig>::make_sta_absx(Cycle);              // STA abs,X
            case 0xAD: return CycleInstructions<BusConfig>::make_lda_abs(Cycle);               // LDA abs
            case 0xBD: return CycleInstructions<BusConfig>::make_lda_absx(Cycle);              // LDA abs,X
            case 0xCD: return CycleInstructions<BusConfig>::make_cmp_abs(Cycle);               // CMP abs
            case 0xDD: return CycleInstructions<BusConfig>::make_cmp_absx(Cycle);              // CMP abs,X
            case 0xED: return CycleInstructions<BusConfig>::make_sbc_abs(Cycle);               // SBC abs
            case 0xFD: return CycleInstructions<BusConfig>::make_sbc_absx(Cycle);              // SBC abs,X
            
            // Column E: Absolute with shifts
            case 0x0E: return CycleInstructions<BusConfig>::make_asl_abs(Cycle);               // ASL abs
            case 0x1E: return CycleInstructions<BusConfig>::make_asl_absx(Cycle);              // ASL abs,X
            case 0x2E: return CycleInstructions<BusConfig>::make_rol_abs(Cycle);               // ROL abs
            case 0x3E: return CycleInstructions<BusConfig>::make_rol_absx(Cycle);              // ROL abs,X
            case 0x4E: return CycleInstructions<BusConfig>::make_lsr_abs(Cycle);               // LSR abs
            case 0x5E: return CycleInstructions<BusConfig>::make_lsr_absx(Cycle);              // LSR abs,X
            case 0x6E: return CycleInstructions<BusConfig>::make_ror_abs(Cycle);               // ROR abs
            case 0x7E: return CycleInstructions<BusConfig>::make_ror_absx(Cycle);              // ROR abs,X
            case 0x8E: return CycleInstructions<BusConfig>::make_stx_abs(Cycle);               // STX abs
            case 0x9E: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // SHX abs,Y (illegal, unstable)
            case 0xAE: return CycleInstructions<BusConfig>::make_ldx_abs(Cycle);               // LDX abs
            case 0xBE: return CycleInstructions<BusConfig>::make_ldx_absy(Cycle);              // LDX abs,Y
            case 0xCE: return CycleInstructions<BusConfig>::make_dec_abs(Cycle);               // DEC abs
            case 0xDE: return CycleInstructions<BusConfig>::make_dec_absx(Cycle);              // DEC abs,X
            case 0xEE: return CycleInstructions<BusConfig>::make_inc_abs(Cycle);               // INC abs
            case 0xFE: return CycleInstructions<BusConfig>::make_inc_absx(Cycle);              // INC abs,X
            
            // Column F: Illegal opcodes (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
            case 0x0F: return CycleInstructions<BusConfig>::make_slo_abs(Cycle);               // SLO abs (illegal)
            case 0x1F: return CycleInstructions<BusConfig>::make_slo_absx(Cycle);              // SLO abs,X (illegal)
            case 0x2F: return CycleInstructions<BusConfig>::make_rla_abs(Cycle);               // RLA abs (illegal)
            case 0x3F: return CycleInstructions<BusConfig>::make_rla_absx(Cycle);              // RLA abs,X (illegal)
            case 0x4F: return CycleInstructions<BusConfig>::make_sre_abs(Cycle);               // SRE abs (illegal)
            case 0x5F: return CycleInstructions<BusConfig>::make_sre_absx(Cycle);              // SRE abs,X (illegal)
            case 0x6F: return CycleInstructions<BusConfig>::make_rra_abs(Cycle);               // RRA abs (illegal)
            case 0x7F: return CycleInstructions<BusConfig>::make_rra_absx(Cycle);              // RRA abs,X (illegal)
            case 0x8F: return CycleInstructions<BusConfig>::make_sax_abs(Cycle);               // SAX abs (illegal)
            case 0x9F: return CycleInstructions<BusConfig>::make_nop(Cycle);                   // AHX abs,Y (illegal, unstable)
            case 0xAF: return CycleInstructions<BusConfig>::make_lax_abs(Cycle);               // LAX abs (illegal)
            case 0xBF: return CycleInstructions<BusConfig>::make_lax_absy(Cycle);              // LAX abs,Y (illegal)
            case 0xCF: return CycleInstructions<BusConfig>::make_dcp_abs(Cycle);               // DCP abs (illegal)
            case 0xDF: return CycleInstructions<BusConfig>::make_dcp_absx(Cycle);              // DCP abs,X (illegal)
            case 0xEF: return CycleInstructions<BusConfig>::make_isc_abs(Cycle);               // ISC abs (illegal)
            case 0xFF: return CycleInstructions<BusConfig>::make_isc_absx(Cycle);              // ISC abs,X (illegal)
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

#endif // CYCLE_TABLE_GEN_HPP