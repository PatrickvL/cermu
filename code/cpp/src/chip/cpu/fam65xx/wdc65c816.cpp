/*
 * wdc65c816.cpp - C API Wrapper Implementation for WDC 65C816 CPU
 */

#include "wdc65c816.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using wdc65c816_cpu_t = fam65xx::wdc65c816_cpu_impl_t;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<wdc65c816_cpu_t*>(ptr)

// ============================================================================
// WDC 65C816 IMPLEMENTATION
// ============================================================================

wdc65c816_t* wdc65c816_create(void) {
    wdc65c816_t* cpu = reinterpret_cast<wdc65c816_t*>(new wdc65c816_cpu_t());
#ifdef IMGUI_VERSION
    fam65xx::register_wdc65c816_for_gui(cpu);
#endif
    return cpu;
}

void wdc65c816_destroy(wdc65c816_t* cpu) {
#ifdef IMGUI_VERSION
    fam65xx::unregister_cpu_from_gui(cpu);
#endif
    delete CPU_CAST(cpu);
}

bus_state_t wdc65c816_init(wdc65c816_t* cpu, const chip_descriptor_t* desc) {
    return CPU_CAST(cpu)->init(desc);
}

bus_state_t wdc65c816_reset(wdc65c816_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->reset(pins);
}

bus_state_t wdc65c816_tick(wdc65c816_t* cpu, bus_state_t pins) {
    return CPU_CAST(cpu)->tick(pins);
}

bool wdc65c816_opdone(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->opdone();
}

// 8-bit register getters (compatible mode)
uint8_t wdc65c816_get_a(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_A);
}

uint8_t wdc65c816_get_x(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_X);
}

uint8_t wdc65c816_get_y(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_Y);
}

uint8_t wdc65c816_get_s(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_SPL);
}

uint8_t wdc65c816_get_p(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_P);
}

uint16_t wdc65c816_get_pc(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_PC);
}

// 8-bit register setters (compatible mode)
void wdc65c816_set_a(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_A, value);
}

void wdc65c816_set_x(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_X, value);
}

void wdc65c816_set_y(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_Y, value);
}

void wdc65c816_set_s(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_SPL, value);
}

void wdc65c816_set_p(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_P, value);
}

void wdc65c816_set_pc(wdc65c816_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_PC, value);
}

// 16-bit register getters (65C816-specific)
uint16_t wdc65c816_get_a_full(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_A_16);
}

uint16_t wdc65c816_get_x_full(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_X_16);
}

uint16_t wdc65c816_get_y_full(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_Y_16);
}

uint16_t wdc65c816_get_d(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_D_16);
}

uint8_t wdc65c816_get_dbr(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_DBR);
}

uint8_t wdc65c816_get_pbr(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get(REG_PBR);
}

bool wdc65c816_get_emulation_mode(wdc65c816_t* cpu) {
    return CPU_CAST(cpu)->get_emulation_mode();
}

// 16-bit register setters (65C816-specific)
void wdc65c816_set_a_full(wdc65c816_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_A_16, value);
}

void wdc65c816_set_x_full(wdc65c816_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_X_16, value);
}

void wdc65c816_set_y_full(wdc65c816_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_Y_16, value);
}

void wdc65c816_set_d(wdc65c816_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_D_16, value);
}

void wdc65c816_set_dbr(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_DBR, value);
}

void wdc65c816_set_pbr(wdc65c816_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_PBR, value);
}

void wdc65c816_set_emulation_mode(wdc65c816_t* cpu, bool emulation) {
    CPU_CAST(cpu)->set_emulation_mode(emulation);
}

// WDC65C816 chip descriptor
static chip_descriptor_t wdc65c816_base_descriptor;

static void initialize_wdc65c816_descriptor() {
    wdc65c816_base_descriptor.description = "WDC 65C816";
    wdc65c816_base_descriptor.create = [](chip_descriptor_t* desc) -> void* {
        (void)desc; // Suppress unused parameter warning
        return wdc65c816_create();
    };
    wdc65c816_base_descriptor.destroy = [](void* chip) {
        wdc65c816_destroy(reinterpret_cast<wdc65c816_t*>(chip));
    };
    wdc65c816_base_descriptor.bus_attach = nullptr;  // Basic CPU doesn't need bus attach
    wdc65c816_base_descriptor.bank_change = nullptr; // Basic CPU doesn't have banking
#ifdef IMGUI_VERSION
    wdc65c816_base_descriptor.render_debug_window = fam65xx_render_debug_window;
    wdc65c816_base_descriptor.render_settings_window = fam65xx_render_settings_window;
#endif
}

const chip_descriptor_t* wdc65c816_get_chip_descriptor(void) {
    static bool initialized = false;
    if (!initialized) {
        initialize_wdc65c816_descriptor();
        initialized = true;
    }
    return &wdc65c816_base_descriptor;
}
