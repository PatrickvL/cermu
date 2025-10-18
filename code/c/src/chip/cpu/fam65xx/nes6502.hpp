#pragma once
/*
 * nes6502.hpp - Nintendo Entertainment System 6502 Microprocessor Emulator
 *
 * Modified 6502 used in the Nintendo Entertainment System (NES/Famicom).
 * Based on Ricoh 2A03/2A07 which removes decimal mode but keeps illegal opcodes.
 *
 * Features:
 * - All illegal opcodes from original 6502
 * - Decimal mode disabled (SED/CLD still exist but D flag is ignored)
 * - NMOS bugs preserved (JMP indirect page boundary bug)
 * - Additional sound generator on chip (not emulated here)
 * - Used in: Nintendo Famicom, Nintendo Entertainment System
 */

#include "fam65xx_processor_traits.hpp"

// Include tables for template-based opcode generation
#ifndef AIEMUC_IMPL
    #define AIEMUC_IMPL
#endif

// Include the main modular header (which now includes everything)
#include "fam65xx.hpp"
#include "fam65xx_processor_wrappers.hpp"

// NES 6502 uses the unified opcode table system with MOS6502Tag
// The processor-specific table is generated using template-based selection
// ensuring hardware-accurate illegal opcodes with disabled decimal mode

#ifdef __cplusplus

namespace fam65xx_cpu {

// NES 6502 CPU class - 6502 without decimal mode
using NES6502 = fam65xx_t;

// NES-specific CPU wrapper that disables decimal mode
class NES6502CPU {
private:
    nes6502_cpu_t cpu_wrapper; // Use processor-specific wrapper with automatic opcode table init
    
public:
    // CPU interface delegation
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) {
        // Opcode table is automatically initialized by nes6502_cpu_t constructor
        return cpu_wrapper.init(desc);
    }
    bus_state_t reset(bus_state_t pins) { return cpu_wrapper.reset(pins); }
    bus_state_t bootstrap(bus_state_t pins) { return cpu_wrapper.bootstrap(pins); }
    bool opdone() const { return cpu_wrapper.opdone(); }
    
    // Modified tick that ignores decimal flag
    bus_state_t tick(bus_state_t pins) {
        // Clear decimal flag before each instruction to disable decimal mode
        fam65xx_set_p(cpu_wrapper.get_cpu(), fam65xx_p(cpu_wrapper.get_cpu()) & ~0x08);  // Clear D flag
        return cpu_wrapper.tick(pins);
    }
    
    // Register access
    void set_a(uint8_t v) { fam65xx_set_a(cpu_wrapper.get_cpu(), v); }
    void set_x(uint8_t v) { fam65xx_set_x(cpu_wrapper.get_cpu(), v); }
    void set_y(uint8_t v) { fam65xx_set_y(cpu_wrapper.get_cpu(), v); }
    void set_s(uint8_t v) { fam65xx_set_s(cpu_wrapper.get_cpu(), v); }
    void set_pc(uint16_t v) { fam65xx_set_pc(cpu_wrapper.get_cpu(), v); }
    
    // Modified set_p that ignores decimal flag
    void set_p(uint8_t v) {
        fam65xx_set_p(cpu_wrapper.get_cpu(), v & ~0x08);  // Always clear D flag
    }
    
    uint8_t a() const { return fam65xx_a(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t x() const { return fam65xx_x(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t y() const { return fam65xx_y(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t s() const { return fam65xx_s(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint16_t pc() const { return fam65xx_pc(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    
    // Modified get_p that always shows decimal flag as clear
    uint8_t p() const {
        return fam65xx_p(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())) & ~0x08;  // Always clear D flag in return value
    }
    
    // Direct CPU access (for advanced users who know what they're doing)
    NES6502* get_cpu() { return cpu_wrapper.get_cpu(); }
    const NES6502* get_cpu() const { return cpu_wrapper.get_cpu(); }
};

// Convenient creation functions
inline NES6502 create_basic() {
    nes6502_cpu_t cpu_wrapper; // Automatically initializes opcode table
    cpu_wrapper.init(nullptr);
    return *cpu_wrapper.get_cpu(); // Return a copy of the initialized CPU
}

inline NES6502CPU create() {
    return NES6502CPU{};
}

// Initialization with memory callbacks
inline NES6502CPU create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    NES6502CPU cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using nes6502_t = fam65xx_cpu::NES6502CPU;
using ricoh2a03_t = fam65xx_cpu::NES6502CPU;  // Alternative name

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for NES 6502 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
} nes6502_c_t;

// Initialize NES 6502 CPU
inline uint64_t nes6502_init(nes6502_c_t* cpu, const fam65xx_desc_t* desc) {
    // Use processor wrapper for automatic opcode table initialization
    nes6502_cpu_t cpu_wrapper;
    uint64_t result = cpu_wrapper.init(desc);
    cpu->impl = *cpu_wrapper.get_cpu(); // Copy the initialized CPU
    return result;
}

// Reset NES 6502 CPU
inline uint64_t nes6502_reset(nes6502_c_t* cpu, uint64_t pins) {
    uint64_t result = fam65xx_reset(&cpu->impl, pins);
    // Clear decimal flag after reset
    fam65xx_set_p(&cpu->impl, fam65xx_p(&cpu->impl) & ~0x08);
    return result;
}

// Execute one tick with decimal mode disabled
inline uint64_t nes6502_tick(nes6502_c_t* cpu, uint64_t pins) {
    // Clear decimal flag before tick
    fam65xx_set_p(&cpu->impl, fam65xx_p(&cpu->impl) & ~0x08);
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool nes6502_opdone(nes6502_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Register access functions
inline uint8_t nes6502_a(nes6502_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t nes6502_x(nes6502_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t nes6502_y(nes6502_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t nes6502_s(nes6502_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint16_t nes6502_pc(nes6502_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

// Modified P register access that masks decimal flag
inline uint8_t nes6502_p(nes6502_c_t* cpu) { 
    return fam65xx_p(&cpu->impl) & ~0x08;  // Always clear D flag
}

inline void nes6502_set_a(nes6502_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); }
inline void nes6502_set_x(nes6502_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); }
inline void nes6502_set_y(nes6502_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); }
inline void nes6502_set_s(nes6502_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void nes6502_set_pc(nes6502_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

// Modified P register set that ignores decimal flag
inline void nes6502_set_p(nes6502_c_t* cpu, uint8_t v) { 
    fam65xx_set_p(&cpu->impl, v & ~0x08);  // Always clear D flag
}

// Ricoh 2A03 aliases
#define ricoh2a03_init nes6502_init
#define ricoh2a03_reset nes6502_reset
#define ricoh2a03_tick nes6502_tick
#define ricoh2a03_opdone nes6502_opdone
#define ricoh2a03_a nes6502_a
#define ricoh2a03_x nes6502_x
#define ricoh2a03_y nes6502_y
#define ricoh2a03_s nes6502_s
#define ricoh2a03_p nes6502_p
#define ricoh2a03_pc nes6502_pc
#define ricoh2a03_set_a nes6502_set_a
#define ricoh2a03_set_x nes6502_set_x
#define ricoh2a03_set_y nes6502_set_y
#define ricoh2a03_set_s nes6502_set_s
#define ricoh2a03_set_p nes6502_set_p
#define ricoh2a03_set_pc nes6502_set_pc

#ifdef __cplusplus
}
#endif