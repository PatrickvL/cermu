#include "fam65xx_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY SHIFT AND ROTATE OPERATIONS
// ============================================================================

// ============================================================================
// ACCUMULATOR SHIFTS AND ROTATES
// ============================================================================

static inline uint8_t fam65xx_op_asl(fam65xx_t* cpu, uint8_t value) {
    fam65xx_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t fam65xx_op_lsr(fam65xx_t* cpu, uint8_t value) {
    fam65xx_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t fam65xx_op_rol(fam65xx_t* cpu, uint8_t value) {
    bool old_carry = fam65xx_get_flag(cpu, FLAG_C);
    fam65xx_set_flag(cpu, FLAG_C, value & 0x80);
    value = (value << 1) | (old_carry ? 1 : 0);
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

static inline uint8_t fam65xx_op_ror(fam65xx_t* cpu, uint8_t value) {
    bool old_carry = fam65xx_get_flag(cpu, FLAG_C);
    fam65xx_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    fam65xx_set_nz_flags(cpu, value);
    return value;
}

// Accumulator read-modify-write operations (2 cycles)
FAM65XX_HELPER_PROTO(fam65xx_op_rmw_accumulator_helper),
    uint8_t (*operation)(fam65xx_t*, uint8_t)) {
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Dummy read
    cpu->a = operation(cpu, cpu->a);
    FAM65XX_HELPER_FOOTER(cpu);
}

// ASL A - Arithmetic Shift Left (Accumulator)
FAM65XX_OPCODE_PROTO(fam65xx_op_asl_accumulator) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ACC_HELPER(cpu, fam65xx_op_asl); }

// LSR A - Logical Shift Right (Accumulator)
FAM65XX_OPCODE_PROTO(fam65xx_op_lsr_accumulator) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ACC_HELPER(cpu, fam65xx_op_lsr); }

// ROL A - Rotate Left (Accumulator)
FAM65XX_OPCODE_PROTO(fam65xx_op_rol_accumulator) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ACC_HELPER(cpu, fam65xx_op_rol); }

// ROR A - Rotate Right (Accumulator)
FAM65XX_OPCODE_PROTO(fam65xx_op_ror_accumulator) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ACC_HELPER(cpu, fam65xx_op_ror); }

// ============================================================================
// ZERO PAGE SHIFTS AND ROTATES
// ============================================================================

// Zero page read-modify-write operations
FAM65XX_HELPER_PROTO(fam65xx_op_rmw_zero_page_helper),
    uint8_t (*operation)(fam65xx_t*, uint8_t)) {
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch
    fam65xx_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    FAM65XX_HELPER_FOOTER(cpu);
}

// ASL Zero Page - Arithmetic Shift Left
FAM65XX_OPCODE_PROTO(fam65xx_op_asl_zero_page) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZP_HELPER(cpu, fam65xx_op_asl); }

// LSR Zero Page - Logical Shift Right
FAM65XX_OPCODE_PROTO(fam65xx_op_lsr_zero_page) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZP_HELPER(cpu, fam65xx_op_lsr); }

// ROL Zero Page - Rotate Left
FAM65XX_OPCODE_PROTO(fam65xx_op_rol_zero_page) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZP_HELPER(cpu, fam65xx_op_rol); }

// ROR Zero Page - Rotate Right
FAM65XX_OPCODE_PROTO(fam65xx_op_ror_zero_page) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZP_HELPER(cpu, fam65xx_op_ror); }

// ============================================================================
// ZERO PAGE,X SHIFTS AND ROTATES
// ============================================================================

// Zero page,X read-modify-write operations
FAM65XX_HELPER_PROTO(fam65xx_op_rmw_zero_page_x_helper),
    uint8_t (*operation)(fam65xx_t*, uint8_t)) {
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    (void)fam65xx_read_cycle(cpu, cpu->address); // Dummy read
    cpu->address = (cpu->address + cpu->x) & 0xFF;
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch
    fam65xx_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    FAM65XX_HELPER_FOOTER(cpu);
}

