#include "mos6502_opcodes.h"
#include <stdio.h>

// ============================================================================
// MOS 6502 ARITHMETIC WITH FULL DECIMAL MODE SUPPORT
// ============================================================================

void mos6502_adc_decimal(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    fam65xx_t* base = &cpu->base;
    bool carry_in = fam65xx_get_flag(base, FLAG_C);
    
    // Implementation based on 6502.proc.info.txt specification
    uint8_t A = base->a;
    uint8_t s = operand;
    uint8_t C = carry_in ? 1 : 0;
    
    // Calculate the lower nibble
    uint8_t AL = (A & 15) + (s & 15) + C;
    
    // Calculate the upper nibble (BEFORE BCD fixup of lower nibble)
    uint8_t AH = (A >> 4) + (s >> 4) + (AL > 15 ? 1 : 0);
    
    // BCD fixup for lower nibble
    if (AL > 9) AL += 6;
    
    // Calculate binary result for N, Z, V flags (set with binary logic)
    uint16_t binary_sum = A + s + C;
    uint8_t binary_result = binary_sum & 255;
    
    // Zero flag is set just like in Binary mode
    fam65xx_set_flag(base, FLAG_Z, binary_result == 0);
    
    // Negative and Overflow flags are set with the same logic than in Binary mode,
    // but after fixing the lower nibble
    fam65xx_set_flag(base, FLAG_N, (binary_result & 0x80) != 0);
    
    // V flag: overflow in binary arithmetic  
    bool overflow = ((A ^ binary_result) & (s ^ binary_result) & 0x80) != 0;
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    // BCD fixup for upper nibble
    if (AH > 9) AH += 6;
    
    // Carry is the only flag set after fixing the result
    fam65xx_set_flag(base, FLAG_C, AH > 15);
    
    // Update accumulator
    base->a = ((AH << 4) | (AL & 15)) & 255;
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
    
    // Implementation based on 6502.proc.info.txt specification
    uint8_t A = base->a;
    uint8_t s = operand;
    uint8_t C = fam65xx_get_flag(base, FLAG_C) ? 1 : 0;
    
    // Calculate the lower nibble
    uint8_t AL = (A & 15) - (s & 15) - (C ? 0 : 1);
    
    // BCD fixup for lower nibble
    if (AL & 16) AL -= 6;
    
    // Calculate the upper nibble
    uint8_t AH = (A >> 4) - (s >> 4) - (AL & 16 ? 1 : 0);
    
    // BCD fixup for upper nibble
    if (AH & 16) AH -= 6;
    
    // The flags are set just like in Binary mode (NOT affected by decimal mode)
    uint16_t binary_result = A - s - (C ? 0 : 1);
    fam65xx_set_flag(base, FLAG_C, (binary_result & 256) == 0);
    fam65xx_set_flag(base, FLAG_Z, (binary_result & 255) == 0);
    fam65xx_set_flag(base, FLAG_N, (binary_result & 128) != 0);
    
    // V flag
    bool overflow = ((binary_result ^ s) & 128) && ((A ^ s) & 128);
    fam65xx_set_flag(base, FLAG_V, overflow);
    
    // Update accumulator
    base->a = ((AH << 4) | (AL & 15)) & 255;
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
    uint8_t addr_lo = fam65xx_read_cycle(base, 0xFFFE);
    uint8_t addr_hi = fam65xx_read_cycle(base, 0xFFFF);
    
    base->pc = (addr_hi << 8) | addr_lo;
}

// NOP - No Operation
void mos6502_op_nop(mos6502_t* cpu) {
    FAM65XX_OPCODE_FOOTER(&cpu->base);
}

// ============================================================================
// OPCODE TABLE INITIALIZATION
// ============================================================================

void mos6502_init_opcode_table(mos6502_t* cpu) {
    if (!cpu) return;
    
    // Initialize with base family opcodes and decimal mode support
    uint32_t features = FAM65XX_FEATURE_DECIMAL_MODE | FAM65XX_FEATURE_ILLEGAL_OPCODES;
    fam65xx_init_opcode_table(&cpu->base, features);
    
    // Override ADC opcodes with MOS6502-specific decimal implementations
    fam65xx_override_opcode(&cpu->base, 0x69, (fam65xx_opcode_handler_t)mos6502_op_adc_imm);   // ADC #$nn
    fam65xx_override_opcode(&cpu->base, 0x65, (fam65xx_opcode_handler_t)mos6502_op_adc_zp);    // ADC $nn
    fam65xx_override_opcode(&cpu->base, 0x75, (fam65xx_opcode_handler_t)mos6502_op_adc_zpx);   // ADC $nn,X
    fam65xx_override_opcode(&cpu->base, 0x6D, (fam65xx_opcode_handler_t)mos6502_op_adc_abs);   // ADC $nnnn
    fam65xx_override_opcode(&cpu->base, 0x7D, (fam65xx_opcode_handler_t)mos6502_op_adc_absx);  // ADC $nnnn,X
    fam65xx_override_opcode(&cpu->base, 0x79, (fam65xx_opcode_handler_t)mos6502_op_adc_absy);  // ADC $nnnn,Y
    fam65xx_override_opcode(&cpu->base, 0x61, (fam65xx_opcode_handler_t)mos6502_op_adc_indx);  // ADC ($nn,X)
    fam65xx_override_opcode(&cpu->base, 0x71, (fam65xx_opcode_handler_t)mos6502_op_adc_indy);  // ADC ($nn),Y
    
    // Override SBC opcodes with MOS6502-specific decimal implementations
    fam65xx_override_opcode(&cpu->base, 0xE9, (fam65xx_opcode_handler_t)mos6502_op_sbc_imm);   // SBC #$nn
    fam65xx_override_opcode(&cpu->base, 0xE5, (fam65xx_opcode_handler_t)mos6502_op_sbc_zp);    // SBC $nn
    fam65xx_override_opcode(&cpu->base, 0xF5, (fam65xx_opcode_handler_t)mos6502_op_sbc_zpx);   // SBC $nn,X
    fam65xx_override_opcode(&cpu->base, 0xED, (fam65xx_opcode_handler_t)mos6502_op_sbc_abs);   // SBC $nnnn
    fam65xx_override_opcode(&cpu->base, 0xFD, (fam65xx_opcode_handler_t)mos6502_op_sbc_absx);  // SBC $nnnn,X
    fam65xx_override_opcode(&cpu->base, 0xF9, (fam65xx_opcode_handler_t)mos6502_op_sbc_absy);  // SBC $nnnn,Y
    fam65xx_override_opcode(&cpu->base, 0xE1, (fam65xx_opcode_handler_t)mos6502_op_sbc_indx);  // SBC ($nn,X)
    fam65xx_override_opcode(&cpu->base, 0xF1, (fam65xx_opcode_handler_t)mos6502_op_sbc_indy);  // SBC ($nn),Y
}
