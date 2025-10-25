/*
 * illegal.inc - Illegal/Undocumented Operations for MOS 65xx Family
 *
 * This file contains illegal opcode implementations that are conditionally
 * compiled based on processor support for undocumented opcodes.
 */

// ============================================================================
// ILLEGAL LOAD/STORE COMBINATIONS
// ============================================================================

bus_state_t op_lax(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // Load A and X from memory
        pins = this->phi2_read(pins, REG_AB, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        
        CPU_A(this) = CPU_DL(this);
        CPU_X(this) = CPU_DL(this);
        
        fam65xx_update_nz_flags(this, CPU_A(this));
        fam65xx_transition_to_fetch(this);
        return pins;
    } else {
        // Invalid on processors without illegal opcodes
        return pins;
    }
}

bus_state_t op_sax(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // Store A AND X to memory
        CPU_DL(this) = CPU_A(this) & CPU_X(this);
        pins = this->phi2_write(pins, REG_AB, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        
        fam65xx_transition_to_fetch(this);
        return pins;
    } else {
        return pins;
    }
}

// ============================================================================
// ILLEGAL READ-MODIFY-WRITE OPERATIONS
// ============================================================================

bus_state_t op_dcp(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // DCP - Decrement memory and compare with A (DEC memory, then CMP A with result)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform DEC on memory value
            value--;
            
            // Perform CMP A with decremented value
            uint16_t result = CPU_A(this) - value;
            
            // Set carry flag (inverted for CMP)
            if (result < 0x100) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            
            // Update N and Z flags based on comparison result
            update_nz_flags((uint8_t)result);
        });
    }
    return pins;
}

bus_state_t op_isc(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ISC - Increment memory and subtract from A (INC memory, then SBC A with result)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform INC on memory value
            value++;
            
            // Perform SBC A with incremented value (A = A - value - (1 - C))
            uint16_t result = CPU_A(this) - value - (1 - ((CPU_P(this) & FLAG_C) ? 1 : 0));
            
            // Set carry flag (inverted for SBC)
            if (result < 0x100) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            
            // Set overflow flag for SBC
            bool overflow = ((CPU_A(this) ^ value) & 0x80) && ((CPU_A(this) ^ result) & 0x80);
            if (overflow) {
                CPU_P(this) |= FLAG_V;
            } else {
                CPU_P(this) &= ~FLAG_V;
            }
            
            // Store result in A and update N,Z flags
            CPU_A(this) = (uint8_t)result;
            update_nz_flags(CPU_A(this));
        });
    }
    return pins;
}

bus_state_t op_slo(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SLO - Shift Left and OR with A (ASL memory, then ORA A with result)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform ASL on memory value
            if (value & 0x80) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            value <<= 1;
            
            // Perform ORA with accumulator
            CPU_A(this) |= value;
            update_nz_flags(CPU_A(this));
        });
    }
    return pins;
}

bus_state_t op_rla(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // RLA - Rotate Left and AND with A (ROL memory, then AND A with result)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform ROL on memory value
            uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 1 : 0;
            if (value & 0x80) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            value = (value << 1) | old_carry;
            
            // Perform AND with accumulator
            CPU_A(this) &= value;
            update_nz_flags(CPU_A(this));
        });
    }
    return pins;
}

bus_state_t op_sre(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SRE - Shift Right and EOR with A (LSR memory, then EOR result with A)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform LSR on memory value
            if (value & 0x01) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            value >>= 1;
            
            // Perform EOR with accumulator
            CPU_A(this) ^= value;
            update_nz_flags(CPU_A(this));
        });
    }
    return pins;
}

bus_state_t op_rra(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // RRA - Rotate Right and ADC with A (ROR memory, then ADC A with result)
        // This is a Read-Modify-Write operation
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform ROR on memory value
            uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 0x80 : 0;
            if (value & 0x01) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            value = (value >> 1) | old_carry;
            
            // Perform ADC with accumulator
            uint16_t result = CPU_A(this) + value + ((CPU_P(this) & FLAG_C) ? 1 : 0);
            
            // Set carry flag
            if (result > 0xFF) {
                CPU_P(this) |= FLAG_C;
            } else {
                CPU_P(this) &= ~FLAG_C;
            }
            
            // Set overflow flag for ADC
            bool overflow = !((CPU_A(this) ^ value) & 0x80) && ((CPU_A(this) ^ result) & 0x80);
            if (overflow) {
                CPU_P(this) |= FLAG_V;
            } else {
                CPU_P(this) &= ~FLAG_V;
            }
            
            // Store result in A and update N,Z flags
            CPU_A(this) = (uint8_t)result;
            update_nz_flags(CPU_A(this));
        });
    }
    return pins;
}

// ============================================================================
// PROCESSOR JAM/KILL OPERATION
// ============================================================================

bus_state_t op_jam(bus_state_t pins) {
    // JAM/KIL instruction behavior on 6502:
    // - PC stays at opcode address (does not advance)
    // - Performs dummy reads but stays in infinite loop
    // - Processor effectively halts
    
    // Simple JAM implementation: just dummy read and don't transition to fetch
    // This creates the infinite loop behavior since PC won't advance
    pins = phi2_read(pins, REG_PC, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // JAM: DO NOT call transition_to_fetch() 
    // This keeps the processor stuck on this instruction
    return pins;
}

// ============================================================================
// ILLEGAL ACCUMULATOR OPERATIONS
// ============================================================================

bus_state_t op_anc(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ANC - AND with carry (AND immediate, then copy N flag to C flag)
        pins = phi2_read(pins, REG_AB, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        
        // Perform AND with accumulator
        CPU_A(this) &= CPU_DL(this);
        
        // Update N and Z flags
        update_nz_flags(CPU_A(this));
        
        // Copy N flag to C flag (ANC behavior)
        if (CPU_P(this) & FLAG_N) {
            CPU_P(this) |= FLAG_C;
        } else {
            CPU_P(this) &= ~FLAG_C;
        }
        
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_arr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ARR - AND + ROR with BCD correction (AND immediate, then ROR A)
        pins = phi2_read(pins, REG_AB, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        
        // Perform AND with accumulator
        CPU_A(this) &= CPU_DL(this);
        
        // Perform ROR on accumulator
        uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 0x80 : 0;
        if (CPU_A(this) & 0x01) {
            CPU_P(this) |= FLAG_C;
        } else {
            CPU_P(this) &= ~FLAG_C;
        }
        CPU_A(this) = (CPU_A(this) >> 1) | old_carry;
        
        // Update N and Z flags
        update_nz_flags(CPU_A(this));
        
        // Set V flag based on bit 6 XOR bit 5 of result
        if ((CPU_A(this) & 0x40) ^ ((CPU_A(this) & 0x20) << 1)) {
            CPU_P(this) |= FLAG_V;
        } else {
            CPU_P(this) &= ~FLAG_V;
        }
        
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_alr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ALR - AND + LSR (AND immediate, then LSR A)
        pins = phi2_read(pins, REG_AB, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        
        // Perform AND with accumulator
        CPU_A(this) &= CPU_DL(this);
        
        // Perform LSR on accumulator
        if (CPU_A(this) & 0x01) {
            CPU_P(this) |= FLAG_C;
        } else {
            CPU_P(this) &= ~FLAG_C;
        }
        CPU_A(this) >>= 1;
        
        // Update N and Z flags
        update_nz_flags(CPU_A(this));
        
        transition_to_fetch();
    }
    return pins;
}
