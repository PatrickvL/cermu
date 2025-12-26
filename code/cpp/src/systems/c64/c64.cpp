#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "c64.h"
#include "c64_bus.h"
#include "c64_config.h"
#include "c64_test_loader.h" // Test binary loading
/* dual CPU include removed */
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"
#include "../../chip/cpu/fam65xx/mos6510.h" // Direct C++ core
#include "../../chip/io/mos6526.h" // cia
#include "../../chip/sound/mos6581.h" // sid
#include "../../chip/video/vic_ii/mos6569.h" // vicii PAL
#include "../../chip/video/vic_ii/mos6567.h" // vicii NTSC stub
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h" // Color RAM
#include "../../chip/logic/pla.h" // PLA for memory mapping

// Helper function: Initialize RAM with debug test patterns
static void c64_memory_init_debug_patterns(ram_t* ram) {
    if (!ram || !ram->memory) {
        printf("ERROR: RAM pointer invalid for debug pattern initialization\n");
        return;
    }

    printf("Initializing RAM with debug test patterns...\n");

    // Clear zero page and stack
    memset(ram->memory, 0, 0x0200);
    
    // Fill remaining RAM with random bytes for realistic uninitialized memory behavior
    for (uint32_t addr = 0x0200; addr < 0x10000; addr++) {
        ram->memory[addr] = (uint8_t)rand();
    }
    
    // Add test pattern to video matrix at $0400 in all VIC-II banks
    for (int bank = 0; bank < 4; bank++) {
        uint16_t base = bank * 0x4000 + 0x0400;
        for (int i = 0; i < 1000; i++) {
            ram->memory[base + i] = (uint8_t)((base + i) & 0xFF);
        }
        printf("  Bank %d: Screen memory at $%04X filled with address pattern\n", bank, base);
    }
}

// Helper function: Initialize Color RAM with debug patterns
static void c64_colorram_init_debug(mos2114_t* colorram) {
    if (!colorram || !colorram->memory) {
        printf("ERROR: Color RAM pointer invalid for debug initialization\n");
        return;
    }

    // Randomize all 1024 color RAM locations (4-bit values 0-15)
    for (int i = 0; i < 1024; i++) {
        colorram->memory[i] = (uint8_t)(rand() & 0x0F);
    }
    printf("Color RAM initialized with random colors\n");
}

