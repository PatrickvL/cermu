/*
 * mos6510.cpp - C API Wrapper Implementation for MOS 6510 CPU
 */

#include "mos6510.h"
#include "fam65xx.hpp"

using namespace fam65xx;

extern "C" {

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using mos6510_cpu_t = fam65xx_t<MOS6510>;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<mos6510_cpu_t*>(ptr)

// ============================================================================
// MOS 6510 (C64/C128) IMPLEMENTATION
// ============================================================================

mos6510_t* mos6510_create(void) {
    return reinterpret_cast<mos6510_t*>(new mos6510_cpu_t());
}

void mos6510_destroy(mos6510_t* cpu) {
    delete CPU_CAST(cpu);
}

bus_state_t mos6510_init(mos6510_t* cpu, const mos6510_desc_t* desc) {
    // Initialize base CPU with the chip descriptor
    bus_state_t pins = CPU_CAST(cpu)->init(&desc->base);
    
    // Initialize 6510-specific I/O port state
    auto* cpu_impl = CPU_CAST(cpu);
    if constexpr (MOS6510.has_io_port()) {
        // Initialize I/O port with default C64 state
        cpu_impl->init_io_port();
        
        // Note: I/O port callbacks are handled at a higher level (system integration)
        // The mixin provides the basic I/O port register functionality
    }
    
    return pins;
}

bus_state_t mos6510_reset(mos6510_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->reset(pins);
}

bus_state_t mos6510_tick(mos6510_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->tick(pins);
}

bool mos6510_opdone(mos6510_t* cpu) {
    return CPU_CAST(cpu)->opdone();
}

// Register getters
uint8_t mos6510_get_a(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_A];
}

uint8_t mos6510_get_x(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_X];
}

uint8_t mos6510_get_y(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_Y];
}

uint8_t mos6510_get_s(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_SPL];
}

uint8_t mos6510_get_p(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg8[REG_P];
}

uint16_t mos6510_get_pc(mos6510_t* cpu) {
    return CPU_CAST(cpu)->reg16[REG_PC];
}

// Register setters
void mos6510_set_a(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_A] = value;
}

void mos6510_set_x(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_X] = value;
}

void mos6510_set_y(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_Y] = value;
}

void mos6510_set_s(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_SPL] = value;
}

void mos6510_set_p(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->reg8[REG_P] = value;
}

void mos6510_set_pc(mos6510_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->reg16[REG_PC] = value;
}

// I/O Port access (6510-specific)
uint8_t mos6510_get_io_ddr(mos6510_t* cpu) {
    return CPU_CAST(cpu)->io_port.direction;
}

uint8_t mos6510_get_io_data(mos6510_t* cpu) {
    return CPU_CAST(cpu)->io_port.data;
}

uint8_t mos6510_get_io_input(mos6510_t* cpu) {
    return CPU_CAST(cpu)->io_port.input;
}

void mos6510_set_io_input(mos6510_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->io_port.input = value;
}

} // extern "C"