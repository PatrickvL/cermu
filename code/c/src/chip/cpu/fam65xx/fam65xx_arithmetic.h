#ifndef FAM65XX_ARITHMETIC_H
#define FAM65XX_ARITHMETIC_H

#include "fam65xx_core.h"

// ============================================================================
// ARITHMETIC OPERATION DECLARATIONS
// ============================================================================

// Binary mode arithmetic operations - inline for zero call overhead
static inline void fam65xx_op_adc(fam65xx_t* cpu, uint8_t value) {
    uint16_t result = cpu->a + value + (fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow)
    bool overflow = ((cpu->a ^ result) & (value ^ result) & 0x80) != 0;
    fam65xx_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag
    fam65xx_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

static inline void fam65xx_op_sbc(fam65xx_t* cpu, uint8_t value) {
    // SBC is equivalent to ADC with inverted value
    uint8_t inverted_value = ~value;
    uint16_t result = cpu->a + inverted_value + (fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    // Set overflow flag (signed overflow) 
    bool overflow = ((cpu->a ^ result) & (inverted_value ^ result) & 0x80) != 0;
    fam65xx_set_flag(cpu, FLAG_V, overflow);
    
    // Set carry flag (inverted for subtraction)
    fam65xx_set_flag(cpu, FLAG_C, result > 0xFF);
    
    // Update accumulator and set N/Z flags
    cpu->a = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

// ============================================================================
// DECIMAL MODE OPCODE HANDLERS
// ============================================================================

// MOS6502 specific ADC decimal mode opcode handlers
void mos6502_adc_immediate_decimal(fam65xx_t* cpu);
void mos6502_adc_zero_page_decimal(fam65xx_t* cpu);
void mos6502_adc_zero_page_x_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_x_decimal(fam65xx_t* cpu);
void mos6502_adc_absolute_y_decimal(fam65xx_t* cpu);
void mos6502_adc_indirect_x_decimal(fam65xx_t* cpu);
void mos6502_adc_indirect_y_decimal(fam65xx_t* cpu);

// MOS6502 specific SBC decimal mode opcode handlers
void mos6502_sbc_immediate_decimal(fam65xx_t* cpu);
void mos6502_sbc_zero_page_decimal(fam65xx_t* cpu);
void mos6502_sbc_zero_page_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_absolute_y_decimal(fam65xx_t* cpu);
void mos6502_sbc_indirect_x_decimal(fam65xx_t* cpu);
void mos6502_sbc_indirect_y_decimal(fam65xx_t* cpu);


#endif // FAM65XX_ARITHMETIC_H
