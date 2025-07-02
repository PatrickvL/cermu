#include "c64_bus.h"
#include "c64.h"
#include <stdlib.h>
#include <string.h>

/*
 * OPTIMIZED MEMORY ACCESS - Based on fast banking system
 * - 2 ops, 2.5-3.5 cycles for reads (0.5-1 cycle faster)
 * - Split read/write structures for better cache locality
 * - Branchless I/O detection using bit manipulation
 */

// Encoding macros for packing read/write ACIDs into single byte by packing
// the IO pages into one (ACID_VIC_D0, which will be restored to the full
// range by c64_bus_memory_read/write) and decreasing higher ACID by 15,
// Order: I/O pages (0-15), then writable chips (16-19), then read-only (20-24)
// Non-I/O ACIDs 16-24 become 1-9 in encoded form, which fits in 4 bits.
// Writable ACIDs 16-19 become 1-4 in encoded form, which fits in 3 bits.
// Output byte format: [7:5] write code (3 bits), [4] unused (1 bit), [3:0] read code (4 bits)
inline static uint8_t encode_acid_rw(uint8_t read_acid, uint8_t write_acid) {
    read_acid = (read_acid <= ACID_IO2_DF) ? ACID_VIC_D0 : read_acid - ACID_IO2_DF;
    write_acid = (write_acid <= ACID_IO2_DF) ? ACID_VIC_D0 : write_acid - ACID_IO2_DF;
    uint8_t encoded = read_acid | (write_acid << 5);
    return encoded;
}

// Simple 4KB bank calculation for optimized system (0-15)
static inline int c64_bus_get_bank(uint16_t address) {
    return address >> 12;  // Extract 4KB bank (0-15)
}

/* Memory read - 2 ops, 2.5-3.5 cycles (optimized cache usage) */
uint8_t c64_bus_memory_read(c64_bus_t *bus, uint16_t address) {
    uint8_t bank = (uint8_t)c64_bus_get_bank(address);  // Extract 4KB bank (0-15)
    uint8_t encoded = bus->encoded_rwid_per_bank[bank];  // Get banking info for this bank
    uint8_t is_io = -(encoded == 0);  // Branchless I/O detection
    uint8_t acid = (((encoded & 0xF) + ACID_IO2_DF) & ~is_io) | (((address >> 8) & 0xF) & is_io);
    return bus->read_callbacks[acid].read(bus->read_callbacks[acid].context, address);
}

/* Memory write - 2 ops, 3-4 cycles */
void c64_bus_memory_write(c64_bus_t *bus, uint16_t address, uint8_t value) {
    uint8_t bank = (uint8_t)c64_bus_get_bank(address);  // Extract 4KB bank (0-15)
    uint8_t encoded = bus->encoded_rwid_per_bank[bank];  // Get banking info for this bank
    uint8_t is_io = -(encoded == 0);  // Branchless I/O detection
    uint8_t acid = (((encoded >> 5) + ACID_IO2_DF) & ~is_io) | (((address >> 8) & 0xF) & is_io);
    // Note : Write acid will be within writable range (0-19)
    bus->write_funcs[acid](bus->read_callbacks[acid].context, address, value);
    // TODO : Move below signalling of VIC-II bank change to somewhere else with less impact on performance
    if (address == 0xDD00) {
        c64_t* c64 = bus->c64;
        bank = 3 - (value & 0x3);
        if (c64->sid->desc->bank_change) {
            c64->sid->desc->bank_change(c64->sid, bank);
        }
    }
}

void c64_bus_system_destroy(void* chip) {
    free(chip);
}

void* c64_bus_system_create(chip_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    // Initialize bus state
    c64_bus->address = 0;
    c64_bus->data = 0;
    c64_bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
      // Initialize system lines with default cartridge signals (no cartridge)
    c64_bus->system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;  // Both high = no cartridge
    
    // Note: pla_banking_mode will be initialized by c64_bus_mode_switch()
    // after PLA mapping data is set up in c64_pla_maps_generate()
    // Note: calloc already zeroed bank_acid, read_callbacks, and write_funcs
    
    // Initialize the integrated adapter interfaces
    c64_bus_init_adapters(c64_bus);
    
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer
}

