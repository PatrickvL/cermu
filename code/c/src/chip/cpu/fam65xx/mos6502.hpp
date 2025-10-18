#pragma once
/*
 * mos6502.hpp - MOS 6502 Microprocessor Emulator
 *
 * Original NMOS 6502 CPU with illegal opcodes and hardware bugs.
 * Used in: Apple II, Commodore PET, Atari 2600, BBC Micro, and many others.
 *
 * Features:
 * - Full illegal opcode support (LAX, SAX, DCP, ISC, SLO, RLA, SRE, RRA, etc.)
 * - Hardware bugs: JMP indirect page boundary bug, decimal mode N/Z flag bug
 * - Original NMOS timing and behavior
 * - 8-bit accumulator and index registers
 * - 256-byte stack at $0100-$01FF
 * - 64KB address space
 */

#include "fam65xx_processor_traits.hpp"

// Note: AIEMUC_IMPL is defined in fam65xx_core.cpp, not in header files

// Include the main modular header (which now includes everything)
#include "fam65xx.hpp"
#include "fam65xx_core.hpp"

// MOS 6502 uses the unified opcode table system
// The processor-specific table is generated using template-based selection
// ensuring hardware-accurate illegal opcodes and NMOS-specific behavior

#ifdef __cplusplus

// Bring in the unified CPU types
using namespace fam65xx_core;

namespace fam65xx_cpu {

// MOS 6502 CPU type alias - uses unified implementation
using MOS6502 = fam65xx_t;

// Convenient creation function
inline fam65xx_t create() {
    mos6502_cpu_t cpu_wrapper; // Automatically initializes opcode table
    cpu_wrapper.init(nullptr);
    return *cpu_wrapper.get_cpu(); // Return a copy of the initialized CPU
}

// Initialization with memory callbacks
inline fam65xx_t create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    mos6502_cpu_t cpu_wrapper; // Automatically initializes opcode table
    cpu_wrapper.init(&desc);
    return *cpu_wrapper.get_cpu(); // Return a copy of the initialized CPU
}

} // namespace fam65xx_cpu

// Global type alias for convenience
using mos6502_t = fam65xx_t;

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for MOS 6502 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
} mos6502_c_t;

// Initialize MOS 6502 CPU
inline uint64_t mos6502_init(mos6502_c_t* cpu, const fam65xx_desc_t* desc) {
    // Use processor wrapper for automatic opcode table initialization
    mos6502_cpu_t cpu_wrapper;
    uint64_t result = cpu_wrapper.init(desc);
    cpu->impl = *cpu_wrapper.get_cpu(); // Copy the initialized CPU
    return result;
}

// Reset MOS 6502 CPU
inline uint64_t mos6502_reset(mos6502_c_t* cpu, uint64_t pins) {
    return fam65xx_reset(&cpu->impl, pins);
}

// Execute one tick
inline uint64_t mos6502_tick(mos6502_c_t* cpu, uint64_t pins) {
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool mos6502_opdone(mos6502_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Bootstrap CPU for immediate execution (test runner compatibility)
inline uint64_t mos6502_bootstrap(mos6502_c_t* cpu, uint64_t pins) {
    return fam65xx_bootstrap(&cpu->impl, pins);
}

// Register access functions
inline uint8_t mos6502_a(mos6502_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t mos6502_x(mos6502_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t mos6502_y(mos6502_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t mos6502_s(mos6502_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint8_t mos6502_p(mos6502_c_t* cpu) { return fam65xx_p(&cpu->impl); }
inline uint16_t mos6502_pc(mos6502_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

inline void mos6502_set_a(mos6502_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); }
inline void mos6502_set_x(mos6502_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); }
inline void mos6502_set_y(mos6502_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); }
inline void mos6502_set_s(mos6502_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void mos6502_set_p(mos6502_c_t* cpu, uint8_t v) { fam65xx_set_p(&cpu->impl, v); }
inline void mos6502_set_pc(mos6502_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

#ifdef __cplusplus
}
#endif