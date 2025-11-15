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
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ASL: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_out = (val8 >> 7) & FLAG_C;
        val8 <<= 1;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using explicit 8-bit functions for memory operations
        this->update_nz_flags_8bit(val8);
        this->update_flag(FLAG_C, carry_out != 0);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // LSR: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_out = val8 & FLAG_C;
        val8 >>= 1;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using explicit 8-bit functions for memory operations
        this->update_nz_flags_8bit(val8);
        this->update_flag(FLAG_C, carry_out != 0);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROL: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = (val8 >> 7) & FLAG_C;
        val8 = (val8 << 1) | carry_in;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using explicit 8-bit functions for memory operations
        this->update_nz_flags_8bit(val8);
        this->update_flag(FLAG_C, carry_out != 0);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROR: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = val8 & FLAG_C;
        val8 = (val8 >> 1) | (carry_in << 7);
        value = val8;  // Store back the 8-bit result
        
        // Update flags using explicit 8-bit functions for memory operations
        this->update_nz_flags_8bit(val8);
        this->update_flag(FLAG_C, carry_out != 0);
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
