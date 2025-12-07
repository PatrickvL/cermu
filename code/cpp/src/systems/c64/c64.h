#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include "../../core/aiemuc.h"
#include "../../core/system.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h"
#include "../../chip/sound/mos6581.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/video/vic_ii/mos6569.h"
#include "../../chip/video/vic_ii/mos6567.h"
#include "c64_bus.h"  // Include the bus header to get c64_bus_t definition
#include "c64_config.h"

// Forward declarations
class System8Bit;

// Modern C++ C64 system structure
struct C64System {
    system_8bit_t system;       // Legacy system wrapper
    c64_bus_t bus;
    void* mos6510;              // MOS6510 instance (C++ core)
    ram_t* ram;                 // RAM memory $0000-$FFFF (64KB)
    rom_t* cartridge_roml;      // Cartridge ROM Low $8000-$9FFF (8KB)
    rom_t* cartridge_romh;      // Cartridge ROM High $A000-$BFFF (8KB)
    rom_t* basic;               // Basic ROM $A000-$BFFF (8KB)
    rom_t* charrom;             // Character ROM $D000-$DFFF (4KB) when CHAREN=0
    vicii_t* vicii;             // mos6567_t (NTSC) or mos6569_t (PAL) ($D000-$DFFF, 4KB)
    mos6581_t* sid;             // MOS6581 SID sound chip ($D400-$D7FF, 1KB)
    mos2114_t* colorram;        // Color RAM (1KB at $D800-$DBFF)
    mos6526_t* cia1;            // MOS6526 CIA 1 (BUS_MASK_IRQ) ($DC00-$DDFF, 256 bytes)
    mos6526_t* cia2;            // MOS6526 CIA 2 (BUS_MASK_NMI) ($DD00-$DFFF, 256 bytes)
    // Additional members for ROM slots
    void* io1;                  // Cartridge I/O 1 ($DE00-$DEFF)
    void* io2;                  // Cartridge I/O 2 ($DF00-$DFFF)
    rom_t* kernal;              // Kernal ROM $E000-$FFFF (8KB)

    std::uint64_t total_cycles; // Total cycles executed by the system
};

// Legacy typedefs for compatibility during transition
using c64_t = C64System;
using c64_s = C64System;  // For GUI interface compatibility

// Container-of macro for embedded bus access
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Convenience macro to get c64_t from embedded bus
#define BUS_TO_C64(bus_ptr) container_of(bus_ptr, c64_t, bus)

// Function declarations
c64_t* c64_system_create(const c64_config_t* config);
void c64_system_destroy(c64_t* c64);

// Single unified system tick function - the one place where the entire system is ticked
void c64_system_tick(c64_t* c64);

bool c64_pla_maps_generate(c64_t* c64);  // PLA memory mapping generation
void c64_memory_init(system_8bit_t* system, const c64_config_t* config);
bool c64_reload_roms(c64_t* c64, const rom_config_t* rom_config);

// Set the framebuffer for VIC-II pixel output
void c64_set_framebuffer(c64_t* c64, uint32_t* framebuffer, int width, int height);