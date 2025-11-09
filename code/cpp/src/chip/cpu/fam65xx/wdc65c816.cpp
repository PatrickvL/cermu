/*
 * wdc65c816.cpp - C API Wrapper Implementation for WDC 65C816 CPU
 */

#include "wdc65c816.h"
#include "fam65xx.hpp"
#include "fam65xx_gui.h"

// ============================================================================
// CONCRETE CPU TYPE DEFINITION
// ============================================================================

using wdc65c816_cpu_t = fam65xx::fam65xx_t<fam65xx::WDC_65C816>;

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

// 16-bit register getters (65C816-specific) - PLACEHOLDER IMPLEMENTATIONS
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
    // TODO: Implement direct page register access
    (void)cpu; // Suppress unused parameter warning
    return 0x0000; // Placeholder - return default direct page
}

uint8_t wdc65c816_get_dbr(wdc65c816_t* cpu) {
    // TODO: Implement data bank register access
    (void)cpu; // Suppress unused parameter warning
    return 0x00; // Placeholder - return default data bank
}

uint8_t wdc65c816_get_pbr(wdc65c816_t* cpu) {
    // TODO: Implement program bank register access
    (void)cpu; // Suppress unused parameter warning
    return 0x00; // Placeholder - return default program bank
}

bool wdc65c816_get_emulation_mode(wdc65c816_t* cpu) {
    // TODO: Implement emulation mode status access
    (void)cpu; // Suppress unused parameter warning
    return true; // Placeholder - return emulation mode active
}

// 16-bit register setters (65C816-specific) - PLACEHOLDER IMPLEMENTATIONS
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
    // TODO: Implement direct page register setting
    (void)cpu;   // Suppress unused parameter warning
    (void)value; // Suppress unused parameter warning
}

void wdc65c816_set_dbr(wdc65c816_t* cpu, uint8_t value) {
    // TODO: Implement data bank register setting
    (void)cpu;   // Suppress unused parameter warning
    (void)value; // Suppress unused parameter warning
}

void wdc65c816_set_pbr(wdc65c816_t* cpu, uint8_t value) {
    // TODO: Implement program bank register setting
    (void)cpu;   // Suppress unused parameter warning
    (void)value; // Suppress unused parameter warning
}

void wdc65c816_set_emulation_mode(wdc65c816_t* cpu, bool emulation) {
    // TODO: Implement emulation mode setting
    (void)cpu;       // Suppress unused parameter warning
    (void)emulation; // Suppress unused parameter warning
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