// ASL Zero Page,X - Arithmetic Shift Left
FAM65XX_OPCODE_PROTO(fam65xx_op_asl_zero_page_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZPX_HELPER(cpu, fam65xx_op_asl); }

// LSR Zero Page,X - Logical Shift Right
FAM65XX_OPCODE_PROTO(fam65xx_op_lsr_zero_page_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZPX_HELPER(cpu, fam65xx_op_lsr); }

// ROL Zero Page,X - Rotate Left
FAM65XX_OPCODE_PROTO(fam65xx_op_rol_zero_page_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZPX_HELPER(cpu, fam65xx_op_rol); }

// ROR Zero Page,X - Rotate Right
FAM65XX_OPCODE_PROTO(fam65xx_op_ror_zero_page_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ZPX_HELPER(cpu, fam65xx_op_ror); }

// ============================================================================
// ABSOLUTE SHIFTS AND ROTATES
// ============================================================================

// Absolute read-modify-write operations
FAM65XX_HELPER_PROTO(fam65xx_op_rmw_absolute_helper),
    uint8_t (*operation)(fam65xx_t*, uint8_t)) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address = addr_lo;
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    cpu->address |= (addr_hi << 8);
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch
    fam65xx_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    FAM65XX_HELPER_FOOTER(cpu);
}

// ASL Absolute - Arithmetic Shift Left
FAM65XX_OPCODE_PROTO(fam65xx_op_asl_absolute) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABS_HELPER(cpu, fam65xx_op_asl); }

// LSR Absolute - Logical Shift Right
FAM65XX_OPCODE_PROTO(fam65xx_op_lsr_absolute) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABS_HELPER(cpu, fam65xx_op_lsr); }

// ROL Absolute - Rotate Left
FAM65XX_OPCODE_PROTO(fam65xx_op_rol_absolute) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABS_HELPER(cpu, fam65xx_op_rol); }

// ROR Absolute - Rotate Right
FAM65XX_OPCODE_PROTO(fam65xx_op_ror_absolute) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABS_HELPER(cpu, fam65xx_op_ror); }

// ============================================================================
// ABSOLUTE,X SHIFTS AND ROTATES
// ============================================================================

// Absolute,X read-modify-write operations
FAM65XX_HELPER_PROTO(fam65xx_op_rmw_absolute_x_helper),
    uint8_t (*operation)(fam65xx_t*, uint8_t)) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);  // T1: Operand fetch
    uint16_t base_addr = (addr_hi << 8) | addr_lo;
    cpu->address = base_addr + cpu->x;
    (void)fam65xx_read_cycle(cpu, (addr_hi << 8) | ((addr_lo + cpu->x) & 0xFF)); // Dummy read
    uint8_t value = fam65xx_read_cycle(cpu, cpu->address);  // T2: Data fetch
    fam65xx_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    fam65xx_write_cycle(cpu, cpu->address, value);  // T2: Data store
    FAM65XX_HELPER_FOOTER(cpu);
}

// ASL Absolute,X - Arithmetic Shift Left
FAM65XX_OPCODE_PROTO(fam65xx_op_asl_absolute_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABSX_HELPER(cpu, fam65xx_op_asl); }

// LSR Absolute,X - Logical Shift Right
FAM65XX_OPCODE_PROTO(fam65xx_op_lsr_absolute_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABSX_HELPER(cpu, fam65xx_op_lsr); }

// ROL Absolute,X - Rotate Left
FAM65XX_OPCODE_PROTO(fam65xx_op_rol_absolute_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABSX_HELPER(cpu, fam65xx_op_rol); }

// ROR Absolute,X - Rotate Right
FAM65XX_OPCODE_PROTO(fam65xx_op_ror_absolute_x) { FAM65XX_PROTO_RETURN FAM65XX_RMW_ABSX_HELPER(cpu, fam65xx_op_ror); }
