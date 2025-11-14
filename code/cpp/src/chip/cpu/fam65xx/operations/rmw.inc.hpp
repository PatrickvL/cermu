/*
 * rmw.inc - Read-Modify-Write Operations for MOS 65xx Family
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// RMW HELPER FUNCTIONS
// ============================================================================

/* Helper for 16-bit RMW operations in 65C816 native mode with M=0 */
template<typename OperationFunc>
bus_state_t rmw_16bit_helper(bus_state_t pins, OperationFunc operation_func) {
    switch (this->cycle_index) {
        case 0:
            // Cycle 0: Read low byte from memory
            pins = this->phi2_read<Addr::AB>(pins, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            // Cycle 1: Read high byte from memory
            this->inc(REG_AB);
            pins = this->phi2_read<Addr::AB>(pins, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 2:
        {
            // Cycle 2: Dummy read cycle (65C816 is CMOS, no dummy write)
            pins = this->phi2_dummy_read<Addr::AB>(pins);
            if (!FAM65XX_GET_RDY(pins)) {
                return pins;
            }
            
            // Perform 16-bit operation
            this->set(REG_ABL, this->get(REG_DL));
            uint16_t value = this->get(REG_AB);
            operation_func(value);
            this->set(REG_AB, value);
            
            this->cycle_index++;
            return pins;
        }
        
        case 3:
            // Cycle 3: Write high byte back to memory
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write<Addr::AB>(pins, this->get(REG_ABH));
                this->cycle_index++;
            }
            return pins;
            
        case 4:
            // Cycle 4: Write low byte back to memory
            this->dec(REG_AB);
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write<Addr::AB>(pins, this->get(REG_ABL));
                this->transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* Helper for RMW operations that support both accumulator and memory modes */
template<typename OperationFunc8, typename OperationFunc16>
bus_state_t rmw_operation_helper(bus_state_t pins, OperationFunc8 operation_func_8bit, OperationFunc16 operation_func_16bit) {
    // Check for 65C816 native mode with 16-bit memory operations (M=0)
    if constexpr (this->has_wide_registers()) {
        if (!this->get_emulation_mode() && !(this->get(REG_P) & FLAG_M) &&
            (this->opcode_entry.flags & to_index(OF::RMW))) {
            // 65C816 native mode, 16-bit memory operation
            return rmw_16bit_helper(pins, operation_func_16bit);
        }
    }
    
    // Standard 8-bit RMW operations
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
                    if constexpr (this->has_rmw_dummy_write()) {
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
                    operation_func_8bit(value);
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
            // Check for 65C816 native mode with 16-bit accumulator (M=0)
            if constexpr (this->has_wide_registers()) {
                if (!this->get_emulation_mode() && !(this->get(REG_P) & FLAG_M)) {
                    // 16-bit accumulator operation
                    uint16_t value = this->get(REG_A_16);
                    operation_func_16bit(value);
                    this->set(REG_A_16, value);
                    this->transition_to_fetch();
                    return pins;
                }
            }
            
            // Standard 8-bit accumulator operation
            uint8_t value = this->get(REG_A);
            operation_func_8bit(value);
            this->set(REG_A, value);
            this->transition_to_fetch();
        }
    }
    return pins;
}

/* Overload for RMW operations that only support 8-bit mode */
template<typename OperationFunc8>
bus_state_t rmw_operation_helper(bus_state_t pins, OperationFunc8 operation_func_8bit) {
    // For operations that only support 8-bit mode, create a dummy 16-bit lambda
    auto dummy_16bit_func = [](uint16_t& value) {
        // This should never be called for non-65C816 processors or 8-bit only operations
        (void)value; // Suppress unused parameter warning
    };
    
    return rmw_operation_helper(pins, operation_func_8bit, dummy_16bit_func);
}

// ============================================================================
// READ-MODIFY-WRITE OPERATIONS
// ============================================================================

/* ASL - Arithmetic Shift Left */
bus_state_t op_asl(bus_state_t pins) {
    return rmw_operation_helper(pins,
        [this](uint8_t& value) {
            const uint8_t carry_out = value >> 7;  // Extract bit 7 into bit 0 position (FLAG_C)
            value <<= 1;
            // Update flags - ASL only affects N, Z, C (V flag unchanged)
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
        },
        [this](uint16_t& value) {
            const uint8_t carry_out = (value >> 15) & 1;  // Extract bit 15 into bit 0 position (FLAG_C)
            value <<= 1;
            // Update flags - ASL only affects N, Z, C (V flag unchanged)
            this->update_flag(FLAG_C, carry_out != 0);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
        }
    );
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return rmw_operation_helper(pins,
        [this](uint8_t& value) {
            const uint8_t carry_out = value & 1;  // Get bit 0 for carry flag
            value >>= 1;
            // Update flags - LSR only affects N, Z, C (V flag unchanged)
            // LSR always clears N flag since bit 7 becomes 0
            uint8_t new_flags = (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C));
            if (value == 0) new_flags |= FLAG_Z;  // Set Z if result is zero
            if (carry_out) new_flags |= FLAG_C;   // Set C if bit 0 was set
            // N flag stays clear (LSR always produces positive result)
            this->set(REG_P, new_flags);
        },
        [this](uint16_t& value) {
            const uint8_t carry_out = value & 1;
            value >>= 1;
            // Update flags - LSR only affects N, Z, C (V flag unchanged)
            this->update_flag(FLAG_C, carry_out != 0);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
        }
    );
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return rmw_operation_helper(pins,
        [this](uint8_t& value) {
            const uint8_t carry_in = this->get(REG_P) & FLAG_C;
            const uint8_t carry_out = value >> 7;  // Extract bit 7 into bit 0 position (FLAG_C)
            value = (value << 1) | carry_in;
            // Update flags - ROL only affects N, Z, C (V flag unchanged)
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
        },
        [this](uint16_t& value) {
            const bool carry_in = (this->get(REG_P) & FLAG_C) != 0;
            const uint8_t carry_out = (value >> 15) & 1;  // Extract bit 15 into bit 0 position (FLAG_C)
            value = (value << 1) | (carry_in ? 1 : 0);
            // Update flags - ROL only affects N, Z, C (V flag unchanged)
            this->update_flag(FLAG_C, carry_out != 0);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
        }
    );
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return rmw_operation_helper(pins,
        [this](uint8_t& value) {
            const uint8_t carry_in = this->get(REG_P) & FLAG_C;
            const uint8_t carry_out = value & 1;  // Fix: get bit 0, not FLAG_C
            value = (value >> 1) | (carry_in << 7);
            // Update flags - ROR only affects N, Z, C (V flag unchanged)
            this->set(REG_P, (this->get(REG_P) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                          this->calc_nz_flags(value) | carry_out);
        },
        [this](uint16_t& value) {
            const uint16_t carry_in = (this->get(REG_P) & FLAG_C) ? 0x8000 : 0;
            const uint8_t carry_out = value & 1;
            value = (value >> 1) | carry_in;
            // Update flags - ROR only affects N, Z, C (V flag unchanged)
            this->update_flag(FLAG_C, carry_out != 0);
            this->update_flag(FLAG_Z, value == 0);
            this->update_flag(FLAG_N, (value & 0x8000) != 0);
        }
    );
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