chip_descriptor_t c64_bus_descriptor = {
    .description = "C64 System Bus Controller",
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .read = NULL,
    .write = NULL,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    // Update the optimized banking for the current mode
    c64_bus->pla_banking_mode = mode & 0x1F;
    memcpy(c64_bus->encoded_rwid_per_bank, c64_bus->encoded_rwid_per_bank_per_mode[mode], 16);
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    c64_t* c64 = c64_bus->c64;

    // READ CYCLES CAN BE HALTED: Wait for both BA and AEC
    while (!(c64_bus->control_lines & BA_LINE) || !(c64_bus->control_lines & AEC_LINE)) {
        c64_non_cpu_cycle(c64, true, false);
    }

    c64_bus->address = addr; // Perhaps this is no longer needed
    uint8_t data = c64_bus_memory_read(c64_bus, addr);
    c64_bus->data = data; // Used for "floating" bus state

    // Tick system through complete cycle
    c64_non_cpu_cycle(c64, true, false);    
    return data;
}

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    c64_t* c64 = c64_bus->c64;

    // WRITE CYCLES CANNOT BE INTERRUPTED by BA, but need AEC
    while (!(c64_bus->control_lines & AEC_LINE)) {
        c64_non_cpu_cycle(c64, true, false);
    }    

    c64_bus->address = addr; // Perhaps this is no longer needed
    c64_bus->data = value; // Used for "floating" bus state for subsequent unattached reads
    c64_bus_memory_write(c64_bus, addr, value);

    // Advance system
    c64_non_cpu_cycle(c64, true, false);
}

// PLA integration functions
#include "../../chip/logic/pla.h"

void c64_bus_populate_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Map memory regions based on PLA outputs
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Initialize read/write ACIDs
        uint8_t read_acid = ACID_UNMAPPED;
        uint8_t write_acid = ACID_UNMAPPED;
        // Set address in PLA
        pla_906114_01_set_address_high((pla_906114_01_t*)pla, (uint8_t)bank);
        
        // Configure PLA for READ mode
        ((pla_906114_01_t*)pla)->inputs.r_w = true;  // Read mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        
        // Determine read ACID based on PLA outputs for read mode
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (bank == 0) {
                // Special handling for CPU I/O ports in zero bank (4KB bank $0000-$0FFF)
                read_acid = ACID_ZEROBANK;
            } else {
                // Main RAM (read/write)
                read_acid = ACID_RAM;
            }
        } else if (!pla->outputs.n_basic) {
            // BASIC ROM (read-only)
            read_acid = ACID_BASIC;
        } else if (!pla->outputs.n_kernal) {
            // KERNAL ROM (read-only)
            read_acid = ACID_KERNAL;
        } else if (!pla->outputs.n_charrom) {
            // Character ROM (read-only)
            read_acid = ACID_CHARROM;
        } else if (!pla->outputs.n_io) {
            // I/O region - I/O bank (read-write)
            read_acid = ACID_VIC_D0; // c64_bus_memory_read will handle mapping to full 16 I/O pages
        } else if (!pla->outputs.n_roml) {
            // Cartridge ROM Low (read-only)
            read_acid = ACID_ROML;
        } else if (!pla->outputs.n_romh) {
            // Cartridge ROM High (read-only)
            read_acid = ACID_ROMH;
        }
        
        // Configure PLA for WRITE mode
        ((pla_906114_01_t*)pla)->inputs.r_w = false;  // Write mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        
        // Determine write ACID based on PLA outputs for write mode
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (bank == 0) {
                // Special handling for CPU I/O ports in zero bank (4KB bank $0000-$0FFF)
                write_acid = ACID_ZEROBANK;
            } else {
                // Main RAM (read/write)
                write_acid = ACID_RAM;
            }
        } else if (!pla->outputs.n_io) {
            // I/O region - I/O bank (read-write)
            write_acid = ACID_VIC_D0; // c64_bus_memory_write will handle mapping to full 16 I/O pages
        }
        // Note: ROM areas (BASIC, KERNAL, Character ROM, Cartridge) are not writable, 
        // so write_acid remains ACID_UNMAPPED for those regions
        
        // Encode both read and write ACIDs into the mapping
        bus->encoded_rwid_per_bank[bank] = encode_acid_rw(read_acid, write_acid);
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    pla_906114_01_t* pla_impl = (pla_906114_01_t*)pla;
    
    // Generate all 32 memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        pla_impl->inputs.n_loram = (mode & 0x01) == 0;    // LORAM (inverted)
        pla_impl->inputs.n_hiram = (mode & 0x02) == 0;    // HIRAM (inverted)
        pla_impl->inputs.n_charen = (mode & 0x04) == 0;   // CHAREN (inverted)
        pla_impl->inputs.n_exrom = (mode & 0x08) == 0;    // EXROM (inverted)
        pla_impl->inputs.n_game = (mode & 0x10) == 0;     // GAME (inverted)
        
        // Set other inputs for normal CPU operation
        pla_impl->inputs.aec = true;     // CPU has bus control
        pla_impl->inputs.ba = true;      // Bus available
        pla_impl->inputs.r_w = true;     // Read mode
        pla_impl->inputs.n_cas = true;   // No CAS
        pla_impl->inputs.n_va14 = true;  // VA14 high
        pla_impl->inputs.va13 = false;   // VA13 low
        pla_impl->inputs.va12 = false;   // VA12 low
        pla_impl->inputs.n_ce = false;   // Chip enabled
        // Populate mapping for this mode
        c64_bus_populate_pla_mapping(bus, pla);
        // Copy the mapping to the mode-specific array
        memcpy(bus->encoded_rwid_per_bank_per_mode[mode], bus->encoded_rwid_per_bank, 16);
    }
}

