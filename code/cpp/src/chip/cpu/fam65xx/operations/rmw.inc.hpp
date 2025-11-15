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
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ASL: shift left, carry out from MSB using 16-bit detection
        const uint8_t carry_out = this->is_register_16bit<REG_A>()
            ? ((value >> 15) & FLAG_C)
            : ((value >> 7) & FLAG_C);
        value <<= 1;
        
        // Update flags using consolidated helper with automatic register detection
        this->update_nzc_flags<REG_A>(value, carry_out);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // LSR: shift right, carry out from LSB (always bit 0 regardless of width)
        const uint8_t carry_out = value & FLAG_C;
        value >>= 1;
        
        // Update flags using consolidated helper with automatic register detection
        this->update_nzc_flags<REG_A>(value, carry_out);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROL: rotate left through carry
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = this->is_register_16bit<REG_A>()
            ? ((value >> 15) & FLAG_C)
            : ((value >> 7) & FLAG_C);
        value = (value << 1) | carry_in;
        
        // Update flags using consolidated helper with automatic register detection
        this->update_nzc_flags<REG_A>(value, carry_out);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROR: rotate right through carry
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = value & FLAG_C;
        
        // Calculate high bit position using 16-bit detection
        const data_t high_bit_shift = this->is_register_16bit<REG_A>() ? 15 : 7;
        value = (value >> 1) | (carry_in << high_bit_shift);
        
        // Update flags using consolidated helper with automatic register detection
        this->update_nzc_flags<REG_A>(value, carry_out);
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
