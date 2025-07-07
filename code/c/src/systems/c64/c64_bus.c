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
    // Also copy VIC-II active array for optimal performance
    memcpy(c64_bus->vic_ii_acid_per_bank, c64_bus->vic_ii_acid_per_bank_per_mode[mode], 16);
}

static void c64_bus_update_pla_mode(c64_bus_t* c64_bus) {
    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

// Waits for the bus to be ready, ticking non-CPU chips.
static inline void c64_wait_for_bus_ready(c64_bus_t *c64_bus, bool is_read_cycle) {
    c64_t* c64 = c64_bus->c64;

    if (is_read_cycle) {
        // A CPU read must wait for VIC to release the bus (AEC high) AND
        // for the BA/RDY line to be high.
        while (!(c64_bus->control_lines & AEC_LINE) || !(c64_bus->control_lines & BA_LINE)) {
            c64_non_cpu_cycle(c64);
        }
    } else {
        // A CPU write only needs to wait for VIC to release the address bus.
        // It is NOT affected by the BA/RDY line.
        while (!(c64_bus->control_lines & AEC_LINE)) {
            c64_non_cpu_cycle(c64);
        }
    }
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    // The core cycle function handles all bus contention (waiting) and performs
    // the final tick for all non-CPU chips. This happens concurrently with the
    // CPU's memory access.
    c64_wait_for_bus_ready(c64_bus, true);

    // The bus is now guaranteed to be ready for the CPU.
    c64_bus->address = addr; // Perhaps this is no longer needed
    uint8_t data = c64_bus_memory_read(c64_bus, addr);
    c64_bus->data = data; // Used for "floating" bus state for subsequent unattached reads

    // Tick system through complete cycle
    c64_non_cpu_cycle(c64_bus->c64);
    return data;
}

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    // The core cycle function handles all bus contention (waiting) and performs
    // the final tick for all non-CPU chips.
    c64_wait_for_bus_ready(c64_bus, false);

    // The bus is now guaranteed to be ready for the CPU.
    c64_bus->address = addr; // Perhaps this is no longer needed
    c64_bus->data = value; // Used for "floating" bus state for subsequent unattached reads
    c64_bus_memory_write(c64_bus, addr, value);

    // Advance system
    c64_non_cpu_cycle(c64_bus->c64);
}

// PLA integration functions
#include "../../chip/logic/pla.h"

uint8_t pla_906114_01_outputs_to_acid(pla_906114_01_t* pla) {
    if (!pla->outputs.n_casram) {
        // Main RAM (read/write)
        return ACID_RAM;
    } else if (!pla->outputs.n_basic) {
        // BASIC ROM (read-only)
        return ACID_BASIC;
    } else if (!pla->outputs.n_kernal) {
        // KERNAL ROM (read-only)
        return ACID_KERNAL;
    } else if (!pla->outputs.n_charrom) {
        // Character ROM (read-only)
        return ACID_CHARROM;
    } else if (!pla->outputs.gr_w) {
        // Color RAM (write-only)
        return ACID_COLORRAM_D8;
    } else if (!pla->outputs.n_io) {
        // I/O region - I/O bank (read-write)
        return ACID_VIC_D0; // c64_bus_memory_read will handle mapping to full 16 I/O pages
    } else if (!pla->outputs.n_roml) {
        // Cartridge ROM Low (read-only)
        return ACID_ROML;
    } else if (!pla->outputs.n_romh) {
        // Cartridge ROM High (read-only)
        return ACID_ROMH;
    }

    // Default to unmapped when no chip is selected
    // This happens when all PLA outputs are inactive (high)
    return ACID_UNMAPPED;
}

void c64_bus_populate_cpu_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Set other inputs for normal CPU operation (not VIC-II access)
    pla->inputs.n_aec = false;   // CPU has bus control (AEC high (#EAC low) = CPU access)
    pla->inputs.ba = true;       // Bus available (BA high = no DMA)
    pla->inputs.n_cas = false;   // CAS active (CAS low = enable RAM access for CPU)
    // Map memory regions based on PLA outputs
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Configure PLA for READ mode
        pla->inputs.r_w = true;  // Read mode
        // Set address in PLA (will call pla_906114_01_update_outputs)
        pla_906114_01_set_cpu_address_bank((pla_906114_01_t*)pla, (uint8_t)bank);
        // Determine read ACID based on PLA outputs for read mode
        uint8_t read_acid = pla_906114_01_outputs_to_acid((pla_906114_01_t*)pla);
        // Special handling for CPU I/O ports in zero bank (4KB bank $0000-$0FFF)
        if (bank == 0 && read_acid == ACID_RAM) {
            read_acid = ACID_ZEROBANK;
        }

        // Configure PLA for WRITE mode
        pla->inputs.r_w = false;  // Write mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        uint8_t write_acid = pla_906114_01_outputs_to_acid((pla_906114_01_t*)pla);
        // Special handling for CPU I/O ports in zero bank (4KB bank $0000-$0FFF)
        if (bank == 0 && write_acid == ACID_RAM) {
            write_acid = ACID_ZEROBANK;
        }
        
        // Note: ROM areas (BASIC, KERNAL, Character ROM, Cartridge) are not writable, 
        // so write_acid remains ACID_UNMAPPED for those regions
        
        // Encode both read and write ACIDs into the mapping
        bus->encoded_rwid_per_bank[bank] = encode_acid_rw(read_acid, write_acid);
    }
}

