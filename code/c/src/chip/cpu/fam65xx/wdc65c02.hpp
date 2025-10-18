#pragma once
/*
 * wdc65c02.hpp - Western Design Center 65C02 Microprocessor Emulator
 *
 * CMOS version of the 6502 with enhancements and bug fixes.
 * Used in: Apple IIc, Apple IIe (Enhanced), BBC Master, and many others.
 *
 * Features:
 * - All illegal opcodes converted to NOPs
 * - Hardware bugs fixed (JMP indirect, decimal mode N/Z flags)  
 * - New instructions: BRA, PHX, PHY, PLX, PLY, STZ, TRB, TSB, WAI, STP
 * - New addressing modes: Zero Page Indirect, Absolute Indexed Indirect
 * - Enhanced BIT instruction with immediate mode
 * - Lower power consumption (CMOS)
 * - Improved timing on some instructions
 */

#include "fam65xx_processor_traits.hpp"

// Note: AIEMUC_IMPL is defined in fam65xx_core.cpp, not in header files

// Include core definitions first
#include "fam65xx_types.hpp"
#include "fam65xx_tables.hpp"

// Include the implementation after the table definitions
#include "fam65xx_impl.hpp"
#include "fam65xx_core.hpp"

#ifdef __cplusplus

// Bring in the unified CPU types
using namespace fam65xx_core;

namespace fam65xx_cpu {

// WDC 65C02 CPU class - CMOS with enhancements
using WDC65C02 = fam65xx_t;

// Enhanced instruction set opcodes (for reference)
enum WDC65C02Instructions {
    // New instructions not in NMOS 6502
    INS_BRA = 0x80,    // Branch Always
    INS_PHX = 0xDA,    // Push X Register  
    INS_PHY = 0x5A,    // Push Y Register
    INS_PLX = 0xFA,    // Pull X Register
    INS_PLY = 0x7A,    // Pull Y Register
    INS_STZ_ZP = 0x64, // Store Zero - Zero Page
    INS_STZ_ZPX = 0x74, // Store Zero - Zero Page,X
    INS_STZ_ABS = 0x9C, // Store Zero - Absolute
    INS_STZ_ABX = 0x9E, // Store Zero - Absolute,X
    INS_TRB_ZP = 0x14,  // Test and Reset Bits - Zero Page
    INS_TRB_ABS = 0x1C, // Test and Reset Bits - Absolute
    INS_TSB_ZP = 0x04,  // Test and Set Bits - Zero Page  
    INS_TSB_ABS = 0x0C, // Test and Set Bits - Absolute
    INS_BIT_IMM = 0x89, // BIT Immediate
    INS_BIT_ZPX = 0x34, // BIT Zero Page,X
    INS_BIT_ABX = 0x3C, // BIT Absolute,X
    INS_JMP_AIX = 0x7C, // JMP Absolute Indexed Indirect
    INS_INC_ACC = 0x1A, // INC Accumulator
    INS_DEC_ACC = 0x3A, // DEC Accumulator
    INS_WAI = 0xCB,     // Wait for Interrupt
    INS_STP = 0xDB      // Stop
};

// New addressing modes (for reference)
enum WDC65C02AddressingModes {
    // Zero Page Indirect: LDA ($12) 
    ZPI_LDA = 0xB2, ZPI_STA = 0x92, ZPI_CMP = 0xD2,
    ZPI_AND = 0x32, ZPI_EOR = 0x52, ZPI_ORA = 0x12,
    ZPI_ADC = 0x72, ZPI_SBC = 0xF2,
    
    // Enhanced addressing for existing instructions
    JMP_ABS_IND_X = 0x7C  // JMP ($1234,X) - Absolute Indexed Indirect
};

// WDC 65C02 with enhanced instruction tracking
class WDC65C02CPU {
private:
    wdc65c02_cpu_t cpu_wrapper; // Use processor-specific wrapper with automatic opcode table init
    uint32_t enhanced_instruction_count = 0;
    
public:
    // CPU interface delegation
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) {
        // Opcode table is automatically initialized by wdc65c02_cpu_t constructor
        return cpu_wrapper.init(desc);
    }
    bus_state_t reset(bus_state_t pins) {
        enhanced_instruction_count = 0;
        return cpu_wrapper.reset(pins);
    }
    bus_state_t bootstrap(bus_state_t pins) { return cpu_wrapper.bootstrap(pins); }
    bool opdone() const { return cpu_wrapper.opdone(); }
    
    // Enhanced tick with instruction tracking
    bus_state_t tick(bus_state_t pins) {
        // Track when enhanced instructions are used
        if (opdone()) {
            uint8_t opcode = 0; // Would need to get current opcode from CPU state
            if (is_enhanced_instruction(opcode)) {
                enhanced_instruction_count++;
            }
        }
        return cpu_wrapper.tick(pins);
    }
    
    // Register access
    void set_a(uint8_t v) { fam65xx_set_a(cpu_wrapper.get_cpu(), v); }
    void set_x(uint8_t v) { fam65xx_set_x(cpu_wrapper.get_cpu(), v); }
    void set_y(uint8_t v) { fam65xx_set_y(cpu_wrapper.get_cpu(), v); }
    void set_s(uint8_t v) { fam65xx_set_s(cpu_wrapper.get_cpu(), v); }
    void set_p(uint8_t v) { fam65xx_set_p(cpu_wrapper.get_cpu(), v); }
    void set_pc(uint16_t v) { fam65xx_set_pc(cpu_wrapper.get_cpu(), v); }
    
