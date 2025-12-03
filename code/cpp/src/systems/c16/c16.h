#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include "../../core/aiemuc.h"
#include "../../core/system.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/video/vic_ii/mos7360.h"
#include "c16_bus.h"
#include "c16_config.h"

// Forward declarations
class System8Bit;

// Modern C++ C16 system structure
struct C16System {
    system_8bit_t system;       // Legacy system wrapper
    c16_bus_t bus;
    void* mos7501;              // MOS7501 CPU
    ram_t* ram;                 // RAM memory $0000-$FFFF (64KB)
    rom_t* basic;               // BASIC ROM $8000-$BFFF (16KB)
    rom_t* kernal;              // Kernal ROM $C000-$FFFF (16KB)
    vicii_t* vicii;             // TED 7360 ($FD00-$FEFF, 4KB)
    mos6526_t* cia;             // MOS6526 CIA ($FD30-$FD3F, 16 bytes)

    std::uint64_t total_cycles; // Total cycles executed by the system
};

// Legacy typedefs for compatibility during transition
using c16_t = C16System;
using c16_s = C16System;  // For GUI interface compatibility

// Container-of macro for embedded bus access
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Convenience macro to get c16_t from embedded bus
#define BUS_TO_C16(bus_ptr) container_of(bus_ptr, c16_t, bus)

// Function declarations
c16_t* c16_system_create(const c16_config_t* config);
void c16_system_destroy(c16_t* c16);

// CPU execution functions
void c16_cpu_cycle(c16_t* c16);     // Execute one CPU cycle

// Ticks all non-CPU chips once to complete a cycle.
void c16_non_cpu_cycle(void* c16_ptr);
bool c16_pla_maps_generate(c16_t* c16);  // PLA memory mapping generation
void c16_memory_init(system_8bit_t* system, const rom_config_t* rom_config);
bool c16_reload_roms(c16_t* c16, const rom_config_t* rom_config);

// Set the framebuffer for TED pixel output
void c16_set_framebuffer(c16_t* c16, uint32_t* framebuffer, int width, int height);