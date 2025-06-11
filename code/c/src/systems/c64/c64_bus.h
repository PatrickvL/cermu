#ifndef C64_BUS_H
#define C64_BUS_H

#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "../../core/system_lines.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/control_lines_interface.h"
#include "../../chip/cpu/mos6510/mos6510_io_interface.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declaration will be replaced by proper include when needed

// Bus control line definitions
#define IRQ_LINE    (1 << 0)
#define NMI_LINE    (1 << 1)
#define BA_LINE     (1 << 2)
#define AEC_LINE    (1 << 3)
#define RDY_LINE    (1 << 4)

// Macro definitions for ACID (ACcessor InDex) extraction

// ACID definitions (ACcessor InDex for callback dispatch)
// ACIDs identify which accessor callbacks to use for memory operations.
// Accessor Callback IDs for optimized callback system
enum {
    /* I/O pages 0-15 (direct page mapping for $D000-$DFFF, read/write) */
    ACID_VIC_D0 = 0,      /* $D000-$D0FF */
    ACID_VIC_D1 = 1,      /* $D100-$D1FF */
    ACID_VIC_D2 = 2,      /* $D200-$D2FF */
    ACID_VIC_D3 = 3,      /* $D300-$D3FF */
    ACID_SID_D4 = 4,      /* $D400-$D4FF */
    ACID_SID_D5 = 5,      /* $D500-$D5FF */
    ACID_SID_D6 = 6,      /* $D600-$D6FF */
    ACID_SID_D7 = 7,      /* $D700-$D7FF */
    ACID_COLORRAM_D8 = 8, /* $D800-$D8FF */
    ACID_COLORRAM_D9 = 9, /* $D900-$D9FF */
    ACID_COLORRAM_DA = 10,/* $DA00-$DAFF */
    ACID_COLORRAM_DB = 11,/* $DB00-$DBFF */
    ACID_CIA1_DC = 12,    /* $DC00-$DCFF */
    ACID_CIA2_DD = 13,    /* $DD00-$DDFF */
    ACID_IO1_DE = 14,     /* $DE00-$DEFF */
    ACID_IO2_DF = 15,     /* $DF00-$DFFF */
    /* Non-I/O chips 16-23 */
    ACID_RAM = 16,        // Main RAM (64KB)
    ACID_ZEROBANK = 17,   // Zero bank (4KB) overlaps RAM, required for CPU I/O ports at $0000-$0001
    // Read-only ACIDs (8+): These chips only support read operations
    ACID_BASIC = 18,      // BASIC ROM $A000-$BFFF (read-only)
    ACID_KERNAL = 19,     // KERNAL ROM $E000-$FFFF (read-only)
    ACID_CHARROM = 20,
    ACID_ROML = 21,       // Cartridge ROM $8000-$9FFF (read-only)
    ACID_ROMH = 22,
    ACID_UNMAPPED = 23,   // Unmapped/detached operations (no real chip)
};

typedef struct c64_bus_s {
    chip_descriptor_t* desc;
    void* c64;  // c64_t* - opaque pointer to avoid circular dependency
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
    uint8_t  data;          // D0-D7
    uint16_t address;       // A0-A15
      // System lines for control signals (includes EXROM and GAME)
    uint32_t system_lines;  // System-wide control lines including cartridge signals
    
    // Current PLA banking mode (0-31) derived from CPU port + cartridge signals
    uint8_t pla_banking_mode;  // Current banking mode for fast switching
    
    // OPTIMIZED MEMORY BANKING - Cache-friendly layout
    // 16 bytes: encoded_rwid_per_bank mapping (4KB banks 0-15) - fits in single cache line
    alignas(16) uint8_t encoded_rwid_per_bank[16];
    
    // Split read/write for better cache usage (reads are 3-4x more frequent)
    alignas(64) struct {
        void *context;
        chip_read_func_t read;
    } read_callbacks[24];        // 384 bytes - hot cache for reads
    
    alignas(64) chip_write_func_t write_funcs[24]; // 192 bytes - separate cache line for writes
    
    // Banking configurations per mode (32 modes x 16 banks = 512 bytes)
    alignas(64) uint8_t encoded_rwid_per_bank_per_mode[32][16]; // Banking configurations per mode
    
    // Integrated adapter interfaces - can be passed out as pointers
    bus_cycle_ops_t bus_adapter;
    control_lines_interface_t control_lines_adapter;
    mos6510_io_port_interface_t io_port_adapter;
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// PLA mode generation function - maps CPU I/O port bits + cartridge signals to 5-bit PLA mode
uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits);

// Bus cycle functions
uint8_t c64_bus_read_cycle(c64_bus_t *bus, uint16_t addr);
void c64_bus_write_cycle(c64_bus_t* bus, uint16_t addr, uint8_t value);

// Memory functions
uint8_t c64_bus_memory_read(c64_bus_t *bus, uint16_t address);
void c64_bus_memory_write(c64_bus_t *bus, uint16_t address, uint8_t value);

// System functions  
void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64);  // c64_t*

// Cartridge interface functions for controlling EXROM and GAME signals
void c64_bus_set_exrom_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_game_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_cartridge_signals(c64_bus_t* c64_bus, bool exrom_active, bool game_active);
bool c64_bus_get_exrom_signal(c64_bus_t* c64_bus);
bool c64_bus_get_game_signal(c64_bus_t* c64_bus);

// Optimized callback management
void c64_bus_register_chip_callbacks(c64_bus_t* bus, uint8_t chip_id,
                                    chip_read_func_t read_func, chip_write_func_t write_func,
                                    void* context);

// Forward declaration for PLA
struct pla_906114_01_s;

// PLA-based bus mapping functions
void c64_bus_populate_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla);

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

#endif // C64_BUS_H