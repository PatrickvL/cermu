#include "mos6502_family_core.h"

// ============================================================================
// SHARED MOS 6502 FAMILY SHIFT AND ROTATE OPERATIONS
// ============================================================================

// ============================================================================
// ACCUMULATOR SHIFTS AND ROTATES
// ============================================================================

// ASL A - Arithmetic Shift Left (Accumulator)
void mos6502_family_asl_accumulator(mos6502_family_t* cpu) {
    mos6502_family_rmw_accumulator(cpu, mos6502_family_op_asl);
}

// LSR A - Logical Shift Right (Accumulator)
void mos6502_family_lsr_accumulator(mos6502_family_t* cpu) {
    mos6502_family_rmw_accumulator(cpu, mos6502_family_op_lsr);
}

// ROL A - Rotate Left (Accumulator)
void mos6502_family_rol_accumulator(mos6502_family_t* cpu) {
    mos6502_family_rmw_accumulator(cpu, mos6502_family_op_rol);
}

// ROR A - Rotate Right (Accumulator)
void mos6502_family_ror_accumulator(mos6502_family_t* cpu) {
    mos6502_family_rmw_accumulator(cpu, mos6502_family_op_ror);
}

// ============================================================================
// ZERO PAGE SHIFTS AND ROTATES
// ============================================================================

// ASL Zero Page - Arithmetic Shift Left
void mos6502_family_asl_zero_page(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page(cpu, mos6502_family_op_asl);
}

// LSR Zero Page - Logical Shift Right
void mos6502_family_lsr_zero_page(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page(cpu, mos6502_family_op_lsr);
}

// ROL Zero Page - Rotate Left
void mos6502_family_rol_zero_page(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page(cpu, mos6502_family_op_rol);
}

// ROR Zero Page - Rotate Right
void mos6502_family_ror_zero_page(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page(cpu, mos6502_family_op_ror);
}

// ============================================================================
// ZERO PAGE,X SHIFTS AND ROTATES
// ============================================================================

// ASL Zero Page,X - Arithmetic Shift Left
void mos6502_family_asl_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page_x(cpu, mos6502_family_op_asl);
}

// LSR Zero Page,X - Logical Shift Right
void mos6502_family_lsr_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page_x(cpu, mos6502_family_op_lsr);
}

// ROL Zero Page,X - Rotate Left
void mos6502_family_rol_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page_x(cpu, mos6502_family_op_rol);
}

// ROR Zero Page,X - Rotate Right
void mos6502_family_ror_zero_page_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_zero_page_x(cpu, mos6502_family_op_ror);
}

// ============================================================================
// ABSOLUTE SHIFTS AND ROTATES
// ============================================================================

// ASL Absolute - Arithmetic Shift Left
void mos6502_family_asl_absolute(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute(cpu, mos6502_family_op_asl);
}

// LSR Absolute - Logical Shift Right
void mos6502_family_lsr_absolute(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute(cpu, mos6502_family_op_lsr);
}

// ROL Absolute - Rotate Left
void mos6502_family_rol_absolute(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute(cpu, mos6502_family_op_rol);
}

// ROR Absolute - Rotate Right
void mos6502_family_ror_absolute(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute(cpu, mos6502_family_op_ror);
}

// ============================================================================
// ABSOLUTE,X SHIFTS AND ROTATES
// ============================================================================

// ASL Absolute,X - Arithmetic Shift Left
void mos6502_family_asl_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute_x(cpu, mos6502_family_op_asl);
}

// LSR Absolute,X - Logical Shift Right
void mos6502_family_lsr_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute_x(cpu, mos6502_family_op_lsr);
}

// ROL Absolute,X - Rotate Left
void mos6502_family_rol_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute_x(cpu, mos6502_family_op_rol);
}

// ROR Absolute,X - Rotate Right
void mos6502_family_ror_absolute_x(mos6502_family_t* cpu) {
    mos6502_family_rmw_absolute_x(cpu, mos6502_family_op_ror);
}