// ============================================================================
// PLA MODE GENERATION - Convert CPU port bits + cartridge signals to 5-bit PLA mode
// ============================================================================

uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits) {
    // The PLA expects a 5-bit mode value with the following bit mapping:
    // Bit 0: LORAM (from CPU port bit 0)
    // Bit 1: HIRAM (from CPU port bit 1) 
    // Bit 2: CHAREN (from CPU port bit 2)
    // Bit 3: EXROM (from cartridge signal)
    // Bit 4: GAME (from cartridge signal)
    
    uint8_t pla_mode = 0;
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    pla_mode |= (cpu_port_bits & 0x01);      // LORAM (bit 0)
    pla_mode |= (cpu_port_bits & 0x02);      // HIRAM (bit 1)
    pla_mode |= (cpu_port_bits & 0x04);      // CHAREN (bit 2)
    
    // Add cartridge control signals from system lines
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_EXROM) ? 0x08 : 0); // EXROM (bit 3)
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_GAME) ? 0x10 : 0);  // GAME (bit 4)
    
    return pla_mode;
}

// ============================================================================
// ADAPTER INTERFACES - Integrated adapter initialization
// ============================================================================

// Adapter function implementations for C64 bus
static uint8_t c64_bus_adapter_bus_read_cycle(void* context, uint16_t address) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    return c64_bus_read_cycle(c64_bus, address);
}

static void c64_bus_adapter_bus_write_cycle(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_bus_write_cycle(c64_bus, address, value);
}

static void c64_bus_adapter_non_cpu_cycle(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_non_cpu_cycle(c64_bus->c64, true, false);
}

// Control lines adapter functions
static uint32_t c64_control_lines_get(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert the C64's uint8_t control_lines to the new uint32_t format
    // For now, just extend it to 32 bits
    return (uint32_t)c64_bus->control_lines;
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's uint8_t format
    // For now, just truncate (assumes lower 8 bits contain the relevant data)
    c64_bus->control_lines = (uint8_t)(lines & 0xFF);
}

// I/O port adapter functions
static void c64_io_port_output_changed(void* context, uint8_t port_value, uint8_t ddr) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Generate proper 5-bit PLA mode from CPU port bits and cartridge signals
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, port_value);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

static uint8_t c64_io_port_input_read(void* context, uint8_t port_value, uint8_t ddr) {
    // For C64, the I/O port typically reads the current port state
    // This can be extended to read actual external signals if needed
    (void)context;    // Unused for now
    (void)port_value; // Unused for now
    (void)ddr;        // Unused for now
    return 0xFF; // Default to all inputs high
}

