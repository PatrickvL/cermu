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
        if (this->opcode_entry.flags & to_index(OF::RMW)) {
            // Memory operation: always work with 8-bit data
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = (val8 >> 7) & FLAG_C;
            val8 <<= 1;
            value = val8;
            this->update_nz_flags_8bit(val8);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // Accumulator operation: work with full register width
            if (this->is_accumulator_16bit()) {
                const uint16_t carry_out = (value >> 15) & FLAG_C;
                value = (value << 1) & 0xFFFF;
                this->update_nz_flags<REG_A>(value);
                this->update_flag(FLAG_C, carry_out != 0);
            } else {
                uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
                const uint8_t carry_out = (val8 >> 7) & FLAG_C;
                val8 <<= 1;
                value = val8;
                this->update_nz_flags<REG_A>(val8);
                this->update_flag(FLAG_C, carry_out != 0);
            }
        }
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        if (this->opcode_entry.flags & to_index(OF::RMW)) {
            // Memory operation: always work with 8-bit data
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = val8 & FLAG_C;
            val8 >>= 1;
            value = val8;
            this->update_nz_flags_8bit(val8);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // Accumulator operation: work with full register width
            if (this->is_accumulator_16bit()) {
                const uint16_t carry_out = value & FLAG_C;
                value = (value >> 1) & 0x7FFF;
                this->update_nz_flags<REG_A>(value);
                this->update_flag(FLAG_C, carry_out != 0);
            } else {
                uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
                const uint8_t carry_out = val8 & FLAG_C;
                val8 >>= 1;
                value = val8;
                this->update_nz_flags<REG_A>(val8);
                this->update_flag(FLAG_C, carry_out != 0);
            }
        }
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        
        if (this->opcode_entry.flags & to_index(OF::RMW)) {
            // Memory operation: always work with 8-bit data
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = (val8 >> 7) & FLAG_C;
            val8 = (val8 << 1) | carry_in;
            value = val8;
            this->update_nz_flags_8bit(val8);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // Accumulator operation: work with full register width
            if (this->is_accumulator_16bit()) {
                const uint16_t carry_out = (value >> 15) & FLAG_C;
                value = ((value << 1) | carry_in) & 0xFFFF;
                this->update_nz_flags<REG_A>(value);
                this->update_flag(FLAG_C, carry_out != 0);
            } else {
                uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
                const uint8_t carry_out = (val8 >> 7) & FLAG_C;
                val8 = (val8 << 1) | carry_in;
                value = val8;
                this->update_nz_flags<REG_A>(val8);
                this->update_flag(FLAG_C, carry_out != 0);
            }
        }
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        
        if (this->opcode_entry.flags & to_index(OF::RMW)) {
            // Memory operation: always work with 8-bit data
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = val8 & FLAG_C;
            val8 = (val8 >> 1) | (carry_in << 7);
            value = val8;
            this->update_nz_flags_8bit(val8);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // Accumulator operation: work with full register width
            if (this->is_accumulator_16bit()) {
                const uint16_t carry_out = value & FLAG_C;
                value = ((value >> 1) | (static_cast<uint16_t>(carry_in) << 15)) & 0xFFFF;
                this->update_nz_flags<REG_A>(value);
                this->update_flag(FLAG_C, carry_out != 0);
            } else {
                uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
                const uint8_t carry_out = val8 & FLAG_C;
                val8 = (val8 >> 1) | (carry_in << 7);
                value = val8;
                this->update_nz_flags<REG_A>(val8);
                this->update_flag(FLAG_C, carry_out != 0);
            }
        }
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
