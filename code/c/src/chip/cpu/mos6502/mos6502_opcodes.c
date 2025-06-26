#include "mos6502_opcodes.h"
#include "../fam65xx/fam65xx_arithmetic.h"
#include <stdio.h>

// ============================================================================
// MOS 6502 ARITHMETIC OPERATIONS
// ============================================================================

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
    // Only set up the base table with decimal mode and illegal opcode support.
    // Do NOT override ADC/SBC handlers here; let fam65xx_init_opcode_table handle it based on the feature flag.
    uint32_t features = FAM65XX_FEATURE_DECIMAL_MODE | FAM65XX_FEATURE_ILLEGAL_OPCODES;
    fam65xx_init_opcode_table(&cpu->base, features);
}
