/*
 * wdc65c02.cpp - C API Wrapper Implementation for WDC 65C02 CPU
 */

#include "wdc65c02.h"
#include "fam65xx.hpp"

using namespace fam65xx_cpp;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using wdc65c02_cpu_t = fam65xx_t<WDC_65C02_EARLY>;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<wdc65c02_cpu_t*>(ptr)

// ============================================================================
// WDC 65C02 IMPLEMENTATION
// ============================================================================

extern "C" {

wdc65c02_t* wdc65c02_create(void) {
    return reinterpret_cast<wdc65c02_t*>(new wdc65c02_cpu_t());
}

void wdc65c02_destroy(wdc65c02_t* cpu) {
    delete CPU_CAST(cpu);
}

bus_state_t wdc65c02_init(wdc65c02_t* cpu, const chip_descriptor_t* desc) {
    return CPU_CAST(cpu)->init(desc);
}

bus_state_t wdc65c02_reset(wdc65c02_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->reset(pins);
}

bus_state_t wdc65c02_tick(wdc65c02_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->tick(pins);
}

bool wdc65c02_opdone(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->opdone();
}

// Register getters
uint8_t wdc65c02_get_a(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_A];
}

uint8_t wdc65c02_get_x(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_X];
}

uint8_t wdc65c02_get_y(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_Y];
}

uint8_t wdc65c02_get_s(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_SPL];
}

uint8_t wdc65c02_get_p(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_P];
}

uint16_t wdc65c02_get_pc(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->reg16[REG_PC];
}

// Register setters
void wdc65c02_set_a(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_A] = value;
}

void wdc65c02_set_x(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_X] = value;
}

void wdc65c02_set_y(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_Y] = value;
}

void wdc65c02_set_s(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_SPL] = value;
}

void wdc65c02_set_p(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_P] = value;
}

void wdc65c02_set_pc(wdc65c02_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->reg16[REG_PC] = value;
}

} // extern "C"