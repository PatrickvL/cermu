/*
 * mos6502.cpp - C API Wrapper Implementation
 *
 * This file implements C-compatible wrapper functions for the C++
 * template-based CPU emulator. It creates concrete instances of the template
 * classes and provides simple C function interfaces.
 */

#include "mos6502.h"
#include "fam65xx.hpp"
#include <cstdio>

// ============================================================================
// CONCRETE CPU TYPE DEFINITIONS
// ============================================================================

// Define concrete CPU types for easier use
using mos6502_cpu_t = fam65xx::mos6502_cpu_impl_t;

// Cast helpers for opaque handles
#define CPU_CAST(type, ptr) reinterpret_cast<type *>(ptr)
#define CPU_CONST_CAST(type, ptr) reinterpret_cast<const type *>(ptr)

// ============================================================================
// MOS 6502 (Original NMOS) IMPLEMENTATION
// ============================================================================

mos6502_t *mos6502_create(void) {
  return reinterpret_cast<mos6502_t *>(new mos6502_cpu_t());
}

void mos6502_destroy(mos6502_t *cpu) {
  delete CPU_CAST(mos6502_cpu_t, cpu);
}

ChipBase* mos6502_as_chip_base(mos6502_t *cpu) {
  return static_cast<ChipBase*>(CPU_CAST(mos6502_cpu_t, cpu));
}

bus_state_t mos6502_init(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->init();
}

bus_state_t mos6502_bootstrap(mos6502_t *cpu, bus_state_t pins) {
  return CPU_CAST(mos6502_cpu_t, cpu)->bootstrap(pins);
}

bus_state_t mos6502_reset(mos6502_t *cpu, bus_state_t pins) {
  return CPU_CAST(mos6502_cpu_t, cpu)->reset(pins);
}

bus_state_t mos6502_tick(mos6502_t *cpu, bus_state_t pins) {
  return CPU_CAST(mos6502_cpu_t, cpu)->tick<mos6502_cpu_t::Phase::PHI2>(pins);
}

bus_state_t mos6502_tick_phi2(mos6502_t *cpu, bus_state_t pins) {
  return CPU_CAST(mos6502_cpu_t, cpu)->tick<mos6502_cpu_t::Phase::PHI2>(pins);
}

bus_state_t mos6502_tick_phi1(mos6502_t *cpu, bus_state_t pins) {
  return CPU_CAST(mos6502_cpu_t, cpu)->tick<mos6502_cpu_t::Phase::PHI1>(pins);
}

bool mos6502_opdone(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->opdone();
}

// Register getters
uint8_t mos6502_get_a(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_A);
}

uint8_t mos6502_get_x(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_X);
}

uint8_t mos6502_get_y(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_Y);
}

uint8_t mos6502_get_s(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_S);
}

uint8_t mos6502_get_p(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_P);
}

uint16_t mos6502_get_pc(mos6502_t *cpu) {
  return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_PC);
}

// Register setters
void mos6502_set_a(mos6502_t *cpu, uint8_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_A, value);
}

void mos6502_set_x(mos6502_t *cpu, uint8_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_X, value);
}

void mos6502_set_y(mos6502_t *cpu, uint8_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_Y, value);
}

void mos6502_set_s(mos6502_t *cpu, uint8_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_S, value);
}

void mos6502_set_p(mos6502_t *cpu, uint8_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_P, value);
}

void mos6502_set_pc(mos6502_t *cpu, uint16_t value) {
  CPU_CAST(mos6502_cpu_t, cpu)->set(REG_PC, value);
}
