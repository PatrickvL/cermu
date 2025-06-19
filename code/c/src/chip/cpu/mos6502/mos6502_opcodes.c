#include "mos6502_opcodes.h"
#include <stdio.h>

// ============================================================================
// MOS 6502 ARITHMETIC WITH FULL DECIMAL MODE SUPPORT
// ============================================================================

void mos6502_adc_decimal(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    fam65xx_t* base = &cpu->base;
    bool carry_in = fam65xx_get_flag(base, FLAG_C);
    
    // BCD Addition: Handle each nibble separately
    uint8_t acc_lo = base->a & 0x0F;
    uint8_t acc_hi = (base->a >> 4) & 0x0F;
    uint8_t op_lo = operand & 0x0F;
    uint8_t op_hi = (operand >> 4) & 0x0F;
    
    // Add low nibbles with carry
    uint8_t result_lo = acc_lo + op_lo + (carry_in ? 1 : 0);
    bool carry_to_hi = false;
    if (result_lo > 9) {
        result_lo += 6; // BCD correction
        carry_to_hi = true;
    }
    result_lo &= 0x0F;
    
    // Add high nibbles with carry from low nibble
    uint8_t result_hi = acc_hi + op_hi + (carry_to_hi ? 1 : 0);
    bool carry_out = false;
    if (result_hi > 9) {
        result_hi += 6; // BCD correction
        carry_out = true;
    }
    result_hi &= 0x0F;
    
    uint8_t bcd_result = (result_hi << 4) | result_lo;
    
    // Calculate binary result for flag setting (N, Z, V flags use binary math)
    uint16_t binary_result = base->a + operand + (carry_in ? 1 : 0);
    uint8_t binary_result_8 = binary_result & 0xFF;
    
    // Set flags based on binary operation (6502 behavior)
    fam65xx_set_flag(base, FLAG_C, carry_out);
    fam65xx_set_flag(base, FLAG_Z, binary_result_8 == 0);
    fam65xx_set_flag(base, FLAG_N, (binary_result_8 & 0x80) != 0);
    
    // V flag: overflow in binary arithmetic
    bool overflow = ((base->a ^ binary_result_8) & (operand ^ binary_result_8) & 0x80) != 0;
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    base->a = bcd_result;
}

void mos6502_adc_binary(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    fam65xx_t* base = &cpu->base;
    bool carry_in = fam65xx_get_flag(base, FLAG_C);
    
    uint16_t result = base->a + operand + (carry_in ? 1 : 0);
    uint8_t result_8 = result & 0xFF;
    
    fam65xx_set_flag(base, FLAG_C, result > 0xFF);
    fam65xx_set_flag(base, FLAG_Z, result_8 == 0);
    fam65xx_set_flag(base, FLAG_N, (result_8 & 0x80) != 0);
    
    // V flag: overflow if both inputs have same sign, but result has different sign
    bool overflow = ((base->a ^ result_8) & (operand ^ result_8) & 0x80) != 0;
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    base->a = result_8;
}

