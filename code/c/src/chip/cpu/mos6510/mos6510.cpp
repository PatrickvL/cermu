#include "mos6510.h"
#define AIEMUC_IMPL
#include "../fam65xx/fam65xx_variants.hpp"
#include "../../../core/chip.h"
#include <cstring>

// Provide a simple implementation of the C API functions that the template system expects
// These will be implemented as lightweight wrappers around the template system

extern "C" {

// Simple placeholder implementations for the C API functions
bus_state_t fam65xx_init(fam65xx_t* cpu, const fam65xx_desc_t* desc) {
    // Basic initialization - clear the CPU state
    memset(cpu, 0, sizeof(fam65xx_t));
    
    // Set up memory callbacks if provided
    if (desc) {
        cpu->mem_read = desc->mem_read;
        cpu->mem_write = desc->mem_write;
        cpu->mem_user_data = desc->mem_user_data;
    }
    
    // Initialize register layout
    cpu->reg8[REG_ZPH] = 0x00;  // Zero page high byte
    cpu->reg8[REG_SPH] = 0x01;  // Stack pointer high byte
    
    // Initialize interrupt state
    cpu->interrupt_shift_register = 0x00000000;
    cpu->nmi_prev = 1;
    
    // Initialize default CPU state
    CPU_P(cpu) = FLAG_U | FLAG_I;
    CPU_S(cpu) = 0xFD;
    
    // Set up default bus pins state
    bus_state_t pins = 0;
    pins |= FAM65XX_RDY;
    pins |= FAM65XX_RW;
    pins |= FAM65XX_IRQ;
    pins |= FAM65XX_NMI;
    pins |= FAM65XX_RES;
    
    // CPU is initialized but not executing
    cpu->current_handler = NULL;
    cpu->cycle_index = 0;
    
    return pins;
}

bus_state_t fam65xx_tick(fam65xx_t* cpu, bus_state_t pins) {
    // Simple tick implementation - just return pins for now
    // In a full implementation, this would execute one CPU cycle
    return pins;
}

bus_state_t fam65xx_reset(fam65xx_t* cpu, bus_state_t pins) {
    // Simple reset implementation
    cpu->interrupt_shift_register = 0x00000000;
    cpu->nmi_prev = 1;
    cpu->brk_flags |= FAM65XX_BRK_RESET;
    CPU_P(cpu) = FLAG_U | FLAG_I;
    CPU_S(cpu) = 0xFF;
    return pins;
}

bus_state_t fam65xx_bootstrap(fam65xx_t* cpu, bus_state_t pins) {
    // Simple bootstrap implementation
    pins |= FAM65XX_RDY;
    pins |= FAM65XX_RW;
    pins |= FAM65XX_IRQ;
    pins |= FAM65XX_NMI;
    pins |= FAM65XX_RES;
    cpu->brk_flags = 0;
    cpu->interrupt_shift_register = 0x00000000;
    cpu->nmi_prev = 1;
    cpu->cycle_index = 0;
    return pins;
}

bool fam65xx_opdone(fam65xx_t* cpu) {
    // Simple implementation - always return true for now
    return true;
}

// CPU state accessor functions
void fam65xx_set_a(fam65xx_t* cpu, uint8_t v) { CPU_A(cpu) = v; }
void fam65xx_set_x(fam65xx_t* cpu, uint8_t v) { CPU_X(cpu) = v; }
void fam65xx_set_y(fam65xx_t* cpu, uint8_t v) { CPU_Y(cpu) = v; }
void fam65xx_set_s(fam65xx_t* cpu, uint8_t v) { CPU_S(cpu) = v; }
void fam65xx_set_p(fam65xx_t* cpu, uint8_t v) { CPU_P(cpu) = v; }
void fam65xx_set_pc(fam65xx_t* cpu, uint16_t v) { CPU_PC(cpu) = v; }

uint8_t fam65xx_a(fam65xx_t* cpu) { return CPU_A(cpu); }
uint8_t fam65xx_x(fam65xx_t* cpu) { return CPU_X(cpu); }
uint8_t fam65xx_y(fam65xx_t* cpu) { return CPU_Y(cpu); }
uint8_t fam65xx_s(fam65xx_t* cpu) { return CPU_S(cpu); }
uint8_t fam65xx_p(fam65xx_t* cpu) { return CPU_P(cpu); }
uint16_t fam65xx_pc(fam65xx_t* cpu) { return CPU_PC(cpu); }

} // extern "C"

// C wrapper around the template-based CPU - zero overhead
struct mos6510_chip {
    fam65xx_variants::CPU<fam65xx_variants::MOS6510Tag> cpu;
};

mos6510_chip_t* mos6510_create(void) {
    return new mos6510_chip_t;
}

void mos6510_destroy(mos6510_chip_t* cpu) {
    delete cpu;
}

void mos6510_init(mos6510_chip_t* cpu,
                  void (*io_callback)(uint16_t addr, uint8_t data, bool write)) {
    (void)io_callback; // TODO: Implement IO callback integration
    cpu->cpu.init();
}

bus_state_t mos6510_tick(mos6510_chip_t* cpu, bus_state_t bus_state) {
    return cpu->cpu.tick(bus_state);
}

uint16_t mos6510_get_pc(mos6510_chip_t* cpu) {
    return cpu->cpu.pc();
}

uint8_t mos6510_get_a(mos6510_chip_t* cpu) {
    return cpu->cpu.a();
}

uint8_t mos6510_get_x(mos6510_chip_t* cpu) {
    return cpu->cpu.x();
}

uint8_t mos6510_get_y(mos6510_chip_t* cpu) {
    return cpu->cpu.y();
}

uint8_t mos6510_get_s(mos6510_chip_t* cpu) {
    return cpu->cpu.s();
}

uint8_t mos6510_get_p(mos6510_chip_t* cpu) {
    return cpu->cpu.p();
}

// Chip interface functions for system integration
static void* mos6510_chip_create(chip_descriptor_t* desc) {
    (void)desc; // Unused parameter
    return mos6510_create();
}

static void mos6510_chip_destroy(void* chip) {
    mos6510_destroy((mos6510_chip_t*)chip);
}

bus_state_t mos6510_tick_chip(void* chip, bus_state_t bus_state) {
    return mos6510_tick((mos6510_chip_t*)chip, bus_state);
}

// Chip descriptor for system registration
chip_descriptor_t mos6510_descriptor = {
    .description = "MOS6510 CPU",
    .create = mos6510_chip_create,
    .destroy = mos6510_chip_destroy,
    .bus_attach = NULL, // No special bus attachment needed
    .bank_change = NULL, // No banking change needed
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = NULL, // No GUI debug window implemented yet
    .render_settings_window = NULL // No GUI settings window implemented yet
#endif
};