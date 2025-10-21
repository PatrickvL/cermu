/*
 * transfers.inc - Register Transfer Operations for MOS 65xx Family
 */

// ============================================================================
// TRANSFER HELPER FUNCTIONS
// ============================================================================

/* Helper for transfer operations with flags */
bus_state_t transfer_with_flags_helper(bus_state_t pins, uint8_t value, uint8_t& target_reg) {
    /* Dummy cycle for internal operation */
    pins = phi2_read(pins, REG_PC, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer and update flags */
    target_reg = value;
    update_nz_flags(target_reg);
    transition_to_fetch();
    return pins;
}

/* Helper for transfer operations without flags */
bus_state_t transfer_no_flags_helper(bus_state_t pins, uint8_t value, uint8_t& target_reg) {
    /* Dummy cycle for internal operation */
    pins = phi2_read(pins, REG_PC, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer without updating flags */
    target_reg = value;
    transition_to_fetch();
    return pins;
}

// ============================================================================
// REGISTER TRANSFER OPERATIONS
// ============================================================================

/* TAX - Transfer A to X */
bus_state_t op_tax(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_A(this), CPU_X(this));
}

/* TAY - Transfer A to Y */
bus_state_t op_tay(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_A(this), CPU_Y(this));
}

/* TSX - Transfer S to X */
bus_state_t op_tsx(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_S(this), CPU_X(this));
}

/* TXA - Transfer X to A */
bus_state_t op_txa(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_X(this), CPU_A(this));
}

/* TXS - Transfer X to S */
bus_state_t op_txs(bus_state_t pins) {
    return transfer_no_flags_helper(pins, CPU_X(this), CPU_S(this));
}

/* TYA - Transfer Y to A */
bus_state_t op_tya(bus_state_t pins) {
    return transfer_with_flags_helper(pins, CPU_Y(this), CPU_A(this));
}