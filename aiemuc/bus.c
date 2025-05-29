#include "bus.h"
#include "c64.h"
#include <string.h>

// Global bus state
bus_state_t bus;

// ============================================================================
// PLA EMULATION - Pre-computed callback maps for each memory mode
// ============================================================================

// 32 possible PLA modes, 256 memory blocks each (256-byte granularity for CIA compatibility)
device_callbacks_t chip_select_maps[32][256];
device_callbacks_t* chip_select_map; // Current active map

// Switch CPU mode
void switch_cpu_mode(uint8_t mode) {
    mode &= 0x1F; // Mask to 5 bits (0-31) for safety
    chip_select_map = chip_select_maps[mode];
}

// Callback for each bus cycle (can be set by test harness)
void (*bus_cycle_callback)(void) = NULL;

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe device lifecycle management and callback dispatch
// ============================================================================
// Static pointer to current C64 instance for bus operations
static c64_state_t* current_c64 = NULL;

void bus_cycle(void) {
    bus.total_cycles++;
    
    // All chips always run for cycle accuracy - using safe device callers
    if (current_c64) {
        device_cycle((struct device_s*)&current_c64->vic);
        device_cycle((struct device_s*)&current_c64->cia1);
        device_cycle((struct device_s*)&current_c64->cia2);
        device_cycle((struct device_s*)&current_c64->sid);
    }

    // Update RDY line based on BA (hardware accurate)
    if (bus.control_lines & BA_LINE) {
        bus.control_lines |= RDY_LINE;
    } else {
        bus.control_lines &= ~RDY_LINE;
    }
    // Call the callback if set
    if (bus_cycle_callback) bus_cycle_callback();
}

// Optimized read cycle implementation - direct callback dispatch
void cpu_read_cycle(uint16_t addr) {
    bus.address = addr;
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    bus.data = cb->read(cb->read_device, addr);
    bus_cycle();
}

// Optimized write cycle implementation - direct callback dispatch
void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus.address = addr;
    bus.data = value;
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    cb->write(cb->write_device, addr, value);
    bus_cycle();
}


// Helper function to get callbacks based on address and mode bits
static device_callbacks_t get_device_callbacks_for_address(c64_state_t* c64, uint16_t addr, bool loram, bool hiram, bool charen, bool game) {
    device_callbacks_t callbacks;
    
    // Start with RAM as default (most common case)
    callbacks.read_device = (struct device_s*)&(c64->ram);
    callbacks.write_device = (struct device_s*)&(c64->ram);
    
    // $0000-$0100: CPU I/O ports ($0x0002 and up forward to RAM)
    if (addr < 0x0100) {
        // CPU port uses CPU device which forwards to RAM internally
        // The CPU device is accessed from the C64 system, not as global
        callbacks.read_device = (struct device_s*)&(c64->cpu);
        callbacks.write_device = (struct device_s*)&(c64->cpu);
    }
    // $0100-$9FFF: Always RAM (already set as default)
    // $A000-$BFFF: BASIC ROM area
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (loram && !game) {
            callbacks.read_device = (struct device_s*)&(c64->basic_rom);
            // BASIC ROM writes fall through to RAM (write_device stays RAM from default)
        }
        // else: stays RAM (default)
    }
    // $D000-$DFFF: I/O or Character ROM area
    else if (addr >= 0xD000 && addr < 0xE000) {
        if (charen) {
            // I/O area - determine specific device by address
            if (addr < 0xD400) {
                // $D000-$D3FF: VIC I/O
                callbacks.read_device = (struct device_s*)&(c64->vic);
                callbacks.write_device = (struct device_s*)&(c64->vic);
            } else if (addr < 0xD800) {
                // $D400-$D7FF: SID I/O
                callbacks.read_device = (struct device_s*)&(c64->sid);
                callbacks.write_device = (struct device_s*)&(c64->sid);
            } else if (addr < 0xDC00) {
                // $D800-$DBFF: Color RAM - stays RAM (default)
            } else if (addr < 0xDD00) {
                // $DC00-$DCFF: CIA 1 I/O
                callbacks.read_device = (struct device_s*)&(c64->cia1);
                callbacks.write_device = (struct device_s*)&(c64->cia1);
            } else if (addr < 0xDE00) {
                // $DD00-$DDFF: CIA 2 I/O
                callbacks.read_device = (struct device_s*)&(c64->cia2);
                callbacks.write_device = (struct device_s*)&(c64->cia2);
            } else {
                // $DE00-$DFFF: I/O expansion - no devices implemented
                callbacks.read_device = NULL; // No device for unmapped areas
                callbacks.write_device = NULL;
            }
        } else {
            // Character ROM
            callbacks.read_device = (struct device_s*)&(c64->char_rom);
            // CHAR ROM writes fall through to RAM (write_device stays RAM from default)
        }
    }
    // $E000-$FFFF: KERNAL ROM area
    else if (addr >= 0xE000) {
        if (hiram && !game) {
            callbacks.read_device = (struct device_s*)&(c64->kernal_rom);
            // KERNEL ROM writes fall through to RAM (write_device stays RAM from default)
        }
        // else: stays RAM (default)
    }
    
    // Extract function pointers from devices (after all device pointers are set)
    callbacks.read = get_device_read_callback(callbacks.read_device);
    callbacks.write = get_device_write_callback(callbacks.write_device);
    
    return callbacks;
}

// Generate PLA maps with direct callback assignment based on address and mode bits
void generate_pla_maps(c64_state_t* c64) {
    for (int mode = 0; mode < 32; ++mode) { // As written to ram[0x0001]
        bool loram = mode & 1, hiram = mode & 2, charen = mode & 4;
        bool game  = mode & 8;
        (void)(mode & 16); // exrom - reserved for future cartridge support

        for (int block = 0; block < 256; block++) {
            uint16_t addr = block << 8;  // 256-byte blocks
            
            // Directly assign callbacks based on address and mode bits
            chip_select_maps[mode][block] = get_device_callbacks_for_address(c64, addr, loram, hiram, charen, game);
        }
    }
}

void bus_init(c64_state_t* c64) {
    // Set the current C64 instance for bus operations
    current_c64 = c64;
    
    // Initialize bus state
    bus.address = 0;
    bus.data = 0;
    bus.control_lines = BA_LINE | AEC_LINE | RDY_LINE;
    bus.total_cycles = 0;

    // Generate PLA maps with integrated device callback assignment
    generate_pla_maps(c64);
}