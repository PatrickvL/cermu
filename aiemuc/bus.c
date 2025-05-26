#include "bus.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"
#include "cpu6510.h"
#include <string.h>

// NOTE: If all device base addresses (RAM, ROM, IO, etc.) are guaranteed to be 256-byte aligned,
// the 'mask' member can be merged into the low 8 bits of the 'base' pointer, since those bits
// will always be zero. This allows storing both the base pointer and the mask in a single uintptr_t,
// using (base | mask). To extract them:
//   - base = (void*)((entry & ~0xFF));   // clear low 8 bits for base
//   - mask = (uint8_t)(entry & 0xFF);   // low 8 bits for mask
// This optimization is only safe if all mapped regions are aligned to 256 bytes.
typedef struct {
    uint8_t* base;
    uint8_t mask;
} pla_map_entry_t;

// Global bus state
bus_state_t bus_state;

// ============================================================================
// MEMORY - 64K RAM + ROM images
// ============================================================================
uint8_t ram[65536];
uint8_t kernal_rom[8192];
uint8_t basic_rom[8192];
uint8_t char_rom[4096];

// ============================================================================
// PLA EMULATION - Pre-computed chip select maps for each memory mode
// ============================================================================

// 32 possible PLA modes, 256 memory blocks each (256-byte granularity for CIA compatibility)
uint8_t chip_select_maps[32][256];
uint8_t* chip_select_map; // Current active map

pla_map_entry_t mode_read_map[32][256];  // 32 configs × 256 256-byte blocks
pla_map_entry_t* read_map; // points to mode_read_map[mode]

// External CPU state
extern cpu6510_state_t cpu;

// Switch CPU mode
void switch_cpu_mode(uint8_t mode) {
    mode &= 0x1F; // Mask to 5 bits (0-31) // TODO : no mask needed when mode argument is guaranteed to be between 0 and 31
    chip_select_map = chip_select_maps[mode];
    read_map = &mode_read_map[mode][0];
}

// Ultra-fast address decoding - single memory access
static inline void update_chip_selects(uint16_t addr) {
    bus_state.chip_selects = chip_select_map[addr >> 8];
}

void bus_init(void) {
    // Initialize bus
    bus_state.raw = 0;
    bus_state.bus_control = BA_LINE | AEC_LINE | RDY_LINE;
}

// Callback for each bus cycle (can be set by test harness)
void (*bus_cycle_callback)(void) = NULL;

// ============================================================================
// UNIFIED BUS CYCLE - All chips react to bus state simultaneously
// ============================================================================
void bus_cycle(void) {
    cpu.total_cycles++;
    // All chips always run for cycle accuracy - no conditionals for performance
    vic_cycle();    // Video timing, BA control, sprites
    cia1_cycle();   // Timers, keyboard, joystick
    cia2_cycle();   // Timers, serial, user port
    sid_cycle();    // Sound generation, envelope generators

    // Update RDY line based on BA (hardware accurate)
    if (bus_state.bus_control & BA_LINE) {
        bus_state.bus_control |= RDY_LINE;
    } else {
        bus_state.bus_control &= ~RDY_LINE;
    }
    // Call the callback if set
    if (bus_cycle_callback) bus_cycle_callback();
}

// Read cycle implementation
void cpu_read_cycle(uint16_t addr) {
    bus_state.address = addr;
    bus_state.control_lines |= READ_CYCLE;
    bus_state.control_lines &= ~WRITE_CYCLE;
    update_chip_selects(addr);    
    pla_map_entry_t entry = read_map[addr >> 8];
    bus_state.data = entry.base[addr & entry.mask];
    bus_cycle();
}

// Write cycle implementation
void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus_state.address = addr;
    bus_state.data = value;
    bus_state.control_lines |= WRITE_CYCLE;
    bus_state.control_lines &= ~READ_CYCLE;
    update_chip_selects(addr);

    // Determine if this address is writeable (RAM or IO only)
    uint8_t cs = chip_select_map[addr >> 8];
    // Only allow writes to RAM or IO, never to ROM or CHAR ROM
    if ((cs & RAM_CS) != 0) {
        // Handle the CPU port address writes
        if (addr == 0x001) {
            uint8_t direction = ram[0x0000]; // Data Direction Register (DDR at $0000). 1 = set, 0 = read&clear
            uint8_t io_mask = ram[0x0001]; // I/O Port Data (at $0001)

            io_mask &= !direction; // clear the mask bits that will be overwritten
            io_mask |= direction & value; // set the appropriate bits from value
            switch_cpu_mode(io_mask); // Apply the new mode to the PLA
            value = io_mask; // Fall through and write the adjusted value to I/O Port Data (at $0001)
        }

        // Normal RAM writes
        ram[addr] = value;
    }
    // All other address writes are handled by device cycle handlers via chip selects
    bus_cycle();
}

// Generate PLA maps (stub for test)
void generate_pla_maps(void) {
    for (int mode = 0; mode < 32; ++mode) { // As written to ram[0x0001]
        bool loram = mode & 1, hiram = mode & 2, charen = mode & 4;
        bool game  = mode & 8;
        (void)(mode & 16); // exrom - reserved for future cartridge support

        for (int block = 0; block < 256; block++) {
            uint16_t addr = block << 8;  // 256-byte blocks
            uint8_t cs = RAM_CS;

            // NOTE : CART ROM LO will start at 0x8000!
            if (addr >= 0xA000 && addr < 0xC000) cs = (loram && !game) ? ROM_CS : RAM_CS;
            else if (addr >= 0xD000 && addr < 0xE000) cs = charen ? IO_CS : CHAR_ROM_CS;
            else if (addr >= 0xE000) cs = (hiram && !game) ? ROM_CS : RAM_CS;

            uint8_t* read_base = &ram[addr]; // Default for RAM_CS (might get overwritten below)
            uint8_t read_mask = 0xFF;  // Default 256-byte mask

            if (cs & CHAR_ROM_CS) {
                read_base = &char_rom[addr - 0xD000];
            } else if (cs & ROM_CS) {
                if (addr >= 0xE000)
                    read_base = &kernal_rom[addr - 0xE000];
                else
                    read_base = &basic_rom[addr - 0xA000];
            } else if (cs & IO_CS) {
                // TODO : Once each block has a single chip select, IO_CS can be aliassed to any
                if (addr < 0xD400) {
                    cs = VIC_CS;  // VIC I/O
                    read_base = vic.registers;
                    read_mask = 0x3F;  // VIC: 64 registers (6 bits)
                } else if (addr < 0xD800) {
                    cs = SID_CS;  // SID I/O
                    read_base = sid.registers;
                    read_mask = 0x1F;  // SID: 32 registers (5 bits)
                } else if (addr < 0xDC00) {
                    // TODO : Add support for 4-bit color ram up to DBE7 (DBE8 to DBFF unused)
                } else if (addr < 0xDD00) {
                    cs = CIA1_CS;  // CIA 1 I/O
                    read_base = cia1.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else if (addr < 0xDE00) {
                    cs = CIA2_CS;  // CIA 2 I/O
                    read_base = cia2.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else if (addr < 0xDF00) {
                    // TODO : Add support for DE00 to DEFF (I/O 1)
                } else {
                    // TODO : Add support for DE00 to DEFF (I/O 1) and DF00 to DFFF (I/O 2)
                }
            }

            chip_select_maps[mode][block] = cs;
            mode_read_map[mode][block].base = read_base;
            mode_read_map[mode][block].mask = read_mask;
        }
    }
}