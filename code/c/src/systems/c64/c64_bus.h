#ifndef C64_BUS_H
#define C64_BUS_H

#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
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

// Macro definitions for device ID extraction

// Device ID definitions (unified index space, ordered by base address)
// The first 7 IDs are for devices supporting both read and write callbacks.
// The rest (CARTRIDGE, BASIC, KERNAL) are read-only.
#define DEVID_UNMAPPED    0   // Unmapped (PLA hole)
#define DEVID_ZEROPAGE    1   // Zero page (special handling for MOS6410 CPU's I/O ports at $0000/$0001)
#define DEVID_RAM         2   // RAM (main memory)
#define DEVID_VIC         3   // VIC-II ($D000)
#define DEVID_SID         4   // SID ($D400)
#define DEVID_COLORRAM    5   // Color RAM ($D800)
#define DEVID_CIA         6   // CIA1 ($DC00) and CIA2 ($DD00), same device type, different context
// Devices below only support read (ROM/Cartridge)
#define DEVID_CARTRIDGE   7   // Cartridge ROM ($8000)
#define DEVID_BASIC_ROM   8   // BASIC ROM ($A000)
#define DEVID_KERNAL_ROM  9   // KERNAL ROM ($E000)
// Add more as needed, keeping IDs unique and ordered by base address

// Encoding macros ([7:5] write ID, [4]:spare bit, [3:0] read ID)
#define DEVIDS_RW_ENCODE(read_id, write_id) \
    (((read_id) & 0x0F) | (((write_id) & 0x07) << 5)) // TODO : Encode spare bit once needed
#define DEVID_READ_DECODE(entry) ((entry) & 0x0F)
#define DEVID_WRITE_DECODE(entry) ((entry) >> 5)
#define DECODE_SPARE(entry) (((entry) >> 4) & 0x01)

typedef struct c64_bus_s {
    chip_descriptor_t* desc;
    void* c64;  // c64_t* - opaque pointer to avoid circular dependency
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
    uint8_t  data;          // D0-D7
    uint16_t address;       // A0-A15
    alignas(64) access_callback_t access_callback_per_devid[16]; // indexed by device ID - unified read/write/context
    alignas(64) uint8_t devid_per_bankidx[32]; // Maps each condensed index (32 entries) to a device ID
    alignas(64) uint8_t devid_per_bankidx_per_mode[32][32]; // Condensed from 256 to 32 entries per mode
    
    // Integrated adapter interfaces - can be passed out as pointers
    bus_cycle_ops_t bus_adapter;
    control_lines_interface_t control_lines_adapter;
    mos6510_io_port_interface_t io_port_adapter;
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// Bus cycle functions
uint8_t c64_bus_read_cycle(c64_bus_t *bus, uint16_t addr);
void c64_bus_write_cycle(c64_bus_t* bus, uint16_t addr, uint8_t value);

// Memory functions
uint8_t c64_bus_memory_read(c64_bus_t *bus, uint16_t address);
void c64_bus_memory_write(c64_bus_t *bus, uint16_t address, uint8_t value);

// System functions  
void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64);  // c64_t*

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