// Main memory initialization function with test mode support
void c64_memory_init(system_8bit_t* system, const c64_config_t* config) {
    // Use default ROM configuration if none provided
    const rom_config_t* rom_config = config && config->rom_config ?
                                      config->rom_config :
                                      system_config_get_default_roms();

    // Discover ROM root path for C64 system
    char rom_root_path[1024];
    bool rom_root_found = system_config_discover_rom_root("c64", rom_root_path, sizeof(rom_root_path));

    for (int i = 0; i < system->chip_count; i++) {
        chip_entry_t* dev = &system->chips[i];

        // Find and initialize RAM
        if (dev->desc == &ram_descriptor) {
            ram_t* ram = (ram_t*)dev->chip;
            if (!ram->memory) {
                printf("ERROR: RAM memory pointer is NULL!\n");
                continue;
            }
            // Initialize based on test mode
            c64_test_mode_t test_mode = config ? config->test_mode : C64_TEST_MODE_NORMAL;
            
            switch (test_mode) {
                case C64_TEST_MODE_NORMAL:
                    printf("Normal boot mode: RAM cleared\n");
                    memset(ram->memory, 0, 0x10000);
                    break;

                case C64_TEST_MODE_DEBUG_PATTERNS:
                    c64_memory_init_debug_patterns(ram);
                    break;

                case C64_TEST_MODE_PRG_FILE:
                    if (config && config->test_binary_config && config->test_binary_config->filename) {
                        printf("Loading PRG file: %s\n", config->test_binary_config->filename);
                        memset(ram->memory, 0, 0x10000);  // Clear RAM first
                        uint16_t load_addr = 0, sys_addr = 0;
                        if (!c64_test_load_prg_file(config->test_binary_config->filename,
                                                     ram, &load_addr, &sys_addr)) {
                            printf("ERROR: Failed to load PRG file, falling back to normal init\n");
                            memset(ram->memory, 0, 0x10000);
                        }
                    } else {
                        printf("ERROR: PRG mode selected but no filename provided\n");
                        memset(ram->memory, 0, 0x10000);
                    }
                    break;

                case C64_TEST_MODE_BIN_FILE:
                    if (config && config->test_binary_config && config->test_binary_config->filename) {
                        printf("Loading BIN file: %s at $%04X\n",
                               config->test_binary_config->filename,
                               config->test_binary_config->load_address);
                        memset(ram->memory, 0, 0x10000);  // Clear RAM first
                        if (!c64_test_load_bin_file(config->test_binary_config->filename,
                                                     ram, config->test_binary_config->load_address)) {
                            printf("ERROR: Failed to load BIN file, falling back to normal init\n");
                            memset(ram->memory, 0, 0x10000);
                        }
                    } else {
                        printf("ERROR: BIN mode selected but no filename provided\n");
                        memset(ram->memory, 0, 0x10000);
                    }
                    break;

                default:
                    printf("WARNING: Unknown test mode, using normal init\n");
                    memset(ram->memory, 0, 0x10000);
                    break;
            }
        }
        
        // Find and initialize Color RAM
        else if (dev->desc == &mos2114_descriptor) {
            mos2114_t* colorram = (mos2114_t*)dev->chip;
            if (colorram->memory) {
                // Initialize Color RAM based on test mode
                c64_test_mode_t test_mode = config ? config->test_mode : C64_TEST_MODE_NORMAL;
                if (test_mode == C64_TEST_MODE_DEBUG_PATTERNS) {
                    c64_colorram_init_debug(colorram);
                } else {
                    // Normal mode: clear Color RAM
                    memset(colorram->memory, 0, 1024);
                }
            }
        }

        // Initialize ROM chips by loading from files
        else if (dev->desc == &rom_descriptor) {
            rom_t* rom = (rom_t*)dev->chip;
            if (!rom->memory) {
                printf("Warning: ROM chip has no allocated memory\n");
                continue;
            }

            bool rom_loaded = false;
              // Only attempt to load ROMs if we found the ROM root directory
            if (rom_root_found) {
                // Determine ROM type based on memory address and size
                if (dev->base_address == 0xA000 && dev->size == 8192) {
                    // BASIC ROM
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->basic_rom_filenames, 8192,
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load BASIC ROM\n");
                    }
                }
                else if (dev->base_address == 0xE000 && dev->size == 8192) {
                    // KERNAL ROM
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->kernal_rom_filenames, 8192,
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load KERNAL ROM\n");
                    }
                }
                else if (dev->base_address == 0xD000 && dev->size == 4096) {
                    // Character ROM
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->chargen_rom_filenames, 4096,
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load Character ROM\n");
                    }
                }
            } else {
                printf("Warning: ROM root not found, skipping ROM loading\n");
            }

            // If ROM loading failed, fill with default pattern (0xFF for unloaded ROM)
            if (!rom_loaded) {
                memset(rom->memory, 0xFF, dev->size);
            }
        }
    }
}

bool c64_pla_maps_generate(c64_t* c64) {
    c64_bus_t* bus = &(c64->bus);

    // Create a temporary PLA instance for generating memory maps
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla)
        return false;

    // Generate all 32 memory modes using PLA
    c64_bus_generate_all_pla_modes(bus, (struct pla_906114_01_s*)pla);

    // Clean up PLA instance
    pla_906114_01_destroy(pla);

    // Initialize with mode $17 (standard C64 configuration with no cartridge, I/O enabled)
    // Mode $17 = LORAM=1, HIRAM=1, CHAREN=0, EXROM=1, GAME=1
    // LORAM=1: BASIC ROM enabled at $A000-$BFFF
    // HIRAM=1: KERNAL ROM enabled at $E000-$FFFF
    // CHAREN=0: I/O devices enabled at $D000-$DFFF (not Character ROM)
    // EXROM=1, GAME=1: No cartridge present (standard C64 operation)
    // This is the standard boot configuration - I/O accessible, BASIC/KERNAL enabled
    uint8_t initial_pla_mode = 0x17;
    c64_bus_mode_switch(bus, initial_pla_mode);
    
    printf("C64 initial banking: PLA mode=$%02X (standard config, no cartridge)\n", initial_pla_mode);
    
    return true;
}

