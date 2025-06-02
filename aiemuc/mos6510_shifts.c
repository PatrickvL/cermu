#include "mos6510.h"

// ============================================================================
// MOS 6510 SHIFT AND ROTATE INSTRUCTIONS
// ============================================================================

// ASL - Arithmetic Shift Left
void asl_accumulator_func(mos6510_t* cpu_dev) {
    cpu_rmw_accumulator(cpu_dev, op_asl);
}

void asl_zero_page_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page(cpu_dev, op_asl);
}

void asl_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page_x(cpu_dev, op_asl);
}

void asl_absolute_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute(cpu_dev, op_asl);
}

void asl_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute_x(cpu_dev, op_asl);
}

// LSR - Logical Shift Right
void lsr_accumulator_func(mos6510_t* cpu_dev) {
    cpu_rmw_accumulator(cpu_dev, op_lsr);
}

void lsr_zero_page_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page(cpu_dev, op_lsr);
}

void lsr_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page_x(cpu_dev, op_lsr);
}

void lsr_absolute_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute(cpu_dev, op_lsr);
}

void lsr_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute_x(cpu_dev, op_lsr);
}

// ROL - Rotate Left
void rol_accumulator_func(mos6510_t* cpu_dev) {
    cpu_rmw_accumulator(cpu_dev, op_rol);
}

void rol_zero_page_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page(cpu_dev, op_rol);
}

void rol_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page_x(cpu_dev, op_rol);
}

void rol_absolute_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute(cpu_dev, op_rol);
}

void rol_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute_x(cpu_dev, op_rol);
}

// ROR - Rotate Right
void ror_accumulator_func(mos6510_t* cpu_dev) {
    cpu_rmw_accumulator(cpu_dev, op_ror);
}

void ror_zero_page_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page(cpu_dev, op_ror);
}

void ror_zero_page_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_zero_page_x(cpu_dev, op_ror);
}

void ror_absolute_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute(cpu_dev, op_ror);
}

void ror_absolute_x_func(mos6510_t* cpu_dev) {
    cpu_rmw_absolute_x(cpu_dev, op_ror);
}
