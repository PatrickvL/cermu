#include "nes6502.h"
#include "../fam65xx/fam65xx_core.h"
#include "../../../core/system.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// NES 6502 IMPLEMENTATION
// ============================================================================

// No custom opcode handlers needed - feature flags handle NES characteristics

// Chip interface implementation
bool nes6502_create(chip_descriptor_t* desc, nes6502_t* cpu) {
    if (!desc || !cpu) {
        return false;
    }
    
    // Initialize base 6502 family structure
    cpu->base.desc = desc;
    
    // Initialize CPU state
    cpu->base.pc = 0x0000;
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    cpu->base.address = 0x0000;
    
    // Initialize opcode table with NES6502 features (no decimal mode)
    uint32_t features = FAM65XX_FEATURE_ILLEGAL_OPCODES;  // No decimal mode, no I/O ports
    fam65xx_init_opcode_table(&cpu->base, features);
    // No decimal mode flag means binary-only ADC/SBC will be automatically installed
  
    return true;
}

void nes6502_destroy(nes6502_t* cpu) {
    if (!cpu) return;
    
    // Nothing special to clean up for NES 6502
    // Just clear the structure
    memset(cpu, 0, sizeof(nes6502_t));
}

void nes6502_reset(nes6502_t* cpu) {
    if (!cpu) return;
    
    // Reset CPU state to power-on defaults
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    
    // Load reset vector
    uint8_t addr_lo = fam65xx_read_cycle(&cpu->base, 0xFFFC);
    uint8_t addr_hi = fam65xx_read_cycle(&cpu->base, 0xFFFD);
    cpu->base.pc = (addr_hi << 8) | addr_lo;
}

bool nes6502_step(nes6502_t* cpu) {
    if (!cpu) return false;
    return fam65xx_step(&cpu->base);
}

// Configuration and setup functions
void nes6502_attach_bus(nes6502_t* cpu, const bus_cycle_ops_t* bus_interface) {
    if (cpu && bus_interface) {
        cpu->base.bus_interface = *bus_interface;
    }
}

void nes6502_attach_control_lines(nes6502_t* cpu, const control_lines_interface_t* control_interface) {
    if (cpu && control_interface) {
        cpu->base.control_interface = *control_interface;
    }
}

void nes6502_attach_bus_state(nes6502_t* cpu, bus_state_t* bus_state) {
    if (cpu) {
        cpu->base.bus_state = bus_state;
    }
}

// State access functions
uint16_t nes6502_get_pc(nes6502_t* cpu) { return cpu ? cpu->base.pc : 0; }
uint8_t nes6502_get_a(nes6502_t* cpu) { return cpu ? cpu->base.a : 0; }
uint8_t nes6502_get_x(nes6502_t* cpu) { return cpu ? cpu->base.x : 0; }
uint8_t nes6502_get_y(nes6502_t* cpu) { return cpu ? cpu->base.y : 0; }
uint8_t nes6502_get_sp(nes6502_t* cpu) { return cpu ? cpu->base.sp : 0; }
uint8_t nes6502_get_p(nes6502_t* cpu) { return cpu ? cpu->base.p : 0; }

void nes6502_set_pc(nes6502_t* cpu, uint16_t value) { if (cpu) cpu->base.pc = value; }
void nes6502_set_a(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.a = value; }
void nes6502_set_x(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.x = value; }
void nes6502_set_y(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.y = value; }
void nes6502_set_sp(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.sp = value; }
void nes6502_set_p(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.p = value; }

// Test and debug support
void nes6502_start_intercept(nes6502_t* cpu) {
    if (cpu) fam65xx_start_intercept(&cpu->base);
}

void nes6502_stop_intercept(nes6502_t* cpu) {
    if (cpu) fam65xx_stop_intercept(&cpu->base);
}

bool nes6502_is_intercepting(nes6502_t* cpu) {
    return cpu ? fam65xx_is_intercepting(&cpu->base) : false;
}

// NES 6502 uses feature flags for automatic binary-only arithmetic
// No custom opcode implementations needed

// ============================================================================
// CHIP DESCRIPTOR
// ============================================================================

// Wrapper functions for chip descriptor interface
static void* nes6502_create_wrapper(chip_descriptor_t* desc) {
    nes6502_t* cpu = malloc(sizeof(nes6502_t));
    if (!cpu) return NULL;
    
    if (!nes6502_create(desc, cpu)) {
        free(cpu);
        return NULL;
    }
    return cpu;
}

static void nes6502_destroy_wrapper(void* chip) {
    if (chip) {
        nes6502_destroy((nes6502_t*)chip);
        free(chip);
    }
}

chip_descriptor_t nes6502_descriptor = {
    .description = "NES 6502 CPU (No Decimal Mode)",
    .create = nes6502_create_wrapper,
    .destroy = nes6502_destroy_wrapper,
    .bus_attach = NULL,
    .read = NULL,  // NES 6502 doesn't have special read behavior
    .write = NULL, // NES 6502 doesn't have special write behavior
    .bank_change = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = nes6502_render_debug_window,
    .render_settings_window = NULL
#endif
};
