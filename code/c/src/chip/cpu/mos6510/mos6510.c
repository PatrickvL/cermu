#include "mos6510.h"
#include "../../../core/chip.h"
#include "../fam65xx/fam65xx_core.h"
#include "../../../core/ioport.h"
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

// ============================================================================
// MOS6510 I/O PORT CALLBACKS - Interface between generic I/O ports and C64 system
// ============================================================================

// Callback for reading external pins from the C64 system
static uint32_t mos6510_read_external_pins(void* context, uint8_t port_index) {
    // For the C64, certain bits are driven by hardware
    // Since we removed the legacy interface, we use default behavior
    // In a real system, this would read actual hardware pin states
    uint8_t external = 0xFF; // Default all inputs high
    
    // Bit 4 (cassette sense) might be driven low by hardware
    // For now, keep it high (no cassette)
    
    return (uint32_t)external;
}

// Callback for output pin changes - handles banking change detection
static void mos6510_output_pins_changed(void* context, uint8_t port_index, uint32_t new_value, uint32_t ddr) {
    mos6510_t* cpu = (mos6510_t*)context;
    
    // Banking control and other system functions (Port 1 equivalent - data register)
    uint8_t old_banking_bits = (cpu->banking_state.loram ? 0x01 : 0) |
                              (cpu->banking_state.hiram ? 0x02 : 0) |
                              (cpu->banking_state.charen ? 0x04 : 0);
    
    // Update banking state from new port value
    cpu->banking_state.loram = (new_value & 0x01) != 0;
    cpu->banking_state.hiram = (new_value & 0x02) != 0;
    cpu->banking_state.charen = (new_value & 0x04) != 0;
    cpu->banking_state.cassette_write = (new_value & 0x08) != 0;
    // Note: cassette_sense (bit 4) is read-only and not updated here
    cpu->banking_state.tape_motor = (new_value & 0x20) != 0;
    
    // Check if banking bits changed
    uint8_t new_banking_bits = new_value & 0x07;
    if (old_banking_bits != new_banking_bits) {
        // Notify system of banking change for PLA recalculation
        if (cpu->banking_callback && cpu->banking_context) {
            cpu->banking_callback(cpu->banking_context, new_banking_bits);
        }
    }
}

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

    // Initialize MOS6510 single I/O port (6-bit port with floating bus for bits 6-7)
    // The MOS6510 has only 6 physical I/O pins (bits 0-5), bits 6-7 are internal/floating
    // The ioport_t handles both DDR (address 0) and Data (address 1) internally
    ioport_init(&cpu->io_port, 0, 6, 0x2F, 0x37, 0xFF); // 6-bit port, DDR=0x2F, Data=0x37, bits 6-7 float
    
    // Set up I/O port interface
    ioport_interface_t interface = {
        .context = cpu,
        .read_external_pins = mos6510_read_external_pins,
        .output_pins_changed = mos6510_output_pins_changed
    };
    ioport_attach_interface(&cpu->io_port, &interface);
    
    // Initialize banking state from default port value (0x37)
    cpu->banking_state.loram = true;         // Bit 0 set
    cpu->banking_state.hiram = true;         // Bit 1 set
    cpu->banking_state.charen = true;        // Bit 2 set
    cpu->banking_state.cassette_write = true; // Bit 3 set
    cpu->banking_state.cassette_sense = false; // Bit 4 clear (read-only)
    cpu->banking_state.tape_motor = true;    // Bit 5 set
    
    // Initialize banking callback
    cpu->banking_context = NULL;
    cpu->banking_callback = NULL;
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
// MOS6510 BUS INTERFACE CALLBACKS - Forward Declarations
// ============================================================================

// Forward declarations for bus interface callbacks
static uint8_t mos6510_bus_read_callback(void* context, uint16_t address);
static void mos6510_bus_write_callback(void* context, uint16_t address, uint8_t data);

// ============================================================================
// PERFORMANCE-OPTIMIZED INTERFACE ATTACHMENT FUNCTIONS
// ============================================================================

/**
 * Attach bus interface to CPU with MOS6510 I/O port integration.
 * This preserves the original interface and installs MOS6510 bus callbacks
 * to handle I/O port addresses (0-1) transparently.
 */
