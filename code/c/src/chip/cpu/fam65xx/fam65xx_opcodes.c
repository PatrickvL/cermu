#include "fam65xx_core.h"
#include <string.h>

// Opcode implementations are now included directly
#include "fam65xx_arithmetic.inc"
#include "fam65xx_control.inc"
#include "fam65xx_illegal.inc"
#include "fam65xx_memory.inc"
#include "fam65xx_misc.inc"
#include "fam65xx_registers.inc"
#include "fam65xx_shifts.inc"

// ============================================================================
// SHARED MOS 6502 FAMILY DEFAULT OPCODE HANDLER TABLE
// ============================================================================

// This is the base opcode handler table used by all family members.
// Individual CPU types can override specific opcodes as needed.
// For example:
// - NES6502 disables decimal mode operations (SED/CLD do nothing)
// - MOS6502 enables all operations including decimal mode
// - MOS6510 adds I/O port handling and illegal opcodes

// Default opcode handler table - all 256 opcodes
// This provides a complete baseline that all family members can use
fam65xx_opcode_handler_t fam65xx_op_default_handlers[256] = {
    [0x00] = fam65xx_op_brk,                // BRK
    [0x01] = fam65xx_op_ora_indirect_x,     // ORA ($nn,X)
    [0x02] = fam65xx_op_jam,                // JAM (illegal)
    [0x03] = fam65xx_op_slo_indirect_x,     // SLO ($nn,X) (illegal)
    [0x04] = fam65xx_op_nop_zero_page,      // NOP $nn (illegal)
    [0x05] = fam65xx_op_ora_zero_page,      // ORA $nn
    [0x06] = fam65xx_op_asl_zero_page,      // ASL $nn
    [0x07] = fam65xx_op_slo_zero_page,      // SLO $nn (illegal)
    [0x08] = fam65xx_op_php,                // PHP
    [0x09] = fam65xx_op_ora_immediate,      // ORA #$nn
    [0x0A] = fam65xx_op_asl_accumulator,    // ASL A
    [0x0B] = fam65xx_op_anc_immediate,      // ANC #$nn (illegal)
    [0x0C] = fam65xx_op_nop_absolute,       // NOP $nnnn (illegal)
    [0x0D] = fam65xx_op_ora_absolute,       // ORA $nnnn
    [0x0E] = fam65xx_op_asl_absolute,       // ASL $nnnn
    [0x0F] = fam65xx_op_slo_absolute,       // SLO $nnnn (illegal)

    [0x10] = fam65xx_op_bpl,                // BPL rel
    [0x11] = fam65xx_op_ora_indirect_y,     // ORA ($nn),Y
    [0x12] = fam65xx_op_jam,                // JAM (illegal)
    [0x13] = fam65xx_op_slo_indirect_y,     // SLO ($nn),Y (illegal)
    [0x14] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x15] = fam65xx_op_ora_zero_page_x,    // ORA $nn,X
    [0x16] = fam65xx_op_asl_zero_page_x,    // ASL $nn,X
    [0x17] = fam65xx_op_slo_zero_page_x,    // SLO $nn,X (illegal)
    [0x18] = fam65xx_op_clc,                // CLC
    [0x19] = fam65xx_op_ora_absolute_y,     // ORA $nnnn,Y
    [0x1A] = fam65xx_op_nop,                // NOP (illegal)
    [0x1B] = fam65xx_op_slo_absolute_y,     // SLO $nnnn,Y (illegal)
    [0x1C] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x1D] = fam65xx_op_ora_absolute_x,     // ORA $nnnn,X
    [0x1E] = fam65xx_op_asl_absolute_x,     // ASL $nnnn,X
    [0x1F] = fam65xx_op_slo_absolute_x,     // SLO $nnnn,X (illegal)

    [0x20] = fam65xx_op_jsr,                // JSR $nnnn
    [0x21] = fam65xx_op_and_indirect_x,     // AND ($nn,X)
    [0x22] = fam65xx_op_jam,                // JAM (illegal)
    [0x23] = fam65xx_op_rla_indirect_x,     // RLA ($nn,X) (illegal)
    [0x24] = fam65xx_op_bit_zero_page,      // BIT $nn
    [0x25] = fam65xx_op_and_zero_page,      // AND $nn
    [0x26] = fam65xx_op_rol_zero_page,      // ROL $nn
    [0x27] = fam65xx_op_rla_zero_page,      // RLA $nn (illegal)
    [0x28] = fam65xx_op_plp,                // PLP
    [0x29] = fam65xx_op_and_immediate,      // AND #$nn
    [0x2A] = fam65xx_op_rol_accumulator,    // ROL A
    [0x2B] = fam65xx_op_anc_immediate,      // ANC #$nn (illegal)
    [0x2C] = fam65xx_op_bit_absolute,       // BIT $nnnn
    [0x2D] = fam65xx_op_and_absolute,       // AND $nnnn
    [0x2E] = fam65xx_op_rol_absolute,       // ROL $nnnn
    [0x2F] = fam65xx_op_rla_absolute,       // RLA $nnnn (illegal)

    [0x30] = fam65xx_op_bmi,                // BMI rel
    [0x31] = fam65xx_op_and_indirect_y,     // AND ($nn),Y
    [0x32] = fam65xx_op_jam,                // JAM (illegal)
    [0x33] = fam65xx_op_rla_indirect_y,     // RLA ($nn),Y (illegal)
    [0x34] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x35] = fam65xx_op_and_zero_page_x,    // AND $nn,X
    [0x36] = fam65xx_op_rol_zero_page_x,    // ROL $nn,X
    [0x37] = fam65xx_op_rla_zero_page_x,    // RLA $nn,X (illegal)
    [0x38] = fam65xx_op_sec,                // SEC
    [0x39] = fam65xx_op_and_absolute_y,     // AND $nnnn,Y
    [0x3A] = fam65xx_op_nop,                // NOP (illegal)
    [0x3B] = fam65xx_op_rla_absolute_y,     // RLA $nnnn,Y (illegal)
    [0x3C] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x3D] = fam65xx_op_and_absolute_x,     // AND $nnnn,X
    [0x3E] = fam65xx_op_rol_absolute_x,     // ROL $nnnn,X
    [0x3F] = fam65xx_op_rla_absolute_x,     // RLA $nnnn,X (illegal)

    [0x40] = fam65xx_op_rti,                // RTI
    [0x41] = fam65xx_op_eor_indirect_x,     // EOR ($nn,X)
    [0x42] = fam65xx_op_jam,                // JAM (illegal)
    [0x43] = fam65xx_op_sre_indirect_x,     // SRE ($nn,X) (illegal)
    [0x44] = fam65xx_op_nop_zero_page,      // NOP $nn (illegal)
    [0x45] = fam65xx_op_eor_zero_page,      // EOR $nn
    [0x46] = fam65xx_op_lsr_zero_page,      // LSR $nn
    [0x47] = fam65xx_op_sre_zero_page,      // SRE $nn (illegal)
    [0x48] = fam65xx_op_pha,                // PHA
    [0x49] = fam65xx_op_eor_immediate,      // EOR #$nn
    [0x4A] = fam65xx_op_lsr_accumulator,    // LSR A
    [0x4B] = fam65xx_op_alr_immediate,      // ALR #$nn (illegal)
    [0x4C] = fam65xx_op_jmp_absolute,       // JMP $nnnn
    [0x4D] = fam65xx_op_eor_absolute,       // EOR $nnnn
    [0x4E] = fam65xx_op_lsr_absolute,       // LSR $nnnn
    [0x4F] = fam65xx_op_sre_absolute,       // SRE $nnnn (illegal)

    [0x50] = fam65xx_op_bvc,                // BVC rel
    [0x51] = fam65xx_op_eor_indirect_y,     // EOR ($nn),Y
    [0x52] = fam65xx_op_jam,                // JAM (illegal)
    [0x53] = fam65xx_op_sre_indirect_y,     // SRE ($nn),Y (illegal)
    [0x54] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x55] = fam65xx_op_eor_zero_page_x,    // EOR $nn,X
    [0x56] = fam65xx_op_lsr_zero_page_x,    // LSR $nn,X
    [0x57] = fam65xx_op_sre_zero_page_x,    // SRE $nn,X (illegal)
    [0x58] = fam65xx_op_cli,                // CLI
    [0x59] = fam65xx_op_eor_absolute_y,     // EOR $nnnn,Y
    [0x5A] = fam65xx_op_nop,                // NOP (illegal)
    [0x5B] = fam65xx_op_sre_absolute_y,     // SRE $nnnn,Y (illegal)
    [0x5C] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x5D] = fam65xx_op_eor_absolute_x,     // EOR $nnnn,X
    [0x5E] = fam65xx_op_lsr_absolute_x,     // LSR $nnnn,X
    [0x5F] = fam65xx_op_sre_absolute_x,     // SRE $nnnn,X (illegal)

    [0x60] = fam65xx_op_rts,                // RTS
    [0x61] = fam65xx_op_adc_indirect_x,     // ADC ($nn,X)
    [0x62] = fam65xx_op_jam,                // JAM (illegal)
    [0x63] = fam65xx_op_rra_indirect_x,     // RRA ($nn,X) (illegal)
    [0x64] = fam65xx_op_nop_zero_page,      // NOP $nn (illegal)
    [0x65] = fam65xx_op_adc_zero_page,      // ADC $nn
    [0x66] = fam65xx_op_ror_zero_page,      // ROR $nn
    [0x67] = fam65xx_op_rra_zero_page,      // RRA $nn (illegal)
    [0x68] = fam65xx_op_pla,                // PLA
    [0x69] = fam65xx_op_adc_immediate,      // ADC #$nn
    [0x6A] = fam65xx_op_ror_accumulator,    // ROR A
    [0x6B] = fam65xx_op_arr_immediate,      // ARR #$nn (illegal)
    [0x6C] = fam65xx_op_jmp_indirect,       // JMP ($nnnn)
    [0x6D] = fam65xx_op_adc_absolute,       // ADC $nnnn
    [0x6E] = fam65xx_op_ror_absolute,       // ROR $nnnn
    [0x6F] = fam65xx_op_rra_absolute,       // RRA $nnnn (illegal)

    [0x70] = fam65xx_op_bvs,                // BVS rel
    [0x71] = fam65xx_op_adc_indirect_y,     // ADC ($nn),Y
    [0x72] = fam65xx_op_jam,                // JAM (illegal)
    [0x73] = fam65xx_op_rra_indirect_y,     // RRA ($nn),Y (illegal)
    [0x74] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x75] = fam65xx_op_adc_zero_page_x,    // ADC $nn,X
    [0x76] = fam65xx_op_ror_zero_page_x,    // ROR $nn,X
    [0x77] = fam65xx_op_rra_zero_page_x,    // RRA $nn,X (illegal)
    [0x78] = fam65xx_op_sei,                // SEI
    [0x79] = fam65xx_op_adc_absolute_y,     // ADC $nnnn,Y
    [0x7A] = fam65xx_op_nop,                // NOP (illegal)
    [0x7B] = fam65xx_op_rra_absolute_y,     // RRA $nnnn,Y (illegal)
    [0x7C] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x7D] = fam65xx_op_adc_absolute_x,     // ADC $nnnn,X
    [0x7E] = fam65xx_op_ror_absolute_x,     // ROR $nnnn,X
    [0x7F] = fam65xx_op_rra_absolute_x,     // RRA $nnnn,X (illegal)

    [0x80] = fam65xx_op_nop_immediate,      // NOP #$nn (illegal)
    [0x81] = fam65xx_op_sta_indirect_x,     // STA ($nn,X)
    [0x82] = fam65xx_op_nop_immediate,      // NOP #$nn (illegal)
    [0x83] = fam65xx_op_sax_indirect_x,     // SAX ($nn,X) (illegal)
    [0x84] = fam65xx_op_sty_zero_page,      // STY $nn
    [0x85] = fam65xx_op_sta_zero_page,      // STA $nn
    [0x86] = fam65xx_op_stx_zero_page,      // STX $nn
    [0x87] = fam65xx_op_sax_zero_page,      // SAX $nn (illegal)
    [0x88] = fam65xx_op_dey,                // DEY
    [0x89] = fam65xx_op_nop_immediate_special, // NOP #$nn (special)
    [0x8A] = fam65xx_op_txa,                // TXA
    [0x8B] = fam65xx_op_xaa_immediate,      // XAA #$nn (illegal)
    [0x8C] = fam65xx_op_sty_absolute,       // STY $nnnn
    [0x8D] = fam65xx_op_sta_absolute,       // STA $nnnn
    [0x8E] = fam65xx_op_stx_absolute,       // STX $nnnn
    [0x8F] = fam65xx_op_sax_absolute,       // SAX $nnnn (illegal)

    [0x90] = fam65xx_op_bcc,                // BCC rel
    [0x91] = fam65xx_op_sta_indirect_y,     // STA ($nn),Y
    [0x92] = fam65xx_op_jam,                // JAM (illegal)
    [0x93] = fam65xx_op_jam,                // AHX ($nn),Y (illegal)
    [0x94] = fam65xx_op_sty_zero_page_x,    // STY $nn,X
    [0x95] = fam65xx_op_sta_zero_page_x,    // STA $nn,X
    [0x96] = fam65xx_op_stx_zero_page_y,    // STX $nn,Y
    [0x97] = fam65xx_op_sax_zero_page_y,    // SAX $nn,Y (illegal)
    [0x98] = fam65xx_op_tya,                // TYA
    [0x99] = fam65xx_op_sta_absolute_y,     // STA $nnnn,Y
    [0x9A] = fam65xx_op_txs,                // TXS
    [0x9B] = fam65xx_op_jam,                // TAS $nnnn,Y (illegal)
    [0x9C] = fam65xx_op_jam,                // SHY $nnnn,X (illegal)
    [0x9D] = fam65xx_op_sta_absolute_x,     // STA $nnnn,X
    [0x9E] = fam65xx_op_jam,                // SHX $nnnn,Y (illegal)
    [0x9F] = fam65xx_op_jam,                // AHX $nnnn,Y (illegal)

    [0xA0] = fam65xx_op_ldy_immediate,      // LDY #$nn
    [0xA1] = fam65xx_op_lda_indirect_x,     // LDA ($nn,X)
    [0xA2] = fam65xx_op_ldx_immediate,      // LDX #$nn
    [0xA3] = fam65xx_op_lax_indirect_x,     // LAX ($nn,X) (illegal)
    [0xA4] = fam65xx_op_ldy_zero_page,      // LDY $nn
    [0xA5] = fam65xx_op_lda_zero_page,      // LDA $nn
    [0xA6] = fam65xx_op_ldx_zero_page,      // LDX $nn
    [0xA7] = fam65xx_op_lax_zero_page,      // LAX $nn (illegal)
    [0xA8] = fam65xx_op_tay,                // TAY
    [0xA9] = fam65xx_op_lda_immediate,      // LDA #$nn
    [0xAA] = fam65xx_op_tax,                // TAX
    [0xAB] = fam65xx_op_lax_immediate,      // LAX #$nn (illegal)
    [0xAC] = fam65xx_op_ldy_absolute,       // LDY $nnnn
    [0xAD] = fam65xx_op_lda_absolute,       // LDA $nnnn
    [0xAE] = fam65xx_op_ldx_absolute,       // LDX $nnnn
    [0xAF] = fam65xx_op_lax_absolute,       // LAX $nnnn (illegal)

    [0xB0] = fam65xx_op_bcs,                // BCS rel
    [0xB1] = fam65xx_op_lda_indirect_y,     // LDA ($nn),Y
    [0xB2] = fam65xx_op_jam,                // JAM (illegal)
    [0xB3] = fam65xx_op_lax_indirect_y,     // LAX ($nn),Y (illegal)
    [0xB4] = fam65xx_op_ldy_zero_page_x,    // LDY $nn,X
    [0xB5] = fam65xx_op_lda_zero_page_x,    // LDA $nn,X
    [0xB6] = fam65xx_op_ldx_zero_page_y,    // LDX $nn,Y
    [0xB7] = fam65xx_op_lax_zero_page_y,    // LAX $nn,Y (illegal)
    [0xB8] = fam65xx_op_clv,                // CLV
    [0xB9] = fam65xx_op_lda_absolute_y,     // LDA $nnnn,Y
    [0xBA] = fam65xx_op_tsx,                // TSX
    [0xBB] = fam65xx_op_jam,                // LAS $nnnn,Y (illegal)
    [0xBC] = fam65xx_op_ldy_absolute_x,     // LDY $nnnn,X
    [0xBD] = fam65xx_op_lda_absolute_x,     // LDA $nnnn,X
    [0xBE] = fam65xx_op_ldx_absolute_y,     // LDX $nnnn,Y
    [0xBF] = fam65xx_op_lax_absolute_y,     // LAX $nnnn,Y (illegal)

    [0xC0] = fam65xx_op_cpy_immediate,      // CPY #$nn
    [0xC1] = fam65xx_op_cmp_indirect_x,     // CMP ($nn,X)
    [0xC2] = fam65xx_op_nop_immediate,      // NOP #$nn (illegal)
    [0xC3] = fam65xx_op_dcp_indirect_x,     // DCP ($nn,X) (illegal)
    [0xC4] = fam65xx_op_cpy_zero_page,      // CPY $nn
    [0xC5] = fam65xx_op_cmp_zero_page,      // CMP $nn
    [0xC6] = fam65xx_op_dec_zero_page,      // DEC $nn
    [0xC7] = fam65xx_op_dcp_zero_page,      // DCP $nn (illegal)
    [0xC8] = fam65xx_op_iny,                // INY
    [0xC9] = fam65xx_op_cmp_immediate,      // CMP #$nn
    [0xCA] = fam65xx_op_dex,                // DEX
    [0xCB] = fam65xx_op_axs_immediate,      // AXS #$nn (illegal)
    [0xCC] = fam65xx_op_cpy_absolute,       // CPY $nnnn
    [0xCD] = fam65xx_op_cmp_absolute,       // CMP $nnnn
    [0xCE] = fam65xx_op_dec_absolute,       // DEC $nnnn
    [0xCF] = fam65xx_op_dcp_absolute,       // DCP $nnnn (illegal)

    [0xD0] = fam65xx_op_bne,                // BNE rel
    [0xD1] = fam65xx_op_cmp_indirect_y,     // CMP ($nn),Y
    [0xD2] = fam65xx_op_jam,                // JAM (illegal)
    [0xD3] = fam65xx_op_dcp_indirect_y,     // DCP ($nn),Y (illegal)
    [0xD4] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0xD5] = fam65xx_op_cmp_zero_page_x,    // CMP $nn,X
    [0xD6] = fam65xx_op_dec_zero_page_x,    // DEC $nn,X
    [0xD7] = fam65xx_op_dcp_zero_page_x,    // DCP $nn,X (illegal)
    [0xD8] = fam65xx_op_cld,                // CLD
    [0xD9] = fam65xx_op_cmp_absolute_y,     // CMP $nnnn,Y
    [0xDA] = fam65xx_op_nop,                // NOP (illegal)
    [0xDB] = fam65xx_op_dcp_absolute_y,     // DCP $nnnn,Y (illegal)
    [0xDC] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0xDD] = fam65xx_op_cmp_absolute_x,     // CMP $nnnn,X
    [0xDE] = fam65xx_op_dec_absolute_x,     // DEC $nnnn,X
    [0xDF] = fam65xx_op_dcp_absolute_x,     // DCP $nnnn,X (illegal)

    [0xE0] = fam65xx_op_cpx_immediate,      // CPX #$nn
    [0xE1] = fam65xx_op_sbc_indirect_x,     // SBC ($nn,X)
    [0xE2] = fam65xx_op_nop_immediate,      // NOP #$nn (illegal)
    [0xE3] = fam65xx_op_isc_indirect_x,     // ISC ($nn,X) (illegal)
    [0xE4] = fam65xx_op_cpx_zero_page,      // CPX $nn
    [0xE5] = fam65xx_op_sbc_zero_page,      // SBC $nn
    [0xE6] = fam65xx_op_inc_zero_page,      // INC $nn
    [0xE7] = fam65xx_op_isc_zero_page,      // ISC $nn (illegal)
    [0xE8] = fam65xx_op_inx,                // INX
    [0xE9] = fam65xx_op_sbc_immediate,      // SBC #$nn
    [0xEA] = fam65xx_op_nop,                // NOP
    [0xEB] = fam65xx_op_sbc_immediate,      // SBC #$nn (illegal)
    [0xEC] = fam65xx_op_cpx_absolute,       // CPX $nnnn
    [0xED] = fam65xx_op_sbc_absolute,       // SBC $nnnn
    [0xEE] = fam65xx_op_inc_absolute,       // INC $nnnn
    [0xEF] = fam65xx_op_isc_absolute,       // ISC $nnnn (illegal)

    [0xF0] = fam65xx_op_beq,                // BEQ rel
    [0xF1] = fam65xx_op_sbc_indirect_y,     // SBC ($nn),Y
    [0xF2] = fam65xx_op_jam,                // JAM (illegal)
    [0xF3] = fam65xx_op_isc_indirect_y,     // ISC ($nn),Y (illegal)
    [0xF4] = fam65xx_op_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0xF5] = fam65xx_op_sbc_zero_page_x,    // SBC $nn,X
    [0xF6] = fam65xx_op_inc_zero_page_x,    // INC $nn,X
    [0xF7] = fam65xx_op_isc_zero_page_x,    // ISC $nn,X (illegal)
    [0xF8] = fam65xx_op_sed,                // SED
    [0xF9] = fam65xx_op_sbc_absolute_y,     // SBC $nnnn,Y
    [0xFA] = fam65xx_op_nop,                // NOP (illegal)
    [0xFB] = fam65xx_op_isc_absolute_y,     // ISC $nnnn,Y (illegal)
    [0xFC] = fam65xx_op_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0xFD] = fam65xx_op_sbc_absolute_x,     // SBC $nnnn,X
    [0xFE] = fam65xx_op_inc_absolute_x,     // INC $nnnn,X
    [0xFF] = fam65xx_op_isc_absolute_x,     // ISC $nnnn,X (illegal)
};

