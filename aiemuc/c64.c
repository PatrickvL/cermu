#include "c64.h"
#include "bus.h"

// ============================================================================
// MAIN EMULATION LOOP
// ============================================================================
void c64_emulate_frame(c64_state_t* c64) {
    // Set initial PLA mode (all RAM/ROM enabled)
    switch_cpu_mode(0x07); // LORAM=1, HIRAM=1, CHAREN=1

    // Initialize bus state
    bus_init(c64);

    // Start execution (commented out to avoid infinite loop in testing)
    // cpu6510_execute();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void c64_init(c64_state_t* c64) {
    // Initialize C64 state structure devices
    device_init((struct device_s*)&(c64->ram));
    device_init((struct device_s*)&(c64->vic));
    device_init((struct device_s*)&(c64->cia1));
    device_init((struct device_s*)&(c64->cia2));
    device_init((struct device_s*)&(c64->sid));
    device_init((struct device_s*)&(c64->cpu));
    
    // Attach RAM to CPU so it can access it directly
    cpu_attach_ram(&c64->cpu, &c64->ram);
    
    // Attach bus to CPU so it can access bus state directly
    cpu_attach_bus(&c64->cpu, &bus);
    
    // ROM devices need special setup parameters before device_init
    rom_setup(&(c64->basic_rom), 8192, 0xA000);    // Basic ROM: 8K at $A000-$BFFF
    device_init((struct device_s*)&(c64->basic_rom));
    rom_setup(&(c64->kernal_rom), 8192, 0xE000);   // Kernal ROM: 8K at $E000-$FFFF
    device_init((struct device_s*)&(c64->kernal_rom));
    rom_setup(&(c64->char_rom), 4096, 0xD000);     // Character ROM: 4K at $D000-$DFFF
    device_init((struct device_s*)&(c64->char_rom));
    
    // Initialize bus system (generates PLA maps internally)
    bus_init(c64);
    
    // Load ROM images (external function)
    // load_roms(kernal_rom, basic_rom, char_rom);
}

