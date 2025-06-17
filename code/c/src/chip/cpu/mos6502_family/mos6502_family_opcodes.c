#include "mos6502_family_core.h"
#include <string.h>

// ============================================================================
// SHARED MOS 6502 FAMILY DEFAULT OPCODE HANDLER TABLE
// ============================================================================

// This is the base opcode handler table used by all family members.
// Individual CPU types can override specific opcodes as needed.
// For example:
// - NES6502 disables decimal mode operations (SED/CLD do nothing)
// - MOS6502 enables all operations including decimal mode
// - MOS6510 adds I/O port handling and illegal opcodes

// External declarations for the opcode implementations
// These are declared in the various mos6502_family_*.c files

// Arithmetic operations
extern void mos6502_family_adc_immediate(mos6502_family_t* cpu);
extern void mos6502_family_adc_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_adc_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_adc_absolute(mos6502_family_t* cpu);
extern void mos6502_family_adc_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_adc_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_adc_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_adc_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_sbc_immediate(mos6502_family_t* cpu);
extern void mos6502_family_sbc_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_sbc_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_sbc_absolute(mos6502_family_t* cpu);
extern void mos6502_family_sbc_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_sbc_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_sbc_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_sbc_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_and_immediate(mos6502_family_t* cpu);
extern void mos6502_family_and_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_and_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_and_absolute(mos6502_family_t* cpu);
extern void mos6502_family_and_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_and_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_and_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_and_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_ora_immediate(mos6502_family_t* cpu);
extern void mos6502_family_ora_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_ora_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_ora_absolute(mos6502_family_t* cpu);
extern void mos6502_family_ora_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_ora_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_ora_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_ora_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_eor_immediate(mos6502_family_t* cpu);
extern void mos6502_family_eor_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_eor_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_eor_absolute(mos6502_family_t* cpu);
extern void mos6502_family_eor_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_eor_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_eor_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_eor_indirect_y(mos6502_family_t* cpu);

// Memory operations
extern void mos6502_family_lda_immediate(mos6502_family_t* cpu);
extern void mos6502_family_lda_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_lda_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_lda_absolute(mos6502_family_t* cpu);
extern void mos6502_family_lda_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_lda_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_lda_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_lda_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_ldx_immediate(mos6502_family_t* cpu);
extern void mos6502_family_ldx_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_ldx_zero_page_y(mos6502_family_t* cpu);
extern void mos6502_family_ldx_absolute(mos6502_family_t* cpu);
extern void mos6502_family_ldx_absolute_y(mos6502_family_t* cpu);

extern void mos6502_family_ldy_immediate(mos6502_family_t* cpu);
extern void mos6502_family_ldy_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_ldy_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_ldy_absolute(mos6502_family_t* cpu);
extern void mos6502_family_ldy_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_sta_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_sta_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_sta_absolute(mos6502_family_t* cpu);
extern void mos6502_family_sta_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_sta_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_sta_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_sta_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_stx_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_stx_zero_page_y(mos6502_family_t* cpu);
extern void mos6502_family_stx_absolute(mos6502_family_t* cpu);

extern void mos6502_family_sty_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_sty_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_sty_absolute(mos6502_family_t* cpu);

// Control flow
extern void mos6502_family_bpl(mos6502_family_t* cpu);
extern void mos6502_family_bmi(mos6502_family_t* cpu);
extern void mos6502_family_bvc(mos6502_family_t* cpu);
extern void mos6502_family_bvs(mos6502_family_t* cpu);
extern void mos6502_family_bcc(mos6502_family_t* cpu);
extern void mos6502_family_bcs(mos6502_family_t* cpu);
extern void mos6502_family_bne(mos6502_family_t* cpu);
extern void mos6502_family_beq(mos6502_family_t* cpu);

extern void mos6502_family_jmp_absolute(mos6502_family_t* cpu);
extern void mos6502_family_jmp_indirect(mos6502_family_t* cpu);
extern void mos6502_family_jsr(mos6502_family_t* cpu);
extern void mos6502_family_rts(mos6502_family_t* cpu);

extern void mos6502_family_brk(mos6502_family_t* cpu);
extern void mos6502_family_rti(mos6502_family_t* cpu);

