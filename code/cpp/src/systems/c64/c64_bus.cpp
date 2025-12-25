#include "c64_bus.h"
#include "c64.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/cpu/fam65xx/mos6510.h"
#include "../../chip/logic/pla.h"
#include "../../core/aiemuc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// ============================================================================
// BANKING CHANGE CALLBACK
// ============================================================================

void c64_bus_on_banking_change(void* bus_ptr, uint8_t banking_state) {
    c64_bus_t* bus = (c64_bus_t*)bus_ptr;
    if (!bus || !bus->c64) return;
    
    // Convert MOS6510 banking state to C64 bus mode
    // banking_state contains LORAM (bit 0), HIRAM (bit 1), CHAREN (bit 2)
    // Need to add EXROM and GAME bits from system lines
    uint8_t exrom = (bus->system_lines & SYS_MASK_EXROM) ? 1 : 0;
    uint8_t game = (bus->system_lines & SYS_MASK_GAME) ? 1 : 0;
    
    // Construct full PLA mode: GAME | EXROM | CHAREN | HIRAM | LORAM
    uint8_t pla_mode = (game << 4) | (exrom << 3) | (banking_state & 0x07);
    
    // Debug output for banking changes
    printf("[BANKING] CPU port=$%02X → mode=$%02X (GAME=%d EXROM=%d CHAREN=%d HIRAM=%d LORAM=%d)\n",
           banking_state, pla_mode, game, exrom,
           (banking_state & 0x04) ? 1 : 0,
           (banking_state & 0x02) ? 1 : 0,
           (banking_state & 0x01) ? 1 : 0);
    
    // Also print what bank 13 ($D000-$DFFF) will be after the switch
    uint8_t bank13_chip_read = decode_read_chip(bus->cpu_encoded_chip_per_bank_per_mode[pla_mode][13]);
    uint8_t bank13_chip_write = decode_write_chip(bus->cpu_encoded_chip_per_bank_per_mode[pla_mode][13]);
    printf("[BANKING] Bank 13 ($D000-$DFFF): READ=CHIP_%d WRITE=CHIP_%d\n", bank13_chip_read, bank13_chip_write);
    
    // Switch to new memory mapping mode
    c64_bus_mode_switch(bus, pla_mode);
}

// Chip accessor functions - these use the chip descriptor's read/write callbacks using bus_state_t pattern

// Cartridge expansion port IO functions removed (io1_read, io1_write, io2_read, io2_write)
// These were unused and producing compiler warnings
// TODO: Re-implement when cartridge support is added

// Simple 4KB bank calculation for optimized system (0-15)
static inline int8_t c64_get_address_bank(uint16_t address) {
    return address >> 12;  // Extract 4KB bank (0-15)
}

// ULTRA-OPTIMIZED VIC-II MEMORY READ - Better performance than CPU version
// VIC-II uses pre-selected active array (indexed by CHIP), can only read, never write
// Uses unified memory buffer for branchless access to ROM and RAM
bus_state_t c64_bus_vic_read(c64_bus_t* c64_bus, bus_state_t bus_state, uint16_t address) {
    BUS_SET_ADDR(bus_state, address); // Perhaps this is no longer needed
    // Extract 4KB bank from address (0-15 for VIC-II's 64KB addressable space)
    const uint8_t vicii_bank = c64_get_address_bank(address);
    // Get raw CHIP directly from pre-selected active array (no mode indexing)
    const uint8_t chip = c64_bus->vicii_chip_per_bank[vicii_bank];

    // Early return for unmapped regions - use whatever is on the bus
    if (unlikely(chip == CHIP_UNMAPPED)) {
        // Leave bus data unchanged (floating bus state)
        return bus_state;
    }
    
    const uint8_t data = c64_bus_read_chip_byte(c64_bus, chip, address);

    BUS_SET_DATA(bus_state, data);
    return bus_state;
}

