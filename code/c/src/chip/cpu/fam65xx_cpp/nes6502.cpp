/*
 * nes6502.cpp - C API Wrapper Implementation for NES 6502 CPU
 */

#include "nes6502.h"
#include "fam65xx.hpp"

using namespace fam65xx_cpp;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using nes6502_cpu_t = fam65xx_t<NES6502Tag>;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<nes6502_cpu_t*>(ptr)

// ============================================================================
// NES 6502 IMPLEMENTATION
// ============================================================================

extern "C" {

nes6502_t* nes6502_create(void) {
    return reinterpret_cast<nes6502_t*>(new nes6502_cpu_t());
}

void nes6502_destroy(nes6502_t* cpu) {
    delete CPU_CAST(cpu);
}

bus_state_t nes6502_init(nes6502_t* cpu, const chip_descriptor_t* desc) {
    return CPU_CAST(cpu)->init(desc);
}

bus_state_t nes6502_reset(nes6502_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->reset(pins);
}

bus_state_t nes6502_tick(nes6502_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->tick(pins);
}

bool nes6502_opdone(nes6502_t* cpu) {
    return CPU_CAST(cpu)->opdone();
}

// Register getters
uint8_t nes6502_get_a(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_A];
}

uint8_t nes6502_get_x(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_X];
}

uint8_t nes6502_get_y(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_Y];
}

uint8_t nes6502_get_s(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_SPL];
}

uint8_t nes6502_get_p(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_P];
}

uint16_t nes6502_get_pc(nes6502_t* cpu) {
    return CPU_CAST(cpu)->reg16[REG_PC];
}

// Register setters
void nes6502_set_a(nes6502_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_A] = value;
}

void nes6502_set_x(nes6502_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_X] = value;
}

void nes6502_set_y(nes6502_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_Y] = value;
}

void nes6502_set_s(nes6502_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_SPL] = value;
}

void nes6502_set_p(nes6502_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_P] = value;
}

void nes6502_set_pc(nes6502_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->reg16[REG_PC] = value;
}

} // extern "C"