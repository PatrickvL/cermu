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
// The allocation order prioritizes write-capable devices for 3-bit encoding.
// ACID 0 is reserved for unmapped/detached operations.
#define ACID_UNMAPPED    0   // Reserved: Unmapped/detached operations (no real device)

// Write-capable ACIDs (1-7): These devices support write operations and get low IDs
// to fit in the 3-bit write field of the encoding
#define ACID_ZEROPAGE    1   // CPU I/O ports at $0000/$0001 (read/write)
#define ACID_RAM         2   // Main RAM (read/write)
#define ACID_VIC         3   // VIC-II $D000-$D3FF (read/write)
#define ACID_SID         4   // SID $D400-$D7FF (read/write)
#define ACID_COLORRAM    5   // Color RAM $D800-$DBFF (read/write)
#define ACID_CIA         6   // CIA1/CIA2 $DC00-$DDFF (read/write)
// ACID 7 available for future write-capable device

// Read-only ACIDs (8+): These devices only support read operations
#define ACID_BASIC_ROM   8   // BASIC ROM $A000-$BFFF (read-only)
#define ACID_KERNAL_ROM  9   // KERNAL ROM $E000-$FFFF (read-only)
#define ACID_CARTRIDGE   10  // Cartridge ROM $8000-$9FFF (read-only)
// Add more read-only ACIDs as needed

// Encoding macros for packing read/write ACIDs into single byte
// Format: [7:5] write ACID (3 bits), [4] spare, [3:0] read ACID (4 bits)
#define ACIDS_RW_ENCODE(read_acid, write_acid) \
    (((read_acid) & 0x0F) | (((write_acid) & 0x07) << 5))
#define ACID_READ_DECODE(entry) ((entry) & 0x0F)
#define ACID_WRITE_DECODE(entry) ((entry) >> 5)
#define ACID_DECODE_SPARE(entry) (((entry) >> 4) & 0x01)

typedef struct c64_bus_s {
    chip_descriptor_t* desc;
    void* c64;  // c64_t* - opaque pointer to avoid circular dependency
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
    uint8_t  data;          // D0-D7
    uint16_t address;       // A0-A15
    
    // System lines for control signals (includes EXROM and GAME)
    uint32_t system_lines;  // System-wide control lines including cartridge signals
    
    // ACID allocation tracking
    uint8_t next_write_acid;    // Next available write-capable ACID (1-7)
    uint8_t next_read_acid;     // Next available read-only ACID (8+)
    uint8_t chip_id_to_acid[16]; // Maps chip ID to allocated ACID
    
    alignas(64) access_callback_t access_callback_per_acid[16]; // indexed by ACID - unified read/write/context
    alignas(64) uint8_t acid_per_bankidx[32]; // Maps each condensed index (32 entries) to an ACID
    alignas(64) uint8_t acid_per_bankidx_per_mode[32][32]; // Condensed from 256 to 32 entries per mode
    
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

// ACID allocation and management
uint8_t c64_bus_allocate_acid(c64_bus_t* bus, uint8_t chip_id, 
                              chip_read_func_t read_func, chip_write_func_t write_func, 
                              void* context);
void c64_bus_initialize_acids(c64_bus_t* bus);

// Forward declaration for PLA
struct pla_906114_01_s;

// PLA-based bus mapping functions
void c64_bus_populate_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla, 
                                 uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id, 
                                 uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id, 
                                 uint8_t cartridge_romh_id, uint8_t colorram_id);

// Generate all 32 memory modes using PLA
void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla,
                                   uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id,
                                   uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id,
                                   uint8_t cartridge_romh_id, uint8_t colorram_id);

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