/**
 * New cycle-accurate memory tick function for the refactored architecture.
 * This function will be used by the new MOS6510 implementation to handle
 * memory access in a cycle-accurate manner.
 *
 * Optimized for register-based calling convention to avoid host stack accesses.
 * Takes bus state by value and returns updated bus state for efficient register usage.
 *
 * NOTE: I/O port addresses (0-1) are now handled directly by mos6510_tick()
 * early in the CPU tick to prevent memory system from overwriting I/O port data.
 *
 * PHASE 4.1: Enhanced fast path implementation for all unified buffer chips
 * - Direct unified buffer access for CHIP_RAM, CHIP_BASIC, CHIP_KERNAL, CHIP_CHARROM, CHIP_ROML, CHIP_ROMH
 * - Bypass callback system for ROM/RAM access using pointer arithmetic
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param bus_state Current bus state (passed by value for register optimization)
 * @return Updated bus state (for register-to-register operation)
 */
bus_state_t REGISTER_CALL c64_memory_tick(c64_bus_t* c64_bus, bus_state_t bus_state) {
    if (unlikely(!c64_bus)) return bus_state;
    
    // Determine if this is a read or write operation
    const bool is_read = BUS_GET_LINES(bus_state) & BUS_MASK_RW;
    const uint16_t address = BUS_GET_ADDR(bus_state);
    const uint8_t address_bank = c64_get_address_bank(address);  // Extract 4KB bank (0-15)
    
    // NOTE: I/O port addresses (0-1) are now handled by mos6510_tick() early in the CPU tick
    // This prevents the memory system from overwriting I/O port read data with RAM data
    if (is_read) {
        // === READ OPERATION ===
        // CRITICAL: Determine which chip has bus control by checking AEC line
        // According to VIC-II documentation section 2.4.3 "Memory access of the 6510 and VIC":
        // - AEC HIGH (during PHI2): CPU has bus control → use CPU memory mapping
        // - AEC LOW (during PHI1): VIC-II has bus control → use VIC-II memory mapping
        //
        // The VIC-II has its own separate memory map (section 2.4.2) where:
        // - Character ROM appears at $1000-$1FFF in banks 0 and 2 (not at $D000 like CPU sees it)
        // - VIC-II uses its own chip lookup array: vicii_chip_per_bank
        //
        // When VIC-II owns the bus (AEC low), we must use VIC-II's chip lookup array
        // to correctly access Character ROM and other memory regions as VIC-II sees them.
        const bool cpu_has_bus = (BUS_GET_LINES(bus_state) & BUS_MASK_AEC) != 0;
        const uint8_t chip = cpu_has_bus ?
            decode_read_chip(c64_bus->cpu_encoded_chip_per_bank[address_bank]) :
            c64_bus->vicii_chip_per_bank[address_bank];

        // ENHANCED FAST PATH: Direct unified buffer access for all memory chips
        // Fast path handles: CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC, CHIP_CHARROM, CHIP_RAM
        if (likely(chip <= CHIP_RAM)) {
            // Use unified buffer read helper for all ROM/RAM types
            const uint8_t data = c64_bus_read_chip_byte(c64_bus, chip, address);
            BUS_SET_DATA(bus_state, data);
        } else if (chip == CHIP_IO) {
            // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
            // Calculate IO page number from address (0-15 for $D000-$DFFF)
            const uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

            // Straight call to the appropriate handler - no conditionals needed
            bus_state = c64_bus->io_handlers[io_page].read_handler(
                c64_bus->io_handlers[io_page].chip_instance, bus_state);
        } else {
            // CHIP_UNMAPPED and others: floating bus behavior
            // Leave BUS_GET_DATA(bus_state) unchanged (floating bus state)
        }
    } else {
        // === WRITE OPERATION ===
        // Writes always use CPU memory mapping regardless of AEC state
        // The CPU can complete writes even when BA/RDY is low (early warning from VIC-II)
        // BA goes low 3 cycles early to allow the CPU to complete up to 3
        // consecutive write operations before halting on the first read access.
        const uint8_t chip = decode_write_chip(c64_bus->cpu_encoded_chip_per_bank[address_bank]);

        // ENHANCED FAST PATH: Handle all writable unified buffer regions
        if (likely(chip == CHIP_RAM)) {
            const uint8_t data = BUS_GET_DATA(bus_state);

            c64_bus_write_ram_byte(c64_bus, address, data);
        } else if (chip == CHIP_IO) {
            // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
            // Calculate IO page number from address (0-15 for $D000-$DFFF)
            const uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

            // Straight call to the appropriate handler - no conditionals needed
            bus_state = c64_bus->io_handlers[io_page].write_handler(
                c64_bus->io_handlers[io_page].chip_instance, bus_state);
        } else {
            // Writes to UNMAPPED and ROM areas are ignored (no action needed)
            // ROM chips (BASIC, KERNAL, CHARROM, ROML, ROMH) are read-only in hardware
        }
    }
    
    return bus_state;
}