void mos6502_sbc_decimal(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    fam65xx_t* base = &cpu->base;
    bool borrow = !fam65xx_get_flag(base, FLAG_C); // Carry clear means borrow
    
    // BCD Subtraction: Handle each nibble separately
    uint8_t acc_lo = base->a & 0x0F;
    uint8_t acc_hi = (base->a >> 4) & 0x0F;
    uint8_t op_lo = operand & 0x0F;
    uint8_t op_hi = (operand >> 4) & 0x0F;
    
    // Subtract low nibbles with borrow
    int16_t result_lo = acc_lo - op_lo - (borrow ? 1 : 0);
    bool borrow_from_hi = false;
    if (result_lo < 0) {
        result_lo -= 6; // BCD correction
        borrow_from_hi = true;
    }
    result_lo &= 0x0F;
    
    // Subtract high nibbles with borrow from low nibble
    int16_t result_hi = acc_hi - op_hi - (borrow_from_hi ? 1 : 0);
    bool borrow_out = false;
    if (result_hi < 0) {
        result_hi -= 6; // BCD correction
        borrow_out = true;
    }
    result_hi &= 0x0F;
    
    uint8_t bcd_result = (result_hi << 4) | result_lo;
    
    // Calculate binary result for flag setting
    uint16_t binary_result = base->a - operand - (borrow ? 1 : 0);
    uint8_t binary_result_8 = binary_result & 0xFF;
    
    // Set flags based on binary operation
    fam65xx_set_flag(base, FLAG_C, !borrow_out); // Carry set means no borrow
    fam65xx_set_flag(base, FLAG_Z, binary_result_8 == 0);
    fam65xx_set_flag(base, FLAG_N, (binary_result_8 & 0x80) != 0);
    
    // V flag: overflow in binary arithmetic
    bool overflow = ((base->a ^ binary_result_8) & ((~operand) ^ binary_result_8) & 0x80) != 0;
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    base->a = bcd_result;
}

void mos6502_sbc_binary(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    fam65xx_t* base = &cpu->base;
    bool borrow = !fam65xx_get_flag(base, FLAG_C);
    
    uint16_t result = base->a - operand - (borrow ? 1 : 0);
    uint8_t result_8 = result & 0xFF;
    
    fam65xx_set_flag(base, FLAG_C, (result & 0x8000) == 0); // No borrow
    fam65xx_set_flag(base, FLAG_Z, result_8 == 0);
    fam65xx_set_flag(base, FLAG_N, (result_8 & 0x80) != 0);
    
    // V flag: overflow in subtraction
    bool overflow = ((base->a ^ result_8) & ((~operand) ^ result_8) & 0x80) != 0;
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    base->a = result_8;
}

// ============================================================================
// OPCODE IMPLEMENTATIONS
// ============================================================================

// ADC - Add with Carry (supports both binary and decimal modes)
void mos6502_op_adc_imm(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_imm(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_zp(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_zp(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_zpx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_zpx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_abs(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_abs(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_absx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_absx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_absy(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_absy(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_indx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_indx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_adc_indy(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_indy(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_adc_decimal(cpu, operand);
    } else {
        mos6502_adc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

// SBC - Subtract with Carry (supports both binary and decimal modes)
void mos6502_op_sbc_imm(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_imm(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_zp(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_zp(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_zpx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_zpx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_abs(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_abs(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_absx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_absx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_absy(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_absy(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_indx(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_indx(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

void mos6502_op_sbc_indy(mos6502_t* cpu) {
    uint8_t operand = fam65xx_addr_indy(&cpu->base);
    if (fam65xx_get_flag(&cpu->base, FLAG_D)) {
        mos6502_sbc_decimal(cpu, operand);
    } else {
        mos6502_sbc_binary(cpu, operand);
    }
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

// BRK - Break instruction
void mos6502_op_brk(mos6502_t* cpu) {
    fam65xx_t* base = &cpu->base;
    
    // BRK is a 2-byte instruction
    base->pc++; // Skip signature byte
    
    // Push return address (PC after signature byte)
    fam65xx_push(base, (base->pc >> 8) & 0xFF);
    fam65xx_push(base, base->pc & 0xFF);
    
    // Push status register with B flag set
    fam65xx_push(base, base->p | FLAG_B);
    
    // Set interrupt disable flag
    fam65xx_set_flag(base, FLAG_I, true);
    
    // Jump to IRQ vector
    FAM65XX_INTRA_CYCLE(base);
    uint8_t addr_lo = fam65xx_read_cycle(base, 0xFFFE);
    FAM65XX_INTRA_CYCLE(base);
    uint8_t addr_hi = fam65xx_read_cycle(base, 0xFFFF);
    
    base->pc = (addr_hi << 8) | addr_lo;
}

// NOP - No Operation
void mos6502_op_nop(mos6502_t* cpu) {
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}
