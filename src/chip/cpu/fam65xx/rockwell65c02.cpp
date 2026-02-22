/*
 * rockwell65c02.cpp - C API Wrapper Implementation for Rockwell 65C02 CPU
 */

#include "rockwell65c02.h"
#include "fam65xx.hpp"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using rockwell65c02_cpu_t = fam65xx::rockwell65c02_cpu_impl_t;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<rockwell65c02_cpu_t *>(ptr)

// ============================================================================
// ROCKWELL 65C02 IMPLEMENTATION
// ============================================================================

rockwell65c02_t *rockwell65c02_create(void) {
  return reinterpret_cast<rockwell65c02_t *>(new rockwell65c02_cpu_t());
}

void rockwell65c02_destroy(rockwell65c02_t *cpu) {
  delete CPU_CAST(cpu);
}

bus_state_t rockwell65c02_init(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->init();
}

bus_state_t rockwell65c02_reset(rockwell65c02_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->reset(pins);
}

bus_state_t rockwell65c02_tick(rockwell65c02_t *cpu, bus_state_t pins) {
  return CPU_CAST(cpu)->tick<rockwell65c02_cpu_t::Phase::PHI2>(pins);
}

bool rockwell65c02_opdone(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->opdone();
}

// Register getters
uint8_t rockwell65c02_get_a(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_A);
}

uint8_t rockwell65c02_get_x(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_X);
}

uint8_t rockwell65c02_get_y(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_Y);
}

uint8_t rockwell65c02_get_s(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_SPL);
}

uint8_t rockwell65c02_get_p(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_P);
}

uint16_t rockwell65c02_get_pc(rockwell65c02_t *cpu) {
  return CPU_CAST(cpu)->get(REG_PC);
}

// Register setters
void rockwell65c02_set_a(rockwell65c02_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_A, value);
}

void rockwell65c02_set_x(rockwell65c02_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_X, value);
}

void rockwell65c02_set_y(rockwell65c02_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_Y, value);
}

void rockwell65c02_set_s(rockwell65c02_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_SPL, value);
}

void rockwell65c02_set_p(rockwell65c02_t *cpu, uint8_t value) {
  CPU_CAST(cpu)->set(REG_P, value);
}

void rockwell65c02_set_pc(rockwell65c02_t *cpu, uint16_t value) {
  CPU_CAST(cpu)->set(REG_PC, value);
}
