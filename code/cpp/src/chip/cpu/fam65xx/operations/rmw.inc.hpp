/*
 * rmw.inc - Read-Modify-Write Operations for MOS 65xx Family
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// READ-MODIFY-WRITE OPERATIONS
// ============================================================================

/* ASL - Arithmetic Shift Left */
bus_state_t op_asl(bus_state_t pins) {
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
        // Memory mode - multi-cycle RMW operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read<Addr::AB>(pins, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy cycle and perform modification
                if (this->has_rmw_dummy_write()) {
                    // NMOS: Dummy write of original value
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                } else {
                    // CMOS: Dummy read instead of write
                    pins = this->phi2_dummy_read<Addr::AB>(pins);
                }
                
                if (FAM65XX_GET_RDY(pins)) {
                    uint8_t value = this->get(REG_DL);
                    // ASL operation
                    const uint8_t carry_out = (value >> 7) & FLAG_C;
                    value <<= 1;
                    this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                                  this->calc_nz_flags(value) | carry_out);
                    this->set(REG_DL, value);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified value back to memory
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle operation
        pins = this->phi2_dummy_read<Addr::PC>(pins);
        if (FAM65XX_GET_RDY(pins)) {
            uint8_t value = this->get(REG_A);
            // ASL operation
            const uint8_t carry_out = (value >> 7) & FLAG_C;
            value <<= 1;
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
            this->set(REG_A, value);
            this->transition_to_fetch();
        }
    }
    return pins;
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
        // Memory mode - multi-cycle RMW operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read<Addr::AB>(pins, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy cycle and perform modification
                if (this->has_rmw_dummy_write()) {
                    // NMOS: Dummy write of original value
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                } else {
                    // CMOS: Dummy read instead of write
                    pins = this->phi2_dummy_read<Addr::AB>(pins);
                }
                
                if (FAM65XX_GET_RDY(pins)) {
                    uint8_t value = this->get(REG_DL);
                    // LSR operation - RIGHT shift (not left!)
                    const uint8_t carry_out = value & FLAG_C;
                    value >>= 1;
                    this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                                  this->calc_nz_flags(value) | carry_out);
                    this->set(REG_DL, value);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified value back to memory
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle operation
        pins = this->phi2_dummy_read<Addr::PC>(pins);
        if (FAM65XX_GET_RDY(pins)) {
            uint8_t value = this->get(REG_A);
            // LSR operation - RIGHT shift (not left!)
            const uint8_t carry_out = value & FLAG_C;
            value >>= 1;
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
            this->set(REG_A, value);
            this->transition_to_fetch();
        }
    }
    return pins;
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
        // Memory mode - multi-cycle RMW operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read<Addr::AB>(pins, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy cycle and perform modification
                if (this->has_rmw_dummy_write()) {
                    // NMOS: Dummy write of original value
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                } else {
                    // CMOS: Dummy read instead of write
                    pins = this->phi2_dummy_read<Addr::AB>(pins);
                }
                
                if (FAM65XX_GET_RDY(pins)) {
                    uint8_t value = this->get(REG_DL);
                    // ROL operation
                    const uint8_t carry_in = this->get(REG_P) & FLAG_C;
                    const uint8_t carry_out = (value >> 7) & FLAG_C;
                    value = (value << 1) | carry_in;
                    this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                                  this->calc_nz_flags(value) | carry_out);
                    this->set(REG_DL, value);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified value back to memory
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle operation
        pins = this->phi2_dummy_read<Addr::PC>(pins);
        if (FAM65XX_GET_RDY(pins)) {
            uint8_t value = this->get(REG_A);
            // ROL operation
            const uint8_t carry_in = this->get(REG_P) & FLAG_C;
            const uint8_t carry_out = (value >> 7) & FLAG_C;
            value = (value << 1) | carry_in;
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
            this->set(REG_A, value);
            this->transition_to_fetch();
        }
    }
    return pins;
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
        // Memory mode - multi-cycle RMW operation
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read<Addr::AB>(pins, REG_DL);
                if (FAM65XX_GET_RDY(pins)) {
                    this->cycle_index++;
                }
                return pins;
                
            case 1:
                // Cycle 1: Dummy cycle and perform modification
                if (this->has_rmw_dummy_write()) {
                    // NMOS: Dummy write of original value
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                } else {
                    // CMOS: Dummy read instead of write
                    pins = this->phi2_dummy_read<Addr::AB>(pins);
                }
                
                if (FAM65XX_GET_RDY(pins)) {
                    uint8_t value = this->get(REG_DL);
                    // ROR operation
                    const uint8_t carry_in = this->get(REG_P) & FLAG_C;
                    const uint8_t carry_out = value & FLAG_C;
                    value = (value >> 1) | (carry_in << 7);
                    this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                                  this->calc_nz_flags(value) | carry_out);
                    this->set(REG_DL, value);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified value back to memory
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle operation
        pins = this->phi2_dummy_read<Addr::PC>(pins);
        if (FAM65XX_GET_RDY(pins)) {
            uint8_t value = this->get(REG_A);
            // ROR operation
            const uint8_t carry_in = this->get(REG_P) & FLAG_C;
            const uint8_t carry_out = value & FLAG_C;
            value = (value >> 1) | (carry_in << 7);
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
            this->set(REG_A, value);
            this->transition_to_fetch();
        }
    }
    return pins;
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
