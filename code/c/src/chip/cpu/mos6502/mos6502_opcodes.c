#include "mos6502_opcodes.h"
#include "../fam65xx/fam65xx_arithmetic.h"
#include <stdio.h>

// ============================================================================
// MOS 6502 ARITHMETIC OPERATIONS
// ============================================================================

void mos6502_op_adc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic - MOS6502 specific implementation
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
        uint8_t A = cpu->a;
        uint8_t s = operand;
        
        // Convert to BCD
        uint8_t al = (A & 0x0F) + (s & 0x0F) + carry_in;
        uint8_t ah = (A >> 4) + (s >> 4);
        
        // Handle low nibble carry
        if (al > 9) {
            al = (al + 6) & 0x0F;
            ah++;
        }
        
        // Set N and Z flags based on binary result (6502 quirk)
        uint16_t binary_result = A + s + carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        
        // Set overflow flag based on binary result
        bool overflow = ((A ^ binary_result) & (s ^ binary_result) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Handle high nibble carry
        if (ah > 9) {
            ah = (ah + 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, true);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, false);
        }
        
        // Update accumulator with BCD result
        cpu->a = (ah << 4) | al;
    } else {
        // Binary mode ADC - use inlined base family implementation
        fam65xx_op_adc(cpu, operand);
    }
}

void mos6502_op_sbc(fam65xx_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    if (fam65xx_get_flag(cpu, FLAG_D)) {
        // Decimal (BCD) mode arithmetic - MOS6502 specific implementation
        uint8_t carry_in = fam65xx_get_flag(cpu, FLAG_C) ? 0 : 1; // Inverted for SBC
        uint8_t A = cpu->a;
        uint8_t s = operand;
        
        // Convert to BCD for subtraction
        int8_t al = (A & 0x0F) - (s & 0x0F) - carry_in;
        int8_t ah = (A >> 4) - (s >> 4);
        
        // Handle low nibble borrow
        if (al < 0) {
            al = (al - 6) & 0x0F;
            ah--;
        }
        
        // Set N and Z flags based on binary result (6502 quirk)
        uint16_t binary_result = A - s - carry_in;
        fam65xx_set_flag(cpu, FLAG_N, (binary_result & 0x80) != 0);
        fam65xx_set_flag(cpu, FLAG_Z, (binary_result & 0xFF) == 0);
        
        // Set overflow flag based on binary result
        bool overflow = ((A ^ binary_result) & ((~s) ^ binary_result) & 0x80) != 0;
        fam65xx_set_flag(cpu, FLAG_V, overflow);
        
        // Handle high nibble borrow
        if (ah < 0) {
            ah = (ah - 6) & 0x0F;
            fam65xx_set_flag(cpu, FLAG_C, false);
        } else {
            fam65xx_set_flag(cpu, FLAG_C, true);
        }
        
        // Update accumulator with BCD result
        cpu->a = (ah << 4) | al;
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
