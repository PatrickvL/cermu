/*
 * rmw.inc - Read-Modify-Write Operations for MOS 65xx Family
 */

// ============================================================================
// RMW HELPER FUNCTIONS
// ============================================================================

/* Helper for RMW operations that support both accumulator and memory modes */
template<typename OperationFunc>
bus_state_t rmw_operation_helper(bus_state_t pins, OperationFunc operation_func) {
    if (this->opcode_entry.flags & OF_RMW) {
        // Memory mode - 3-cycle RMW operation (hardware-accurate)
        switch (this->cycle_index++) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                return pins;
                
            case 1:
                // Cycle 1: Dummy write original value back (hardware behavior)
                pins = this->phi2_write(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins))
                    // Perform operation on the read data (modify step)
                    operation_func(CPU_DL(this));
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back to memory
                pins = this->phi2_write(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins))
                    this->transition_to_fetch();
                return pins;
        }
    } else {
        // Accumulator mode - single cycle with dummy PHI2 read
        pins = this->phi2_read(pins, REG_PC, REG_DL);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        // Perform operation on accumulator (modify step)
        operation_func(CPU_A(this));
        this->transition_to_fetch();
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
        update_c_flag(value, 7);
        // Shift left
        value <<= 1;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Set carry flag from bit 0
        update_c_flag(value, 0);
        // Shift right
        value >>= 1;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Get old carry flag
        uint8_t carry_in = this->get_carry_bit_0();
        // Set new carry flag from bit 7
        update_c_flag(value, 7);
        // Rotate left with old carry
        value = (value << 1) | carry_in;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        // Get old carry flag
        uint8_t carry_in = this->get_carry_bit_7();
        // Set new carry flag from bit 0
        update_c_flag(value, 0);
        // Rotate right with old carry
        value = (value >> 1) | carry_in;
        // Update N and Z flags
        this->update_nz_flags(value);
    });
}