// Made non-static for banking verification utility
void c64_bus_system_destroy(void* chip) {
    c64_bus_t* c64_bus = (c64_bus_t*)chip;
    if (c64_bus && c64_bus->allocated_buffer) {
        aiemuc_aligned_free(c64_bus->allocated_buffer);
    }
    free(chip);
}

// Made non-static for banking verification utility
void* c64_bus_system_create(chip_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    
    // Initialize default_state with pull-up resistors HIGH (IRQ, NMI, BA, AEC, RDY)
    // This is the state each tick starts with before any chip asserts lines
    c64_bus->default_state = BUS_STATE(0, 0, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY | BUS_MASK_IRQ | BUS_MASK_NMI) | BUS_BIT(BUS_RES_BIT);
    
    // Initialize current state to match default state
    c64_bus->state = c64_bus->default_state;
    
    // Initialize system lines with default cartridge signals (no cartridge)
    c64_bus->system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;  // Both high = no cartridge
    
    // Note: pla_banking_mode will be initialized by c64_bus_mode_switch()
    // after PLA mapping data is set up in c64_pla_maps_generate()
    // Note: calloc already zeroed *chip_per_bank* arrays
    
    // Initialize the integrated adapter interfaces
    c64_bus_init_adapters(c64_bus);
    
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer

    // Initialize ROM/RAM pointers and allocate unified buffer with default configuration
    c64_config_t default_config;
    c64_config_init_defaults(&default_config);
    c64_bus_init_unified_pointers(c64_bus, c64, &default_config);

    // Initialize compact IO page handlers for efficient I/O access
    c64_bus_init_io_handlers(c64_bus);
}

chip_descriptor_t c64_bus_descriptor = {
    /* description */ "C64 System Bus Controller",
    /* create */ c64_bus_system_create,
    /* destroy */ c64_bus_system_destroy,
    /* bus_attach */ NULL,
    /* bank_change */ NULL
#ifdef IMGUI_VERSION
    , /* render_debug_window */ NULL, // No GUI debug window implemented yet
    /* render_settings_window */ NULL // No GUI settings window implemented yet
#endif
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    // Update the optimized banking for the current mode
    c64_bus->pla_banking_mode = mode & 0x1F;
    
    memcpy(c64_bus->cpu_encoded_chip_per_bank, c64_bus->cpu_encoded_chip_per_bank_per_mode[mode], 16);
    // Also copy VIC-II active array for optimal performance
    memcpy(c64_bus->vicii_chip_per_bank, c64_bus->vicii_chip_per_bank_per_mode[mode], 16);
}

static void c64_bus_update_pla_mode(c64_bus_t* c64_bus) {
    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

// ============================================================================
// PLA integration functions
// ============================================================================

uint8_t pla_906114_01_outputs_to_chip(pla_906114_01_t* pla) {
    if (!pla->outputs.n_casram) {
        // Main RAM (read/write)
        return CHIP_RAM;
    } else if (!pla->outputs.n_basic) {
        // BASIC ROM (read-only)
        return CHIP_BASIC;
    } else if (!pla->outputs.n_kernal) {
        // KERNAL ROM (read-only)
        return CHIP_KERNAL;
    } else if (!pla->outputs.n_io) {
        // I/O region - includes VIC-II, SID, Color RAM, CIA1, CIA2 (read/write)
        return CHIP_IO;
    } else if (!pla->outputs.n_charrom) {
        // Character ROM (read-only)
        // Only reached when IO is not active, ensuring Character ROM is only
        // selected for reads when CHAREN=1 but not for writes.
        return CHIP_CHARROM;
    } else if (!pla->outputs.n_roml) {
        // Cartridge ROM Low (read-only)
        return CHIP_ROML;
    } else if (!pla->outputs.n_romh) {
        // Cartridge ROM High (read-only)
        return CHIP_ROMH;
    }

    // Default to unmapped when no chip is selected
    // This happens when all PLA outputs are inactive (high)
    return CHIP_UNMAPPED;
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
        // Determine read CHIP based on PLA outputs for read mode
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);

        // Configure PLA for WRITE mode
        pla->inputs.r_w = false;  // Write mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        uint8_t write_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);

        // Encode both read and write CHIPs into the mapping
        bus->cpu_encoded_chip_per_bank[bank] = encode_chip_rw(read_chip, write_chip);
    }
}

