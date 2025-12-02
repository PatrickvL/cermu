#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "../../core/system_lines.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/control_lines_interface.h"
#include "c64_config.h"
#include "c64_chips.h"

// =============================
// Bus Types & Macros
// =============================

// System line masks for cartridge signals (moved out of control lines to separate field)
#define SYS_MASK_EXROM 0   // EXROM signal
#define SYS_MASK_GAME  1  // GAME signal

// C64 bus controller structure
typedef struct c64_bus_s {
    chip_descriptor_t* desc;
    void* c64;  // c64_t* - opaque pointer to avoid circular dependency
    bus_state_t state; // Unified bus state (data, address, control lines)
    // System lines for control signals (includes EXROM and GAME)
    uint8_t system_lines;  // System-wide control lines including cartridge signals

    // Current PLA banking mode (0-31) derived from CPU port + cartridge signals
    uint8_t pla_banking_mode;  // Current banking mode for fast switching

    // UNIFIED MEMORY BUFFER FOR OPTIMIZED OPCODE FETCH - STRATEGIC LAYOUT
    // Layout optimized for branchless calculation: ROML + ROMH + KERNAL + BASIC + CHARROM + RAM
    // Offsets: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x8000, RAM=0x9000
    // Total: Up to 100KB unified buffer (36KB ROM space + 64KB RAM) for branchless memory access
    // Strategic CHIP numbering enables pure arithmetic: offset = chip << 12 (chip * 4096)
    // Dynamic allocation skips unused cartridge ROMs at buffer start to save memory
    uint8_t* unified_memory_buffer;   // Points to usable memory (may be offset from allocated memory)
    uint8_t* allocated_buffer;        // Points to actual allocated memory
    size_t allocated_size;            // Actual allocated size
    bool roml_present;                // Whether ROML cartridge ROM is attached
    bool romh_present;                // Whether ROMH cartridge ROM is attached

    // OPTIMIZED MEMORY BANKING - Cache-friendly layout
    // Banking configurations per mode (32 modes x 16 banks = 512 bytes)
    alignas(64) uint8_t cpu_encoded_chip_per_bank_per_mode[32][16]; // Encoded chip select for all PLA modes
    // 16 bytes: cpu_encoded_chip_per_bank mapping (4KB banks 0-15) - fits in single cache line
    alignas(16) uint8_t cpu_encoded_chip_per_bank[16]; // CPU banking configurations per mode

    // VIC-II active array for optimized access (raw CHIPs, no encoding)
    alignas(16) uint8_t vicii_chip_per_bank[16];

    // VIC-II banking configurations per mode (32 modes x 16 banks = 512 bytes)
    // VIC-II uses direct CHIP values, not encoded, since it only does read accesses
    alignas(64) uint8_t vicii_chip_per_bank_per_mode[32][16]; // VIC-II direct CHIP per mode

    // Integrated adapter interfaces - can be passed out as pointers
    bus_cycle_ops_t bus_adapter;
    control_lines_interface_t control_lines_adapter;
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// PLA mode generation function - maps CPU I/O port bits + cartridge signals to 5-bit PLA mode
uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits);

// Memory functions
void c64_bus_vic_read(c64_bus_t* c64_bus, uint16_t address);

/**
 * New cycle-accurate memory tick function for the refactored architecture.
 * This function will be used by the new MOS6510 implementation to handle
 * memory access in a cycle-accurate manner.
 *
 * Optimized for register-based calling convention to avoid host stack accesses.
 * Takes bus state by value and returns updated bus state for efficient register usage.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param bus_state Current bus state (passed by value for register optimization)
 * @return Updated bus state (for register-to-register operation)
 */
bus_state_t REGISTER_CALL c64_memory_tick(c64_bus_t* c64_bus, bus_state_t bus_state);

/**
 * Initialize RAM/ROM pointers to point into the unified memory buffer.
 * This eliminates separate memory allocations and ensures consistency.
 * Uses configuration structure to determine cartridge ROM presence.
 * Should be called after system is attached.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param c64_system Pointer to the C64 system (for pointer updates)
 * @param config Pointer to the C64 system configuration structure
 */
void c64_bus_init_unified_pointers(c64_bus_t* c64_bus, void* c64_system, const c64_config_t* config);

// Bus cycle functions
uint8_t c64_bus_read_cycle(c64_bus_t *bus, uint16_t addr);
void c64_bus_write_cycle(c64_bus_t* bus, uint16_t addr, uint8_t value);

// System functions
void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64);  // c64_t*

// Cartridge interface functions for controlling EXROM and GAME signals
void c64_bus_set_exrom_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_game_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_cartridge_signals(c64_bus_t* c64_bus, bool exrom_active, bool game_active);
bool c64_bus_get_exrom_signal(c64_bus_t* c64_bus);
bool c64_bus_get_game_signal(c64_bus_t* c64_bus);

// Banking change callback function for MOS6510
void c64_bus_on_banking_change(void* bus_ptr, uint8_t banking_state);

// Forward declaration for PLA
struct pla_906114_01_s;

// PLA-based bus mapping functions
void c64_bus_populate_cpu_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla);
void c64_bus_populate_vicii_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla);

// Generate all 32 memory modes using PLA
void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla);

extern chip_descriptor_t c64_bus_descriptor;

// ============================================================================
// ADAPTER INTERFACES - Integrated adapter access
// ============================================================================

/**
 * Initialize the integrated adapter interfaces in the C64 bus.
 * This sets up the adapter interfaces so they can be passed out as pointers.
 * Should be called during bus initialization.
 *
 * @param c64_bus Pointer to the C64 bus implementation
 */
void c64_bus_init_adapters(c64_bus_t* c64_bus);

/**
 * Get a pointer to the bus cycle adapter interface.
 * This allows the C64 bus to work with the refactored MOS6510 CPU.
 *
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Pointer to the bus interface structure configured for the C64 bus
 */
static inline bus_cycle_ops_t* c64_bus_get_adapter(c64_bus_t* c64_bus) {
    return &c64_bus->bus_adapter;
}

/**
 * Get a pointer to the control lines adapter interface.
 * This allows any chip to access the shared control lines.
 *
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Pointer to the control lines interface structure configured for the C64 bus
 */
static inline control_lines_interface_t* c64_control_lines_get_adapter(c64_bus_t* c64_bus) {
    return &c64_bus->control_lines_adapter;
}

