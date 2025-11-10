/*
 * wdc65c02.cpp - C API Wrapper Implementation for WDC 65C02 CPU
 */

#include "wdc65c02.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using wdc65c02_cpu_t = fam65xx::wdc65c02_cpu_impl_t;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<wdc65c02_cpu_t*>(ptr)

// ============================================================================
// WDC 65C02 IMPLEMENTATION
// ============================================================================

wdc65c02_t* wdc65c02_create(void) {
    return reinterpret_cast<wdc65c02_t*>(new wdc65c02_cpu_t());
}

void wdc65c02_destroy(wdc65c02_t* cpu) {
    // Unregister from GUI system before destroying
#ifdef IMGUI_VERSION
    fam65xx::unregister_cpu_from_gui(cpu);
#endif
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
    return CPU_CAST(cpu)->get(REG_A);
}

uint8_t wdc65c02_get_x(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->get(REG_X);
}

uint8_t wdc65c02_get_y(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->get(REG_Y);
}

uint8_t wdc65c02_get_s(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->get(REG_SPL);
}

uint8_t wdc65c02_get_p(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->get(REG_P);
}

uint16_t wdc65c02_get_pc(wdc65c02_t* cpu) {
    return CPU_CAST(cpu)->get(REG_PC);
}

// Register setters
void wdc65c02_set_a(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_A, value);
}

void wdc65c02_set_x(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_X, value);
}

void wdc65c02_set_y(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_Y, value);
}

void wdc65c02_set_s(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_SPL, value);
}

void wdc65c02_set_p(wdc65c02_t* cpu, uint8_t value) {
    CPU_CAST(cpu)->set(REG_P, value);
}

void wdc65c02_set_pc(wdc65c02_t* cpu, uint16_t value) {
    CPU_CAST(cpu)->set(REG_PC, value);
}

// ============================================================================
// CHIP DESCRIPTOR
// ============================================================================

static chip_descriptor_t wdc65c02_base_descriptor;

static void initialize_wdc65c02_descriptor() {
    wdc65c02_base_descriptor.description = "WDC 65C02 (CMOS)";
    wdc65c02_base_descriptor.create = [](chip_descriptor_t* desc) -> void* {
        return wdc65c02_create();
    };
    wdc65c02_base_descriptor.destroy = [](void* chip) {
        wdc65c02_destroy(reinterpret_cast<wdc65c02_t*>(chip));
    };
    wdc65c02_base_descriptor.bus_attach = nullptr;
    wdc65c02_base_descriptor.bank_change = nullptr;
#ifdef IMGUI_VERSION
    wdc65c02_base_descriptor.render_debug_window = fam65xx_render_debug_window;
    wdc65c02_base_descriptor.render_settings_window = fam65xx_render_settings_window;
#endif
}

const chip_descriptor_t* wdc65c02_get_chip_descriptor(void) {
    static bool initialized = false;
    if (!initialized) {
        initialize_wdc65c02_descriptor();
        initialized = true;
    }
    return &wdc65c02_base_descriptor;
}
