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

// =============================
// Bus Types & Macros
// =============================

// Chip IDs ordered by memory size (largest first), then I/O by page number
typedef enum {
    // Must be consecutive for optimal jump table
    CHIP_ZEROBANK     = 0,   // Pseudo chip for CPU I/O ports (4KB bank $0000-$0FFF)
    CHIP_RAM          = 1,   // 64KB RAM (largest)
    CHIP_BASIC        = 2,   // 8KB BASIC ROM
    CHIP_KERNAL       = 3,   // 8KB KERNAL ROM
    CHIP_ROML         = 4,   // 8KB ROM Low (cartridge)
    CHIP_ROMH         = 5,   // 8KB ROM High (cartridge)
    CHIP_CHARROM      = 6,   // 4KB Character ROM
    CHIP_COLORRAM     = 7,   // 1KB Color RAM (smallest memory)
    CHIP_UNMAPPED     = 8,   // Unmapped regions
    CHIP_IO           = 9,   // I/O bank
    // I/O pages in $D000-$DFFF range (16 pages of $100 bytes each)
    CHIP_D0_VIC = CHIP_IO,   // $D000-$D0FF (I/O page 0) - VIC-II registers
    CHIP_D1_VIC       =10,   // $D100-$D1FF (I/O page 1) - VIC-II mirrors
    CHIP_D2_VIC      = 11,   // $D200-$D2FF (I/O page 2) - VIC-II mirrors
    CHIP_D3_VIC      = 12,   // $D300-$D3FF (I/O page 3) - VIC-II mirrors
    CHIP_D4_SID      = 13,   // $D400-$D4FF (I/O page 4) - SID registers
    CHIP_D5_SID      = 14,   // $D500-$D5FF (I/O page 5) - SID mirrors
    CHIP_D6_SID      = 15,   // $D600-$D6FF (I/O page 6) - SID mirrors
    CHIP_D7_SID      = 16,   // $D700-$D7FF (I/O page 7) - SID mirrors
    CHIP_D8_COLORRAM = 17,   // $D800-$D8FF (I/O page 8) - Color RAM via VIC
    CHIP_D9_UNMAPPED = 18,   // $D900-$D9FF (I/O page 9) - Unmapped
    CHIP_DA_UNMAPPED = 19,   // $DA00-$DAFF (I/O page 10) - Unmapped
    CHIP_DB_UNMAPPED = 20,   // $DB00-$DBFF (I/O page 11) - Unmapped
    CHIP_DC_CIA1     = 21,   // $DC00-$DCFF (I/O page 12) - CIA1
    CHIP_DD_CIA2     = 22,   // $DD00-$DDFF (I/O page 13) - CIA2
    CHIP_DE_IO1      = 23,   // $DE00-$DEFF (I/O page 14) - Cartridge I/O 1
    CHIP_DF_IO2      = 24,   // $DF00-$DFFF (I/O page 15) - Cartridge I/O 2

    CHIP_MAX = 25 // Total number of CHIP IDs (0-24)
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
void c64_bus_cpu_read(c64_bus_t *bus, uint16_t address);
void c64_bus_cpu_write(c64_bus_t *bus, uint16_t address, uint8_t value);

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

static inline uint8_t c64_bus_adapter_detached_read(void* context) {
    c64_bus_t* bus = (c64_bus_t*)context;
    return bus->state.data; // Return "floating" bus data for detached reads
    // TODO : These should also decay and float to 0xFF after a while
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
// range by c64_bus_cpu_read/write) and decreasing higher CHIP by 15,
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