    uint8_t a() const { return fam65xx_a(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t x() const { return fam65xx_x(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t y() const { return fam65xx_y(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t s() const { return fam65xx_s(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint8_t p() const { return fam65xx_p(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    uint16_t pc() const { return fam65xx_pc(const_cast<fam65xx_t*>(cpu_wrapper.get_cpu())); }
    
    // Enhanced instruction queries
    uint32_t get_enhanced_instruction_count() const { return enhanced_instruction_count; }
    void reset_enhanced_instruction_count() { enhanced_instruction_count = 0; }
    
    // Check if an opcode is a 65C02 enhancement
    static bool is_enhanced_instruction(uint8_t opcode) {
        switch (opcode) {
            case INS_BRA:
            case INS_PHX: case INS_PHY: case INS_PLX: case INS_PLY:
            case INS_STZ_ZP: case INS_STZ_ZPX: case INS_STZ_ABS: case INS_STZ_ABX:
            case INS_TRB_ZP: case INS_TRB_ABS: case INS_TSB_ZP: case INS_TSB_ABS:
            case INS_BIT_IMM: case INS_BIT_ZPX: case INS_BIT_ABX:
            case INS_JMP_AIX: case INS_INC_ACC: case INS_DEC_ACC:
            case INS_WAI: case INS_STP:
            case ZPI_LDA: case ZPI_STA: case ZPI_CMP: case ZPI_AND:
            case ZPI_EOR: case ZPI_ORA: case ZPI_ADC: case ZPI_SBC:
                return true;
            default:
                return false;
        }
    }
    
    // Direct CPU access
    WDC65C02* get_cpu() { return cpu_wrapper.get_cpu(); }
    const WDC65C02* get_cpu() const { return cpu_wrapper.get_cpu(); }
};

// Convenient creation functions
inline WDC65C02 create_basic() {
    wdc65c02_cpu_t cpu_wrapper; // Automatically initializes opcode table
    cpu_wrapper.init(nullptr);
    return *cpu_wrapper.get_cpu(); // Return a copy of the initialized CPU
}

inline WDC65C02CPU create() {
    return WDC65C02CPU{};
}

// Initialization with memory callbacks
inline WDC65C02CPU create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    WDC65C02CPU cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using wdc65c02_t = fam65xx_cpu::WDC65C02CPU;
using cmos6502_t = fam65xx_cpu::WDC65C02CPU;  // Alternative name

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for WDC 65C02 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
    uint32_t enhanced_count;  // Count of enhanced instructions used
} wdc65c02_c_t;

// Initialize WDC 65C02 CPU
inline uint64_t wdc65c02_init(wdc65c02_c_t* cpu, const fam65xx_desc_t* desc) {
    cpu->enhanced_count = 0;
    
    // Use processor wrapper for automatic opcode table initialization
    wdc65c02_cpu_t cpu_wrapper;
    uint64_t result = cpu_wrapper.init(desc);
    cpu->impl = *cpu_wrapper.get_cpu(); // Copy the initialized CPU
    return result;
}

// Reset WDC 65C02 CPU
inline uint64_t wdc65c02_reset(wdc65c02_c_t* cpu, uint64_t pins) {
    cpu->enhanced_count = 0;
    return fam65xx_reset(&cpu->impl, pins);
}

// Bootstrap WDC 65C02 CPU for immediate execution (test runner compatibility)
inline uint64_t wdc65c02_bootstrap(wdc65c02_c_t* cpu, uint64_t pins) {
    cpu->enhanced_count = 0;
    return fam65xx_bootstrap(&cpu->impl, pins);
}

// Execute one tick
inline uint64_t wdc65c02_tick(wdc65c02_c_t* cpu, uint64_t pins) {
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool wdc65c02_opdone(wdc65c02_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Register access functions
inline uint8_t wdc65c02_a(wdc65c02_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t wdc65c02_x(wdc65c02_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t wdc65c02_y(wdc65c02_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t wdc65c02_s(wdc65c02_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint8_t wdc65c02_p(wdc65c02_c_t* cpu) { return fam65xx_p(&cpu->impl); }
inline uint16_t wdc65c02_pc(wdc65c02_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

inline void wdc65c02_set_a(wdc65c02_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); }
inline void wdc65c02_set_x(wdc65c02_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); }
inline void wdc65c02_set_y(wdc65c02_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); }
inline void wdc65c02_set_s(wdc65c02_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void wdc65c02_set_p(wdc65c02_c_t* cpu, uint8_t v) { fam65xx_set_p(&cpu->impl, v); }
inline void wdc65c02_set_pc(wdc65c02_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

// Enhanced instruction tracking
inline uint32_t wdc65c02_get_enhanced_count(wdc65c02_c_t* cpu) { return cpu->enhanced_count; }
inline void wdc65c02_reset_enhanced_count(wdc65c02_c_t* cpu) { cpu->enhanced_count = 0; }

#ifdef __cplusplus
}
#endif