#include "c64.h"
#include "bus.h"
#include "ram.h"
#include "rom.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"
#include "cpu6510.h"
#include <string.h>

// ============================================================================
// MAIN EMULATION LOOP
// ============================================================================
void c64_emulate_frame(void) {
    // Set initial PLA mode (all RAM/ROM enabled)
    switch_cpu_mode(0x07); // LORAM=1, HIRAM=1, CHAREN=1
    
    // Initialize bus state
    bus_init();
    
    // Start execution (commented out to avoid infinite loop in testing)
    // cpu6510_execute();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void c64_init(void) {
    // Initialize all devices (each device initializes its own state and callbacks)
    ram_init();
    rom_init();
    vic_init();
    cia1_init();
    cia2_init();
    sid_init();
    
    // Initialize bus system (generates PLA maps internally)
    bus_init();

    // Initialize CPU state
    cpu6510_init();
    
    // Load ROM images (external function)
    // load_roms(kernal_rom, basic_rom, char_rom);
}
