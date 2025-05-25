#include "c64.h"
#include "bus.h"
#include "memory.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"
#include "cpu.h"
#include <string.h>

// ============================================================================
// MAIN EMULATION LOOP
// ============================================================================
void c64_emulate_frame(void) {
    // Set initial PLA mode (all RAM/ROM enabled)
    chip_select_map = chip_select_maps[0x07]; // LORAM=1, HIRAM=1, CHAREN=1
    
    // Initialize bus state
    bus_state.raw = 0;
    bus_state.bus_control = BA_LINE | AEC_LINE | RDY_LINE;
    
    // Start execution
    cpu_execute();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void c64_init(void) {
    // Generate all PLA memory maps
    generate_pla_maps();
    
    // Initialize CPU state
    cpu_pc = 0xFCE2; // RESET vector
    cpu_a = 0;
    cpu_sp = 0xFF;
    cpu_p = 0x04; // Interrupt disable flag set
    
    // Initialize chip states
    memset(&vic, 0, sizeof(vic));
    memset(&cia1, 0, sizeof(cia1));
    memset(&cia2, 0, sizeof(cia2));
    memset(&sid, 0, sizeof(sid));
    
    // Load ROM images (external function)
    // load_roms(kernal_rom, basic_rom, char_rom);
}

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    c64_init();
    // c64_emulate_frame(); // Would run indefinitely, so commented out for testing
    return 0;
}