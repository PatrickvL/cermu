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
        // LAX - Load A and X from memory
        // Special case: LAX immediate has unstable behavior - uses (A | 0xEE) & operand
        if (opcode_entry.am_index == AM_IMM) {
            // LAX immediate - unstable behavior with magic constant
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_PC(this)++;
                // Hardware quirk: LAX immediate uses unstable internal state
                // Result is (A | 0xEE) & operand
                uint8_t result = (CPU_A(this) | 0xEE) & CPU_DL(this);
                CPU_A(this) = result;
                CPU_X(this) = result;
                
                update_nz_flags(result);
                transition_to_fetch();
            }
        } else {
            // LAX memory modes - normal behavior
            pins = phi2_read(pins, REG_AB, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                CPU_A(this) = CPU_DL(this);
                CPU_X(this) = CPU_DL(this);
                
                update_nz_flags(CPU_A(this));
                transition_to_fetch();
            }
        }
        return pins;
    } else {
        // Invalid on processors without illegal opcodes
        return pins;
    }
}

bus_state_t op_sax(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // Store A AND X to memory with processor-specific RDY handling
        if (should_complete_write_cycle(pins)) {
            CPU_DL(this) = CPU_A(this) & CPU_X(this);
            pins = phi2_write(pins, REG_AB, REG_DL);
            transition_to_fetch();
        }
    }
    return pins;
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
            // Set carry flag (CMP uses subtraction semantics: carry = no borrow)
            update_flag(FLAG_C, !(result & 0x100));
            
            // Update N and Z flags based on comparison result
            update_nz_flags((uint8_t)result);
        });
    }
    return pins;
}