// ============================================================================
// SINGLE UNIFIED SYSTEM TICK FUNCTION
// ============================================================================

// Callback for each bus cycle (can be set by test harness)
// Usually NULL during normal emulation, only set for testing/debugging
void (*bus_cycle_callback)(void) = NULL;

// Single unified system tick function - the one place where the entire system is ticked
void c64_system_tick(c64_t* c64) {
    if (unlikely(!c64)) {
        printf("ERROR: c64_system_tick called with NULL c64\n");
        fflush(stdout);
        return;
    }

    // Optimized null check with unlikely hint - callback rarely set during normal emulation
    if (unlikely(bus_cycle_callback != NULL)) {
        bus_cycle_callback();
    }

    // Safety checks
    if (unlikely(!c64->vicii)) {
        printf("ERROR: c64->vicii is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }

    if (unlikely(!c64->cia1)) {
        printf("ERROR: c64->cia1 is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }

    if (unlikely(!c64->cia2)) {
        printf("ERROR: c64->cia2 is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }

    if (unlikely(!c64->sid)) {
        printf("ERROR: c64->sid is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }

    c64->total_cycles++;
    c64_bus_t* bus = &(c64->bus);
    
    // =========================================================================
    // PULL-UP RESISTOR MODEL - DUAL STATE ARCHITECTURE
    // =========================================================================
    // Start each cycle with default_state (pull-up resistors HIGH)
    // - default_state contains: IRQ=1, NMI=1, BA=1, AEC=1, RDY=1 (all inactive/HIGH)
    // - Address/data from previous cycle is preserved in bus.state
    // - Copy address/data from previous state, control lines from default_state
    //
    // This ensures each cycle starts fresh with pull-ups HIGH, preventing chips
    // from overwriting each other's signal assertions.
    bus_state_t s = c64->bus.default_state;  // Start with pull-up resistors HIGH
    
    // Preserve address and data from previous cycle for continuity
    BUS_SET_ADDR(s, BUS_GET_ADDR(c64->bus.state));
    BUS_SET_DATA(s, BUS_GET_DATA(c64->bus.state));

    // =========================================================================
    // UNIFIED TIMING MODEL
    // =========================================================================
    // PHASE 1: VIC-II TICKING
    // VIC-II reads data from previous cycle, processes it, and sets up next memory access
    
    s = vicii_tick(c64->vicii, s);

    // =========================================================================
    // PHASE 1.5: CIA TICKING (BEFORE CPU PHI2)
    // =========================================================================
    // CRITICAL FIX: CIA chips must tick BEFORE CPU PHI2 so interrupt lines are set
    // when the CPU samples them. The CPU samples IRQ/NMI during PHI2, so CIA must
    // update these lines beforehand.
    //
    // CIA chips handle I/O and timing functions including interrupt generation.
    // CIA2 must be ticked before CIA1 because CIA2 controls VIC-II bank switching.
    
    s = mos6526_tick(c64->cia2, s);
    s = mos6526_tick(c64->cia1, s);

    // =========================================================================
    // HARDWARE WIRING - BA signal to CPU RDY pin
    // =========================================================================
    // The VIC-II's BA (Bus Available) output is connected to the CPU's RDY input.
    // BA goes LOW 3 cycles before VIC needs the bus, giving the CPU time to finish writes.
    // The CPU checks RDY and halts on READ operations when RDY is LOW.
    //
    // With the pull-up resistor model, VIC-II sets BA state, and we copy it to RDY here.
    // This happens after VIC-II tick but before CPU tick, so CPU sees the correct RDY state.
    if (BUS_GET_LINES(s) & BUS_MASK_BA) {
        BUS_SET_LINES(s, BUS_GET_LINES(s) | BUS_MASK_RDY);
    } else {
        BUS_SET_LINES(s, BUS_GET_LINES(s) & ~BUS_MASK_RDY);
    }

    // =========================================================================
    // PHASE 2: CPU TICKING (PHI2 phase)
    // CPU executes in PHI2 phase and may set up memory access
    // CPU samples IRQ/NMI lines during this phase (after CIA has set them above)
    // =========================================================================
    s = mos6510_tick_phi2(c64->mos6510, s);

    // =========================================================================
    // PHASE 3: MEMORY SERVICE PHASE
    // Services memory access from either CPU or VIC-II
    // The c64_memory_tick function checks AEC line to determine which chip has bus control
    // CRITICAL: This must happen AFTER vicii_tick sets up PHI2 access and AFTER CPU tick
    // =========================================================================
    s = c64_memory_tick(&c64->bus, s);

    // =========================================================================
    // PHASE 4: CPU TICKING (PHI1 phase)
    // CPU prepares next instruction fetch in PHI1 phase
    // =========================================================================
    s = mos6510_tick_phi1(c64->mos6510, s);

    // =========================================================================
    // PHASE 5: SID TICKING
    // =========================================================================
    // SID - sound generation
    s = mos6581_tick(c64->sid, s);

    // Update the bus state with the final result
    bus->state = s;
}

// Compact chip creation helper - creates and registers a chip
static inline void* create_and_register_chip(c64_t* c64, chip_descriptor_t* desc, uint16_t addr, unsigned int size) {
    void* chip;

    // Special handling for ROM - allocate memory based on requested size before registration
    if (desc == &rom_descriptor) {
        chip = rom_system_create_with_size(desc, size);
    } else {
        chip = desc->create(desc);
    }

    if (!chip) {
        printf("ERROR: Failed to create chip: %s\n", desc->description);
        fflush(stdout);
        return NULL;
    }

    uint8_t chip_id = system_chip_register(&c64->system, chip, desc, addr, size);
    if (chip_id == 0xFF) {
        printf("ERROR: Failed to register chip: %s\n", desc->description);
        fflush(stdout);
        return NULL;
    }
    return chip;
}

// Callback function for CIA2 Port A changes - updates VIC-II bank
// Called whenever CIA2 Port A output changes (considering DDR masking)
//
// CIA2 Port A bits 0-1 are the VIC-II bank select lines:
//   Bit 0: ~VA14 (inverted VA14) - Video Address line 14
//   Bit 1: ~VA15 (inverted VA15) - Video Address line 15
//
// These bits select which 16K bank the VIC-II can access:
//   Bits 1-0:  Bank:  Address Range:
//   --------   ----   --------------
//      11        0    $0000-$3FFF
//      10        1    $4000-$7FFF
//      01        2    $8000-$BFFF
//      00        3    $C000-$FFFF
//
// Note: The VIC-II address lines VA14 and VA15 are INVERTED versions of these bits
// (active-low logic). When the port bits are HIGH, the corresponding VA lines are LOW.
static void c64_cia2_port_a_callback(void* context, uint8_t port_a_value) {
    c64_t* c64 = (c64_t*)context;
    
    // The VIC-II bank is determined by CIA2 Port A bits 0-1
    // These bits are INVERTED: 0b11 = Bank 0, 0b00 = Bank 3
    //
    // CRITICAL: The VIC sees the actual PORT VALUE (physical pins), not PRA register
    // - For OUTPUT bits (DDR=1): pin value = PRA bit value
    // - For INPUT bits (DDR=0): pin value = pulled high = 1
    //
    // The port_a_value parameter already represents the correct physical pin state
    uint8_t vic_bank_bits = port_a_value & 0x03;
    
    vicii_memory_bank_change(c64->vicii, vic_bank_bits);
}

// Callback function for MOS6510 I/O port changes - updates memory banking
// Called whenever CPU writes to I/O port $01 (banking bits)
// Must be extern "C" so it can be called from mos6510.cpp
extern "C" void c64_cpu_banking_callback(void* context, uint8_t banking_state) {
    c64_t* c64 = (c64_t*)context;
    
    // The banking_state contains bits 0-2: LORAM, HIRAM, CHAREN
    // These are already masked by the I/O port direction register
    // Call the existing banking change handler
    c64_bus_on_banking_change(&c64->bus, banking_state);
}

// Keyboard matrix scanning callbacks for CIA1
// CIA1 Port A is used to select keyboard columns (active LOW - writing 0 selects column)
// CIA1 Port B is used to read keyboard rows (reading 0 means key pressed in that row)
//
// Hardware: The C64 keyboard is an 8x8 matrix where:
// - CIA1 Port A outputs select columns (bits driven LOW select those columns)
// - CIA1 Port B inputs read rows (bits read as LOW indicate keys pressed in selected columns)
//
// When a key is pressed, it connects a row line to a column line.
// The CIA drives Port A lines LOW to select columns, and reads Port B to see which rows
// are pulled LOW by pressed keys in those columns.

static uint8_t c64_cia1_port_a_read_callback(void* context, uint8_t port_a_output) {
    c64_t* c64 = (c64_t*)context;
    // Port A is output-only for keyboard scanning, so just return the output value
    // No external device pulls these lines
    return port_a_output;
}

static uint8_t c64_cia1_port_b_read_callback(void* context, uint8_t port_b_output) {
    c64_t* c64 = (c64_t*)context;
    
    if (!c64 || !c64->keyboard || !c64->cia1) {
        // No keyboard connected - return pull-up value (all HIGH = no keys pressed)
        return 0xFF;
    }
    
    // Get the column selection from CIA1 Port A
    // Columns are selected by driving Port A lines LOW (active-low logic)
    // IMPORTANT: Read port_a_value safely - it's already set by the CIA before calling this callback
    uint8_t port_a_value = c64->cia1->port_a_value;
    uint8_t column_select = ~port_a_value;  // Invert to get active columns
    
    // Start with all rows HIGH (no keys pressed)
    uint8_t row_state = 0xFF;
    
    // For each selected column, check if any keys are pressed
    for (int col = 0; col < 8; col++) {
        if (column_select & (1 << col)) {
            // This column is selected - check for pressed keys
            // Pull down the row lines for any pressed keys in this column
            row_state &= c64->keyboard->row_open_contacts[col];
        }
    }
    
    // Return the row state (0 = key pressed, 1 = no key)
    return row_state;
}

void c64_system_destroy(c64_t* c64) {
    if (!c64) return;

    // Destroy keyboard if allocated
    if (c64->keyboard) {
        commodore_keyboard_destroy(c64->keyboard);
        c64->keyboard = NULL;
    }

    system_chips_destroy(&c64->system);
    free(c64);
}

c64_t* c64_system_create(const c64_config_t* config) {
    c64_t* c64 = static_cast<c64_t*>(calloc(1, sizeof(c64_t)));
    if (!c64) {
        printf("ERROR: Failed to allocate C64 system\n");
        return NULL;
    }

    // Initialize the legacy system wrapper first
    system_8bit_init(&c64->system);
    if (!c64->system.cpp_system) {
        printf("ERROR: Failed to initialize system\n");
        free(c64);
        return NULL;
    }

    chip_descriptor_t* vicii_descriptor = (config->vicii_standard == VIC_PAL ? &mos6569_descriptor : &mos6567_descriptor);

    // Initialize bus as embedded struct
    // NOTE: The bus is embedded in c64_t (not allocated separately), so we initialize it here
    // rather than calling c64_bus_system_create(). This is the ONLY initialization point.
    c64->bus.desc = &c64_bus_descriptor;
    c64->bus.c64 = c64;
    
    // Initialize bus state with pull-up resistors (centralized definition in c64_bus.h)
    c64->bus.default_state = C64_BUS_DEFAULT_STATE();
    c64->bus.state = c64->bus.default_state;
    
    // Initialize system lines with default cartridge signals (no cartridge)
    c64->bus.system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;

    // One line per chip - create, register, assign memory address/size, and assign to C64 field
    if (!(c64->ram = static_cast<ram_t*>(create_and_register_chip(c64, &ram_descriptor, 0x0000, 65536)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->mos6510 = create_and_register_chip(c64, &mos6510_descriptor, 0x0000, 4096))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->cartridge_roml = static_cast<rom_t*>(create_and_register_chip(c64, &rom_descriptor, 0x8000, 8192)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->basic = static_cast<rom_t*>(create_and_register_chip(c64, &rom_descriptor, 0xA000, 8192)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->cartridge_romh = static_cast<rom_t*>(create_and_register_chip(c64, &rom_descriptor, 0xC000, 8192)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->charrom = static_cast<rom_t*>(create_and_register_chip(c64, &rom_descriptor, 0xD000, 4096)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->vicii = static_cast<vicii_t*>(create_and_register_chip(c64, vicii_descriptor, 0xD000, 1024)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->sid = static_cast<mos6581_t*>(create_and_register_chip(c64, &mos6581_descriptor, 0xD400, 1024)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->colorram = static_cast<mos2114_t*>(create_and_register_chip(c64, &mos2114_descriptor, 0xD800, 1024)))) { c64_system_destroy(c64); return NULL; }
    c64->vicii->colorram = c64->colorram; // Also assign to VIC-II for compatibility
    
    // VIC-II bank selection is handled internally via bank_base offset
    // No bus callback needed - PLA pre-calculation covers all #VA14 states
    c64->vicii->bus.bus = &c64->bus;
    c64->vicii->bus.bank_change = NULL;
    
    if (!(c64->cia1 = static_cast<mos6526_t*>(create_and_register_chip(c64, &mos6526_descriptor, 0xDC00, 256)))) { c64_system_destroy(c64); return NULL; }
    if (!(c64->cia2 = static_cast<mos6526_t*>(create_and_register_chip(c64, &mos6526_descriptor, 0xDD00, 256)))) { c64_system_destroy(c64); return NULL; }
    
    // Create keyboard and initialize with no keys pressed
    c64->keyboard = commodore_keyboard_create();
    if (!c64->keyboard) {
        printf("ERROR: Failed to create keyboard\n");
        c64_system_destroy(c64);
        return NULL;
    }
    commodore_keyboard_reset(c64->keyboard);
    printf("C64 System: Keyboard matrix initialized (all keys released)\n");
    if (!(c64->kernal = static_cast<rom_t*>(create_and_register_chip(c64, &rom_descriptor, 0xE000, 8192)))) { c64_system_destroy(c64); return NULL; }

    // Initialize placeholders for missing components
    c64->io1 = NULL; // No cartridge I/O by default
    c64->io2 = NULL; // No cartridge I/O by default

    // Register PLA for GUI debugging (special case - chip is the C64 system itself)
    uint8_t pla_chip_id = system_chip_register(&c64->system, c64, &pla_descriptor, 0x0000, 0);
    if (pla_chip_id == 0xFF) { c64_system_destroy(c64); return NULL; }

    // Now having a registry of all chips, the PLA maps can be generated
    if (!c64_pla_maps_generate(c64)) { c64_system_destroy(c64); return NULL; }
    
    // DEBUG: Print VIC-II memory mapping for bank 0 (addresses 0x0000-0x0FFF)
    printf("VIC-II memory mapping for mode 0x07:\n");
    for (int bank = 0; bank < 16; bank++) {
        uint8_t chip = c64->bus.vicii_chip_per_bank[bank];
        printf("  Bank %d (0x%04X-0x%04X): CHIP=%d (%s)\n",
               bank, bank * 0x1000, (bank + 1) * 0x1000 - 1,
               chip, c64_chips_to_title(chip));
    }

    // Attach bus to C64 system first to initialize unified memory pointers
    c64_bus_system_attach(&(c64->bus), c64);

    // Set default memory contents and load ROMs from configured paths
    c64_memory_init(&c64->system, config);
    
    // Re-initialize unified pointers after ROM loading to copy loaded ROM data into unified buffer
    c64_bus_init_unified_pointers(&c64->bus, c64, config);
    
    // Hardware: CIA1 Port A/B are used for keyboard matrix scanning
    // Register read callbacks with CIA1 for keyboard input
    c64->cia1->port_a_read_callback = c64_cia1_port_a_read_callback;
    c64->cia1->port_a_read_context = c64;
    c64->cia1->port_b_read_callback = c64_cia1_port_b_read_callback;
    c64->cia1->port_b_read_context = c64;
    printf("C64 System: Registered CIA1 Port A/B callbacks for keyboard matrix scanning\n");
    
    // Hardware: CIA2 Data Port A bits 0-1 control VIC-II memory bank selection
    // Register callback with CIA2 to receive Port A change notifications
    // This callback will update VIC-II bank when CIA2 Port A output changes
    c64->cia2->port_a_change_callback = c64_cia2_port_a_callback;
    c64->cia2->port_a_callback_context = c64;
    printf("C64 System: Registered CIA2 Port A callback for VIC-II bank switching\n");
    
    // Manually invoke callback with current CIA2 Port A value to set initial VIC-II bank
    // (callback wasn't available during mos6526_reset, so we call it now)
    c64_cia2_port_a_callback(c64, c64->cia2->port_a_value);
    
    // Hardware: CPU I/O port bits 0-2 control memory banking (LORAM, HIRAM, CHAREN)
    // Assign the C64 banking callback directly to the MOS6510 descriptor
    // This is where both the descriptor and the callback are in scope
    mos6510_descriptor.bank_change = c64_cpu_banking_callback;
    printf("C64 System: Assigned MOS6510 descriptor bank_change callback\n");
    
    // NOTE: Initial banking mode is already set correctly at line 226 (mode $17: I/O enabled)
    // The CPU I/O port initializes to $37 (DDR=$2F) which gives banking bits = $07
    // This matches the PLA mode $17 already set (with EXROM=1, GAME=1 from system_lines)
    // The banking callback will be triggered when the CPU writes to $0001 during boot
    uint8_t initial_banking_bits = mos6510_get_io_data((mos6510_t*)c64->mos6510) &
                                    mos6510_get_io_ddr((mos6510_t*)c64->mos6510) & 0x07;
    printf("C64 System: Initial CPU I/O port: DATA=$%02X DDR=$%02X (banking bits=$%02X) → mode=$17\n",
           mos6510_get_io_data((mos6510_t*)c64->mos6510),
           mos6510_get_io_ddr((mos6510_t*)c64->mos6510),
           initial_banking_bits);

    // Set CIA2 interrupt line to NMI (CIA1 defaults to IRQ in constructor)
    ((mos6526_t*)c64->cia2)->interrupt_line = BUS_MASK_NMI;

    // After ROMs are loaded, read the reset vector and initialize CPU for immediate execution
    uint16_t reset_vector = c64_read_kernal_reset_vector(&c64->bus);
    mos6510_set_pc((mos6510_t*)c64->mos6510, reset_vector);
    
    // CRITICAL: Also set the address bus register to match PC
    // Without this, the first instruction fetch reads from address $0000 instead of the reset vector
    // The CPU's AB register must match PC for the first fetch to work correctly
    mos6510_set_ab((mos6510_t*)c64->mos6510, reset_vector);

    printf("C64 System: Loaded reset vector $%04X from KERNAL ROM and initialized CPU PC and AB\n", reset_vector);

    // Attach all other chips with bus_attach callbacks
    for (int i = 0; i < c64->system.chip_count; i++) {
        chip_entry_t* chip = &c64->system.chips[i];
        if (chip->desc && chip->desc->bus_attach && chip->desc != &c64_bus_descriptor) {
            chip->desc->bus_attach(chip->chip, &(c64->bus));
        }
    }

    // Initialize bus state with reset released (HIGH = inactive for active-low reset)
    printf("C64 System: Reset complete, ready to run\n");

    return c64;
}

// Set the framebuffer for VIC-II pixel output
void c64_set_framebuffer(c64_t* c64, uint32_t* framebuffer, int width, int height) {
    if (!c64 || !c64->vicii || !framebuffer) return;

    // Set the framebuffer on the VIC-II chip
    vicii_set_framebuffer(c64->vicii, framebuffer, width, height);

}

bool c64_reload_roms(c64_t* c64, const rom_config_t* rom_config) {
    if (!c64 || !rom_config) {
        return false;
    }

    // Create a temporary config with just the ROM config for reloading
    c64_config_t reload_config;
    c64_config_init_defaults(&reload_config);
    reload_config.rom_config = (rom_config_t*)rom_config;
    reload_config.test_mode = C64_TEST_MODE_NORMAL;  // Always use normal mode for ROM reload

    // Reload ROMs using the memory initialization function
    c64_memory_init(&c64->system, &reload_config);

    printf("ROMs reloaded successfully\n");
    return true;
}
