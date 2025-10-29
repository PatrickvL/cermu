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
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read(pins, REG_AB, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy write original value back with processor-specific RDY handling
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    operation_func(CPU_DL(this));
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back with processor-specific RDY handling
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, REG_AB, REG_DL);
                    transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle with dummy PHI2 read
        pins = this->phi2_read(pins, REG_PC, REG_DL);
        if (FAM65XX_GET_RDY(pins)) {
            // Perform operation on accumulator (modify step)
            operation_func(CPU_A(this));
            this->transition_to_fetch();
        }
    }
    return pins;
}

// ============================================================================
// READ-MODIFY-WRITE OPERATIONS
// ============================================================================

/* ASL - Arithmetic Shift Left */
bus_state_t op_asl(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        uint8_t carry_out;
        value = this->shift_left(value, carry_out);
        // Update flags
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        uint8_t carry_out;
        value = this->shift_right(value, carry_out);
        // Update flags
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        uint8_t carry_in = this->get_carry_bit_0();
        uint8_t carry_out;
        value = this->rotate_left(value, carry_in, carry_out);
        // Update flags
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        uint8_t carry_in = this->get_carry_bit_7();
        uint8_t carry_out;
        value = this->rotate_right(value, carry_in, carry_out);
        // Update flags
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}