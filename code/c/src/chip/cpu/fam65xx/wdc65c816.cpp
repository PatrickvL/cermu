/*
 * wdc65c816.cpp - C API Wrapper Implementation for WDC 65C816 CPU
 */

#include "wdc65c816.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

using namespace fam65xx;

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using wdc65c816_cpu_t = fam65xx_t<WDC_65C816>;

// Cast helper for opaque handle
#define CPU_CAST(ptr) reinterpret_cast<wdc65c816_cpu_t*>(ptr)

// ============================================================================
// WDC 65C816 IMPLEMENTATION
// ============================================================================

extern "C" {

wdc65c816_t* wdc65c816_create(void) {
    return reinterpret_cast<wdc65c816_t*>(new wdc65c816_cpu_t());
}

void wdc65c816_destroy(wdc65c816_t* cpu) {
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    unregister_cpu_from_gui(cpu);
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
// Note: These will need to be implemented once the wide_registers mixin is properly defined
uint16_t wdc65c816_get_a_full(wdc65c816_t* cpu) {
    // TODO: Access full 16-bit A register from wide_registers mixin
    return CPU_CAST(cpu)->get(REG_A); // Placeholder - return 8-bit for now
}

uint16_t wdc65c816_get_x_full(wdc65c816_t* cpu) {
    // TODO: Access full 16-bit X register from wide_registers mixin
    return CPU_CAST(cpu)->get(REG_X); // Placeholder - return 8-bit for now
}

uint16_t wdc65c816_get_y_full(wdc65c816_t* cpu) {
    // TODO: Access full 16-bit Y register from wide_registers mixin
    return CPU_CAST(cpu)->get(REG_Y); // Placeholder - return 8-bit for now
}

uint16_t wdc65c816_get_d(wdc65c816_t* cpu) {
    // TODO: Access D (direct page) register from wide_registers mixin
    return 0x0000; // Placeholder
}

uint8_t wdc65c816_get_dbr(wdc65c816_t* cpu) {
    // TODO: Access DBR (data bank register) from wide_registers mixin
    return 0x00; // Placeholder
}

uint8_t wdc65c816_get_pbr(wdc65c816_t* cpu) {
    // TODO: Access PBR (program bank register) from wide_registers mixin
    return 0x00; // Placeholder
}

bool wdc65c816_get_emulation_mode(wdc65c816_t* cpu) {
    // TODO: Access emulation mode from wide_registers mixin
    return true; // Placeholder - start in emulation mode
}

// 16-bit register setters (65C816-specific)
void wdc65c816_set_a_full(wdc65c816_t* cpu, uint16_t value) {
    // TODO: Set full 16-bit A register in wide_registers mixin
    CPU_CAST(cpu)->set(REG_A, value & 0xFF); // Placeholder - set 8-bit for now
}

void wdc65c816_set_x_full(wdc65c816_t* cpu, uint16_t value) {
    // TODO: Set full 16-bit X register in wide_registers mixin
    CPU_CAST(cpu)->set(REG_X, value & 0xFF); // Placeholder - set 8-bit for now
}

void wdc65c816_set_y_full(wdc65c816_t* cpu, uint16_t value) {
    // TODO: Set full 16-bit Y register in wide_registers mixin
    CPU_CAST(cpu)->set(REG_Y, value & 0xFF); // Placeholder - set 8-bit for now
}

void wdc65c816_set_d(wdc65c816_t* cpu, uint16_t value) {
    // TODO: Set D (direct page) register in wide_registers mixin
    (void)value; // Placeholder
}

void wdc65c816_set_dbr(wdc65c816_t* cpu, uint8_t value) {
    // TODO: Set DBR (data bank register) in wide_registers mixin
    (void)value; // Placeholder
}

void wdc65c816_set_pbr(wdc65c816_t* cpu, uint8_t value) {
    // TODO: Set PBR (program bank register) in wide_registers mixin
    (void)value; // Placeholder
}

void wdc65c816_set_emulation_mode(wdc65c816_t* cpu, bool emulation) {
    // TODO: Set emulation mode in wide_registers mixin
    (void)emulation; // Placeholder
}

// WDC65C816 chip descriptor
static chip_descriptor_t wdc65c816_base_descriptor;

static void initialize_wdc65c816_descriptor() {
    wdc65c816_base_descriptor.description = "WDC 65C816";
    wdc65c816_base_descriptor.create = [](chip_descriptor_t* desc) -> void* {
        return wdc65c816_create();
    };
    wdc65c816_base_descriptor.destroy = [](void* chip) {
        wdc65c816_destroy(reinterpret_cast<wdc65c816_t*>(chip));
    };
    wdc65c816_base_descriptor.bus_attach = nullptr;  // Basic CPU doesn't need bus attach
    wdc65c816_base_descriptor.bank_change = nullptr; // Basic CPU doesn't have banking
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    wdc65c816_base_descriptor.render_debug_window = [](void* cpu_handle, bool* show_window) {
        wdc65c816_t* cpu = static_cast<wdc65c816_t*>(cpu_handle);
        render_cpu_debug_window<WDC_65C816>(cpu, "WDC65C816");
    };
    wdc65c816_base_descriptor.render_settings_window = [](void* cpu_handle, bool* show_window) {
        wdc65c816_t* cpu = static_cast<wdc65c816_t*>(cpu_handle);
        render_cpu_settings_window<WDC_65C816>(cpu, "WDC65C816");
    };
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

} // extern "C"