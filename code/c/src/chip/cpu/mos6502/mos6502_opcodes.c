#include "mos6502_opcodes.h"
#include <stdio.h>

// ============================================================================
// MOS 6502 ARITHMETIC OPERATIONS
// ============================================================================

void mos6502_op_adc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    bool carry_in = fam65xx_get_flag(cpu, FLAG_C);
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal mode ADC
        uint8_t A = cpu->a;
        uint8_t s = operand;
        uint8_t C = carry_in ? 1 : 0;
          // Lower nibble calculation
        uint8_t al = (A & 0x0f) + (s & 0x0f) + C;
        
        // Upper nibble calculation with carry from lower nibble
        uint8_t ah = (A >> 4) + (s >> 4);
        if (al > 0x09) {  // BCD carry at > 9, not > 15
            ah += 1;  // Carry from lower nibble
        }
        
        // BCD adjustments
        if (al > 0x09) {
            al += 0x06;
        }
        if (ah > 0x09) {
            ah += 0x06;
        }
        
        // Calculate binary result for flag setting
        uint16_t binary_sum = A + s + C;
        
        // Set flags
        fam65xx_set_flag(cpu, FLAG_C, ah > 0x0f);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_sum & 0xFF) == 0);
        fam65xx_set_flag(cpu, FLAG_N, (binary_sum & 0x80) != 0);
        
        // V flag: overflow if both inputs have same sign, but result has different sign
        uint8_t binary_result = binary_sum & 0xFF;
        bool overflow = ((A ^ binary_result) & (s ^ binary_result) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Update accumulator with BCD result
        cpu->a = ((ah & 0x0f) << 4) | (al & 0x0f);
    } else {
        // Binary mode ADC
        uint16_t result = cpu->a + operand + (carry_in ? 1 : 0);
        uint8_t result_8 = result & 0xFF;
        
        fam65xx_set_flag(cpu, FLAG_C, result > 0xFF);
        fam65xx_set_flag(cpu, FLAG_Z, result_8 == 0);
        fam65xx_set_flag(cpu, FLAG_N, (result_8 & 0x80) != 0);
        
        // V flag: overflow if both inputs have same sign, but result has different sign
        bool overflow = ((cpu->a ^ result_8) & (operand ^ result_8) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        cpu->a = result_8;
    }
}

void mos6502_op_sbc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal mode SBC
        uint8_t A = cpu->a;
        uint8_t s = operand;
        bool carry_flag = fam65xx_get_flag(cpu, FLAG_C);
        uint8_t borrow = carry_flag ? 0 : 1;  // Carry clear = borrow needed
        
        // Calculate the lower nibble
        uint8_t AL = (A & 15) - (s & 15) - borrow;
        
        // BCD fixup for lower nibble
        if (AL & 16) AL -= 6;
        
        // Calculate the upper nibble
        uint8_t AH = (A >> 4) - (s >> 4) - (AL & 16 ? 1 : 0);
        
        // BCD fixup for upper nibble
        if (AH & 16) AH -= 6;
        
        // The flags are set just like in Binary mode (NOT affected by decimal mode)
        uint16_t binary_result = A - s - borrow;
        fam65xx_set_flag(cpu, FLAG_C, (binary_result & 256) == 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 255) == 0);
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 128) != 0);
        
        // V flag
        bool overflow = ((binary_result ^ s) & 128) && ((A ^ s) & 128);
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Update accumulator
        cpu->a = ((AH << 4) | (AL & 15)) & 255;
    } else {
        // Binary mode SBC
        bool borrow = !fam65xx_get_flag(cpu, FLAG_C);
        
        uint16_t result = cpu->a - operand - (borrow ? 1 : 0);
        uint8_t result_8 = result & 0xFF;
        
        fam65xx_set_flag(cpu, FLAG_C, (result & 0x8000) == 0); // No borrow
        fam65xx_set_flag(cpu, FLAG_Z, result_8 == 0);
        fam65xx_set_flag(cpu, FLAG_N, (result_8 & 0x80) != 0);
        
        // V flag: overflow in subtraction
        bool overflow = ((cpu->a ^ result_8) & ((~operand) ^ result_8) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        cpu->a = result_8;
    }
}

// ============================================================================
// OPCODE IMPLEMENTATIONS
// ============================================================================

// ADC - Add with Carry (supports both binary and decimal modes)
void mos6502_op_adc_imm(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_imm, mos6502_op_adc);
}

void mos6502_op_adc_zp(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_zp, mos6502_op_adc);
}

void mos6502_op_adc_zpx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_zpx, mos6502_op_adc);
}

void mos6502_op_adc_abs(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_abs, mos6502_op_adc);
}

void mos6502_op_adc_absx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_absx, mos6502_op_adc);
}

void mos6502_op_adc_absy(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_absy, mos6502_op_adc);
}

void mos6502_op_adc_indx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_indx, mos6502_op_adc);
}

void mos6502_op_adc_indy(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_indy, mos6502_op_adc);
}

// SBC - Subtract with Carry (supports both binary and decimal modes)
void mos6502_op_sbc_imm(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_imm, mos6502_op_sbc);
}

void mos6502_op_sbc_zp(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_zp, mos6502_op_sbc);
}

void mos6502_op_sbc_zpx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_zpx, mos6502_op_sbc);
}

void mos6502_op_sbc_abs(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_abs, mos6502_op_sbc);
}

void mos6502_op_sbc_absx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_absx, mos6502_op_sbc);
}

void mos6502_op_sbc_absy(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_absy, mos6502_op_sbc);
}

void mos6502_op_sbc_indx(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_indx, mos6502_op_sbc);
}

void mos6502_op_sbc_indy(mos6502_t* cpu) {
    fam65xx_arithmetic_helper(&cpu->base, fam65xx_addr_indy, mos6502_op_sbc);
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