// ============================================================================
// FEATURE-BASED OPCODE HANDLERS
// ============================================================================

void fam65xx_op_sed_with_flag(fam65xx_t* cpu) {
    // Set decimal flag even if decimal arithmetic is not supported
    fam65xx_set_flag(cpu, FLAG_D, true);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_cld_with_flag(fam65xx_t* cpu) {
    // Clear decimal flag even if decimal arithmetic is not supported
    fam65xx_set_flag(cpu, FLAG_D, false);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_ror_absolute_x_buggy(fam65xx_t* cpu) {
    // Buggy ROR absolute,X that doesn't handle page crossing correctly
    // This is a simplified implementation - the real bug is more complex
    uint16_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t effective_addr = addr + cpu->x;
    
    // Bug: doesn't add extra cycle for page crossing like other absolute,X instructions
    uint8_t value = fam65xx_read_cycle(cpu, effective_addr);  // Bus read cycle
    
    bool old_carry = fam65xx_get_flag(cpu, FLAG_C);
    fam65xx_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0x00);
    fam65xx_set_nz_flags(cpu, value);
    
    fam65xx_write_cycle(cpu, effective_addr, value);  // Bus write cycle
    FAM65XX_OPCODE_FOOTER(cpu);
}

// ============================================================================
// MOS 6502 ARITHMETIC OPERATIONS
// ============================================================================

// NMOS 6502 decimal mode core logic (shared for all MOS6502 decimal-aware handlers)
void mos6502_op_adc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic - NMOS 6502 implementation
        // The NMOS 6502 has specific behavior: N,V,Z flags based on binary result,
        // but the accumulator and C flag are based on BCD arithmetic
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
        uint8_t A = cpu->a;
        uint8_t operand_val = operand;
        uint16_t binary_result = A + operand_val + carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        fam65xx_set_flag(cpu, FLAG_V, ((A ^ binary_result) & (operand_val ^ binary_result) & 0x80) != 0);
        uint16_t al = (A & 0x0F) + (operand_val & 0x0F) + carry_in;
        if (al >= 0x0A) {
            al = ((al + 0x06) & 0x0F) + 0x10;
        }
        uint16_t result = (A & 0xF0) + (operand_val & 0xF0) + al;
        if (result >= 0xA0) {
            result += 0x60;
        }
        fam65xx_set_flag(cpu, FLAG_C, result >= 0x100);
        cpu->a = result & 0xFF;
    } else {
        fam65xx_op_adc(cpu, operand);
    }
}

