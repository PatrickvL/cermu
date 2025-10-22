/*
 * mos6502.cpp - C API Wrapper Implementation
 *
 * This file implements C-compatible wrapper functions for the C++ template-based
 * CPU emulator. It creates concrete instances of the template classes and provides
 * simple C function interfaces.
 */

#include "mos6502.h"
#include "fam65xx.hpp"
#include <cstdio>

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

bus_state_t mos6502_init_enhanced(mos6502_t* cpu, const fam65xx_chip_descriptor_t* enhanced_desc) {
    auto* cpu_impl = CPU_CAST(mos6502_cpu_t, cpu);
    
    // Initialize with base descriptor first (this clears memory callbacks)
    bus_state_t result = cpu_impl->init(&enhanced_desc->base);
    
    // IMPORTANT: Set memory callbacks AFTER init() since init() clears them
    cpu_impl->set_memory_callbacks(enhanced_desc->mem_read, enhanced_desc->mem_write, enhanced_desc->mem_user_data);
    
    printf("DEBUG: Enhanced init complete - mem_read=%p set after init\n", cpu_impl->mem_read);
    
    return result;
}

bus_state_t mos6502_bootstrap(mos6502_t* cpu, bus_state_t pins) {
    return CPU_CAST(mos6502_cpu_t, cpu)->bootstrap(pins);
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

// ============================================================================
// Enhanced Descriptor API
// ============================================================================

static chip_descriptor_t mos6502_base_descriptor = {
    .description = "MOS Technology 6502 (NMOS)",
    .create = [](chip_descriptor_t* desc) -> void* {
        return mos6502_create();
    },
    .destroy = [](void* chip) {
        mos6502_destroy(reinterpret_cast<mos6502_t*>(chip));
    },
    .bus_attach = nullptr,  // Basic CPU doesn't need bus attach
    .bank_change = nullptr, // Basic CPU doesn't have banking
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = nullptr,
    .render_settings_window = nullptr
#endif
};

fam65xx_chip_descriptor_t* mos6502_create_descriptor(
    uint8_t (*read_callback)(void* user_data, uint16_t addr, uint8_t bus_state),
    void (*write_callback)(void* user_data, uint16_t addr, uint8_t data),
    void* user_data) {
    
    auto* desc = new fam65xx_chip_descriptor_t;
    desc->base = mos6502_base_descriptor;
    desc->mem_read = read_callback;
    desc->mem_write = write_callback;
    desc->mem_user_data = user_data;
    
    return desc;
}

void mos6502_destroy_descriptor(fam65xx_chip_descriptor_t* desc) {
    delete desc;
}

const chip_descriptor_t* mos6502_get_chip_descriptor(void) {
    return &mos6502_base_descriptor;
}

} // extern "C"