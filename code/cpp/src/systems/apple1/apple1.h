#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include "../../core/aiemuc.h"
#include "../../core/system.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/cpu/fam65xx/mos6502.h"
#include "apple1_bus.h"
#include "apple1_config.h"

// Forward declarations
class System8Bit;

// Modern C++ Apple 1 system structure
struct Apple1System {
    system_8bit_t system;       // Legacy system wrapper
    apple1_bus_t bus;
    void* mos6502;              // MOS6502 CPU
    ram_t* ram;                 // RAM memory $0000-$FFFF (4KB)
    rom_t* monitor;             // Monitor ROM $FF00-$FFFF (256 bytes)

    std::uint64_t total_cycles; // Total cycles executed by the system
};

// Legacy typedefs for compatibility during transition
using apple1_t = Apple1System;
using apple1_s = Apple1System;  // For GUI interface compatibility

// Container-of macro for embedded bus access
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Convenience macro to get apple1_t from embedded bus
#define BUS_TO_APPLE1(bus_ptr) container_of(bus_ptr, apple1_t, bus)

// Function declarations
apple1_t* apple1_system_create(const apple1_config_t* config);
void apple1_system_destroy(apple1_t* apple1);

// CPU execution functions
void apple1_cpu_cycle(apple1_t* apple1);     // Execute one CPU cycle

// Ticks all non-CPU chips once to complete a cycle.
void apple1_non_cpu_cycle(void* apple1_ptr);
void apple1_memory_init(system_8bit_t* system, const rom_config_t* rom_config);
bool apple1_reload_roms(apple1_t* apple1, const rom_config_t* rom_config);