// Flag operations
extern void mos6502_family_clc(mos6502_family_t* cpu);
extern void mos6502_family_sec(mos6502_family_t* cpu);
extern void mos6502_family_cli(mos6502_family_t* cpu);
extern void mos6502_family_sei(mos6502_family_t* cpu);
extern void mos6502_family_clv(mos6502_family_t* cpu);
extern void mos6502_family_cld(mos6502_family_t* cpu);
extern void mos6502_family_sed(mos6502_family_t* cpu);

// Register operations
extern void mos6502_family_tax(mos6502_family_t* cpu);
extern void mos6502_family_tay(mos6502_family_t* cpu);
extern void mos6502_family_txa(mos6502_family_t* cpu);
extern void mos6502_family_tya(mos6502_family_t* cpu);
extern void mos6502_family_tsx(mos6502_family_t* cpu);
extern void mos6502_family_txs(mos6502_family_t* cpu);

extern void mos6502_family_pha(mos6502_family_t* cpu);
extern void mos6502_family_pla(mos6502_family_t* cpu);
extern void mos6502_family_php(mos6502_family_t* cpu);
extern void mos6502_family_plp(mos6502_family_t* cpu);

extern void mos6502_family_inx(mos6502_family_t* cpu);
extern void mos6502_family_iny(mos6502_family_t* cpu);
extern void mos6502_family_dex(mos6502_family_t* cpu);
extern void mos6502_family_dey(mos6502_family_t* cpu);

extern void mos6502_family_inc_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_inc_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_inc_absolute(mos6502_family_t* cpu);
extern void mos6502_family_inc_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_dec_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_dec_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_dec_absolute(mos6502_family_t* cpu);
extern void mos6502_family_dec_absolute_x(mos6502_family_t* cpu);

// Shift operations
extern void mos6502_family_asl_accumulator(mos6502_family_t* cpu);
extern void mos6502_family_asl_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_asl_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_asl_absolute(mos6502_family_t* cpu);
extern void mos6502_family_asl_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_lsr_accumulator(mos6502_family_t* cpu);
extern void mos6502_family_lsr_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_lsr_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_lsr_absolute(mos6502_family_t* cpu);
extern void mos6502_family_lsr_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_rol_accumulator(mos6502_family_t* cpu);
extern void mos6502_family_rol_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_rol_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_rol_absolute(mos6502_family_t* cpu);
extern void mos6502_family_rol_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_ror_accumulator(mos6502_family_t* cpu);
extern void mos6502_family_ror_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_ror_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_ror_absolute(mos6502_family_t* cpu);
extern void mos6502_family_ror_absolute_x(mos6502_family_t* cpu);

// Miscellaneous operations
extern void mos6502_family_cmp_immediate(mos6502_family_t* cpu);
extern void mos6502_family_cmp_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_cmp_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_cmp_absolute(mos6502_family_t* cpu);
extern void mos6502_family_cmp_absolute_x(mos6502_family_t* cpu);
extern void mos6502_family_cmp_absolute_y(mos6502_family_t* cpu);
extern void mos6502_family_cmp_indirect_x(mos6502_family_t* cpu);
extern void mos6502_family_cmp_indirect_y(mos6502_family_t* cpu);

extern void mos6502_family_cpx_immediate(mos6502_family_t* cpu);
extern void mos6502_family_cpx_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_cpx_absolute(mos6502_family_t* cpu);

extern void mos6502_family_cpy_immediate(mos6502_family_t* cpu);
extern void mos6502_family_cpy_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_cpy_absolute(mos6502_family_t* cpu);

extern void mos6502_family_bit_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_bit_absolute(mos6502_family_t* cpu);

extern void mos6502_family_nop(mos6502_family_t* cpu);
extern void mos6502_family_nop_immediate(mos6502_family_t* cpu);
extern void mos6502_family_nop_zero_page(mos6502_family_t* cpu);
extern void mos6502_family_nop_zero_page_x(mos6502_family_t* cpu);
extern void mos6502_family_nop_absolute(mos6502_family_t* cpu);
extern void mos6502_family_nop_absolute_x(mos6502_family_t* cpu);

extern void mos6502_family_jam(mos6502_family_t* cpu);

