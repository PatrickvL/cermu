#include "bus.h"
#include "cpu6510.h"
#include "ram.h"
#include "rom.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"
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
    mode &= 0x1F; // Mask to 5 bits (0-31) // TODO : no mask needed when mode argument is guaranteed to be between 0 and 31
    chip_select_map = chip_select_maps[mode];
}

// Callback for each bus cycle (can be set by test harness)
void (*bus_cycle_callback)(void) = NULL;

// ============================================================================
// OPTIMIZED BUS CYCLE - Direct callback dispatch, no chip select checks
// ============================================================================
void bus_cycle(void) {
    bus.total_cycles++;
    
    // All chips always run for cycle accuracy - no conditionals for performance
    vic_cycle();       // Video timing, BA control, sprites
    cia1_cycle();      // Timers, keyboard, joystick
    cia2_cycle();      // Timers, serial, user port
    sid_cycle();       // Sound generation, envelope generators

    // Update RDY line based on BA (hardware accurate)
    if (bus.control_lines & BA_LINE) {
        bus.control_lines |= RDY_LINE;
    } else {
        bus.control_lines &= ~RDY_LINE;
    }
    // Call the callback if set
    if (bus_cycle_callback) bus_cycle_callback();
}

// Optimized read cycle implementation - preselected callback dispatch
void cpu_read_cycle(uint16_t addr) {
    bus.address = addr;
    // Ultra-fast address decoding with direct callback selection
    chip_select_map[addr >> 8].write();
    bus_cycle();
}

// Optimized write cycle implementation - preselected callback dispatch
void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus.address = addr;
    bus.data = value;
    // Ultra-fast address decoding with direct callback selection
    chip_select_map[addr >> 8].write();
    bus_cycle();
}

// Default no-op handlers for unmapped areas
static void nop_read_handler(void) {
    bus.data = 0xFF; // Default read value for unmapped areas
}

static void nop_write_handler(void) {
    // Writes to unmapped areas are ignored
}

// Single device callback table - indexed by chip select value
device_callbacks_t device_callbacks[1024];

// Chip select bit definitions
#define RAM_CS      (1 << 0)
#define CPU_CS      (1 << 1) // CPU port handling (0x0000/0x0001)
#define KERNEL_CS   (1 << 2) 
#define BASIC_CS    (1 << 3) 
#define CHAR_CS     (1 << 4)
#define IO_CS       (1 << 5)
#define VIC_CS      (1 << 6)
#define SID_CS      (1 << 7)
#define CIA1_CS     (1 << 8)
#define CIA2_CS     (1 << 9)

void initialize_device_callbacks()
{
    // Set up device-specific handlers for each chip select combination
    // Priority order: I/O devices > ROM > RAM (highest to lowest priority)
    for (int cs = 0; cs < 1024; cs++) {
        // Start with default handlers
        device_callbacks[cs].read = nop_read_handler;
        device_callbacks[cs].write = nop_write_handler;
        
        // Check in priority order - last match wins
        if (cs & RAM_CS) {
            device_callbacks[cs].read = ram_read_handler;
            device_callbacks[cs].write = ram_write_handler;
        }
        if (cs & CPU_CS) { // CPU port handling (0x0000/0x0001)
            device_callbacks[cs].read = cpu_read_handler;
            device_callbacks[cs].write = cpu_write_handler;
        }
        if (cs & BASIC_CS) {
            device_callbacks[cs].read = basic_read_handler;
            // ROM writes remain no-op
        }
        if (cs & KERNEL_CS) {
            device_callbacks[cs].read = kernel_read_handler;
            // ROM writes remain no-op
        }
        if (cs & CHAR_CS) {
            device_callbacks[cs].read = char_rom_read_handler;
            // CHAR ROM writes remain no-op
        }
        
        // I/O devices have highest priority
        if (cs & VIC_CS) {
            device_callbacks[cs].read = vic_handle_read;
            device_callbacks[cs].write = vic_handle_write;
        }
        if (cs & SID_CS) {
            device_callbacks[cs].read = sid_handle_read;
            device_callbacks[cs].write = sid_handle_write;
        }
        if (cs & CIA1_CS) {
            device_callbacks[cs].read = cia1_handle_read;
            device_callbacks[cs].write = cia1_handle_write;
        }
        if (cs & CIA2_CS) {
            device_callbacks[cs].read = cia2_handle_read;
            device_callbacks[cs].write = cia2_handle_write;
        }
    }
}

// Generate PLA maps with direct callback assignment
void generate_pla_maps(void) {
    for (int mode = 0; mode < 32; ++mode) { // As written to ram[0x0001]
        bool loram = mode & 1, hiram = mode & 2, charen = mode & 4;
        bool game  = mode & 8;
        (void)(mode & 16); // exrom - reserved for future cartridge support

        for (int block = 0; block < 256; block++) {
            uint16_t addr = block << 8;  // 256-byte blocks
            uint16_t cs = RAM_CS;

            // NOTE : CART ROM LO will start at 0x8000!
            if (addr >= 0xA000 && addr < 0xC000) cs = (loram && !game) ? BASIC_CS : RAM_CS;
            else if (addr >= 0xD000 && addr < 0xE000) cs = charen ? IO_CS : CHAR_CS;
            else if (addr >= 0xE000) cs = (hiram && !game) ? KERNEL_CS : RAM_CS;

            // CPU I/O port
            if ((cs & RAM_CS) && (addr < 0x0100))
                cs = CPU_CS;

            // Handle I/O area chip selects
            if (cs & IO_CS) {
                if (addr < 0xD400) {
                    cs = VIC_CS;  // VIC I/O
                } else if (addr < 0xD800) {
                    cs = SID_CS;  // SID I/O
                } else if (addr < 0xDC00) {
                    // TODO : Add support for 4-bit color ram up to DBE7 (DBE8 to DBFF unused)
                } else if (addr < 0xDD00) {
                    cs = CIA1_CS;  // CIA 1 I/O
                } else if (addr < 0xDE00) {
                    cs = CIA2_CS;  // CIA 2 I/O
                } else if (addr < 0xDF00) {
                    // TODO : Add support for DE00 to DEFF (I/O 1)
                } else {
                    // TODO : Add support for DE00 to DEFF (I/O 1) and DF00 to DFFF (I/O 2)
                }
            }

            // Directly assign callbacks based on chip select priority
            chip_select_maps[mode][block] = device_callbacks[cs];
        }
    }
}

void bus_init(void) {
    // Initialize bus state
    bus.address = 0;
    bus.data = 0;
    bus.control_lines = BA_LINE | AEC_LINE | RDY_LINE;
    bus.total_cycles = 0;

    initialize_device_callbacks();
    // Generate PLA maps after device callbacks are initialized
    generate_pla_maps();
}