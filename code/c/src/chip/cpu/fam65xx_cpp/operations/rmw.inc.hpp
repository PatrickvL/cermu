/*
 * rmw.inc - Read-Modify-Write Operations for MOS 65xx Family
 */

// ============================================================================
// RMW HELPER FUNCTIONS
// ============================================================================

/* Helper for RMW operations that support both accumulator and memory modes */
template<typename OperationFunc>
bus_state_t rmw_operation_helper(bus_state_t pins, OperationFunc operation_func) {
    if (this->opcode_entry.rmw) {
        // Memory mode - 3-cycle RMW operation
        switch (this->cycle_index++) {
            case 0:
                // Cycle 0: Read from memory
                pins = phi2_read(pins, REG_AB, REG_DL);
                if (!FAM65XX_GET_RDY(pins)) return pins;
                return pins;
                
            case 1:
                // Cycle 1: Dummy write (hardware behavior), then perform operation
                pins = phi2_write_internal(pins, REG_AB, REG_DL);
                if (!FAM65XX_GET_RDY(pins)) return pins;
                // Perform operation on the data
                operation_func(CPU_DL(this));
                return pins;
                
            case 2:
                // Cycle 2: Write result back to memory
                pins = phi2_write_internal(pins, REG_AB, REG_DL);
                if (!FAM65XX_GET_RDY(pins)) return pins;
                transition_to_fetch();
                return pins;
        }
    } else {
        // Accumulator mode - single cycle with dummy PHI2 read
        pins = phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        // Perform operation on accumulator
        operation_func(CPU_A(this));
        transition_to_fetch();
    }
    return pins;
}

// ============================================================================
// READ-MODIFY-WRITE OPERATIONS
// ============================================================================

/* ASL - Arithmetic Shift Left */
bus_state_t op_asl(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Set carry flag from bit 7
        if (value & 0x80) CPU_P(this) |= FLAG_C;
        else CPU_P(this) &= ~FLAG_C;
        // Shift left
        value <<= 1;
        // Update N and Z flags
        update_nz_flags(value);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Set carry flag from bit 0
        if (value & 0x01) CPU_P(this) |= FLAG_C;
        else CPU_P(this) &= ~FLAG_C;
        // Shift right
        value >>= 1;
        // Update N and Z flags
        update_nz_flags(value);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Get old carry flag
        uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 1 : 0;
        // Set new carry flag from bit 7
        if (value & 0x80) CPU_P(this) |= FLAG_C;
        else CPU_P(this) &= ~FLAG_C;
        // Rotate left with old carry
        value = (value << 1) | old_carry;
        // Update N and Z flags
        update_nz_flags(value);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Get old carry flag
        uint8_t old_carry = (CPU_P(this) & FLAG_C) ? 0x80 : 0;
        // Set new carry flag from bit 0
        if (value & 0x01) CPU_P(this) |= FLAG_C;
        else CPU_P(this) &= ~FLAG_C;
        // Rotate right with old carry
        value = (value >> 1) | old_carry;
        // Update N and Z flags
        update_nz_flags(value);
    });
}