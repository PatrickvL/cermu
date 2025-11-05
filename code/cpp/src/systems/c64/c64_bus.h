#ifndef C64_BUS_H
#define C64_BUS_H

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

// =============================
// Bus Types & Macros
// =============================

// Chip IDs strategically numbered for branchless unified memory buffer address calculation
typedef enum {
    // Strategic numbering with 4KB step size: offset = chip << 12 (chip * 4096)
    // 8KB regions take 2 steps each: ROML(0), ROMH(2), KERNAL(4), BASIC(6), CHARROM(8), RAM(9)
    // Buffer: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x8000) + RAM(0x9000)
    CHIP_ROML         = 0,   // 8KB ROM Low (cartridge) - maps to offset 0x0000 (0 << 12 = 0x0000)
    CHIP_ROMH         = 2,   // 8KB ROM High (cartridge) - maps to offset 0x2000 (2 << 12 = 0x2000)
    CHIP_KERNAL       = 4,   // 8KB KERNAL ROM - maps to offset 0x4000 (4 << 12 = 0x4000)
    CHIP_BASIC        = 6,   // 8KB BASIC ROM - maps to offset 0x6000 (6 << 12 = 0x6000)
    CHIP_CHARROM      = 8,   // 4KB Character ROM - maps to offset 0x8000 (8 << 12 = 0x8000)
    CHIP_RAM          = 9,   // 64KB RAM - maps to offset 0x9000 (9 << 12 = 0x9000)
    
    // Non-offset values (not used in address calculation)
    CHIP_UNMAPPED     = 10,  // Unmapped regions (not in unified buffer)
    CHIP_IO           = 11,  // I/O region ($D000-$DFFF) - special case, not in unified buffer
} chip_id_t;

// Array of valid CHIP IDs for iteration (due to irregular numbering)
static const uint8_t VALID_CHIP_IDS[] = {
    CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC,
    CHIP_CHARROM, CHIP_RAM, CHIP_UNMAPPED, CHIP_IO
};
static const size_t VALID_CHIP_COUNT = sizeof(VALID_CHIP_IDS) / sizeof(VALID_CHIP_IDS[0]);

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


// CHIP descriptor struct for tooling
typedef struct {
    uint16_t base;
    size_t size;
//    uint16_t end;
//    const char* size_str;
    const char* label; // always from chip descriptor if available
//    const char* title;
} chip_description_t;


/**
 * Fetch descriptor for a given CHIP from registered chips or synthesize for I/O/special
 * Returns true if found, false if not (out is only valid if true)
 */
bool c64_bus_get_chip_description(const c64_bus_t* bus, uint8_t chip, chip_description_t* out);

// Tooling: Map CHIP to a concise type/title string (not address/size)
const char* c64_bus_chip_to_title(uint8_t chip);

// Utility: Convert a size in bytes to a human-readable string ("256B", "4KB", etc.)
const char* c64_bus_size_to_str(size_t size);

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


// Encoding macros for packing read/write CHIPs into single byte by packing
// the IO pages into one (CHIP_VIC_D0, which will be restored to the full
// range by c64_memory_tick) and decreasing higher CHIP by 15,
// Order: I/O pages (0-15), then writable chips (16-19), then read-only (20-24)
// Non-I/O CHIPs 16-24 become 1-9 in encoded form, which fits in 4 bits.
// Writable CHIPs 16-19 become 1-4 in encoded form, which fits in 3 bits.
// Output byte format: [7:5] write code (3 bits), [4] unused (1 bit), [3:0] read code (4 bits)
static inline uint8_t encode_chip_rw(uint8_t read_chip, uint8_t write_chip) {
    uint8_t encoded = read_chip | (write_chip << 4);
    return encoded;
}

static inline uint8_t decode_read_chip(uint8_t encoded) {
    return encoded & 0x0F; // Lower 4 bits are the read chip
}
    
static inline uint8_t decode_write_chip(uint8_t encoded) {
    return (encoded >> 4) & 0x0F; // Upper 4 bits are the write chip
}

#endif // C64_BUS_H