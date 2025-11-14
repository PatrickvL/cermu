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
    if (this->opcode_entry.flags & to_index(OF::RMW)) {
        // Memory mode - 3-cycle RMW operation (hardware-accurate)
        switch (this->cycle_index) {
            case 0:
                // Cycle 0: Read original value from memory
                pins = this->phi2_read<Addr::AB>(pins, REG_DL);
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
                        pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    } else {
                        // CMOS processors: Dummy read cycle instead of write
                        pins = this->phi2_dummy_read<Addr::AB>(pins);
                        if (!FAM65XX_GET_RDY(pins)) {
                            return pins;
                        }
                    }
                    uint8_t value = this->get(REG_DL);
                    operation_func(value);
                    this->set(REG_DL, value);
                    this->cycle_index++;
                }
                return pins;
                
            case 2:
                // Cycle 2: Write modified result back with processor-specific RDY handling
                if (this->should_complete_write_cycle(pins)) {
                    pins = this->phi2_write<Addr::AB>(pins, this->get(REG_DL));
                    this->transition_to_fetch();
                }
                return pins;
        }
    } else {
        // Accumulator mode - single cycle with dummy PHI2 read
        pins = this->phi2_dummy_read<Addr::PC>(pins);
        if (FAM65XX_GET_RDY(pins)) {
            // Perform operation on accumulator (modify step)
            uint8_t value = this->get(REG_A);
            operation_func(value);
            this->set(REG_A, value);
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
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    // Check for 65C816 native mode with 16-bit memory operations (M=0)
    if constexpr (this->has_wide_registers()) {
        if (!this->get_emulation_mode() && !(this->get(REG_P) & FLAG_M) && 
            (this->opcode_entry.flags & to_index(OF::RMW))) {
            // 65C816 native mode, 16-bit memory operation - perform 16-bit LSR
            switch (this->cycle_index) {
                case 0:
                    // Cycle 0: Read low byte from memory
                    pins = this->phi2_read<Addr::AB>(pins, REG_DL);
                    if (FAM65XX_GET_RDY(pins)) {
                        this->inc(REG_AB);
                        this->cycle_index++;
                    }
                    return pins;
                    
                case 1:
                    // Cycle 1: Read high byte from memory
                    pins = this->phi2_read<Addr::AB>(pins, REG_ABH);
                    if (FAM65XX_GET_RDY(pins)) {
                        this->cycle_index++;
                    }
                    return pins;
                    
                case 2:
                {
                    // Cycle 2: Hardware-accurate dummy cycle
                    if constexpr (this->has_rmw_dummy_write()) {
                        // NMOS processors: Write original high byte back (dummy write)
                        pins = this->phi2_write<Addr::AB>(pins, this->get(REG_ABH));
                    } else {
                        // CMOS processors: Dummy read cycle instead of write
                        pins = this->phi2_dummy_read<Addr::AB>(pins);
                        if (!FAM65XX_GET_RDY(pins)) {
                            return pins;
                        }
                    }
                    
                    // Perform 16-bit LSR operation
                    this->set(REG_ABL, this->get(REG_DL));
                    uint16_t value = this->get(REG_AB);
                    const uint8_t carry_out = value & FLAG_C;
                    value >>= 1;
                    this->set(REG_AB, value);
                    
                    // Update flags - LSR only affects N, Z, C (V flag unchanged)
                    this->update_flag(FLAG_C, carry_out != 0);
                    this->update_flag(FLAG_Z, value == 0);
                    this->update_flag(FLAG_N, (value & 0x8000) != 0);
                    
                    this->cycle_index++;
                    return pins;
                }
                    
                case 3:
                    // Cycle 3: Write high byte back to memory
                    if (this->should_complete_write_cycle(pins)) {
                        pins = this->phi2_write<Addr::AB>(pins, this->get(REG_ABH));
                        this->dec(REG_AB);
                        this->cycle_index++;
                    }
                    return pins;
                    
                case 4:
                    // Cycle 4: Write low byte back to memory
                    if (this->should_complete_write_cycle(pins)) {
                        pins = this->phi2_write<Addr::AB>(pins, this->get(REG_ABL));
                        this->transition_to_fetch();
                    }
                    return pins;
            }
            return pins;
        }
    }
    
    // Standard 8-bit LSR operation (emulation mode, non-wide CPUs, and accumulator mode)
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_out = value & FLAG_C;
        value >>= 1;
        // Update flags - LSR only affects N, Z, C (V flag unchanged)
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = value >> 7;  // Extract bit 7 into bit 0 position (FLAG_C)
        value = (value << 1) | carry_in;
        // Update flags - ROL only affects N, Z, C (V flag unchanged)
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins, [this](uint8_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = value & FLAG_C;
        value = (value >> 1) | (carry_in << 7);
        // Update flags - ROR only affects N, Z, C (V flag unchanged)
        this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                      this->calc_nz_flags(value) | carry_out);
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