void c64_bus_populate_vicii_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Set other inputs for VIC-II access (not normal CPU operation)
    pla->inputs.n_aec = true;   // VIC-II has bus control (AEC low (#EAC high) = VIC-II access)
    pla->inputs.ba = false;     // Bus available (BA low = DMA)
    pla->inputs.n_cas = false;  // CAS active for VIC-II regular memory access (not refresh)
    // Configure PLA for READ mode (VIC-II can only read, never write)
    pla->inputs.r_w = true;     // Read mode

    // VIC-II can access all 16 banks (full 16-bit address space)
    // VA14 and VA15 are driven by CIA, so VIC-II can reach all 16 banks
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Set address in PLA (will call pla_906114_01_update_outputs)
        pla_906114_01_set_vicii_address_bank((pla_906114_01_t*)pla, (uint8_t)bank);
        // Determine read CHIP based on PLA outputs for read mode
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);
        // VIC-II banking stores direct CHIP values, no encoding needed
        bus->vicii_chip_per_bank[bank] = read_chip;        
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Generate all 32 CPU memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        // CRITICAL FIX: Mode bits represent SIGNAL LEVELS, not functional states!
        // Mode bit = 1 means SIGNAL is HIGH (e.g., #LORAM signal = HIGH)
        // NO INVERSION - direct mapping from mode bits to PLA signal level variables
        // The PLA product terms handle the active-low interpretation internally
        pla->inputs.n_loram = (mode & 0x01) != 0;    // Direct: mode bit → PLA signal level
        pla->inputs.n_hiram = (mode & 0x02) != 0;    // Direct: mode bit → PLA signal level
        pla->inputs.n_charen = (mode & 0x04) != 0;   // Direct: mode bit → PLA signal level
        pla->inputs.n_exrom = (mode & 0x08) != 0;    // Direct: mode bit → PLA signal level
        pla->inputs.n_game = (mode & 0x10) != 0;     // Direct: mode bit → PLA signal level
        // CPU address bits will be set during populate_pla_mapping for each bank
        // Populate mapping for this mode
        c64_bus_populate_cpu_pla_mapping(bus, pla);
        // Copy the CPU mapping to the mode-specific array
        memcpy(bus->cpu_encoded_chip_per_bank_per_mode[mode], bus->cpu_encoded_chip_per_bank, 16);
    }
    
    // Generate VIC-II memory modes
    // VIC-II uses: #GAME, #EXROM (both from CPU mode)
    // #VA14 is now automatically set from the address bit in pla_906114_01_set_vicii_address_bank()
    for (int cpu_mode = 0; cpu_mode < 32; cpu_mode++) {
        // Extract mode bits as signal levels (no inversion)
        // Mode bits represent signal levels directly
        pla->inputs.n_game = (cpu_mode & 0x10) != 0;     // Direct: mode bit → PLA signal level
        pla->inputs.n_exrom = (cpu_mode & 0x08) != 0;    // Direct: mode bit → PLA signal level
        
        // Populate VIC-II mapping for this mode (stores direct CHIPs)
        // #VA14 will be automatically set from address bit 14 during population
        c64_bus_populate_vicii_pla_mapping(bus, pla);
        // Copy the VIC-II raw CHIPs to the mode-specific array
        memcpy(bus->vicii_chip_per_bank_per_mode[cpu_mode], bus->vicii_chip_per_bank, 16);
    }
}

// ============================================================================
// PLA MODE GENERATION - Convert CPU port bits + cartridge signals to 5-bit PLA mode
// ============================================================================

uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits) {
    // The mode value is used as an index into the pre-generated PLA mode tables.
    // The c64_bus_generate_all_pla_modes() function generates these tables with inverted logic:
    //   mode bit 0 set → n_loram = false (LORAM enabled)
    //   mode bit 0 clear → n_loram = true (LORAM disabled)
    //
    // CPU port bits are active-high: 1 = enable ROM/CHAR
    // Mode bits should also be active-high to match the table generation
    // When CPU port = $37 (bits 0-2 = 111), mode should = $07 to enable all ROMs
    //
    // NO INVERSION NEEDED - direct mapping from CPU port bits to mode bits
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    uint8_t pla_mode = cpu_port_bits & 0x07;
    
    // Add cartridge control signals from system lines
    // System lines are active-high: bit set = signal inactive
    // Mode bits should also be active-high: bit set = signal inactive
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_EXROM) ? 0x08 : 0); // EXROM (bit 3)
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_GAME) ? 0x10 : 0);  // GAME (bit 4)

    return pla_mode;
}

// ============================================================================
// ADAPTER INTERFACES - Integrated adapter initialization
// ============================================================================

// Control lines adapter functions
static uint32_t c64_control_lines_get(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert the C64's bus state to the new uint32_t format
    return (uint32_t)BUS_GET_LINES(c64_bus->state);
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's bus state format
    BUS_SET_LINES(c64_bus->state, (uint8_t)(lines & 0xFF));
}