#if false // TODO : de-duplicate
void mos6502_op_adc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic - NMOS 6502 implementation
        // The NMOS 6502 has specific behavior: N,V,Z flags based on binary result,
        // but the accumulator and C flag are based on BCD arithmetic
        
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
        uint8_t A = cpu->a;
        uint8_t operand_val = operand;
        
        // First, do binary addition for N, V, Z flags
        uint16_t binary_result = A + operand_val + carry_in;
        
        // Set N, V, Z flags based on binary result (NMOS 6502 behavior)
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        fam65xx_set_flag(cpu, FLAG_V, ((A ^ binary_result) & (operand_val ^ binary_result) & 0x80) != 0);
        
        // Now do BCD arithmetic for the accumulator and carry flag
        uint16_t al = (A & 0x0F) + (operand_val & 0x0F) + carry_in;
        if (al >= 0x0A) {
            al = ((al + 0x06) & 0x0F) + 0x10;
        }
        
        uint16_t result = (A & 0xF0) + (operand_val & 0xF0) + al;
        if (result >= 0xA0) {
            result += 0x60;
        }
        
        // Set carry flag based on BCD result
        fam65xx_set_flag(cpu, FLAG_C, result >= 0x100);
        
        // Update accumulator with BCD result
        cpu->a = result & 0xFF;
    } else {
        // Binary mode ADC - use inlined base family implementation
        fam65xx_op_adc(cpu, operand);
    }
}

