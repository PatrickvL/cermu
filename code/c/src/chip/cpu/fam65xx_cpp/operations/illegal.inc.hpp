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
        // TODO: Implement DCP - Decrement memory and compare with A
        // For now, just dummy read and transition to fetch
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_isc(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement ISC - Increment memory and subtract from A
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_slo(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement SLO - Shift left and OR with A
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_rla(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement RLA - Rotate left and AND with A
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_sre(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement SRE - Shift right and EOR with A
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_rra(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement RRA - Rotate right and ADC with A
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
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
        // TODO: Implement ANC - AND with carry
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_arr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement ARR - AND + ROR with BCD correction
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}

bus_state_t op_alr(bus_state_t pins) {
    if constexpr (has_illegal_opcodes<ProcessorTag>()) {
        // TODO: Implement ALR - AND + LSR
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        transition_to_fetch();
    }
    return pins;
}
