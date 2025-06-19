#ifndef FAM65XX_ARITHMETIC_H
#define FAM65XX_ARITHMETIC_H

#include <stdint.h>

// Forward declaration to avoid circular dependency
typedef struct fam65xx_s fam65xx_t;

// ============================================================================
// ARITHMETIC OPERATION FUNCTION DECLARATIONS
// ============================================================================

// Generic family arithmetic operations (used by all family members)
void fam65xx_op_adc(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_sbc(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_and(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_ora(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_eor(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_cmp(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_cpx(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_cpy(fam65xx_t* cpu, uint8_t value);
void fam65xx_op_bit(fam65xx_t* cpu, uint8_t value);

// MOS6502-specific decimal mode operation functions
void mos6502_op_adc_decimal(fam65xx_t* cpu, uint8_t value);
void mos6502_op_sbc_decimal(fam65xx_t* cpu, uint8_t value);

// MOS6502-specific decimal-aware opcode handlers
void mos6502_adc_immediate_decimal(fam65xx_t* cpu);
void mos6502_adc_zero_page_decimal(fam65xx_t* cpu);
void mos6502_adc_zero_page_x_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_x_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_y_decimal(fam65xx_t* cpu);
void mos6502_adc_indirect_x_decimal(fam65xx_t* cpu);
void mos6502_adc_indirect_y_decimal(fam65xx_t* cpu);

void mos6502_sbc_immediate_decimal(fam65xx_t* cpu);
void mos6502_sbc_zero_page_decimal(fam65xx_t* cpu);
void mos6502_sbc_zero_page_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_y_decimal(fam65xx_t* cpu);
void mos6502_sbc_indirect_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_indirect_y_decimal(fam65xx_t* cpu);

// Arithmetic helper function types
typedef uint8_t (*fam65xx_addr_func_t)(fam65xx_t* cpu);
typedef void (*fam65xx_op_func_t)(fam65xx_t* cpu, uint8_t value);

#endif // FAM65XX_ARITHMETIC_H
