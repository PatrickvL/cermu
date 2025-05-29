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
// DEVICE INSTANCES - Centralized device state management
// ============================================================================
cpu6510_state_t cpu;
ram_state_t ram;
rom_state_t basic_rom;
rom_state_t kernal_rom;
rom_state_t char_rom;
vic_state_t vic;
cia_state_t cia1, cia2;
sid_state_t sid;

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
    // Initialize all devices using generic device function callers
    device_init((struct device_s*)&ram);
    device_init((struct device_s*)&vic);
    device_init((struct device_s*)&cia1);
    device_init((struct device_s*)&cia2);
    device_init((struct device_s*)&sid);
    device_init((struct device_s*)&cpu);
    
    // ROM devices need special setup parameters before device_init
    rom_setup(&basic_rom, 8192, 0xA000);    // Basic ROM: 8K at $A000-$BFFF
    device_init((struct device_s*)&basic_rom);
    rom_setup(&kernal_rom, 8192, 0xE000);   // Kernal ROM: 8K at $E000-$FFFF
    device_init((struct device_s*)&kernal_rom);
    rom_setup(&char_rom, 4096, 0xD000);     // Character ROM: 4K at $D000-$DFFF
    device_init((struct device_s*)&char_rom);
    
    // Initialize bus system (generates PLA maps internally)
    bus_init();
    
    // Load ROM images (external function)
    // load_roms(kernal_rom, basic_rom, char_rom);
}