void mos6502_op_sbc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic - NMOS 6502 implementation
        // The NMOS 6502 has specific behavior: N,V,Z flags based on binary result,
        // but the accumulator and C flag are based on BCD arithmetic
        
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 0 : 1; // Inverted for SBC
        uint8_t A = cpu->a;
        uint8_t operand_val = operand;
        
        // First, do binary subtraction for N, V, Z flags
        uint16_t binary_result = A - operand_val - carry_in;
        
        // Set N, V, Z flags based on binary result (NMOS 6502 behavior)
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        fam65xx_set_flag(cpu, FLAG_V, ((A ^ binary_result) & ((~operand_val) ^ binary_result) & 0x80) != 0);
        
        // Now do BCD arithmetic for the accumulator and carry flag
        int16_t al = (A & 0x0F) - (operand_val & 0x0F) - carry_in;
        int16_t ah = (A >> 4) - (operand_val >> 4);
        
        // Handle low nibble borrow
        if (al < 0) {
            al = (al - 6) & 0x0F;
            ah--;
        }
        
        // Handle high nibble borrow
        if (ah < 0) {
            ah = (ah - 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, false);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, true);
        }
        
        // Update accumulator with BCD result
        cpu->a = ((ah << 4) & 0xF0) | (al & 0x0F);
    } else {
        // Binary mode SBC - use base family implementation
        fam65xx_op_sbc(cpu, operand);
    }
}
#endif

