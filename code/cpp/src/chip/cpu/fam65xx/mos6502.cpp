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

#ifdef IMGUI_VERSION
#include "fam65xx_gui.h"
#endif

// ============================================================================
// CONCRETE CPU TYPE DEFINITIONS
// ============================================================================

// Define concrete CPU types for easier use
using mos6502_cpu_t = fam65xx::mos6502_cpu_impl_t;

// Cast helpers for opaque handles
#define CPU_CAST(type, ptr) reinterpret_cast<type*>(ptr)
#define CPU_CONST_CAST(type, ptr) reinterpret_cast<const type*>(ptr)

// ============================================================================
// MOS 6502 (Original NMOS) IMPLEMENTATION
// ============================================================================

mos6502_t* mos6502_create(void) {
    mos6502_t* cpu = reinterpret_cast<mos6502_t*>(new mos6502_cpu_t());
#ifdef IMGUI_VERSION
    fam65xx::register_mos6502_for_gui(cpu);
#endif
    return cpu;
}

void mos6502_destroy(mos6502_t* cpu) {
    // Unregister from GUI system before destroying
#ifdef IMGUI_VERSION
    fam65xx::unregister_cpu_from_gui(cpu);
#endif
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
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_A);
}

uint8_t mos6502_get_x(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_X);
}

uint8_t mos6502_get_y(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_Y);
}

uint8_t mos6502_get_s(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_S);
}

uint8_t mos6502_get_p(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_P);
}

uint16_t mos6502_get_pc(mos6502_t* cpu) {
    return CPU_CAST(mos6502_cpu_t, cpu)->get(REG_PC);
}

// Register setters
void mos6502_set_a(mos6502_t* cpu, uint8_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_A, value);
}

void mos6502_set_x(mos6502_t* cpu, uint8_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_X, value);
}

void mos6502_set_y(mos6502_t* cpu, uint8_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_Y, value);
}

void mos6502_set_s(mos6502_t* cpu, uint8_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_S, value);
}

void mos6502_set_p(mos6502_t* cpu, uint8_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_P, value);
}

void mos6502_set_pc(mos6502_t* cpu, uint16_t value) {
    CPU_CAST(mos6502_cpu_t, cpu)->set(REG_PC, value);
}

// ============================================================================
// Enhanced Descriptor API
// ============================================================================

static chip_descriptor_t mos6502_base_descriptor;

static void initialize_mos6502_descriptor() {
    mos6502_base_descriptor.description = "MOS Technology 6502 (NMOS)";
    mos6502_base_descriptor.create = [](chip_descriptor_t* desc) -> void* {
        return mos6502_create();
    };
    mos6502_base_descriptor.destroy = [](void* chip) {
        mos6502_destroy(reinterpret_cast<mos6502_t*>(chip));
    };
    mos6502_base_descriptor.bus_attach = nullptr;  // Basic CPU doesn't need bus attach
    mos6502_base_descriptor.bank_change = nullptr; // Basic CPU doesn't have banking
#ifdef IMGUI_VERSION
    mos6502_base_descriptor.render_debug_window = fam65xx_render_debug_window;
    mos6502_base_descriptor.render_settings_window = fam65xx_render_settings_window;
#endif
}

fam65xx_chip_descriptor_t* mos6502_create_descriptor(
    uint8_t (*read_callback)(void* user_data, uint16_t addr, uint8_t bus_state),
    void (*write_callback)(void* user_data, uint16_t addr, uint8_t data),
    void* user_data) {
    
    auto* desc = new fam65xx_chip_descriptor_t;
    desc->base = *mos6502_get_chip_descriptor();
    desc->mem_read = read_callback;
    desc->mem_write = write_callback;
    desc->mem_user_data = user_data;
    
    return desc;
}

void mos6502_destroy_descriptor(fam65xx_chip_descriptor_t* desc) {
    delete desc;
}

const chip_descriptor_t* mos6502_get_chip_descriptor(void) {
    static bool initialized = false;
    if (!initialized) {
        initialize_mos6502_descriptor();
        initialized = true;
    }
    return &mos6502_base_descriptor;
}
