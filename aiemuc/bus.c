#include "bus.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"
#include "cpu6510.h"
#include <string.h>

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

uintptr_t mode_read_map[32][256];  // 32 configs × 256 256-byte blocks
uintptr_t* read_map; // points to mode_read_map[mode]

// External CPU state
extern cpu6510_state_t cpu;

// Switch CPU mode
void switch_cpu_mode(uint8_t mode) {
    chip_select_map = chip_select_maps[mode & 0x1F]; // TODO : no mask needed when mode argument is guaranteed to be between 0 and 31
    read_map = mode_read_map[mode];
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
}

// Read cycle implementation
void cpu_read_cycle(uint16_t addr) {
    bus_state.address = addr;
    bus_state.control_lines |= READ_CYCLE;
    bus_state.control_lines &= ~WRITE_CYCLE;
    update_chip_selects(addr);    
    // Read from the appropriate memory map
    //uint16_t addr = bus_state.address;
    uintptr_t entry = read_map[addr >> 8];
    // Extract base pointer (256-byte aligned, so lower 8 bits are available for mask)
    uint8_t* base = (uint8_t*)(entry & ~0xFF);
    // Extract mask directly from LSB bits (no shifting needed)
    uint8_t mask = (uint8_t)(entry & 0xFF);
    bus_state.data = base[addr & mask];
    // TODO : Are all clear-after-read registers implemented in the various devices? What about ram[0x0001]?
    // TODO : Handle address reads via device cycle handler (using chip select flag)
    bus_cycle();
}

// Write cycle implementation
void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus_state.address = addr;
    bus_state.data = value;
    bus_state.control_lines |= WRITE_CYCLE;
    bus_state.control_lines &= ~READ_CYCLE;
    update_chip_selects(addr);

    // Handle the CPU port address writes
    if (addr == 0x001)
    {
        uint8_t direction = ram[0x0000]; // Data Direction Register (DDR at $0000). 1 = set, 0 = read&clear
        uint8_t io_mask = ram[0x0001]; // I/O Port Data (at $0001)

        io_mask &= !direction; // clear the mask bits that will be overwritten
        io_mask |= direction & value; // set the appropriate bits from value

        ram[0x0001] = io_mask; // Write to I/O Port Data (at $0001)
        switch_cpu_mode(io_mask); // Apply the new mode to the PLA
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

            if (addr >= 0xA000 && addr < 0xC000) cs = (loram && !game) ? ROM_CS : RAM_CS;
            else if (addr >= 0xD000 && addr < 0xE000) cs = charen ? IO_CS : CHAR_ROM_CS;
            else if (addr >= 0xE000) cs = (hiram && !game) ? ROM_CS : RAM_CS;

            chip_select_maps[mode][block] = cs;

            uint8_t* read_base;
            uint8_t read_mask = 0xFF;  // Default 256-byte mask

            if (cs & IO_CS) {
                if (addr < 0xD400) {
                    read_base = vic.registers;
                    read_mask = 0x3F;  // VIC: 64 registers (6 bits)
                } else if (addr < 0xD800) {
                    read_base = sid.registers;
                    read_mask = 0x1F;  // SID: 32 registers (5 bits)
                } else if (addr < 0xDD00) {
                    read_base = cia1.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else if (addr < 0xDE00) {
                    read_base = cia2.registers;
                    read_mask = 0x0F;  // CIA: 16 registers (4 bits)
                } else {
                    read_base = ram + addr;
                }
            } else if (cs & ROM_CS) {
                if (addr >= 0xE000)
                    read_base = &kernal_rom[addr - 0xE000];
                else
                    read_base = &basic_rom[addr - 0xA000];
            } else if (cs & CHAR_ROM_CS) {
                read_base = &char_rom[addr - 0xD000];
            } else {
                read_base = &ram[addr];
            }

            // Store mask directly in LSB bits (256-byte aligned pointers have 8 zero LSBs)
            mode_read_map[mode][block] = (uintptr_t)read_base | read_mask;
        }
    }
}