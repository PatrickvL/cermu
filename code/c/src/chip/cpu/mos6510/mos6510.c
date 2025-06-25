#include "mos6510.h"
#include "../../../core/chip.h"
#include "../fam65xx/fam65xx_core.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// CHIP DESCRIPTOR FUNCTIONS
// ============================================================================

// Chip functions for MOS6510
static void* mos6510_create(chip_descriptor_t* desc) {
    mos6510_t* cpu = malloc(sizeof(mos6510_t));
    if (cpu) {
        cpu->base.desc = desc;
        mos6510_init(cpu);
    }
    return cpu;
}

static void mos6510_destroy(void* chip) {
    if (chip) {
        free(chip);
    }
}

uint8_t mos6510_zeropage_read(void* chip, uint16_t address) {
    mos6510_t* cpu = (mos6510_t*)chip;

    // Handle MOS6510 zero page I/O ports - addresses $0000 and $0001
    if (address == 0) {
        // Return Data Direction Register
        return cpu->io_port[0];
    } else if (address == 1) {
        // Return port: outputs defined by DDR bits, inputs from external pins
        uint8_t ddr = cpu->io_port[0];
        uint8_t data = cpu->io_port[1];
        uint8_t external = cpu->io_interface.read_external_pins(cpu->io_interface.context, data, ddr);
        return (data & ddr) | (external & ~ddr);
    } else {
        // For addresses $0002-$00FF, access system RAM directly to avoid circular dependency
        return cpu->ram_access.read_func(cpu->ram_access.context, address);
    }
}

void mos6510_zeropage_write(void* chip, uint16_t address, uint8_t value) {
    mos6510_t* cpu = (mos6510_t*)chip;

    // Handle MOS6510 zero page I/O ports - addresses $0000 and $0001
    if (address <= 1) {
        // Update Data Direction / Data register
        cpu->io_port[address] = value;

        // Notify system of output pin changes
        uint8_t ddr = cpu->io_port[0];
        uint8_t port_data = cpu->io_port[1];
        cpu->io_interface.output_pins_changed(cpu->io_interface.context, port_data, ddr);
    } else {
        // For addresses $0002-$00FF, write to system RAM directly to avoid circular dependency
        if (cpu->ram_access.write_func) {
            cpu->ram_access.write_func(cpu->ram_access.context, address, value);
        } else {
            // Fallback to bus interface if RAM not attached yet (during initialization)
            cpu->base.bus_interface.bus_write(cpu->base.bus_interface.context, address, value);
        }
    }
}

chip_descriptor_t mos6510_descriptor = {
    .description = "MOS6510 CPU with I/O Ports",
    .create = mos6510_create,
    .destroy = mos6510_destroy,
    .bus_attach = NULL,
    .read = mos6510_zeropage_read,
    .write = mos6510_zeropage_write,
    .bank_change = NULL,
    .get_rwcb_context = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6510_render_debug_window,
    .render_settings_window = mos6510_render_settings_window
#endif
};

// ============================================================================
// CPU OPERATION FUNCTIONS (inline for performance)
// ============================================================================

// Public API to begin interception
void mos6510_start_intercept(mos6510_t* cpu) {
    if (!cpu) return;
    fam65xx_start_intercept(&cpu->base);
}

// Public API to cancel interception early if needed
void mos6510_stop_intercept(mos6510_t* cpu) {
    if (!cpu) return;
    fam65xx_stop_intercept(&cpu->base);
}

// Check if interception is currently active
bool mos6510_is_intercepting(mos6510_t* cpu) {
    return cpu ? fam65xx_is_intercepting(&cpu->base) : false;
}

// ============================================================================
// CPU LIFECYCLE WRAPPER FUNCTIONS
// ============================================================================

// CPU lifecycle wrapper functions
void mos6510_init(mos6510_t* cpu) {
    // Initialize CPU registers and state
    cpu->base.a = 0;
    cpu->base.x = 0;
    cpu->base.y = 0;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I;  // Unused flag always set, interrupt disable
    cpu->base.pc = 0;
    cpu->base.address = 0;

    // Initialize opcode table with MOS6510 features (no decimal mode, has illegal opcodes)
    uint32_t features = FAM65XX_FEATURE_ILLEGAL_OPCODES;
    fam65xx_init_opcode_table(&cpu->base, features);
    
    // MOS6510 specific: decimal mode disabled automatically by feature flags

    // 6510-specific I/O port (addresses $0000/$0001) initialization
    cpu->io_port[0] = 0x2F;  // Default Data Direction Register (DDR at $0000)
    cpu->io_port[1] = 0x37;  // Default I/O Port Data (at $0001)
    
    // Initialize RAM accessors to NULL (will be set by mos6510_attach_ram)
    cpu->ram_access.context = NULL;
    cpu->ram_access.read_func = NULL;
    cpu->ram_access.write_func = NULL;
}

