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
#include "../../chip/cpu/mos6510/mos6510.h"
#include "c64_config.h"

// =============================
// Bus Types & Macros
// =============================

// Chip IDs optimized for unified memory buffer layout
typedef enum {
    // Memory chips (no side effects) - consecutive for unified buffer
    CHIP_ROML         = 0,   // 8KB ROM Low (cartridge) - maps to unified offset 0x0000
    CHIP_ROMH         = 1,   // 8KB ROM High (cartridge) - maps to unified offset 0x2000
    CHIP_KERNAL       = 2,   // 8KB KERNAL ROM - maps to unified offset 0x4000
    CHIP_BASIC        = 3,   // 8KB BASIC ROM - maps to unified offset 0x6000
    CHIP_CHARROM      = 4,   // 4KB Character ROM (+ 4KB padding) - maps to unified offset 0x8000
    CHIP_RAM          = 5,   // 64KB - maps to offset 0x9000 in unified buffer
    // End of unified memory buffer chips - all below ones require callbacks :
    CHIP_ZEROBANK     = 6,   // Pseudo chip for CPU I/O ports (4KB bank $0000-$0FFF)
    CHIP_UNMAPPED     = 7,   // Unmapped regions
    
    // I/O chips start here (all encoded as CHIP_IO = 8 in bank array)
    CHIP_IO           = 8,   // Base I/O (gets expanded)
    // I/O pages in $D000-$DFFF range (16 pages of $100 bytes each)
    CHIP_D0_VIC = CHIP_IO,   // $D000-$D0FF (I/O page 0) - VIC-II registers
    CHIP_D1_VIC      = 9,    // $D100-$D1FF (I/O page 1) - VIC-II mirrors
    CHIP_D2_VIC      = 10,   // $D200-$D2FF (I/O page 2) - VIC-II mirrors
    CHIP_D3_VIC      = 11,   // $D300-$D3FF (I/O page 3) - VIC-II mirrors
    CHIP_D4_SID      = 12,   // $D400-$D4FF (I/O page 4) - SID registers
    CHIP_D5_SID      = 13,   // $D500-$D5FF (I/O page 5) - SID mirrors
    CHIP_D6_SID      = 14,   // $D600-$D6FF (I/O page 6) - SID mirrors
    CHIP_D7_SID      = 15,   // $D700-$D7FF (I/O page 7) - SID mirrors
    CHIP_D8_COLORRAM = 16,   // $D800-$D8FF (I/O page 8) - Color RAM
    CHIP_D9_COLORRAM = 17,   // $D900-$D9FF (I/O page 9) - Color RAM mirror
    CHIP_DA_COLORRAM = 18,   // $DA00-$DAFF (I/O page 10) - Color RAM mirror
    CHIP_DB_COLORRAM = 19,   // $DB00-$DBFF (I/O page 11) - Color RAM mirror
    CHIP_DC_CIA1     = 20,   // $DC00-$DCFF (I/O page 12) - CIA1
    CHIP_DD_CIA2     = 21,   // $DD00-$DDFF (I/O page 13) - CIA2
    CHIP_DE_IO1      = 22,   // $DE00-$DEFF (I/O page 14) - Cartridge I/O 1
    CHIP_DF_IO2      = 23,   // $DF00-$DFFF (I/O page 15) - Cartridge I/O 2

    CHIP_MAX = 24 // Total number of CHIP IDs (0-23)
} chip_id_t;

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
    
    // UNIFIED MEMORY BUFFER FOR OPTIMIZED OPCODE FETCH
    // Layout: ROML(8KB) + ROMH(8KB) + KERNAL(8KB) + BASIC(8KB) + CHARROM(4KB) + RAM(64KB)
    // Total: Up to 100KB unified buffer for branchless memory access (Color RAM handled via I/O callbacks)
    // Dynamic allocation with pointer arithmetic for unused cartridge ROMs
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
    
    // CHIP CALLBACK ARRAYS - Function pointers for chip-specific operations
    alignas(64) chip_callback_t chip_read_callbacks[CHIP_MAX];   // Unified chip_callback_t array
    alignas(64) chip_callback_t chip_write_callbacks[CHIP_MAX]; // Unified chip_callback_t array
    
    // Integrated adapter interfaces - can be passed out as pointers
    bus_cycle_ops_t bus_adapter;
    control_lines_interface_t control_lines_adapter;
    mos6510_io_port_interface_t io_port_adapter;
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
 * Initialize chip callback arrays for optimized memory access.
 * Should be called during bus initialization after system is attached.
 * 
 * @param c64_bus Pointer to the C64 bus controller
 */
void c64_bus_init_chip_callbacks(c64_bus_t* c64_bus);

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
 * Now includes dynamic allocation with cartridge ROM detection.
 * Should be called after system is attached.
 * 
 * @param c64_bus Pointer to the C64 bus controller
 * @param c64 Pointer to the C64 system (for pointer updates)
 * @param roml_present Whether ROML cartridge ROM is attached (optional optimization)
 * @param romh_present Whether ROMH cartridge ROM is attached (optional optimization)
 */
void c64_bus_init_unified_pointers(c64_bus_t* c64_bus, void* c64, bool roml_present, bool romh_present);

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

/**
 * Get a pointer to the I/O port adapter interface.
 * This handles the CPU's I/O ports at addresses $0000 and $0001.
 * 
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Pointer to the I/O port interface structure configured for the C64 system
 */
static inline mos6510_io_port_interface_t* c64_io_port_get_adapter(c64_bus_t* c64_bus) {
    return &c64_bus->io_port_adapter;
}

// Encoding macros for packing read/write CHIPs into single byte by packing
// the IO pages into one (CHIP_VIC_D0, which will be restored to the full
// range by c64_memory_tick) and decreasing higher CHIP by 15,
// Order: I/O pages (0-15), then writable chips (16-19), then read-only (20-24)
// Non-I/O CHIPs 16-24 become 1-9 in encoded form, which fits in 4 bits.
// Writable CHIPs 16-19 become 1-4 in encoded form, which fits in 3 bits.
// Output byte format: [7:5] write code (3 bits), [4] unused (1 bit), [3:0] read code (4 bits)
static inline uint8_t encode_chip_rw(uint8_t read_chip, uint8_t write_chip) {
    read_chip = (read_chip > CHIP_IO) ? CHIP_IO : read_chip;
    write_chip = (write_chip > CHIP_IO) ? CHIP_IO : write_chip;
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