// Default opcode handler table - all 256 opcodes
// This provides a complete baseline that all family members can use
mos6502_family_opcode_handler_t mos6502_family_default_handlers[256] = {
    [0x00] = mos6502_family_brk,                // BRK
    [0x01] = mos6502_family_ora_indirect_x,     // ORA ($nn,X)
    [0x02] = mos6502_family_jam,                // JAM (illegal)
    [0x03] = mos6502_family_jam,                // SLO ($nn,X) (illegal) - placeholder
    [0x04] = mos6502_family_nop_zero_page,      // NOP $nn (illegal)
    [0x05] = mos6502_family_ora_zero_page,      // ORA $nn
    [0x06] = mos6502_family_asl_zero_page,      // ASL $nn
    [0x07] = mos6502_family_jam,                // SLO $nn (illegal) - placeholder
    [0x08] = mos6502_family_php,                // PHP
    [0x09] = mos6502_family_ora_immediate,      // ORA #$nn
    [0x0A] = mos6502_family_asl_accumulator,    // ASL A
    [0x0B] = mos6502_family_jam,                // ANC #$nn (illegal) - placeholder
    [0x0C] = mos6502_family_nop_absolute,       // NOP $nnnn (illegal)
    [0x0D] = mos6502_family_ora_absolute,       // ORA $nnnn
    [0x0E] = mos6502_family_asl_absolute,       // ASL $nnnn
    [0x0F] = mos6502_family_jam,                // SLO $nnnn (illegal) - placeholder

    [0x10] = mos6502_family_bpl,                // BPL rel
    [0x11] = mos6502_family_ora_indirect_y,     // ORA ($nn),Y
    [0x12] = mos6502_family_jam,                // JAM (illegal)
    [0x13] = mos6502_family_jam,                // SLO ($nn),Y (illegal) - placeholder
    [0x14] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x15] = mos6502_family_ora_zero_page_x,    // ORA $nn,X
    [0x16] = mos6502_family_asl_zero_page_x,    // ASL $nn,X
    [0x17] = mos6502_family_jam,                // SLO $nn,X (illegal) - placeholder
    [0x18] = mos6502_family_clc,                // CLC
    [0x19] = mos6502_family_ora_absolute_y,     // ORA $nnnn,Y
    [0x1A] = mos6502_family_nop,                // NOP (illegal)
    [0x1B] = mos6502_family_jam,                // SLO $nnnn,Y (illegal) - placeholder
    [0x1C] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x1D] = mos6502_family_ora_absolute_x,     // ORA $nnnn,X
    [0x1E] = mos6502_family_asl_absolute_x,     // ASL $nnnn,X
    [0x1F] = mos6502_family_jam,                // SLO $nnnn,X (illegal) - placeholder

    [0x20] = mos6502_family_jsr,                // JSR $nnnn
    [0x21] = mos6502_family_and_indirect_x,     // AND ($nn,X)
    [0x22] = mos6502_family_jam,                // JAM (illegal)
    [0x23] = mos6502_family_jam,                // RLA ($nn,X) (illegal) - placeholder
    [0x24] = mos6502_family_bit_zero_page,      // BIT $nn
    [0x25] = mos6502_family_and_zero_page,      // AND $nn
    [0x26] = mos6502_family_rol_zero_page,      // ROL $nn
    [0x27] = mos6502_family_jam,                // RLA $nn (illegal) - placeholder
    [0x28] = mos6502_family_plp,                // PLP
    [0x29] = mos6502_family_and_immediate,      // AND #$nn
    [0x2A] = mos6502_family_rol_accumulator,    // ROL A
    [0x2B] = mos6502_family_jam,                // ANC #$nn (illegal) - placeholder
    [0x2C] = mos6502_family_bit_absolute,       // BIT $nnnn
    [0x2D] = mos6502_family_and_absolute,       // AND $nnnn
    [0x2E] = mos6502_family_rol_absolute,       // ROL $nnnn
    [0x2F] = mos6502_family_jam,                // RLA $nnnn (illegal) - placeholder

    [0x30] = mos6502_family_bmi,                // BMI rel
    [0x31] = mos6502_family_and_indirect_y,     // AND ($nn),Y
    [0x32] = mos6502_family_jam,                // JAM (illegal)
    [0x33] = mos6502_family_jam,                // RLA ($nn),Y (illegal) - placeholder
    [0x34] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x35] = mos6502_family_and_zero_page_x,    // AND $nn,X
    [0x36] = mos6502_family_rol_zero_page_x,    // ROL $nn,X
    [0x37] = mos6502_family_jam,                // RLA $nn,X (illegal) - placeholder
    [0x38] = mos6502_family_sec,                // SEC
    [0x39] = mos6502_family_and_absolute_y,     // AND $nnnn,Y
    [0x3A] = mos6502_family_nop,                // NOP (illegal)
    [0x3B] = mos6502_family_jam,                // RLA $nnnn,Y (illegal) - placeholder
    [0x3C] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x3D] = mos6502_family_and_absolute_x,     // AND $nnnn,X
    [0x3E] = mos6502_family_rol_absolute_x,     // ROL $nnnn,X
    [0x3F] = mos6502_family_jam,                // RLA $nnnn,X (illegal) - placeholder

    [0x40] = mos6502_family_rti,                // RTI
    [0x41] = mos6502_family_eor_indirect_x,     // EOR ($nn,X)
    [0x42] = mos6502_family_jam,                // JAM (illegal)
    [0x43] = mos6502_family_jam,                // SRE ($nn,X) (illegal) - placeholder
    [0x44] = mos6502_family_nop_zero_page,      // NOP $nn (illegal)
    [0x45] = mos6502_family_eor_zero_page,      // EOR $nn
    [0x46] = mos6502_family_lsr_zero_page,      // LSR $nn
    [0x47] = mos6502_family_jam,                // SRE $nn (illegal) - placeholder
    [0x48] = mos6502_family_pha,                // PHA
    [0x49] = mos6502_family_eor_immediate,      // EOR #$nn
    [0x4A] = mos6502_family_lsr_accumulator,    // LSR A
    [0x4B] = mos6502_family_jam,                // ALR #$nn (illegal) - placeholder
    [0x4C] = mos6502_family_jmp_absolute,       // JMP $nnnn
    [0x4D] = mos6502_family_eor_absolute,       // EOR $nnnn
    [0x4E] = mos6502_family_lsr_absolute,       // LSR $nnnn
    [0x4F] = mos6502_family_jam,                // SRE $nnnn (illegal) - placeholder

    [0x50] = mos6502_family_bvc,                // BVC rel
    [0x51] = mos6502_family_eor_indirect_y,     // EOR ($nn),Y
    [0x52] = mos6502_family_jam,                // JAM (illegal)
    [0x53] = mos6502_family_jam,                // SRE ($nn),Y (illegal) - placeholder
    [0x54] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x55] = mos6502_family_eor_zero_page_x,    // EOR $nn,X
    [0x56] = mos6502_family_lsr_zero_page_x,    // LSR $nn,X
    [0x57] = mos6502_family_jam,                // SRE $nn,X (illegal) - placeholder
    [0x58] = mos6502_family_cli,                // CLI
    [0x59] = mos6502_family_eor_absolute_y,     // EOR $nnnn,Y
    [0x5A] = mos6502_family_nop,                // NOP (illegal)
    [0x5B] = mos6502_family_jam,                // SRE $nnnn,Y (illegal) - placeholder
    [0x5C] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x5D] = mos6502_family_eor_absolute_x,     // EOR $nnnn,X
    [0x5E] = mos6502_family_lsr_absolute_x,     // LSR $nnnn,X
    [0x5F] = mos6502_family_jam,                // SRE $nnnn,X (illegal) - placeholder

    [0x60] = mos6502_family_rts,                // RTS
    [0x61] = mos6502_family_adc_indirect_x,     // ADC ($nn,X)
    [0x62] = mos6502_family_jam,                // JAM (illegal)
    [0x63] = mos6502_family_jam,                // RRA ($nn,X) (illegal) - placeholder
    [0x64] = mos6502_family_nop_zero_page,      // NOP $nn (illegal)
    [0x65] = mos6502_family_adc_zero_page,      // ADC $nn
    [0x66] = mos6502_family_ror_zero_page,      // ROR $nn
    [0x67] = mos6502_family_jam,                // RRA $nn (illegal) - placeholder
    [0x68] = mos6502_family_pla,                // PLA
    [0x69] = mos6502_family_adc_immediate,      // ADC #$nn
    [0x6A] = mos6502_family_ror_accumulator,    // ROR A
    [0x6B] = mos6502_family_jam,                // ARR #$nn (illegal) - placeholder
    [0x6C] = mos6502_family_jmp_indirect,       // JMP ($nnnn)
    [0x6D] = mos6502_family_adc_absolute,       // ADC $nnnn
    [0x6E] = mos6502_family_ror_absolute,       // ROR $nnnn
    [0x6F] = mos6502_family_jam,                // RRA $nnnn (illegal) - placeholder

    [0x70] = mos6502_family_bvs,                // BVS rel
    [0x71] = mos6502_family_adc_indirect_y,     // ADC ($nn),Y
    [0x72] = mos6502_family_jam,                // JAM (illegal)
    [0x73] = mos6502_family_jam,                // RRA ($nn),Y (illegal) - placeholder
    [0x74] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0x75] = mos6502_family_adc_zero_page_x,    // ADC $nn,X
    [0x76] = mos6502_family_ror_zero_page_x,    // ROR $nn,X
    [0x77] = mos6502_family_jam,                // RRA $nn,X (illegal) - placeholder
    [0x78] = mos6502_family_sei,                // SEI
    [0x79] = mos6502_family_adc_absolute_y,     // ADC $nnnn,Y
    [0x7A] = mos6502_family_nop,                // NOP (illegal)
    [0x7B] = mos6502_family_jam,                // RRA $nnnn,Y (illegal) - placeholder
    [0x7C] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0x7D] = mos6502_family_adc_absolute_x,     // ADC $nnnn,X
    [0x7E] = mos6502_family_ror_absolute_x,     // ROR $nnnn,X
    [0x7F] = mos6502_family_jam,                // RRA $nnnn,X (illegal) - placeholder

    [0x80] = mos6502_family_nop_immediate,      // NOP #$nn (illegal)
    [0x81] = mos6502_family_sta_indirect_x,     // STA ($nn,X)
    [0x82] = mos6502_family_nop_immediate,      // NOP #$nn (illegal)
    [0x83] = mos6502_family_jam,                // SAX ($nn,X) (illegal) - placeholder
    [0x84] = mos6502_family_sty_zero_page,      // STY $nn
    [0x85] = mos6502_family_sta_zero_page,      // STA $nn
    [0x86] = mos6502_family_stx_zero_page,      // STX $nn
    [0x87] = mos6502_family_jam,                // SAX $nn (illegal) - placeholder
    [0x88] = mos6502_family_dey,                // DEY
    [0x89] = mos6502_family_nop_immediate,      // NOP #$nn (illegal)
    [0x8A] = mos6502_family_txa,                // TXA
    [0x8B] = mos6502_family_jam,                // XAA #$nn (illegal) - placeholder
    [0x8C] = mos6502_family_sty_absolute,       // STY $nnnn
    [0x8D] = mos6502_family_sta_absolute,       // STA $nnnn
    [0x8E] = mos6502_family_stx_absolute,       // STX $nnnn
    [0x8F] = mos6502_family_jam,                // SAX $nnnn (illegal) - placeholder

    [0x90] = mos6502_family_bcc,                // BCC rel
    [0x91] = mos6502_family_sta_indirect_y,     // STA ($nn),Y
    [0x92] = mos6502_family_jam,                // JAM (illegal)
    [0x93] = mos6502_family_jam,                // AHX ($nn),Y (illegal) - placeholder
    [0x94] = mos6502_family_sty_zero_page_x,    // STY $nn,X
    [0x95] = mos6502_family_sta_zero_page_x,    // STA $nn,X
    [0x96] = mos6502_family_stx_zero_page_y,    // STX $nn,Y
    [0x97] = mos6502_family_jam,                // SAX $nn,Y (illegal) - placeholder
    [0x98] = mos6502_family_tya,                // TYA
    [0x99] = mos6502_family_sta_absolute_y,     // STA $nnnn,Y
    [0x9A] = mos6502_family_txs,                // TXS
    [0x9B] = mos6502_family_jam,                // TAS $nnnn,Y (illegal) - placeholder
    [0x9C] = mos6502_family_jam,                // SHY $nnnn,X (illegal) - placeholder
    [0x9D] = mos6502_family_sta_absolute_x,     // STA $nnnn,X
    [0x9E] = mos6502_family_jam,                // SHX $nnnn,Y (illegal) - placeholder
    [0x9F] = mos6502_family_jam,                // AHX $nnnn,Y (illegal) - placeholder

    [0xA0] = mos6502_family_ldy_immediate,      // LDY #$nn
    [0xA1] = mos6502_family_lda_indirect_x,     // LDA ($nn,X)
    [0xA2] = mos6502_family_ldx_immediate,      // LDX #$nn
    [0xA3] = mos6502_family_jam,                // LAX ($nn,X) (illegal) - placeholder
    [0xA4] = mos6502_family_ldy_zero_page,      // LDY $nn
    [0xA5] = mos6502_family_lda_zero_page,      // LDA $nn
    [0xA6] = mos6502_family_ldx_zero_page,      // LDX $nn
    [0xA7] = mos6502_family_jam,                // LAX $nn (illegal) - placeholder
    [0xA8] = mos6502_family_tay,                // TAY
    [0xA9] = mos6502_family_lda_immediate,      // LDA #$nn
    [0xAA] = mos6502_family_tax,                // TAX
    [0xAB] = mos6502_family_jam,                // LAX #$nn (illegal) - placeholder
    [0xAC] = mos6502_family_ldy_absolute,       // LDY $nnnn
    [0xAD] = mos6502_family_lda_absolute,       // LDA $nnnn
    [0xAE] = mos6502_family_ldx_absolute,       // LDX $nnnn
    [0xAF] = mos6502_family_jam,                // LAX $nnnn (illegal) - placeholder

    [0xB0] = mos6502_family_bcs,                // BCS rel
    [0xB1] = mos6502_family_lda_indirect_y,     // LDA ($nn),Y
    [0xB2] = mos6502_family_jam,                // JAM (illegal)
    [0xB3] = mos6502_family_jam,                // LAX ($nn),Y (illegal) - placeholder
    [0xB4] = mos6502_family_ldy_zero_page_x,    // LDY $nn,X
    [0xB5] = mos6502_family_lda_zero_page_x,    // LDA $nn,X
    [0xB6] = mos6502_family_ldx_zero_page_y,    // LDX $nn,Y
    [0xB7] = mos6502_family_jam,                // LAX $nn,Y (illegal) - placeholder
    [0xB8] = mos6502_family_clv,                // CLV
    [0xB9] = mos6502_family_lda_absolute_y,     // LDA $nnnn,Y
    [0xBA] = mos6502_family_tsx,                // TSX
    [0xBB] = mos6502_family_jam,                // LAS $nnnn,Y (illegal) - placeholder
    [0xBC] = mos6502_family_ldy_absolute_x,     // LDY $nnnn,X
    [0xBD] = mos6502_family_lda_absolute_x,     // LDA $nnnn,X
    [0xBE] = mos6502_family_ldx_absolute_y,     // LDX $nnnn,Y
    [0xBF] = mos6502_family_jam,                // LAX $nnnn,Y (illegal) - placeholder

    [0xC0] = mos6502_family_cpy_immediate,      // CPY #$nn
    [0xC1] = mos6502_family_cmp_indirect_x,     // CMP ($nn,X)
    [0xC2] = mos6502_family_nop_immediate,      // NOP #$nn (illegal)
    [0xC3] = mos6502_family_jam,                // DCP ($nn,X) (illegal) - placeholder
    [0xC4] = mos6502_family_cpy_zero_page,      // CPY $nn
    [0xC5] = mos6502_family_cmp_zero_page,      // CMP $nn
    [0xC6] = mos6502_family_dec_zero_page,      // DEC $nn
    [0xC7] = mos6502_family_jam,                // DCP $nn (illegal) - placeholder
    [0xC8] = mos6502_family_iny,                // INY
    [0xC9] = mos6502_family_cmp_immediate,      // CMP #$nn
    [0xCA] = mos6502_family_dex,                // DEX
    [0xCB] = mos6502_family_jam,                // AXS #$nn (illegal) - placeholder
    [0xCC] = mos6502_family_cpy_absolute,       // CPY $nnnn
    [0xCD] = mos6502_family_cmp_absolute,       // CMP $nnnn
    [0xCE] = mos6502_family_dec_absolute,       // DEC $nnnn
    [0xCF] = mos6502_family_jam,                // DCP $nnnn (illegal) - placeholder

    [0xD0] = mos6502_family_bne,                // BNE rel
    [0xD1] = mos6502_family_cmp_indirect_y,     // CMP ($nn),Y
    [0xD2] = mos6502_family_jam,                // JAM (illegal)
    [0xD3] = mos6502_family_jam,                // DCP ($nn),Y (illegal) - placeholder
    [0xD4] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0xD5] = mos6502_family_cmp_zero_page_x,    // CMP $nn,X
    [0xD6] = mos6502_family_dec_zero_page_x,    // DEC $nn,X
    [0xD7] = mos6502_family_jam,                // DCP $nn,X (illegal) - placeholder
    [0xD8] = mos6502_family_cld,                // CLD
    [0xD9] = mos6502_family_cmp_absolute_y,     // CMP $nnnn,Y
    [0xDA] = mos6502_family_nop,                // NOP (illegal)
    [0xDB] = mos6502_family_jam,                // DCP $nnnn,Y (illegal) - placeholder
    [0xDC] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0xDD] = mos6502_family_cmp_absolute_x,     // CMP $nnnn,X
    [0xDE] = mos6502_family_dec_absolute_x,     // DEC $nnnn,X
    [0xDF] = mos6502_family_jam,                // DCP $nnnn,X (illegal) - placeholder

    [0xE0] = mos6502_family_cpx_immediate,      // CPX #$nn
    [0xE1] = mos6502_family_sbc_indirect_x,     // SBC ($nn,X)
    [0xE2] = mos6502_family_nop_immediate,      // NOP #$nn (illegal)
    [0xE3] = mos6502_family_jam,                // ISC ($nn,X) (illegal) - placeholder
    [0xE4] = mos6502_family_cpx_zero_page,      // CPX $nn
    [0xE5] = mos6502_family_sbc_zero_page,      // SBC $nn
    [0xE6] = mos6502_family_inc_zero_page,      // INC $nn
    [0xE7] = mos6502_family_jam,                // ISC $nn (illegal) - placeholder
    [0xE8] = mos6502_family_inx,                // INX
    [0xE9] = mos6502_family_sbc_immediate,      // SBC #$nn
    [0xEA] = mos6502_family_nop,                // NOP
    [0xEB] = mos6502_family_sbc_immediate,      // SBC #$nn (illegal)
    [0xEC] = mos6502_family_cpx_absolute,       // CPX $nnnn
    [0xED] = mos6502_family_sbc_absolute,       // SBC $nnnn
    [0xEE] = mos6502_family_inc_absolute,       // INC $nnnn
    [0xEF] = mos6502_family_jam,                // ISC $nnnn (illegal) - placeholder

    [0xF0] = mos6502_family_beq,                // BEQ rel
    [0xF1] = mos6502_family_sbc_indirect_y,     // SBC ($nn),Y
    [0xF2] = mos6502_family_jam,                // JAM (illegal)
    [0xF3] = mos6502_family_jam,                // ISC ($nn),Y (illegal) - placeholder
    [0xF4] = mos6502_family_nop_zero_page_x,    // NOP $nn,X (illegal)
    [0xF5] = mos6502_family_sbc_zero_page_x,    // SBC $nn,X
    [0xF6] = mos6502_family_inc_zero_page_x,    // INC $nn,X
    [0xF7] = mos6502_family_jam,                // ISC $nn,X (illegal) - placeholder
    [0xF8] = mos6502_family_sed,                // SED
    [0xF9] = mos6502_family_sbc_absolute_y,     // SBC $nnnn,Y
    [0xFA] = mos6502_family_nop,                // NOP (illegal)
    [0xFB] = mos6502_family_jam,                // ISC $nnnn,Y (illegal) - placeholder
    [0xFC] = mos6502_family_nop_absolute_x,     // NOP $nnnn,X (illegal)
    [0xFD] = mos6502_family_sbc_absolute_x,     // SBC $nnnn,X
    [0xFE] = mos6502_family_inc_absolute_x,     // INC $nnnn,X
    [0xFF] = mos6502_family_jam,                // ISC $nnnn,X (illegal) - placeholder
};

// ============================================================================
// OPCODE TABLE INITIALIZATION AND MANAGEMENT
// ============================================================================

/**
 * Initialize CPU with default opcode handler table.
 * This copies the complete base table to the CPU's handler array.
 */
void mos6502_family_init_opcode_table(mos6502_family_t* cpu) {
    memcpy(cpu->opcode_handlers, mos6502_family_default_handlers, sizeof(cpu->opcode_handlers));
}

/**
 * Override a specific opcode handler for CPU-specific behavior.
 * Use this to customize behavior per CPU type (e.g., disable decimal mode).
 */
void mos6502_family_override_opcode(mos6502_family_t* cpu, uint8_t opcode, mos6502_family_opcode_handler_t handler) {
    cpu->opcode_handlers[opcode] = handler;
}
