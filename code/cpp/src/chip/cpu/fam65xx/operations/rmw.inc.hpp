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
        // ASL: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_out = (val8 >> 7) & FLAG_C;
        val8 <<= 1;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using REG_MEM template parameter to force 8-bit behavior
        this->update_nzc_flags<REG_MEM>(val8, carry_out);
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // LSR: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_out = val8 & FLAG_C;
        val8 >>= 1;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using REG_MEM template parameter to force 8-bit behavior
        this->update_nzc_flags<REG_MEM>(val8, carry_out);
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROL: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = (val8 >> 7) & FLAG_C;
        val8 = (val8 << 1) | carry_in;
        value = val8;  // Store back the 8-bit result
        
        // Update flags using REG_MEM template parameter to force 8-bit behavior
        this->update_nzc_flags<REG_MEM>(val8, carry_out);
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // ROR: For memory operations, always work with 8-bit data regardless of data_t width
        uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        const uint8_t carry_out = val8 & FLAG_C;
        val8 = (val8 >> 1) | (carry_in << 7);
        value = val8;  // Store back the 8-bit result
        
        // Update flags using REG_MEM template parameter to force 8-bit behavior
        this->update_nzc_flags<REG_MEM>(val8, carry_out);
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
