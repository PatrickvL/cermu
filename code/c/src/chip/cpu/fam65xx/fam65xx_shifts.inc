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

// ASL A - Arithmetic Shift Left (Accumulator)
void fam65xx_op_asl_accumulator(fam65xx_t* cpu) {
    fam65xx_op_rmw_accumulator_helper(cpu, fam65xx_op_asl);
}

// LSR A - Logical Shift Right (Accumulator)
void fam65xx_op_lsr_accumulator(fam65xx_t* cpu) {
    fam65xx_op_rmw_accumulator_helper(cpu, fam65xx_op_lsr);
}

// ROL A - Rotate Left (Accumulator)
void fam65xx_op_rol_accumulator(fam65xx_t* cpu) {
    fam65xx_op_rmw_accumulator_helper(cpu, fam65xx_op_rol);
}

// ROR A - Rotate Right (Accumulator)
void fam65xx_op_ror_accumulator(fam65xx_t* cpu) {
    fam65xx_op_rmw_accumulator_helper(cpu, fam65xx_op_ror);
}

// ============================================================================
// ZERO PAGE SHIFTS AND ROTATES
// ============================================================================

// ASL Zero Page - Arithmetic Shift Left
void fam65xx_op_asl_zero_page(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_helper(cpu, fam65xx_op_asl);
}

// LSR Zero Page - Logical Shift Right
void fam65xx_op_lsr_zero_page(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_helper(cpu, fam65xx_op_lsr);
}

// ROL Zero Page - Rotate Left
void fam65xx_op_rol_zero_page(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_helper(cpu, fam65xx_op_rol);
}

// ROR Zero Page - Rotate Right
void fam65xx_op_ror_zero_page(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_helper(cpu, fam65xx_op_ror);
}

// ============================================================================
// ZERO PAGE,X SHIFTS AND ROTATES
// ============================================================================

// ASL Zero Page,X - Arithmetic Shift Left
void fam65xx_op_asl_zero_page_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_x_helper(cpu, fam65xx_op_asl);
}

// LSR Zero Page,X - Logical Shift Right
void fam65xx_op_lsr_zero_page_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_x_helper(cpu, fam65xx_op_lsr);
}

// ROL Zero Page,X - Rotate Left
void fam65xx_op_rol_zero_page_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_x_helper(cpu, fam65xx_op_rol);
}

// ROR Zero Page,X - Rotate Right
void fam65xx_op_ror_zero_page_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_zero_page_x_helper(cpu, fam65xx_op_ror);
}

// ============================================================================
// ABSOLUTE SHIFTS AND ROTATES
// ============================================================================

// ASL Absolute - Arithmetic Shift Left
void fam65xx_op_asl_absolute(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_helper(cpu, fam65xx_op_asl);
}

// LSR Absolute - Logical Shift Right
void fam65xx_op_lsr_absolute(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_helper(cpu, fam65xx_op_lsr);
}

// ROL Absolute - Rotate Left
void fam65xx_op_rol_absolute(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_helper(cpu, fam65xx_op_rol);
}

// ROR Absolute - Rotate Right
void fam65xx_op_ror_absolute(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_helper(cpu, fam65xx_op_ror);
}

// ============================================================================
// ABSOLUTE,X SHIFTS AND ROTATES
// ============================================================================

// ASL Absolute,X - Arithmetic Shift Left
void fam65xx_op_asl_absolute_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_x_helper(cpu, fam65xx_op_asl);
}

// LSR Absolute,X - Logical Shift Right
void fam65xx_op_lsr_absolute_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_x_helper(cpu, fam65xx_op_lsr);
}

// ROL Absolute,X - Rotate Left
void fam65xx_op_rol_absolute_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_x_helper(cpu, fam65xx_op_rol);
}

// ROR Absolute,X - Rotate Right
void fam65xx_op_ror_absolute_x(fam65xx_t* cpu) {
    fam65xx_op_rmw_absolute_x_helper(cpu, fam65xx_op_ror);
}
