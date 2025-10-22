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
// NOTE: 65C816 long operations (op_jsl, op_rtl) are implemented in wide.inc.hpp
// NOTE: 65C02 branch always (op_bra) is implemented in cmos.inc.hpp
// ============================================================================