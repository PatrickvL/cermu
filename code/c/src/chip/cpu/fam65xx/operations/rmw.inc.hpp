/*
 * rmw.inc - Read-Modify-Write Operations for MOS 65xx Family
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

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
                // Cycle 1: Hardware-accurate dummy write behavior
                if (this->should_complete_write_cycle(pins)) {
                    if constexpr (has_rmw_dummy_write()) {
                        if (!this->should_complete_write_cycle(pins)) {
                            return pins;
                        }

                        // NMOS processors: Write original value back (dummy write) - optimized
                        pins = this->phi2_write(pins, CPU_AB(this), CPU_DL(this));
                    } else {
                        // CMOS processors: Dummy read cycle instead of write
                        pins = this->phi2_read(pins, REG_AB, REG_TMP);
                        if (!FAM65XX_GET_RDY(pins)) {
                            return pins;
                        }
                    }
                    operation_func(CPU_DL(this));
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back with processor-specific RDY handling
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write(pins, CPU_AB(this), CPU_DL(this));
                    transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle with dummy PHI2 read
        pins = this->phi2_read(pins, REG_PC, REG_TMP);
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
        const uint8_t carry_out = value >> 7;  // Extract bit 7 into bit 0 position (FLAG_C)
        value <<= 1;
        // Update flags - ASL only affects N, Z, C (V flag unchanged)
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_out = value & FLAG_C;
        value >>= 1;
        // Update flags - LSR only affects N, Z, C (V flag unchanged)
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_in = CPU_P(this) & FLAG_C;
        const uint8_t carry_out = value >> 7;  // Extract bit 7 into bit 0 position (FLAG_C)
        value = (value << 1) | carry_in;
        // Update flags - ROL only affects N, Z, C (V flag unchanged)
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_in = CPU_P(this) & FLAG_C;
        const uint8_t carry_out = value & FLAG_C;
        value = (value >> 1) | (carry_in << 7);
        // Update flags - ROR only affects N, Z, C (V flag unchanged)
        CPU_P(this) = (CPU_P(this) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out;
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"