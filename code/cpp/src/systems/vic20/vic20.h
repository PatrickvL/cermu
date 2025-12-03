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
#include "../../chip/io/mos6522.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/video/vic_ii/mos6560.h"
#include "../../chip/video/vic_ii/mos6561.h"
#include "vic20_bus.h"
#include "vic20_config.h"

// Forward declarations
class System8Bit;

// Modern C++ VIC-20 system structure
struct VIC20System {
    system_8bit_t system;       // Legacy system wrapper
    vic20_bus_t bus;
    void* mos6502;              // MOS6502 instance
    ram_t* ram;                 // RAM memory $0000-$FFFF (35KB)
    rom_t* basic;               // BASIC ROM $A000-$BFFF (8KB)
    rom_t* charrom;             // Character ROM $D000-$DFFF (4KB)
    vicii_t* vicii;             // VIC-6560/6561 ($9000-$9FFF, 4KB)
    mos6581_t* sid;             // MOS6581 SID sound chip (optional)
    mos2114_t* colorram;        // Color RAM (1KB at $9400-$97FF)
    mos6526_t* cia1;            // MOS6526 CIA 1 (BUS_MASK_IRQ) ($9110-$911F, 16 bytes)
    mos6522_t* via1;            // MOS6522 VIA 1 ($9120-$912F, 16 bytes)
    rom_t* kernal;              // Kernal ROM $E000-$FFFF (8KB)

    std::uint64_t total_cycles; // Total cycles executed by the system
};

// Legacy typedefs for compatibility during transition
using vic20_t = VIC20System;
using vic20_s = VIC20System;  // For GUI interface compatibility

// Container-of macro for embedded bus access
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Convenience macro to get vic20_t from embedded bus
#define BUS_TO_VIC20(bus_ptr) container_of(bus_ptr, vic20_t, bus)

// Function declarations
vic20_t* vic20_system_create(const vic20_config_t* config);
void vic20_system_destroy(vic20_t* vic20);

// CPU execution functions
void vic20_cpu_cycle(vic20_t* vic20);     // Execute one CPU cycle

// Ticks all non-CPU chips once to complete a cycle.
void vic20_non_cpu_cycle(void* vic20_ptr);
bool vic20_pla_maps_generate(vic20_t* vic20);  // PLA memory mapping generation
void vic20_memory_init(system_8bit_t* system, const rom_config_t* rom_config);
bool vic20_reload_roms(vic20_t* vic20, const rom_config_t* rom_config);

// Set the framebuffer for VIC-II pixel output
void vic20_set_framebuffer(vic20_t* vic20, uint32_t* framebuffer, int width, int height);