bus_state_t op_isc(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ISC - Increment memory and subtract from A (INC memory, then SBC A with result)
        // This is a Read-Modify-Write operation - match reference implementation exactly
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform INC on memory value
            value++;
            
            // Perform SBC A with incremented value - exact reference match
            uint8_t operand = value;
            uint8_t carry_in = (CPU_P(this) & FLAG_C) ? 1 : 0;
            uint8_t a_old = CPU_A(this);
            
            // Check for BCD support and decimal mode
            if constexpr (has_bcd<ProcessorTag>()) {
                if (CPU_P(this) & FLAG_D) {
                    // BCD (Decimal) mode - use BCD subtraction helper matching reference implementation
                    uint8_t bcd_result;
                    uint8_t bcd_flags;
                    
                    bcd_subtraction_helper(a_old, operand, (1 - carry_in), &bcd_result, &bcd_flags);
                    
                    CPU_A(this) = bcd_result;
                    CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | bcd_flags;
                } else {
                    // Binary mode even when BCD supported
                    uint16_t result = a_old - operand - (1 - carry_in);
                    CPU_A(this) = result & 0xFF;
                    
                    // SBC modifies only N, V, Z, C flags - preserve all others exactly
                    CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                                 (CPU_A(this) & FLAG_N) |                                    /* N = bit 7 of result */
                                 (CPU_A(this) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                                 (result < 0x100 ? FLAG_C : 0) |                           /* C = no borrow */
                                 (((a_old ^ operand) & (a_old ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
                }
            } else {
                // No BCD support - binary mode only
                uint16_t result = a_old - operand - (1 - carry_in);
                CPU_A(this) = result & 0xFF;
                
                // SBC modifies only N, V, Z, C flags - preserve all others exactly
                CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                             (CPU_A(this) & FLAG_N) |                                    /* N = bit 7 of result */
                             (CPU_A(this) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                             (result < 0x100 ? FLAG_C : 0) |                           /* C = no borrow */
                             (((a_old ^ operand) & (a_old ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
            }
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
            update_flag(FLAG_C, value & 0x80);
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
            uint8_t carry_in = get_carry_bit_0();
            update_flag(FLAG_C, value & 0x80);
            value = (value << 1) | carry_in;
            
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
            update_flag(FLAG_C, value & 0x01);
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
        // This is a Read-Modify-Write operation - match reference implementation exactly
        return rmw_operation_helper(pins, [this](uint8_t& value) {
            // Perform ROR on memory value - exact reference match
            uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 0x80 : 0;
            if (value & 0x01) CPU_P(this) |= FLAG_C;
            else CPU_P(this) &= ~FLAG_C;
            value = (value >> 1) | old_carry;
            
            // Perform ADC with A - exact reference match
            uint8_t operand = value;
            uint8_t carry_in = (CPU_P(this) & FLAG_C) ? 1 : 0;
            uint8_t a_old = CPU_A(this);
            
            // Check for BCD support and decimal mode
            if constexpr (has_bcd<ProcessorTag>()) {
                if (CPU_P(this) & FLAG_D) {
                    // BCD (Decimal) mode - use BCD addition helper matching reference implementation
                    uint8_t bcd_result;
                    uint8_t bcd_flags;
                    
                    bcd_addition_helper(a_old, operand, carry_in, &bcd_result, &bcd_flags);
                    
                    CPU_A(this) = bcd_result;
                    CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) | bcd_flags;
                } else {
                    // Binary mode even when BCD supported
                    uint16_t result = a_old + operand + carry_in;
                    CPU_A(this) = result & 0xFF;
                    
                    // ADC modifies only N, V, Z, C flags - preserve all others exactly
                    CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                                 (CPU_A(this) & FLAG_N) |                                    /* N = bit 7 of result */
                                 (CPU_A(this) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                                 (result > 0xFF ? FLAG_C : 0) |                            /* C = carry out */
                                 (((a_old ^ result) & (operand ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
                }
            } else {
                // No BCD support - binary mode only
                uint16_t result = a_old + operand + carry_in;
                CPU_A(this) = result & 0xFF;
                
                // ADC modifies only N, V, Z, C flags - preserve all others exactly
                CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                             (CPU_A(this) & FLAG_N) |                                    /* N = bit 7 of result */
                             (CPU_A(this) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                             (result > 0xFF ? FLAG_C : 0) |                            /* C = carry out */
                             (((a_old ^ result) & (operand ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
            }
        });
    }
    return pins;
}

// ============================================================================
// PROCESSOR JAM/KILL OPERATION
// ============================================================================

bus_state_t op_jam(bus_state_t pins) {
    // JAM/KIL instruction behavior on 6502:
    // - PC advances to read operand, then resets to opcode address
    // - Performs 3-cycle pattern: opcode read, operand read, operand read
    // - For test compatibility: complete after 3 cycles with PC at opcode address
    
    switch (cycle_index) {
        case 0:
            // PHI2: Read operand from PC+1 (this was PC++ after opcode fetch)
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                cycle_index++;
            }
            return pins;
            
        case 1:
            // PHI2: Read operand again from same address (PC+1)
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                // JAM: Reset PC back to opcode address (the "jam" effect)
                CPU_PC(this)--; // Go back to opcode address
                
                // For test suite compatibility: complete normally instead of infinite loop
                // In real hardware this would loop forever, but tests expect finite execution
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

// ============================================================================
// ILLEGAL ACCUMULATOR OPERATIONS
// ============================================================================

bus_state_t op_anc(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ANC - AND with carry (AND immediate, then copy N flag to C flag)
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            // Perform AND with accumulator
            CPU_A(this) &= CPU_DL(this);
            
            // Update N and Z flags
            update_nz_flags(CPU_A(this));
            
            // Copy N flag to C flag (ANC behavior)
            update_flag(FLAG_C, CPU_P(this) & FLAG_N);
            
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_arr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ARR - AND + ROR with BCD correction in decimal mode (reference implementation)
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            
            uint8_t operand = CPU_DL(this);
            bool carry_in = (CPU_P(this) & FLAG_C) != 0;
            
            // Step 1: AND A with operand - save original for BCD checks
            uint8_t original_a = CPU_A(this) & operand;
            CPU_A(this) = original_a;
            
            // Step 2: ROR the result (both modes do this)
            uint8_t shifted_a = (CPU_A(this) >> 1) | (carry_in ? 0x80 : 0);
            
            // Clear all flags initially
            CPU_P(this) &= ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C);
            
            // Set N and Z flags based on shifted result (reference does this first)
            update_nz_flags(shifted_a);
            
            if constexpr (has_bcd<ProcessorTag>()) {
                if (CPU_P(this) & FLAG_D) {
                    // Decimal mode - ARR uses reference algorithm
                    
                    // Set V flag based on bit 6 change between original and shifted
                    if ((shifted_a ^ CPU_A(this)) & 0x40) {
                        CPU_P(this) |= FLAG_V;
                    }
                    
                    // BCD correction using ORIGINAL A value for digit checks (reference approach)
                    uint8_t result = shifted_a;
                    
                    // Low nibble BCD correction - use ORIGINAL A value for threshold check
                    if ((CPU_A(this) & 0x0F) >= 5) {
                        result = ((result + 6) & 0x0F) | (result & 0xF0);
                    }
                    
                    // High nibble BCD correction and carry - use ORIGINAL A value for threshold check
                    if ((CPU_A(this) & 0xF0) >= 0x50) {
                        result += 0x60;
                        CPU_P(this) |= FLAG_C;
                    }
                    
                    CPU_A(this) = result;
                    
                    // DO NOT update N and Z flags after BCD correction - reference keeps original flags
                } else {
                    // Binary mode - special C and V flag behavior
                    CPU_A(this) = shifted_a;
                    
                    // ARR has special C and V flag behavior:
                    // C = bit 6 of result (not the shifted-out bit!)
                    // V = bit 6 XOR bit 5 of result
                    if (CPU_A(this) & 0x40) {
                        CPU_P(this) |= FLAG_C | FLAG_V;
                    }
                    if (CPU_A(this) & 0x20) {
                        CPU_P(this) ^= FLAG_V;
                    }
                }
            } else {
                // Binary mode - special C and V flag behavior (for processors without BCD)
                CPU_A(this) = shifted_a;
                
                // ARR has special C and V flag behavior:
                // C = bit 6 of result (not the shifted-out bit!)
                // V = bit 6 XOR bit 5 of result
                if (CPU_A(this) & 0x40) {
                    CPU_P(this) |= FLAG_C | FLAG_V;
                }
                if (CPU_A(this) & 0x20) {
                    CPU_P(this) ^= FLAG_V;
                }
            }
            
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_alr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // ALR - AND + LSR (AND immediate, then LSR A)
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            // Perform AND with accumulator
            CPU_A(this) &= CPU_DL(this);
            
            // Perform LSR on accumulator
            update_flag(FLAG_C, CPU_A(this) & 0x01);
            CPU_A(this) >>= 1;
            
            // Update N and Z flags
            update_nz_flags(CPU_A(this));
            
            transition_to_fetch();
        }
    }
    return pins;
}

// ASR is an alias for ALR (same operation, different names in documentation)
bus_state_t op_asr(bus_state_t pins) {
    return op_alr(pins);
}

// ============================================================================
// ADDITIONAL ILLEGAL OPERATIONS
// ============================================================================

bus_state_t op_xaa(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // XAA - Transfer X AND immediate to A (illegal)
        // Hardware quirk: Uses unstable constant 0xEE like LAX immediate
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            // Hardware behavior: (A | 0xEE) & X & operand
            CPU_A(this) = (CPU_A(this) | 0xEE) & CPU_X(this) & CPU_DL(this);
            update_nz_flags(CPU_A(this));
            
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_sbx(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SBX - Compare X with A AND immediate (illegal) (also called AXS)
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            CPU_PC(this)++;
            uint8_t temp = CPU_A(this) & CPU_X(this);
            uint8_t result = temp - CPU_DL(this);
            // Update X with result
            CPU_X(this) = result;
            
            // Set carry flag using standard subtraction semantics (carry = no borrow)
            update_flag(FLAG_C, temp >= CPU_DL(this));
            
            update_nz_flags(result);
            
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_sha(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SHA - Store A AND X AND (high byte of effective address + 1) (illegal)
        // HARDWARE BEHAVIOR: SHA has conditional address corruption based on page crossing
        if (should_complete_write_cycle(pins)) {
            // Get the current effective address after addressing mode completed
            uint16_t effective_addr = CPU_AB(this);
            uint8_t current_high = CPU_ABH(this);
            uint8_t current_low = CPU_ABL(this);
            
            // Determine if page crossing occurred during addressing calculation
            bool page_crossed = false;
            uint8_t original_high = current_high;
            
            if (opcode_entry.am_index == AM_ABY) {
                // Absolute,Y: base_addr + Y
                // Reconstruct base address by subtracting Y
                uint16_t base_low = (current_low - CPU_Y(this)) & 0xFF;
                if (base_low + CPU_Y(this) > 0xFF) {
                    page_crossed = true;
                    original_high = current_high - 1; // The high byte before page crossing fix
                }
            } else if (opcode_entry.am_index == AM_INY) {
                // Indirect,Y: (zp) + Y
                // Reconstruct base address by subtracting Y
                uint16_t base_low = (current_low - CPU_Y(this)) & 0xFF;
                if (base_low + CPU_Y(this) > 0xFF) {
                    page_crossed = true;
                    original_high = current_high - 1; // The high byte before page crossing fix
                }
            }
            
            // Calculate data to store: A & X & (high_byte + 1)
            // Hardware quirk: Data calculation uses the SAME high byte as address corruption
            uint8_t data_high_byte = page_crossed ? original_high : current_high;
            uint8_t data = CPU_A(this) & CPU_X(this) & ((data_high_byte + 1) & 0xFF);
            CPU_DL(this) = data;
            
            // Apply address corruption if page was crossed
            if (page_crossed) {
                // Corrupt address high byte: A & X & (original_high + 1)
                CPU_ABH(this) = CPU_A(this) & CPU_X(this) & ((original_high + 1) & 0xFF);
            }
            
            pins = phi2_write(pins, REG_AB, REG_DL);
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_shs(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SHS - Store A & X to memory, AND set S = A & X & (high byte + 1)
        // Fixed to match reference implementation behavior
        if (should_complete_write_cycle(pins)) {
            // Calculate A & X first
            uint8_t ax_value = CPU_A(this) & CPU_X(this);
            
            // Set stack pointer to A & X & (high byte + 1)
            CPU_S(this) = ax_value & ((CPU_ABH(this) + 1) & 0xFF);
            
            // Store A & X to memory (not the stack pointer value)
            CPU_DL(this) = ax_value;
            pins = phi2_write(pins, REG_AB, REG_DL);
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_shx(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SHX - Store X AND ((high byte of effective address) + 1) (illegal)
        if (should_complete_write_cycle(pins)) {
            CPU_DL(this) = CPU_X(this) & ((CPU_ABH(this) + 1) & 0xFF);
            pins = phi2_write(pins, REG_AB, REG_DL);
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_shy(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // SHY - Store Y AND ((high byte of effective address) + 1) (illegal)
        if (should_complete_write_cycle(pins)) {
            CPU_DL(this) = CPU_Y(this) & ((CPU_ABH(this) + 1) & 0xFF);
            pins = phi2_write(pins, REG_AB, REG_DL);
            transition_to_fetch();
        }
    }
    return pins;
}

bus_state_t op_las(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // LAS - Load A, X, and S with memory AND stack pointer (illegal)
        pins = phi2_read(pins, REG_AB, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            uint8_t result = CPU_DL(this) & CPU_S(this);
            CPU_A(this) = result;
            CPU_X(this) = result;
            CPU_S(this) = result;
            
            update_nz_flags(result);
            transition_to_fetch();
        }
    }
    return pins;
}