void mos6502_op_sbc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 0 : 1; // Inverted for SBC
        uint8_t A = cpu->a;
        uint8_t operand_val = operand;
        uint16_t binary_result = A - operand_val - carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        fam65xx_set_flag(cpu, FLAG_V, ((A ^ binary_result) & ((~operand_val) ^ binary_result) & 0x80) != 0);
        int16_t al = (A & 0x0F) - (operand_val & 0x0F) - carry_in;
        int16_t ah = (A >> 4) - (operand_val >> 4);
        if (al < 0) {
            al = (al - 6) & 0x0F;
            ah--;
        }
        if (ah < 0) {
            ah = (ah - 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, false);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, true);
        }
        cpu->a = ((ah << 4) & 0xF0) | (al & 0x0F);
    } else {
        fam65xx_op_sbc(cpu, operand);
    }
}

// ============================================================================
// OPCODE IMPLEMENTATIONS
// ============================================================================

// ADC - Add with Carry (supports both binary and decimal modes)
void mos6502_op_adc_imm(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, mos6502_op_adc); }
void mos6502_op_adc_zp(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, mos6502_op_adc); }
void mos6502_op_adc_zpx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, mos6502_op_adc); }
void mos6502_op_adc_abs(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, mos6502_op_adc); }
void mos6502_op_adc_absx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, mos6502_op_adc); }
void mos6502_op_adc_absy(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, mos6502_op_adc); }
void mos6502_op_adc_indx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, mos6502_op_adc); }
void mos6502_op_adc_indy(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, mos6502_op_adc); }

