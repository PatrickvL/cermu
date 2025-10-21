/*
 * mos6502.cpp - C API Wrapper Implementation
 *
 * This file implements C-compatible wrapper functions for the C++ template-based
 * CPU emulator. It creates concrete instances of the template classes and provides
 * simple C function interfaces.
 */

#include "mos6502.h"
#include "fam65xx.hpp"

// Include concrete CPU type definitions
using namespace fam65xx_cpp;

// ============================================================================
// CONCRETE CPU TYPE DEFINITIONS
// ============================================================================

// Define concrete CPU types for easier use
using mos6502_cpu_t = fam65xx_t<MOS6502Tag>;

// Cast helpers for opaque handles
#define CPU_CAST(type, ptr) reinterpret_cast<type*>(ptr)
#define CPU_CONST_CAST(type, ptr) reinterpret_cast<const type*>(ptr)

// ============================================================================
// MOS 6502 (Original NMOS) IMPLEMENTATION
// ============================================================================

extern "C" {

mos6502_t* mos6502_create(void) {
    return reinterpret_cast<mos6502_t*>(new mos6502_cpu_t());
}

void mos6502_destroy(mos6502_t* cpu) {
    delete CPU_CAST(mos6502_cpu_t, cpu);
}

bus_state_t mos6502_init(mos6502_t* cpu, const chip_descriptor_t* desc) {
    return CPU_CAST(mos6502_cpu_t, cpu)->init(desc);
}

bus_state_t mos6502_reset(mos6502_t* cpu, bus_state_t pins) {
    return CPU_CAST(mos6502_cpu_t, cpu)->reset(pins);
}

bus_state_t mos6502_tick(mos6502_t* cpu, bus_state_t pins) {
    return CPU_CAST(mos6502_cpu_t, cpu)->tick(pins);
}

bool mos6502_opdone(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->opdone();
}

// Register getters
uint8_t mos6502_get_a(mos6502_t* cpu) {
    return CPU_A(CPU_CAST(mos6502_cpu_t, cpu));
}

uint8_t mos6502_get_x(mos6502_t* cpu) {
    return CPU_X(CPU_CAST(mos6502_cpu_t, cpu));
}

uint8_t mos6502_get_y(mos6502_t* cpu) {
    return CPU_Y(CPU_CAST(mos6502_cpu_t, cpu));
}

uint8_t mos6502_get_s(mos6502_t* cpu) {
    return CPU_S(CPU_CAST(mos6502_cpu_t, cpu));
}

uint8_t mos6502_get_p(mos6502_t* cpu) {
    return CPU_P(CPU_CAST(mos6502_cpu_t, cpu));
}

uint16_t mos6502_get_pc(mos6502_t* cpu) {
    return CPU_PC(CPU_CAST(mos6502_cpu_t, cpu));
}

// Register setters
void mos6502_set_a(mos6502_t* cpu, uint8_t value) {
    CPU_A(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

void mos6502_set_x(mos6502_t* cpu, uint8_t value) {
    CPU_X(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

void mos6502_set_y(mos6502_t* cpu, uint8_t value) {
    CPU_Y(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

void mos6502_set_s(mos6502_t* cpu, uint8_t value) {
    CPU_S(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

void mos6502_set_p(mos6502_t* cpu, uint8_t value) {
    CPU_P(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

void mos6502_set_pc(mos6502_t* cpu, uint16_t value) {
    CPU_PC(CPU_CAST(mos6502_cpu_t, cpu)) = value;
}

} // extern "C"