/*
 * nes6502.cpp - C API Wrapper Implementation for NES 6502 CPU
 */

#include "nes6502.h"
#include "fam65xx.hpp"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using nes6502_cpu_t = fam65xx_t<RICOH_2A03>;

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

// APU functions (conditionally compiled based on NES6502Tag features)
float nes6502_generate_audio_sample(nes6502_t* cpu) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        return cpu_ptr->generate_audio_sample();
    } else {
        return 0.0f; // No APU
    }
}

bool nes6502_apu_needs_dma(nes6502_t* cpu) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        return cpu_ptr->apu_needs_dma();
    } else {
        return false; // No APU
    }
}

uint16_t nes6502_apu_dma_address(nes6502_t* cpu) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        return cpu_ptr->apu_dma_address();
    } else {
        return 0; // No APU
    }
}

void nes6502_apu_load_dma_sample(nes6502_t* cpu, uint8_t data) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        cpu_ptr->apu_load_dma_sample(data);
    }
    // No-op if no APU
}

bool nes6502_apu_irq(nes6502_t* cpu) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        return cpu_ptr->apu_irq();
    } else {
        return false; // No APU
    }
}

void nes6502_set_apu_region(nes6502_t* cpu, bool is_pal) {
    auto* cpu_ptr = CPU_CAST(cpu);
    if constexpr (RICOH_2A03.has_apu()) {
        cpu_ptr->set_apu_region(is_pal);
    }
    // No-op if no APU
}

} // extern "C"