void c64_bus_populate_vicii_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Set other inputs for VIC-II access (not normal CPU operation)
    pla->inputs.n_aec = true;   // VIC-II has bus control (AEC low (#EAC high) = VIC-II access)
    pla->inputs.ba = false;     // Bus available (BA low = DMA)
    pla->inputs.n_cas = true;   // CAS inactive for VIC-II regular memory access (not refresh)
    // Configure PLA for READ mode (VIC-II can only read, never write)
    pla->inputs.r_w = true;     // Read mode

    // VIC-II can access all 16 banks (full 16-bit address space)
    // VA14 and VA15 are driven by CIA, so VIC-II can reach all 16 banks
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Set address in PLA (will call pla_906114_01_update_outputs)
        pla_906114_01_set_vicii_address_bank((pla_906114_01_t*)pla, (uint8_t)bank);
        // Determine read ACID based on PLA outputs for read mode
        uint8_t read_acid = pla_906114_01_outputs_to_acid((pla_906114_01_t*)pla);

        // VIC-II banking stores direct ACID values, no encoding needed
        bus->vic_ii_acid_per_bank[bank] = read_acid;        
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Generate all 32 CPU memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        pla->inputs.n_loram = (mode & 0x01) == 0;    // LORAM (inverted)
        pla->inputs.n_hiram = (mode & 0x02) == 0;    // HIRAM (inverted)
        pla->inputs.n_charen = (mode & 0x04) == 0;   // CHAREN (inverted)
        pla->inputs.n_exrom = (mode & 0x08) == 0;    // EXROM (inverted)
        pla->inputs.n_game = (mode & 0x10) == 0;     // GAME (inverted)
        // CPU address bits will be set during populate_pla_mapping for each bank
        // Populate mapping for this mode
        c64_bus_populate_cpu_pla_mapping(bus, pla);
        // Copy the CPU mapping to the mode-specific array
        memcpy(bus->encoded_rwid_per_bank_per_mode[mode], bus->encoded_rwid_per_bank, 16);
    }
    
    // Generate VIC-II memory modes (different steering parameters)
    // VIC-II uses: #GAME, #EXROM, #VA14 = 3 bits, but #VA14 is handled per-bank
    // So we have 4 unique configs based on #GAME and #EXROM
    // We'll repeat these 4 configs across all 32 modes for easy indexing
    for (int cpu_mode = 0; cpu_mode < 32; cpu_mode++) {
        // Extract relevant bits for VIC-II: #GAME, #EXROM from CPU mode
        bool n_game = (cpu_mode & 0x10) == 0;     // GAME (inverted)
        bool n_exrom = (cpu_mode & 0x08) == 0;    // EXROM (inverted)
        
        // Set PLA inputs for VIC-II (only the relevant ones)
        pla->inputs.n_game = n_game;
        pla->inputs.n_exrom = n_exrom;
        // Note: #VA14 will be set during populate_vicii_pla_mapping for each bank
        
        // Populate VIC-II mapping for this mode (stores direct ACIDs)
        c64_bus_populate_vicii_pla_mapping(bus, pla);
        // Copy the VIC-II raw ACIDs to the mode-specific array
        memcpy(bus->vic_ii_acid_per_bank_per_mode[cpu_mode], bus->vic_ii_acid_per_bank, 16);
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
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    uint8_t pla_mode = cpu_port_bits & 0x07; // LORAM (bit 0) | HIRAM (bit 1) | CHAREN (bit 2)
    
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

    c64_bus_update_pla_mode(c64_bus);
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
    
    c64_bus_update_pla_mode(c64_bus);
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
    
    c64_bus_update_pla_mode(c64_bus);
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
