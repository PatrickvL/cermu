/*
 * memory.inc - Memory Load/Store Operations for MOS 65xx Family
 *
 * This file contains load and store operation implementations that are
 * included within the fam65xx_t template class. These operations handle
 * data movement between registers and memory.
 */

// ============================================================================
// LOAD ACCUMULATOR (LDA)
// ============================================================================

bus_state_t op_lda(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Load accumulator (8-bit only, wide registers disabled)
    CPU_A(this) = CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_A(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// LOAD X REGISTER (LDX)
// ============================================================================

bus_state_t op_ldx(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Load X register (8-bit only, wide registers disabled)
    CPU_X(this) = CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_X(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// LOAD Y REGISTER (LDY)
// ============================================================================

bus_state_t op_ldy(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Load Y register (8-bit only, wide registers disabled)
    CPU_Y(this) = CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_Y(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// STORE ACCUMULATOR (STA)
// ============================================================================

bus_state_t op_sta(bus_state_t pins) {
    // Store accumulator using current addressing mode result (8-bit only)
    CPU_DL(this) = CPU_A(this);
    pins = this->phi2_write(pins, REG_AB, REG_DL);
    
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Complete instruction (STA doesn't affect flags)
    transition_to_fetch();
    return pins;
}

// ============================================================================
// STORE X REGISTER (STX)
// ============================================================================

bus_state_t op_stx(bus_state_t pins) {
    // Store X register using current addressing mode result (8-bit only)
    CPU_DL(this) = CPU_X(this);
    
    pins = this->phi2_write(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Complete instruction (STX doesn't affect flags)
    transition_to_fetch();
    return pins;
}

// ============================================================================
// STORE Y REGISTER (STY)
// ============================================================================

bus_state_t op_sty(bus_state_t pins) {
    // Store Y register using current addressing mode result (8-bit only)
    CPU_DL(this) = CPU_Y(this);
    
    pins = this->phi2_write(pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Complete instruction (STY doesn't affect flags)
    transition_to_fetch();
    return pins;
}

// ============================================================================
// LOGIC OPERATIONS
// ============================================================================

// AND with Accumulator
bus_state_t op_and(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Perform AND operation
    CPU_A(this) &= CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_A(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// OR with Accumulator
bus_state_t op_ora(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Perform OR operation
    CPU_A(this) |= CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_A(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// Exclusive OR with Accumulator
bus_state_t op_eor(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    // Perform EOR operation
    CPU_A(this) ^= CPU_DL(this);
    
    // Update N and Z flags
    update_nz_flags(CPU_A(this));
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}

// ============================================================================
// BIT TEST OPERATION
// ============================================================================

bus_state_t op_bit(bus_state_t pins) {
    // Read operand with immediate mode handling
    pins = read_operand_immediate_or_memory(pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    uint8_t operand = CPU_DL(this);
    uint8_t result = CPU_A(this) & operand;
    
    // Update flags:
    // N = bit 7 of operand
    // V = bit 6 of operand  
    // Z = result of A & operand
    CPU_P(this) = (CPU_P(this) & 0x3D) |  // Clear N,V,Z
                  (operand & 0x80) |       // N = bit 7 of operand
                  (operand & 0x40) |       // V = bit 6 of operand
                  (result == 0 ? FLAG_Z : 0); // Z = A & operand == 0
    
    // Complete instruction
    transition_to_fetch();
    return pins;
}