// Reset CPU
void mos6510_reset(mos6510_t* cpu) {
    // Read reset vector from $FFFC/$FFFD
    uint8_t pcl = mos6510_read_cycle(cpu, 0xFFFC);
    uint8_t pch = mos6510_read_cycle(cpu, 0xFFFD);

    cpu->base.pc = (pch << 8) | pcl;
    cpu->base.sp = 0xFF;
    cpu->base.p |= FLAG_I;  // Set interrupt disable
}

bool mos6510_step(mos6510_t* cpu) {
    if (!cpu) return false;
    
    // Use the shared family step implementation
    // This ensures consistent single-step behavior across all family members
    return fam65xx_step(&cpu->base);
}

// ============================================================================
// CPU EXECUTION LOOP WITH FUNCTION POINTERS (Universal)
// ============================================================================
void mos6510_execute(mos6510_t* cpu) {
    // Start execution using threaded dispatch
    // The MOS6510_OPCODE_FOOTER macro will chain instructions until intercept is triggered
    FAM65XX_NEXT_INSTRUCTION(&cpu->base);
}

// ============================================================================
// MOS6510 NOW USES FAMILY OPCODE TABLE
// ============================================================================

// Note: MOS6510 now uses the shared family opcode table through fam65xx_init_opcode_table()
// All 256 opcodes are implemented in the family core with proper implementations
// No MOS6510-specific overrides are needed since the family table already provides binary-only arithmetic

// ============================================================================
// PERFORMANCE-OPTIMIZED INTERFACE ATTACHMENT FUNCTIONS
// ============================================================================

/**
 * Attach bus interface to CPU by copying entire interface struct by value.
 * This provides zero-indirection access while maintaining clean organization.
 */
void mos6510_attach_bus_interface(mos6510_t* cpu, const bus_cycle_ops_t* bus_interface) {
    // Copy entire interface struct by value for zero-indirection access
    cpu->base.bus_interface = *bus_interface;
}

/**
 * Attach control lines interface to CPU by copying entire interface struct by value.
 * This provides zero-indirection access while maintaining clean organization.
 */
void mos6510_attach_control_lines_interface(mos6510_t* cpu, const control_lines_interface_t* control_interface) {
    // Copy entire interface struct by value for zero-indirection access
    cpu->base.control_interface = *control_interface;
}

/**
 * Attach I/O port interface to CPU by copying entire interface struct by value.
 * This provides zero-indirection access while maintaining clean organization.
 */
void mos6510_attach_io_interface(mos6510_t* cpu, const mos6510_io_port_interface_t* io_interface) {
    // Copy entire interface struct by value for zero-indirection access
    cpu->io_interface = *io_interface;
}

/**
 * Attach system lines state to CPU.
 * Note: This is a pointer to shared system state, not copied.
 */
void mos6510_attach_system_lines(mos6510_t* cpu, system_lines_t* system_lines) {
    if (!cpu || !system_lines) return;
    
    // Store pointer to shared system state
    cpu->base.system_lines = system_lines;
}

/**
 * Attach RAM directly to CPU for zero page access ($0002-$00FF).
 * This provides direct access to RAM without going through the bus interface,
 * which is essential to avoid circular dependencies when the CPU needs to access
 * zero page memory during system initialization.
 */
void mos6510_attach_ram(mos6510_t* cpu, const access_callback_t* ram_access) {
    if (!cpu || !ram_access) return;
    
    // Copy the access callback structure
    cpu->ram_access = *ram_access;
    
    // Ensure valid function pointers, falling back to generic stubs if not provided
    if (!cpu->ram_access.read_func) {
        cpu->ram_access.read_func = generic_stub_read;
    }
    if (!cpu->ram_access.write_func) {
        cpu->ram_access.write_func = generic_stub_write;
    }
}