// SBC - Subtract with Carry (supports both binary and decimal modes)
void mos6502_op_sbc_imm(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, mos6502_op_sbc); }
void mos6502_op_sbc_zp(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, mos6502_op_sbc); }
void mos6502_op_sbc_zpx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, mos6502_op_sbc); }
void mos6502_op_sbc_abs(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, mos6502_op_sbc); }
void mos6502_op_sbc_absx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, mos6502_op_sbc); }
void mos6502_op_sbc_absy(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, mos6502_op_sbc); }
void mos6502_op_sbc_indx(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, mos6502_op_sbc); }
void mos6502_op_sbc_indy(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, mos6502_op_sbc); }

// ============================================================================
// OPCODE TABLE INITIALIZATION AND MANAGEMENT
// ============================================================================

/**
 * Override a specific opcode handler for CPU-specific behavior.
 * Use this to customize behavior per CPU type (e.g., disable decimal mode).
 */
void fam65xx_override_opcode(fam65xx_t* cpu, uint8_t opcode, fam65xx_opcode_handler_t handler) {
    cpu->opcode_handlers[opcode] = handler;
}

/**
 * Initialize CPU with default opcode handler table with CPU-specific features.
 * This copies the complete base table to the CPU's handler array
 * and processes CPU feature flags and automatically overrides opcodes as needed.
 */
