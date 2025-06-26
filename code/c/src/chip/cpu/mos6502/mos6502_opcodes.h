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

// Opcode table initialization
void mos6502_init_opcode_table(mos6502_t* cpu);

#endif // MOS6502_OPCODES_H
