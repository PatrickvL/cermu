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

chip_descriptor_t mos6510_descriptor = {
    .description = "MOS6510 CPU with I/O Ports",
    .create = mos6510_create,
    .destroy = mos6510_destroy,
    .bus_attach = NULL,
    .read = mos6510_ioport_read,
    .write = mos6510_ioport_write,
    .bank_change = NULL,
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
#ifdef REDESIGN
    // Use the proper Nostradamus Distributor execution engine
    // This will execute 500+ instructions continuously with proper instruction traces
    fam65xx_execute_nostradamus(&cpu->base, 500);
#else
    // Execute multiple instructions in a loop to show more trace output
    // Since fam65xx_next_instruction_dispatch now executes only one instruction per call,
    // we need to call it multiple times to get continuous execution
    for (int i = 0; i < 100; i++) {  // Execute 100 instructions per call
        FAM65XX_NEXT_INSTRUCTION(&cpu->base);
    }
#endif
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
 * Attach bus state to CPU.
 * Note: This is a pointer to shared bus state, not copied.
 */
void mos6510_attach_bus_state(mos6510_t* cpu, bus_state_t* bus_state) {
    if (!cpu || !bus_state) return;
    
    // Store pointer to shared bus state
    cpu->base.bus_state = bus_state;
}

// ============================================================================
// I/O PORT HANDLING FUNCTIONS FOR REFACTORING PLAN
// ============================================================================

/**
 * Handle I/O port read operations for MOS6510 (addresses 0-1).
 * This is part of the refactoring plan to move I/O port handling into the CPU.
 * Initially calls existing ZEROBANK callbacks but will be optimized later.
 */
bus_state_t mos6510_handle_io_read(mos6510_t* cpu, bus_state_t bus_state) {
    if (!cpu) return bus_state;
    
    // For now, delegate to the existing I/O port read function
    // This maintains compatibility with the current ZEROBANK system
    // In future phases, this will be optimized to handle banking changes directly
    return mos6510_ioport_read(cpu, bus_state);
}

/**
 * Handle I/O port write operations for MOS6510 (addresses 0-1).
 * This is part of the refactoring plan to move I/O port handling into the CPU.
 * Initially calls existing ZEROBANK callbacks but will be optimized later.
 */
bus_state_t mos6510_handle_io_write(mos6510_t* cpu, bus_state_t bus_state) {
    if (!cpu) return bus_state;
    
    // For now, delegate to the existing I/O port write function
    // This maintains compatibility with the current ZEROBANK system
    // In future phases, this will:
    // 1. Handle memory banking change notifications (LORAM, HIRAM, CHAREN bits)
    // 2. Update memory_tick() to call MOS6510 I/O functions for addresses 0-1
    // 3. Remove dependency on CHIP_ZEROBANK infrastructure
    return mos6510_ioport_write(cpu, bus_state);
}