void fam65xx_init_opcode_table(fam65xx_t* cpu, uint32_t cpu_features) {
    // Start with the default opcode table (binary-only arithmetic, like MOS6510)
    memcpy(cpu->opcode_handlers, fam65xx_op_default_handlers, sizeof(cpu->opcode_handlers));    // Apply decimal mode overrides if supported
    if (cpu_features & FAM65XX_FEATURE_DECIMAL_MODE) {
        // Use MOS6502-specific decimal-aware handlers for ADC/SBC
        fam65xx_override_opcode(cpu, 0x69, mos6502_op_adc_imm);  // ADC #$nn (decimal-aware)
        fam65xx_override_opcode(cpu, 0x65, mos6502_op_adc_zp);   // ADC $nn (decimal-aware)
        fam65xx_override_opcode(cpu, 0x75, mos6502_op_adc_zpx);  // ADC $nn,X (decimal-aware)
        fam65xx_override_opcode(cpu, 0x6D, mos6502_op_adc_abs);  // ADC $nnnn (decimal-aware)
        fam65xx_override_opcode(cpu, 0x7D, mos6502_op_adc_absx); // ADC $nnnn,X (decimal-aware)
        fam65xx_override_opcode(cpu, 0x79, mos6502_op_adc_absy); // ADC $nnnn,Y (decimal-aware)
        fam65xx_override_opcode(cpu, 0x61, mos6502_op_adc_indx); // ADC ($nn,X) (decimal-aware)
        fam65xx_override_opcode(cpu, 0x71, mos6502_op_adc_indy); // ADC ($nn),Y (decimal-aware)

        fam65xx_override_opcode(cpu, 0xE9, mos6502_op_sbc_imm);  // SBC #$nn (decimal-aware)
        fam65xx_override_opcode(cpu, 0xE5, mos6502_op_sbc_zp);   // SBC $nn (decimal-aware)
        fam65xx_override_opcode(cpu, 0xF5, mos6502_op_sbc_zpx);  // SBC $nn,X (decimal-aware)
        fam65xx_override_opcode(cpu, 0xED, mos6502_op_sbc_abs);  // SBC $nnnn (decimal-aware)
        fam65xx_override_opcode(cpu, 0xFD, mos6502_op_sbc_absx); // SBC $nnnn,X (decimal-aware)
        fam65xx_override_opcode(cpu, 0xF9, mos6502_op_sbc_absy); // SBC $nnnn,Y (decimal-aware)
        fam65xx_override_opcode(cpu, 0xE1, mos6502_op_sbc_indx); // SBC ($nn,X) (decimal-aware)
        fam65xx_override_opcode(cpu, 0xF1, mos6502_op_sbc_indy); // SBC ($nn),Y (decimal-aware)
    }
}
