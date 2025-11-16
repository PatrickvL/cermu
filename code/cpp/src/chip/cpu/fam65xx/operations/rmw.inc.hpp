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
        // rmw_operation_helper already determines memory vs accumulator mode
        // Memory operations are always 8-bit, accumulator operations respect M flag
        if (this->is_accumulator_16bit() && !(this->opcode_entry.flags & to_index(OF::RMW))) {
            // 16-bit accumulator operation
            const uint16_t carry_out = (value >> 15) & FLAG_C;
            value = (value << 1) & 0xFFFF;
            this->update_nz_flags<REG_A>(value);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // 8-bit operation (memory or 8-bit accumulator)
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = (val8 >> 7) & FLAG_C;
            val8 <<= 1;
            value = val8;
            
            if (this->opcode_entry.flags & to_index(OF::RMW)) {
                this->update_nz_flags_8bit(val8);
            } else {
                this->update_nz_flags<REG_A>(val8);
            }
            this->update_flag(FLAG_C, carry_out != 0);
        }
    });
}

/* LSR - Logical Shift Right */
bus_state_t op_lsr(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        // Determine if this is 16-bit operation
        const bool is_memory_rmw = (this->opcode_entry.flags & to_index(OF::RMW)) != 0;
        const bool is_16bit_acc = !is_memory_rmw && this->is_accumulator_16bit();
        const bool is_16bit_mem = is_memory_rmw && this->is_memory_16bit();
        
        if (is_16bit_acc || is_16bit_mem) {
            // 16-bit operation (accumulator or memory)
            const uint16_t carry_out = value & FLAG_C;
            value = (value >> 1) & 0x7FFF;
            
            if (is_16bit_acc) {
                this->update_nz_flags<REG_A>(value);
            } else {
                this->update_nz_flags_16bit(static_cast<uint16_t>(value));
            }
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // 8-bit operation (memory or 8-bit accumulator)
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = val8 & FLAG_C;
            val8 >>= 1;
            value = static_cast<data_t>(val8);
            
            if (is_memory_rmw) {
                this->update_nz_flags_8bit(val8);
            } else {
                this->update_nz_flags<REG_A>(val8);
            }
            this->update_flag(FLAG_C, carry_out != 0);
        }
    });
}

/* ROL - Rotate Left */
bus_state_t op_rol(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        
        // rmw_operation_helper already determines memory vs accumulator mode
        // Memory operations are always 8-bit, accumulator operations respect M flag
        if (this->is_accumulator_16bit() && !(this->opcode_entry.flags & to_index(OF::RMW))) {
            // 16-bit accumulator operation
            const uint16_t carry_out = (value >> 15) & FLAG_C;
            value = ((value << 1) | carry_in) & 0xFFFF;
            this->update_nz_flags<REG_A>(value);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // 8-bit operation (memory or 8-bit accumulator)
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = (val8 >> 7) & FLAG_C;
            val8 = (val8 << 1) | carry_in;
            value = val8;
            
            if (this->opcode_entry.flags & to_index(OF::RMW)) {
                this->update_nz_flags_8bit(val8);
            } else {
                this->update_nz_flags<REG_A>(val8);
            }
            this->update_flag(FLAG_C, carry_out != 0);
        }
    });
}

/* ROR - Rotate Right */
bus_state_t op_ror(bus_state_t pins) {
    trace_operation(__func__);
    return this->rmw_operation_helper(pins, [this](data_t& value) {
        const uint8_t carry_in = this->get(REG_P) & FLAG_C;
        
        // rmw_operation_helper already determines memory vs accumulator mode
        // Memory operations are always 8-bit, accumulator operations respect M flag
        if (this->is_accumulator_16bit() && !(this->opcode_entry.flags & to_index(OF::RMW))) {
            // 16-bit accumulator operation
            const uint16_t carry_out = value & FLAG_C;
            value = ((value >> 1) | (static_cast<uint16_t>(carry_in) << 15)) & 0xFFFF;
            this->update_nz_flags<REG_A>(value);
            this->update_flag(FLAG_C, carry_out != 0);
        } else {
            // 8-bit operation (memory or 8-bit accumulator)
            uint8_t val8 = static_cast<uint8_t>(value & 0xFF);
            const uint8_t carry_out = val8 & FLAG_C;
            val8 = (val8 >> 1) | (carry_in << 7);
            value = val8;
            
            if (this->opcode_entry.flags & to_index(OF::RMW)) {
                this->update_nz_flags_8bit(val8);
            } else {
                this->update_nz_flags<REG_A>(val8);
            }
            this->update_flag(FLAG_C, carry_out != 0);
        }
    });
}

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