void c64_bus_init_adapters(c64_bus_t* c64_bus) {
    // Initialize bus cycle adapter
    c64_bus->bus_adapter.context = c64_bus;
    c64_bus->bus_adapter.bus_read_cycle = c64_bus_adapter_bus_read_cycle;
    c64_bus->bus_adapter.bus_write_cycle = c64_bus_adapter_bus_write_cycle;
    c64_bus->bus_adapter.cycle_tick = c64_bus_adapter_non_cpu_cycle;
    c64_bus->bus_adapter.detached_read = c64_bus_adapter_detached_read;
    
    // Initialize control lines adapter
    c64_bus->control_lines_adapter.get_lines = c64_control_lines_get;
    c64_bus->control_lines_adapter.set_lines = c64_control_lines_set;
    c64_bus->control_lines_adapter.context = c64_bus;
    
    // Initialize I/O port adapter
    c64_bus->io_port_adapter.output_pins_changed = c64_io_port_output_changed;
    c64_bus->io_port_adapter.read_external_pins = c64_io_port_input_read;
    c64_bus->io_port_adapter.context = c64_bus;
}

// ============================================================================
// OPTIMIZED CALLBACK MANAGEMENT
// ============================================================================

// Register a chip's callbacks in the optimized arrays
void c64_bus_register_chip_callbacks(c64_bus_t* bus, uint8_t acid, void* context,
                                    chip_read_func_t read_func, chip_write_func_t write_func) {
    if (acid >= 24) return;  // Invalid chip ID for reads
    
    // Register read callback
    if (read_func) {
        bus->read_callbacks[acid].read = read_func;
        bus->read_callbacks[acid].context = context;
    }
    
    // Register write callback (only for writable chips 0-19)
    if (write_func && acid < 20) {
        bus->write_funcs[acid] = write_func;
    }
}

// ============================================================================
// CARTRIDGE INTERFACE FUNCTIONS - Control EXROM and GAME signals
// ============================================================================

/**
 * Set the EXROM signal state for cartridge control.
 * The EXROM signal controls cartridge ROM visibility and memory mapping.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param active true = EXROM active (signal low), false = EXROM inactive (signal high)
 */
void c64_bus_set_exrom_signal(c64_bus_t* c64_bus, bool active) {
    if (!c64_bus) return;
    
    if (active) {
        c64_bus->system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals
    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;  // Extract CPU port bits
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Set the GAME signal state for cartridge control.
 * The GAME signal controls cartridge ROM banking and Ultimax mode.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_set_game_signal(c64_bus_t* c64_bus, bool active) {
    if (!c64_bus) return;
    
    if (active) {
        c64_bus->system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals
    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;  // Extract CPU port bits
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Set both EXROM and GAME signals simultaneously for cartridge control.
 * This is more efficient than calling the individual functions separately.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param exrom_active true = EXROM active (signal low), false = EXROM inactive (signal high)
 * @param game_active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_set_cartridge_signals(c64_bus_t* c64_bus, bool exrom_active, bool game_active) {
    if (!c64_bus) return;
    
    // Update EXROM signal
    if (exrom_active) {
        c64_bus->system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
      // Update GAME signal
    if (game_active) {
        c64_bus->system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals (once for both signals)
    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;  // Extract CPU port bits
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Get the current EXROM signal state.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @return true = EXROM active (signal low), false = EXROM inactive (signal high)
 */
bool c64_bus_get_exrom_signal(c64_bus_t* c64_bus) {
    if (!c64_bus) return false;
    
    // Return inverted state (bit set = signal high = inactive)
    return (c64_bus->system_lines & SYS_MASK_EXROM) == 0;
}

/**
 * Get the current GAME signal state.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @return true = GAME active (signal low), false = GAME inactive (signal high)
 */
bool c64_bus_get_game_signal(c64_bus_t* c64_bus) {
    if (!c64_bus) return false;
    
    // Return inverted state (bit set = signal high = inactive)
    return (c64_bus->system_lines & SYS_MASK_GAME) == 0;
}
