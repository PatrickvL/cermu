/*
 * control.inc - Control Flow Operations for MOS 65xx Family
 *
 * This file contains control flow operation implementations (JMP, JSR, RTS, etc.)
 * that are included within the fam65xx_t template class.
 */

// ============================================================================
// JUMP OPERATIONS
// ============================================================================

/* JMP - Jump */
bus_state_t op_jmp(bus_state_t pins) {
    /* Set PC to target address (addressing mode has already set up AB register) */
    CPU_PC(this) = CPU_AB(this);
    transition_to_fetch();
    return pins;
}

/* JSR - Jump to Subroutine (simplified implementation) */
bus_state_t op_jsr(bus_state_t pins) {
    // For now, implement as simple jump - full JSR requires stack manipulation
    // TODO: Implement proper JSR with return address stack push
    CPU_PC(this) = CPU_AB(this);
    transition_to_fetch();
    return pins;
}

/* RTS - Return from Subroutine (simplified implementation) */
bus_state_t op_rts(bus_state_t pins) {
    // TODO: Implement proper RTS with stack pull
    // For now, just transition to fetch to avoid crashes
    transition_to_fetch();
    return pins;
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

/* BRK - Break (simplified implementation) */
bus_state_t op_brk(bus_state_t pins) {
    // TODO: Implement proper BRK with interrupt sequence
    // For now, set interrupt flag and transition to fetch
    CPU_P(this) |= FLAG_I;
    transition_to_fetch();
    return pins;
}

/* RTI - Return from Interrupt (simplified implementation) */
bus_state_t op_rti(bus_state_t pins) {
    // TODO: Implement proper RTI with stack pulls
    // For now, just transition to fetch to avoid crashes
    transition_to_fetch();
    return pins;
}

// ============================================================================
// 65C02 ENHANCED CONTROL OPERATIONS
// ============================================================================

bus_state_t op_bra(bus_state_t pins) {
    if constexpr (has_cmos_enhancements<ProcessorTag>()) {
        // Branch always - 65C02 enhancement
        // Implementation would go here
        return pins;
    } else {
        return pins; // Invalid on NMOS
    }
}

// ============================================================================
// 65C816 LONG OPERATIONS
// ============================================================================

bus_state_t op_jsl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Jump to Subroutine Long - 65C816
        return pins;
    } else {
        return pins;
    }
}

bus_state_t op_rtl(bus_state_t pins) {
    if constexpr (has_wide_registers<ProcessorTag>()) {
        // Return from Subroutine Long - 65C816
        return pins;
    } else {
        return pins;
    }
}