void mos6510_attach_bus_interface(mos6510_t* cpu, const bus_cycle_ops_t* bus_interface) {
    if (!cpu || !bus_interface) return;
    
    // Preserve the original bus interface for non-I/O port accesses
    cpu->original_bus_interface = *bus_interface;
    
    // Install MOS6510 bus callbacks that handle I/O ports
    bus_cycle_ops_t mos6510_bus_interface = {
        .context = cpu,  // MOS6510 CPU as context
        .bus_read_cycle = mos6510_bus_read_callback,
        .bus_write_cycle = mos6510_bus_write_callback
    };
    
    // Set the MOS6510 wrapper as the active bus interface
    cpu->base.bus_interface = mos6510_bus_interface;
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
 * This is the new implementation using generic I/O ports with banking detection.
 * Uses actual bus data for proper floating bus behavior.
 */
bus_state_t mos6510_handle_io_read(mos6510_t* cpu, bus_state_t bus_state) {
    if (!cpu || bus_state.addr > 1) {
        return bus_state; // Return unchanged for invalid addresses
    }
    
    // Use single I/O port for the read operation
    // Address 0 = DDR, Address 1 = Port Data
    // Pass actual bus data for proper floating bus behavior (bits 6-7)
    uint32_t bus_data = (uint32_t)bus_state.data;
    bus_state.data = (uint8_t)ioport_read(&cpu->io_port, (uint8_t)bus_state.addr, bus_data);
    return bus_state;
}

/**
 * Handle I/O port write operations for MOS6510 (addresses 0-1).
 * This is the new implementation using generic I/O ports with banking detection.
 * Uses actual bus data for proper floating bus behavior.
 */
bus_state_t mos6510_handle_io_write(mos6510_t* cpu, bus_state_t bus_state) {
    if (!cpu || bus_state.addr > 1) {
        return bus_state; // Return unchanged for invalid addresses
    }
    
    // Use single I/O port for the write operation
    // This will trigger banking change detection through the callback system
    // Address 0 = DDR, Address 1 = Port Data
    // Pass actual bus data for proper floating bus behavior
    uint32_t bus_data = (uint32_t)bus_state.data;
    ioport_write(&cpu->io_port, (uint8_t)bus_state.addr, bus_state.data, bus_data);
    return bus_state;
}

/**
 * Set the banking change callback for memory map updates.
 */
void mos6510_set_banking_callback(mos6510_t* cpu, void* context,
                                 mos6510_banking_change_callback_t callback) {
    if (!cpu) return;
    
    cpu->banking_context = context;
    cpu->banking_callback = callback;
}

/**
 * Get the current banking state from the MOS6510 I/O port.
 */
mos6510_banking_state_t mos6510_get_banking_state(const mos6510_t* cpu) {
    if (!cpu) {
        mos6510_banking_state_t empty = {0};
        return empty;
    }
    
    return cpu->banking_state;
}

// ============================================================================
// MOS6510 TICK FUNCTION - Cycle-accurate execution with I/O port handling
// ============================================================================

/**
 * MOS6510 tick function for cycle-accurate emulation.
 * This function handles I/O port access early in the tick before other processing
 * to prevent c64_memory_tick from overwriting I/O port data with RAM data.
 *
 * According to the refactoring plan, the MOS6510 tick should check for I/O port
 * addresses (0-1) and handle them directly, not the c64_memory_tick function.
 *
 * @param cpu Pointer to the MOS6510 CPU structure
 * @param bus_state Pointer to the shared bus state
 */
void mos6510_tick(mos6510_t* cpu, bus_state_t* bus_state) {
    if (!cpu || !bus_state) return;
    
    // CRITICAL: Handle I/O port addresses (0-1) EARLY in the tick
    // This must be done before any other processing to prevent memory system
    // from overwriting I/O port read data with RAM data
    uint16_t address = bus_state->addr;
    if (unlikely(address <= 1)) {
        // Determine if this is a read or write operation
        bool is_read = bus_state->lines & BUS_MASK_RW;
        
        if (is_read) {
            // I/O port read - handle directly and update bus data
            *bus_state = mos6510_handle_io_read(cpu, *bus_state);
        } else {
            // I/O port write - handle directly using bus data
            *bus_state = mos6510_handle_io_write(cpu, *bus_state);
        }
        
        // I/O port access complete - no further processing needed
        return;
    }
    
    // For addresses > 1, perform normal CPU tick processing
    // This could include instruction execution, interrupt handling, etc.
    // For now, this is a placeholder for future cycle-accurate CPU implementation
    
    // TODO: Add cycle-accurate CPU execution logic here
    // This would include:
    // - Instruction fetch and decode
    // - Address setup operations
    // - Data operations
    // - Interrupt handling
    // - State machine progression
}

// ============================================================================
// MEMORY ACCESS INTEGRATION WITH I/O PORT TICK
// ============================================================================

/**
 * Read cycle with integrated I/O port tick handling.
 * This function replaces direct calls to fam65xx_read_cycle when I/O port
 * handling is needed during memory access.
 */
uint8_t mos6510_read_cycle(mos6510_t* cpu, uint16_t address) {
    if (!cpu) return 0xFF;
    
    // Set up bus state for the read operation
    bus_state_t bus_state = {
        .addr = address,
        .data = 0,
        .lines = BUS_MASK_RW  // Read operation (RW line high)
    };
    
    // Use MOS6510 tick to handle potential I/O port access
    mos6510_tick(cpu, &bus_state);
    
    // If it was an I/O port access (address 0-1), tick handled it and we're done
    if (unlikely(address <= 1)) {
        return bus_state.data;
    }
    
    // For non-I/O addresses, perform normal memory read through family interface
    return fam65xx_read_cycle(&cpu->base, address);
}

/**
 * Write cycle with integrated I/O port tick handling.
 * This function replaces direct calls to fam65xx_write_cycle when I/O port
 * handling is needed during memory access.
 */
void mos6510_write_cycle(mos6510_t* cpu, uint16_t address, uint8_t data) {
    if (!cpu) return;
    
    // Set up bus state for the write operation
    bus_state_t bus_state = {
        .addr = address,
        .data = data,
        .lines = 0  // Write operation (RW line low)
    };
    
    // Use MOS6510 tick to handle potential I/O port access
    mos6510_tick(cpu, &bus_state);
    
    // If it was an I/O port access (address 0-1), tick handled it and we're done
    if (unlikely(address <= 1)) {
        return;
    }
    
    // For non-I/O addresses, perform normal memory write through family interface
    fam65xx_write_cycle(&cpu->base, address, data);
}

// ============================================================================
// MOS6510 BUS INTERFACE CALLBACKS - Integrate I/O port handling with family core
// ============================================================================

/**
 * MOS6510 bus read callback with intelligent I/O port and memory integration.
 * For addresses 0-1: read from memory first, then override with I/O port data if needed.
 * For other addresses: pass through to underlying memory system.
 */
static uint8_t mos6510_bus_read_callback(void* context, uint16_t address) {
    mos6510_t* cpu = (mos6510_t*)context;
    if (!cpu) return 0xFF;
    
    // Always read from the underlying memory system first
    uint8_t memory_data = cpu->original_bus_interface.bus_read_cycle(cpu->original_bus_interface.context, address);
    
    // For I/O port addresses, also get I/O port data and use it as the final result
    if (unlikely(address <= 1)) {
        bus_state_t bus_state = {
            .addr = address,
            .data = memory_data,  // Start with memory data
            .lines = BUS_MASK_RW  // Read operation
        };
        bus_state = mos6510_handle_io_read(cpu, bus_state);
        return bus_state.data;  // Return I/O port result
    }
    
    // For non-I/O addresses, return memory data as-is
    return memory_data;
}

/**
 * MOS6510 bus write callback with pass-through to underlying memory.
 * This callback allows both the memory system and I/O port system to handle writes.
 * For addresses 0-1, both the underlying RAM and I/O ports receive the write.
 */
static void mos6510_bus_write_callback(void* context, uint16_t address, uint8_t data) {
    mos6510_t* cpu = (mos6510_t*)context;
    if (!cpu) return;
    
    // Always write to the underlying memory system
    // This allows addresses 0-1 to be written to RAM as well as I/O ports
    cpu->original_bus_interface.bus_write_cycle(cpu->original_bus_interface.context, address, data);
    
    // For I/O port addresses, also handle through the I/O port system
    // This ensures banking changes and other I/O port functionality works
    if (unlikely(address <= 1)) {
        bus_state_t bus_state = {
            .addr = address,
            .data = data,
            .lines = 0  // Write operation
        };
        mos6510_handle_io_write(cpu, bus_state);
    }
}