void c64_bus_init_adapters(c64_bus_t* c64_bus) {
    // Initialize control lines adapter
    c64_bus->control_lines_adapter.get_lines = c64_control_lines_get;
    c64_bus->control_lines_adapter.set_lines = c64_control_lines_set;
    c64_bus->control_lines_adapter.context = c64_bus;
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

// Chip entry lookup table for description and validation (handles irregular numbering)
typedef struct {
    uint16_t base_address;
    size_t size;
    const char* label;
} c64_chip_entry_t;

// Sparse lookup table indexed by CHIP_* values (supports irregular numbering)
static const c64_chip_entry_t c64_bus_chip_to_entry[] = {
    /* [CHIP_ROML] = */     { 0x8000, 8*1024, "Cartridge ROM Low" },
    { 0, 0, nullptr }, // CHIP_1 unused
    /* [CHIP_ROMH] = */     { 0xA000, 8*1024, "Cartridge ROM High" }, // Note: Can also map to 0xE000
    { 0, 0, nullptr }, // CHIP_3 unused
    /* [CHIP_KERNAL] = */   { 0xE000, 8*1024, "KERNAL ROM" },
    { 0, 0, nullptr }, // CHIP_5 unused
    /* [CHIP_BASIC] = */    { 0xA000, 8*1024, "BASIC ROM" },
    /* [CHIP_CHARROM] = */  { 0xD000, 4*1024, "Character ROM" },
    { 0, 0, nullptr }, // CHIP_8 unused
    /* [CHIP_RAM] = */      { 0x0000, 64*1024, "RAM" },
    /* [CHIP_UNMAPPED] = */ { 0x0000, 0, "Unmapped" },
    /* [CHIP_IO] = */       { 0xD000, 4*1024, "I/O" },
};
static const size_t CHIP_ENTRY_COUNT = sizeof(c64_bus_chip_to_entry) / sizeof(c64_bus_chip_to_entry[0]);

// Update c64_bus_get_chip_description to use the chip entry lookup table
bool c64_bus_get_chip_description(const c64_bus_t* bus, uint8_t chip, chip_description_t* out) {
    if (!bus || !out) return false;

    memset(out, 0, sizeof(*out));
    
    // Validate chip ID and get entry (handles irregular numbering via sparse array)
    if (chip >= CHIP_ENTRY_COUNT) return false;
    
    const c64_chip_entry_t* entry = &c64_bus_chip_to_entry[chip];
    
    // Entry exists if it has a label (even UNMAPPED has a label)
    if (entry->label) {
        out->base = entry->base_address;
        out->size = entry->size;
        out->label = entry->label;
        return true;
    }

    // Fallback for undefined entries
    out->base = 0;
    out->size = 0;
    out->label = "?";
    return false;
}

// Return a concise title string for each CHIP (for legend/tooling/GUI)
const char* c64_bus_chip_to_title(uint8_t chip) {
    switch (chip) {
        case CHIP_ROML:
            return "ROML";
        case CHIP_ROMH:
            return "ROMH";
        case CHIP_KERNAL:
            return "KERNAL";
        case CHIP_BASIC:
            return "BASIC";
        case CHIP_CHARROM:
            return "CHARROM";
        case CHIP_RAM:
            return "RAM";
        case CHIP_UNMAPPED:
            return "-";
        case CHIP_IO:
            return "I/O";
        default:
            return "?";
    }
}

// Utility: Convert a size in bytes to a human-readable string ("256B", "4KB", etc.)
const char* c64_bus_size_to_str(size_t size) {
    static char buf[32];  // Increased buffer size to prevent truncation
    if (size >= (1 << 20) && (size % (1 << 20)) == 0) {
        snprintf(buf, sizeof(buf), "%zuMB", size / (1 << 20));
    } else if (size >= 1024 && (size % 1024) == 0) {
        snprintf(buf, sizeof(buf), "%zuKB", size / 1024);
    } else {
        snprintf(buf, sizeof(buf), "%zuB", size);
    }
    return buf;
}


/**
 * Initialize RAM/ROM pointers to point into the unified memory buffer.
 * This eliminates separate memory allocations and ensures consistency.
 * Uses configuration structure to determine cartridge ROM presence.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param c64_system Pointer to the C64 system (for pointer updates)
 * @param config Pointer to the C64 system configuration structure
 */
void c64_bus_init_unified_pointers(c64_bus_t* c64_bus, void* c64_system, const c64_config_t* config) {
    if (!c64_bus || !c64_system || !config) return;
    
    // Store cartridge ROM presence flags from configuration
    c64_bus->roml_present = config->roml_present;
    c64_bus->romh_present = config->romh_present;
    
    // If buffer already exists, just update the ROM pointers to preserve loaded data
    if (c64_bus->allocated_buffer) {
        c64_t* c64 = (c64_t*)c64_system;
        uint8_t* buffer = c64_bus->unified_memory_buffer;
        
        // Update ROM chip pointers but preserve existing data by copying it
#define DO(c64_device, offset, size, present_flag) \
    if (c64_device && present_flag) { \
        /* Preserve existing ROM data if it was already loaded */ \
        if (c64_device->memory && c64_device->memory != buffer + offset) { \
            /* Copy existing ROM data to unified buffer */ \
            memcpy(buffer + offset, c64_device->memory, size); \
            /* Free the old separate allocation only if we owned it */ \
            if (c64_device->owns_memory) { \
                free(c64_device->memory); \
            } \
        } \
        /* Point to unified buffer location and mark as not owned */ \
        c64_device->memory = buffer + offset; \
        c64_device->owns_memory = false; \
    } else if (c64_device) { \
        /* ROM not present - set to NULL and free existing if needed */ \
        if (c64_device->memory && c64_device->owns_memory) { \
            free(c64_device->memory); \
        } \
        c64_device->memory = NULL; \
        c64_device->owns_memory = false; \
    }

        // Update ROM pointers to preserve loaded data
        DO(c64->cartridge_roml, 0x0000, 8*1024, c64_bus->roml_present);
        DO(c64->cartridge_romh, 0x2000, 8*1024, c64_bus->romh_present);
        DO(c64->kernal, 0x4000, 8*1024, true);
        DO(c64->basic, 0x6000, 8*1024, true);
        DO(c64->charrom, 0x8000, 4*1024, true);
        DO(c64->ram, 0x9000, 64*1024, true);
#undef DO
        
        printf("c64_bus: Updated ROM pointers in existing unified buffer\n");
        return;
    }
    
    // Clean up any existing allocation (shouldn't happen, but be safe)
    if (c64_bus->allocated_buffer) {
        aiemuc_aligned_free(c64_bus->allocated_buffer);
        c64_bus->allocated_buffer = NULL;
    }
    
    // Calculate required memory size for strategic layout (4KB step size)
    // Strategic layout: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x7000) + RAM(0x9000)
    // Total: 0x9000 (36KB) + RAM(64KB) = 100KB maximum
    size_t required_size = 100 * 1024; // Base size: 36KB + 64KB RAM
    size_t offset = 0;                  // No offset needed for strategic layout
    
    // Adjust for missing cartridge ROMs (can save space at beginning)
    if (!config->roml_present && !config->romh_present) {
        // Skip first 16KB: start at KERNAL (0x4000)
        required_size = 84 * 1024;  // (36KB - 16KB) + 64KB RAM = 84KB
        offset = 16 * 1024;         // Offset buffer start by 16KB
    } else if (!config->roml_present) {
        // Skip first 8KB: start at ROMH (0x2000)
        required_size = 92 * 1024;  // (36KB - 8KB) + 64KB RAM = 92KB
        offset = 8 * 1024;          // Offset buffer start by 8KB
    } else if (!config->romh_present) {
        // Keep ROML, skip ROMH: need custom layout
        // For simplicity, allocate full size for now
        required_size = 100 * 1024;
        offset = 0;
    }
    
    // Allocate aligned memory for optimal cache performance
    c64_bus->allocated_buffer = (uint8_t*)aiemuc_aligned_alloc(64, required_size);
    
    if (!c64_bus->allocated_buffer) {
        printf("c64_bus: ERROR - Failed to allocate unified memory buffer (%zu KB)\n", required_size / 1024);
        return;
    }
    
    c64_bus->allocated_size = required_size;
    
    // Apply offset for missing cartridge ROMs (pointer arithmetic for memory savings)
    c64_bus->unified_memory_buffer = c64_bus->allocated_buffer - offset;
    
    // Initialize allocated memory to zero
    memset(c64_bus->allocated_buffer, 0, required_size);
    
    c64_t* c64 = (c64_t*)c64_system;
    uint8_t* buffer = c64_bus->unified_memory_buffer;
    
#define DO(c64_device, offset, size, present_flag) \
    if (c64_device && present_flag) { \
        /* Preserve existing ROM data if it was already loaded */ \
        if (c64_device->memory && c64_device->memory != buffer + offset) { \
            /* Copy existing ROM data to unified buffer */ \
            memcpy(buffer + offset, c64_device->memory, size); \
            /* Free the old separate allocation only if we owned it */ \
            if (c64_device->owns_memory) { \
                free(c64_device->memory); \
            } \
        } \
        /* Point to unified buffer location and mark as not owned */ \
        c64_device->memory = buffer + offset; \
        c64_device->owns_memory = false; \
    } else if (c64_device) { \
        /* ROM not present - set to NULL and free existing if needed */ \
        if (c64_device->memory && c64_device->owns_memory) { \
            free(c64_device->memory); \
        } \
        c64_device->memory = NULL; \
        c64_device->owns_memory = false; \
    }

    // Point the following devices to their respective unified buffer offset
    // Strategic layout: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x7000, RAM=0x9000
    DO(c64->cartridge_roml, 0x0000, 8*1024, c64_bus->roml_present); // CHIP_ROML = 0 -> 0x0000 - optional
    DO(c64->cartridge_romh, 0x2000, 8*1024, c64_bus->romh_present); // CHIP_ROMH = 2 -> 0x2000 - optional
    DO(c64->kernal, 0x4000, 8*1024, true);                          // CHIP_KERNAL = 4 -> 0x4000 - always present
    DO(c64->basic, 0x6000, 8*1024, true);                           // CHIP_BASIC = 6 -> 0x6000 - always present
    DO(c64->charrom, 0x7000, 4*1024, true);                         // CHIP_CHARROM = 7 -> 0x7000 - always present
    DO(c64->ram, 0x9000, 64*1024, true);                            // CHIP_RAM = 9 -> 0x9000 - always present
#undef DO
    
    printf("c64_bus: Allocated %zu KB unified buffer (saved %zu KB), ROML:%s ROMH:%s\n",
           required_size / 1024,
           (100 * 1024 - required_size) / 1024,
           config->roml_present ? "yes" : "no",
           config->romh_present ? "yes" : "no");
}

// ============================================================================
// CHIP CALLBACK FUNCTIONS - Optimized chip-specific operations for hybrid approach
// ============================================================================

/**
 * Floating bus behavior for unmapped IO regions.
 * Returns bus state unchanged (floating bus), writes are ignored.
 */
static bus_state_t c64_bus_unmapped_read(void* chip, bus_state_t bus_state) {
    (void)chip; // Unused - floating bus has no chip instance
    // Leave bus data unchanged - floating bus behavior
    return bus_state;
}

static bus_state_t c64_bus_unmapped_write(void* chip, bus_state_t bus_state) {
    (void)chip; // Unused - floating bus has no chip instance
    // Write is ignored - no operation
    return bus_state;
}

/**
 * Initialize IO page handlers for optimized I/O access.
 * Sets up direct chip callbacks that eliminate wrapper functions and provide
 * efficient targeted access to IO chips, indexed by IO page number.
 *
 * C64 IO Memory Map ($D000-$DFFF):
 * $D000-$D3FF (pages 0-3):   VIC-II (64 bytes mirrored)
 * $D400-$D7FF (pages 4-7):   SID (32 bytes mirrored)
 * $D800-$DBFF (pages 8-11):  Color RAM (1KB)
 * $DC00-$DCFF (page 12):     CIA1 (16 bytes mirrored)
 * $DD00-$DDFF (page 13):     CIA2 (16 bytes mirrored)
 * $DE00-$DEFF (page 14):     I/O1 expansion (unmapped by default)
 * $DF00-$DFFF (page 15):     I/O2 expansion (unmapped by default)
 */
void c64_bus_init_io_handlers(c64_bus_t* c64_bus) {
    c64_t* c64 = (c64_t*)c64_bus->c64;

    // Set up direct callbacks and chip instances for each IO page (0-15 for $D000-$DFFF)
    // Each page gets the appropriate chip register function and chip instance directly

    // Pages 0-3 ($D000-$D3FF): VIC-II (64 bytes mirrored across 1KB)
    for (int page = 0; page <= 3; page++) {
        c64_bus->io_handlers[page].read_handler = vicii_registers_read;
        c64_bus->io_handlers[page].write_handler = vicii_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->vicii;
    }

    // Pages 4-7 ($D400-$D7FF): SID (32 bytes mirrored across 1KB)
    for (int page = 4; page <= 7; page++) {
        c64_bus->io_handlers[page].read_handler = mos6581_registers_read;
        c64_bus->io_handlers[page].write_handler = mos6581_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->sid;
    }

    // Pages 8-11 ($D800-$DBFF): Color RAM (1KB, 1024 bytes)
    for (int page = 8; page <= 11; page++) {
        c64_bus->io_handlers[page].read_handler = mos2114_read;
        c64_bus->io_handlers[page].write_handler = mos2114_write;
        c64_bus->io_handlers[page].chip_instance = c64->colorram;
    }

    // Page 12 ($DC00-$DCFF): CIA1 (16 bytes mirrored across 256 bytes)
    c64_bus->io_handlers[12].read_handler = mos6526_registers_read;
    c64_bus->io_handlers[12].write_handler = mos6526_registers_write;
    c64_bus->io_handlers[12].chip_instance = c64->cia1;

    // Page 13 ($DD00-$DDFF): CIA2 (16 bytes mirrored across 256 bytes)
    c64_bus->io_handlers[13].read_handler = mos6526_registers_read;
    c64_bus->io_handlers[13].write_handler = mos6526_registers_write;
    c64_bus->io_handlers[13].chip_instance = c64->cia2;

    // Page 14 ($DE00-$DEFF): I/O1 expansion port (unmapped by default - floating bus)
    c64_bus->io_handlers[14].read_handler = c64_bus_unmapped_read;
    c64_bus->io_handlers[14].write_handler = c64_bus_unmapped_write;
    c64_bus->io_handlers[14].chip_instance = NULL;

    // Page 15 ($DF00-$DFFF): I/O2 expansion port (unmapped by default - floating bus)
    c64_bus->io_handlers[15].read_handler = c64_bus_unmapped_read;
    c64_bus->io_handlers[15].write_handler = c64_bus_unmapped_write;
    c64_bus->io_handlers[15].chip_instance = NULL;
}
