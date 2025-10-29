/*
 * arithmetic.inc.hpp - Arithmetic Operations for MOS 65xx Family
 *
 * This file contains arithmetic operation implementations (ADC, SBC, CMP, etc.)
 * that are included within the fam65xx_t template class. These operations
 * use the new unified helper functions for optimized performance.
 */

// ============================================================================
// ADD WITH CARRY (ADC)
// ============================================================================

bus_state_t op_adc(bus_state_t pins) {
    // Read operand directly into DL register
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        
        // Use unified ADC operation with processor-specific optimizations
        perform_adc_unified(operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// NO OPERATION (NOP)
// ============================================================================

bus_state_t op_nop(bus_state_t pins) {
    switch (this->opcode_entry.am_index) {
        case AM_IMM:
            // AM_IMM: All immediate NOPs read operand and increment PC
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
            } else {
                return pins;
            }
            break;
            
        case AM_NON:
            // AM_NON: All implicit NOPs do dummy read from PC without increment
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                return pins;
            }
            break;
            
        case AM_ABX: {
            /**
             * Handle NES6502 test syscalls as documented at
             * https://github.com/search?q=repo%3Arofl0r%2Fblargg-6502-cpu-test+syscall&type=code
             * Pattern: $fc, $13, $37 indicates character output syscall
             * Character byte awaits at address 0x2000
             */
            bool is_potential_syscall = false;
            // Handle NES6502 syscall support for "long nop" patterns
            if constexpr (std::is_same_v<ProcessorTag, NES6502Tag>) {
                // Check for syscall pattern: [FC 13] 37
                // Note, that by convention, DL holds the intermediate high byte from the addressing mode
                // so instead of checking DL, we check the most recently read byte on the data bus
                uint8_t opcode = CPU_IR(this);
                uint8_t bus_data = FAM65XX_GET_DATA(pins);
                is_potential_syscall = opcode == 0xFC && bus_data == 0x13;
            }
            // AM_IMM: All immediate NOPs read operand and increment PC
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                // Handle NES6502 syscall support for "long nop" patterns
                if constexpr (std::is_same_v<ProcessorTag, NES6502Tag>) {
                    // Check for syscall pattern: FC 13 [37]
                    uint8_t final_byte = CPU_DL(this);
                    if (is_potential_syscall && final_byte == 0x37) {
                        // Syscall detected - output character from 0x2000
                        if (this->mem_read) {
                            uint8_t character = this->mem_read(this->mem_user_data, 0x2000, 0);
                            if (character != 0) {
                                printf("%c", character);
                                fflush(stdout);
                            }
                        }
                    }
                }
            } else {
                return pins;
            }
            break;
        }
        default:
            // Memory modes: Read from target address and discard
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) {
                return pins;
            }
            break;
    }
    
    // Complete instruction
    this->transition_to_fetch();
    return pins;
}

// ============================================================================
// SUBTRACT WITH CARRY (SBC)
// ============================================================================

bus_state_t op_sbc(bus_state_t pins) {
    // Read operand directly into DL register
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        
        // Use unified SBC operation with processor-specific optimizations
        perform_sbc_unified(operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE ACCUMULATOR (CMP)
// ============================================================================

bus_state_t op_cmp(bus_state_t pins) {
    // Read operand directly into DL register
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t a = CPU_A(this);
        
        // Use optimized unified comparison
        perform_compare_unified(a, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE X REGISTER (CPX)
// ============================================================================

bus_state_t op_cpx(bus_state_t pins) {
    // Read operand directly into DL register
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t x = CPU_X(this);
        
        // Use optimized unified comparison
        perform_compare_unified(x, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE Y REGISTER (CPY)
// ============================================================================

bus_state_t op_cpy(bus_state_t pins) {
    // Read operand directly into DL register
    pins = phi2_read_operand(pins, REG_DL);
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = CPU_DL(this);
        uint8_t y = CPU_Y(this);
        
        // Use optimized unified comparison
        perform_compare_unified(y, operand);
        
        // Complete instruction
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// INCREMENT MEMORY (INC)
// ============================================================================

bus_state_t op_inc(bus_state_t pins) {
    // INC - Increment memory by 1
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Increment the value
        value++;
        // Update N and Z flags using optimized helper
        this->update_nz_flags(value);
    });
}

// ============================================================================
// DECREMENT MEMORY (DEC)
// ============================================================================

bus_state_t op_dec(bus_state_t pins) {
    // DEC - Decrement memory by 1
    // This is a Read-Modify-Write operation
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Decrement the value
        value--;
        // Update N and Z flags using optimized helper
        this->update_nz_flags(value);
    });
}

// ============================================================================
// 16-BIT ARITHMETIC (65C816 only)
// ============================================================================

bus_state_t op_adc_16bit(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 16-bit ADC implementation for 65C816
        // This would be a more complex implementation
        // For now, delegate to 8-bit version
        return op_adc(pins);
    } else {
        // Should not be called on processors without wide registers
        return pins;
    }
}

bus_state_t op_sbc_16bit(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // 16-bit SBC implementation for 65C816
        return op_sbc(pins);
    } else {
        return pins;
    }
}
