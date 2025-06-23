#ifndef MOS6502_OPCODES_H
#define MOS6502_OPCODES_H

#include "mos6502.h"

// ============================================================================
// MOS 6502 SPECIFIC OPCODE IMPLEMENTATIONS
// ============================================================================

// Arithmetic operations with decimal mode support
void mos6502_adc_decimal(mos6502_t* cpu, uint8_t operand);
void mos6502_adc_binary(mos6502_t* cpu, uint8_t operand);
void mos6502_sbc_decimal(mos6502_t* cpu, uint8_t operand);
void mos6502_sbc_binary(mos6502_t* cpu, uint8_t operand);

// ADC opcode implementations (all addressing modes)
void mos6502_op_adc_imm(mos6502_t* cpu);    // $69
void mos6502_op_adc_zp(mos6502_t* cpu);     // $65
void mos6502_op_adc_zpx(mos6502_t* cpu);    // $75
void mos6502_op_adc_abs(mos6502_t* cpu);    // $6D
void mos6502_op_adc_absx(mos6502_t* cpu);   // $7D
void mos6502_op_adc_absy(mos6502_t* cpu);   // $79
void mos6502_op_adc_indx(mos6502_t* cpu);   // $61
void mos6502_op_adc_indy(mos6502_t* cpu);   // $71

// SBC opcode implementations (all addressing modes)
void mos6502_op_sbc_imm(mos6502_t* cpu);    // $E9
void mos6502_op_sbc_zp(mos6502_t* cpu);     // $E5
void mos6502_op_sbc_zpx(mos6502_t* cpu);    // $F5
void mos6502_op_sbc_abs(mos6502_t* cpu);    // $ED
void mos6502_op_sbc_absx(mos6502_t* cpu);   // $FD
void mos6502_op_sbc_absy(mos6502_t* cpu);   // $F9
void mos6502_op_sbc_indx(mos6502_t* cpu);   // $E1
void mos6502_op_sbc_indy(mos6502_t* cpu);   // $F1

// Opcode table initialization
void mos6502_init_opcode_table(mos6502_t* cpu);

#endif // MOS6502_OPCODES_H
