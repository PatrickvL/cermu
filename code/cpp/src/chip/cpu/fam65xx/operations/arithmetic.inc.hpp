/*
 * arithmetic.inc.hpp - Arithmetic Operations for MOS 65xx Family
 *
 * This file contains arithmetic operation implementations (ADC, SBC, CMP, etc.)
 * that are included within the fam65xx_t template class. These operations
 * use the new unified helper functions for optimized performance.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// ADD WITH CARRY (ADC)
// ============================================================================

bus_state_t op_adc(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            // Read operand directly into DL register
            pins = this->phi2_read_operand(pins, static_cast<reg8_t>(REG_DL));
            if (FAM65XX_GET_RDY(pins)) {
                uint8_t operand = this->get(REG_DL);
                
                // Use ADC operation with processor-specific optimizations
                this->perform_adc(operand);
                
                // CMOS processors need extra cycle in decimal mode
                if constexpr (this->has_bcd_extra_cycle()) {
                    if (this->get(REG_P) & FLAG_D) {
                        this->cycle_index++;
                        return pins;
                    }
                }
                
                // Complete instruction if no extra cycle needed
                this->transition_to_fetch();
            }
            return pins;
            
        case 1:
            // Extra cycle for CMOS decimal mode - do dummy read from PC
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

// ============================================================================
// NO OPERATION (NOP)
// ============================================================================

bus_state_t op_nop(bus_state_t pins) {
    // WDC65C02 neutralized illegal opcodes preserve original addressing timing
    if constexpr (this->has_cmos()) {
        if (this->opcode_entry.flags & to_index(OF::RMW)) {
            // RMW mode NOP: Perform full read-modify-write cycle but don't modify the value
            // This preserves the bus cycle timing for WDC65C02 neutralized illegal opcodes
            return this->rmw_operation_helper(pins, [this](uint8_t& value) {
                // NOP operation: read the value but don't modify it
                // This creates the correct bus cycle pattern for WDC65C02 illegal opcodes
                (void)value; // Suppress unused parameter warning
                // No operation performed - value remains unchanged
            });
        }
    }
    
    // Regular NOP handling for non-RMW modes
    switch (this->opcode_entry.am_index) {
        case to_index(AM::IMM):
            // AM_IMM: All immediate NOPs read operand and increment PC
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
            } else {
                return pins;
            }
            break;
            
        case to_index(AM::NON):
            // Implicit NOPs do dummy read from PC without increment
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (!FAM65XX_GET_RDY(pins)) {
                return pins;
            }
            break;

        default:
            // Memory addressing modes (AM_ABS, AM_ABX, AM_ABY, AM_ZER, AM_ZPX, AM_ZPY) need dummy read
            // The addressing mode handler has already consumed operands and set up AB register
            // Now we need to complete the read cycle for proper timing
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_AB));
            if (!FAM65XX_GET_RDY(pins)) {
                return pins;
            }

            /**
             * Handle NES6502 test syscalls as documented at
             * https://github.com/search?q=repo%3Arofl0r%2Fblargg-6502-cpu-test+syscall&type=code
             * Pattern: $fc, $13, $37 indicates character output syscall
             * Character byte awaits at address 0x2000
             */
            // Handle NES6502 syscall support for "long nop" patterns
            if constexpr (this->has_apu()) {
                // Check for syscall pattern: FC 13 37
                // Note : Having this in default instead of a separate case AM_ABX is less host code
                // (at a cost of 1 otherwise needless compare for the other NES6502 memory NOPs)
                if (this->get(REG_IR) == 0xFC && this->get(REG_ABL) == 0x13 && this->get(REG_ABH) == 0x37) {
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
    switch (this->cycle_index) {
        case 0:
            // Read operand directly into DL register
            pins = this->phi2_read_operand(pins, static_cast<reg8_t>(REG_DL));
            if (FAM65XX_GET_RDY(pins)) {
                uint8_t operand = this->get(REG_DL);
                
                // Use SBC operation with processor-specific optimizations
                this->perform_sbc(operand);
                
                // CMOS processors need extra cycle in decimal mode
                if constexpr (this->has_bcd_extra_cycle()) {
                    if (this->get(REG_P) & FLAG_D) {
                        this->cycle_index++;
                        return pins;
                    }
                }
                
                // Complete instruction if no extra cycle needed
                this->transition_to_fetch();
            }
            return pins;
            
        case 1:
            // Extra cycle for CMOS decimal mode - do dummy read from PC
            pins = this->phi2_dummy_read(pins, static_cast<reg16_t>(REG_PC));
            if (FAM65XX_GET_RDY(pins)) {
                this->transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

// ============================================================================
// COMPARE ACCUMULATOR (CMP)
// ============================================================================

bus_state_t op_cmp(bus_state_t pins) {
    // Read operand directly into DL register
    pins = this->phi2_read_operand(pins, static_cast<reg8_t>(REG_DL));
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = this->get(REG_DL);
        uint8_t a = this->get(REG_A);
        
        // Use optimized comparison
        this->perform_compare(a, operand);
        
        // Complete instruction
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE X REGISTER (CPX)
// ============================================================================

bus_state_t op_cpx(bus_state_t pins) {
    // Read operand directly into DL register
    pins = this->phi2_read_operand(pins, static_cast<reg8_t>(REG_DL));
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = this->get(REG_DL);
        uint8_t x = this->get(REG_X);
        
        // Use optimized comparison
        this->perform_compare(x, operand);
        
        // Complete instruction
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// COMPARE Y REGISTER (CPY)
// ============================================================================

bus_state_t op_cpy(bus_state_t pins) {
    // Read operand directly into DL register
    pins = this->phi2_read_operand(pins, static_cast<reg8_t>(REG_DL));
    if (FAM65XX_GET_RDY(pins)) {
        uint8_t operand = this->get(REG_DL);
        uint8_t y = this->get(REG_Y);
        
        // Use optimized comparison
        this->perform_compare(y, operand);
        
        // Complete instruction
        this->transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// INCREMENT MEMORY (INC)
// ============================================================================

bus_state_t op_inc(bus_state_t pins) {
    // INC - Increment memory by 1
    // This is a Read-Modify-Write operation
    return this->rmw_operation_helper(pins, [this](uint8_t& value) {
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
    return this->rmw_operation_helper(pins, [this](uint8_t& value) {
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
    if constexpr (this->has_wide_registers()) {
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
    if constexpr (this->has_wide_registers()) {
        // 16-bit SBC implementation for 65C816
        return op_sbc(pins);
    } else {
        return pins;
    }
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
