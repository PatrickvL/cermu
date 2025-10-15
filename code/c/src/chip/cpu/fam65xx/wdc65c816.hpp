#pragma once
/*
 * wdc65c816.hpp - Western Design Center 65C816 Microprocessor Emulator
 *
 * 16-bit extension of the 65C02 with enhanced capabilities.
 * Used in: Apple IIgs, Super Nintendo Entertainment System (SNES), various others.
 *
 * Features:
 * - All WDC 65C02 features (enhanced instructions, bug fixes)
 * - 16-bit accumulator and index registers (selectable)
 * - 24-bit addressing (16MB address space)
 * - Additional registers: Direct Page, Data Bank, Program Bank
 * - New instructions: REP, SEP, XBA, XCE, COP, WDM, etc.
 * - Long addressing modes for 24-bit operations
 * - Native and Emulation modes for 6502 compatibility
 * - Enhanced stack operations (PEA, PEI, PER)
 * - Block move instructions (MVN, MVP)
 */

#include "fam65xx_variants.hpp"

// Include tables for template-based opcode generation
#ifndef AIEMUC_IMPL
    #define AIEMUC_IMPL
#endif

// Include core definitions first
#include "fam65xx_core.hpp"
#include "fam65xx_tables.hpp"

// Define internal processor-specific opcode table for WDC 65C816
DEFINE_PROCESSOR_OPCODE_TABLE_INTERNAL(fam65xx_variants::WDC65C816Tag)

// Include the implementation after the table definitions
#include "fam65xx_impl.hpp"

#ifdef __cplusplus

namespace fam65xx_cpu {

// WDC 65C816 CPU class - 16-bit enhanced processor
using WDC65C816 = fam65xx_variants::CPU<fam65xx_variants::WDC65C816Tag>;

// 65C816-specific registers and flags
enum WDC65C816Registers {
    // Status register flags (in addition to standard 6502 flags)
    FLAG_M = 0x20,  // Memory/Accumulator size (0=16-bit, 1=8-bit)
    FLAG_X = 0x10,  // Index register size (0=16-bit, 1=8-bit)  
    FLAG_E = 0x100, // Emulation mode (not in P register, separate)
};

// 65C816 enhanced instructions
enum WDC65C816Instructions {
    // Mode control
    REP = 0xC2,  // Reset Processor Status bits
    SEP = 0xE2,  // Set Processor Status bits
    XCE = 0xFB,  // Exchange Carry and Emulation flags
    
    // Register operations
    XBA = 0xEB,  // Exchange B and A (swap high/low bytes of A)
    
    // Stack operations
    PEA = 0xF4,  // Push Effective Absolute address
    PEI = 0xD4,  // Push Effective Indirect address  
    PER = 0x62,  // Push Effective PC Relative address
    PHB = 0x8B,  // Push Data Bank register
    PHD = 0x0B,  // Push Direct Page register
    PHK = 0x4B,  // Push Program Bank register
    PLB = 0xAB,  // Pull Data Bank register
    PLD = 0x2B,  // Pull Direct Page register
    
    // Long addressing
    JSL = 0x22,  // Jump to Subroutine Long
    RTL = 0x6B,  // Return from Subroutine Long
    JML_ABS = 0x5C,  // Jump Long Absolute
    JML_IND = 0xDC,  // Jump Long Indirect
    
    // Block move
    MVN = 0x54,  // Move Block Negative
    MVP = 0x44,  // Move Block Positive
    
    // System
    COP = 0x02,  // Coprocessor
    WDM = 0x42,  // William D. Mensch (reserved for future use)
    STP = 0xDB   // Stop (same as 65C02)
};

// 65C816 CPU state extension
struct WDC65C816State {
    // Additional 16-bit registers
    uint16_t dp;    // Direct Page register
    uint8_t dbr;    // Data Bank register  
    uint8_t pbr;    // Program Bank register
    bool emulation; // Emulation mode flag
    
    // 16-bit register access
    uint16_t a_16;  // 16-bit accumulator (when M=0)
    uint16_t x_16;  // 16-bit X register (when X=0)
    uint16_t y_16;  // 16-bit Y register (when X=0)
    
    WDC65C816State() : dp(0), dbr(0), pbr(0), emulation(true), a_16(0), x_16(0), y_16(0) {}
};

// WDC 65C816 with enhanced state management
class WDC65C816CPU {
private:
    WDC65C816 cpu;
    WDC65C816State extended_state;
    uint32_t long_instruction_count = 0;
    
public:
    // CPU interface delegation
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) { 
        extended_state = WDC65C816State();
        return cpu.init(desc); 
    }
    
    bus_state_t reset(bus_state_t pins) { 
        extended_state = WDC65C816State();
        extended_state.emulation = true;  // Start in emulation mode
        long_instruction_count = 0;
        return cpu.reset(pins); 
    }
    
    bus_state_t bootstrap(bus_state_t pins) { return cpu.bootstrap(pins); }
    bool opdone() const { return cpu.opdone(); }
    
    // Enhanced tick with 16-bit support
    bus_state_t tick(bus_state_t pins) {
        // Track when long/16-bit instructions are used
        if (opdone()) {
            uint8_t opcode = 0; // Would need to get current opcode from CPU state
            if (is_long_instruction(opcode)) {
                long_instruction_count++;
            }
        }
        return cpu.tick(pins);
    }
    
    // Standard register access (8-bit compatibility)
    void set_a(uint8_t v) { cpu.set_a(v); extended_state.a_16 = (extended_state.a_16 & 0xFF00) | v; }
    void set_x(uint8_t v) { cpu.set_x(v); extended_state.x_16 = (extended_state.x_16 & 0xFF00) | v; }
    void set_y(uint8_t v) { cpu.set_y(v); extended_state.y_16 = (extended_state.y_16 & 0xFF00) | v; }
    void set_s(uint8_t v) { cpu.set_s(v); }
    void set_p(uint8_t v) { cpu.set_p(v); }
    void set_pc(uint16_t v) { cpu.set_pc(v); }
    
    uint8_t a() const { return cpu.a(); }
    uint8_t x() const { return cpu.x(); }
    uint8_t y() const { return cpu.y(); }
    uint8_t s() const { return cpu.s(); }
    uint8_t p() const { return cpu.p(); }
    uint16_t pc() const { return cpu.pc(); }
    
    // 16-bit register access
    void set_a_16(uint16_t v) { extended_state.a_16 = v; cpu.set_a(v & 0xFF); }
    void set_x_16(uint16_t v) { extended_state.x_16 = v; cpu.set_x(v & 0xFF); }
    void set_y_16(uint16_t v) { extended_state.y_16 = v; cpu.set_y(v & 0xFF); }
    
    uint16_t a_16() const { return extended_state.a_16; }
    uint16_t x_16() const { return extended_state.x_16; }
    uint16_t y_16() const { return extended_state.y_16; }
    
    // Extended register access
    void set_dp(uint16_t v) { extended_state.dp = v; }
    void set_dbr(uint8_t v) { extended_state.dbr = v; }
    void set_pbr(uint8_t v) { extended_state.pbr = v; }
    void set_emulation(bool e) { extended_state.emulation = e; }
    
    uint16_t dp() const { return extended_state.dp; }
    uint8_t dbr() const { return extended_state.dbr; }
    uint8_t pbr() const { return extended_state.pbr; }
    bool emulation() const { return extended_state.emulation; }
    
    // Mode queries
    bool is_accumulator_16bit() const { return !extended_state.emulation && !(p() & FLAG_M); }
    bool is_index_16bit() const { return !extended_state.emulation && !(p() & FLAG_X); }
    bool is_native_mode() const { return !extended_state.emulation; }
    
    // 24-bit address calculation
    uint32_t make_long_address(uint8_t bank, uint16_t addr) const {
        return (static_cast<uint32_t>(bank) << 16) | addr;
    }
    
    uint32_t make_data_address(uint16_t addr) const {
        return make_long_address(extended_state.dbr, addr);
    }
    
    uint32_t make_program_address(uint16_t addr) const {
        return make_long_address(extended_state.pbr, addr);
    }
    
    // Enhanced instruction queries
    uint32_t get_long_instruction_count() const { return long_instruction_count; }
    void reset_long_instruction_count() { long_instruction_count = 0; }
    
    // Check if an opcode is a 65C816 long/16-bit instruction
    static bool is_long_instruction(uint8_t opcode) {
        switch (opcode) {
            case REP: case SEP: case XCE: case XBA:
            case PEA: case PEI: case PER: case PHB: case PHD: case PHK:
            case PLB: case PLD: case JSL: case RTL: case JML_ABS: case JML_IND:
            case MVN: case MVP: case COP: case WDM:
                return true;
            default:
                return false;
        }
    }
    
    // Direct CPU access
    WDC65C816* get_cpu() { return &cpu; }
    const WDC65C816* get_cpu() const { return &cpu; }
    
    // Extended state access
    WDC65C816State* get_extended_state() { return &extended_state; }
    const WDC65C816State* get_extended_state() const { return &extended_state; }
};

// Convenient creation functions
inline WDC65C816 create_basic() {
    return WDC65C816{};
}

inline WDC65C816CPU create() {
    return WDC65C816CPU{};
}

// Initialization with memory callbacks
inline WDC65C816CPU create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    WDC65C816CPU cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using wdc65c816_t = fam65xx_cpu::WDC65C816CPU;
using snes_cpu_t = fam65xx_cpu::WDC65C816CPU;  // SNES alias

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for WDC 65C816 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
    uint16_t dp;      // Direct Page register
    uint8_t dbr;      // Data Bank register
    uint8_t pbr;      // Program Bank register
    uint8_t emulation; // Emulation mode flag
    uint16_t a_16;    // 16-bit accumulator
    uint16_t x_16;    // 16-bit X register  
    uint16_t y_16;    // 16-bit Y register
    uint32_t long_count; // Count of long instructions used
} wdc65c816_c_t;

// Initialize WDC 65C816 CPU
inline uint64_t wdc65c816_init(wdc65c816_c_t* cpu, const fam65xx_desc_t* desc) {
    cpu->dp = 0;
    cpu->dbr = 0;
    cpu->pbr = 0;
    cpu->emulation = 1;  // Start in emulation mode
    cpu->a_16 = 0;
    cpu->x_16 = 0;
    cpu->y_16 = 0;
    cpu->long_count = 0;
    return fam65xx_init(&cpu->impl, desc);
}

// Reset WDC 65C816 CPU
inline uint64_t wdc65c816_reset(wdc65c816_c_t* cpu, uint64_t pins) {
    cpu->dp = 0;
    cpu->dbr = 0;
    cpu->pbr = 0;
    cpu->emulation = 1;
    cpu->a_16 = 0;
    cpu->x_16 = 0;
    cpu->y_16 = 0;
    cpu->long_count = 0;
    return fam65xx_reset(&cpu->impl, pins);
}

// Execute one tick
inline uint64_t wdc65c816_tick(wdc65c816_c_t* cpu, uint64_t pins) {
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool wdc65c816_opdone(wdc65c816_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Standard register access functions
inline uint8_t wdc65c816_a(wdc65c816_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t wdc65c816_x(wdc65c816_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t wdc65c816_y(wdc65c816_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t wdc65c816_s(wdc65c816_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint8_t wdc65c816_p(wdc65c816_c_t* cpu) { return fam65xx_p(&cpu->impl); }
inline uint16_t wdc65c816_pc(wdc65c816_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

inline void wdc65c816_set_a(wdc65c816_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); cpu->a_16 = (cpu->a_16 & 0xFF00) | v; }
inline void wdc65c816_set_x(wdc65c816_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); cpu->x_16 = (cpu->x_16 & 0xFF00) | v; }
inline void wdc65c816_set_y(wdc65c816_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); cpu->y_16 = (cpu->y_16 & 0xFF00) | v; }
inline void wdc65c816_set_s(wdc65c816_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void wdc65c816_set_p(wdc65c816_c_t* cpu, uint8_t v) { fam65xx_set_p(&cpu->impl, v); }
inline void wdc65c816_set_pc(wdc65c816_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

// 16-bit register access
inline uint16_t wdc65c816_a_16(wdc65c816_c_t* cpu) { return cpu->a_16; }
inline uint16_t wdc65c816_x_16(wdc65c816_c_t* cpu) { return cpu->x_16; }
inline uint16_t wdc65c816_y_16(wdc65c816_c_t* cpu) { return cpu->y_16; }

inline void wdc65c816_set_a_16(wdc65c816_c_t* cpu, uint16_t v) { cpu->a_16 = v; fam65xx_set_a(&cpu->impl, v & 0xFF); }
inline void wdc65c816_set_x_16(wdc65c816_c_t* cpu, uint16_t v) { cpu->x_16 = v; fam65xx_set_x(&cpu->impl, v & 0xFF); }
inline void wdc65c816_set_y_16(wdc65c816_c_t* cpu, uint16_t v) { cpu->y_16 = v; fam65xx_set_y(&cpu->impl, v & 0xFF); }

// Extended register access
inline uint16_t wdc65c816_dp(wdc65c816_c_t* cpu) { return cpu->dp; }
inline uint8_t wdc65c816_dbr(wdc65c816_c_t* cpu) { return cpu->dbr; }
inline uint8_t wdc65c816_pbr(wdc65c816_c_t* cpu) { return cpu->pbr; }
inline uint8_t wdc65c816_emulation(wdc65c816_c_t* cpu) { return cpu->emulation; }

inline void wdc65c816_set_dp(wdc65c816_c_t* cpu, uint16_t v) { cpu->dp = v; }
inline void wdc65c816_set_dbr(wdc65c816_c_t* cpu, uint8_t v) { cpu->dbr = v; }
inline void wdc65c816_set_pbr(wdc65c816_c_t* cpu, uint8_t v) { cpu->pbr = v; }
inline void wdc65c816_set_emulation(wdc65c816_c_t* cpu, uint8_t v) { cpu->emulation = v; }

// Mode queries
inline bool wdc65c816_is_accumulator_16bit(wdc65c816_c_t* cpu) { 
    return !cpu->emulation && !(fam65xx_p(&cpu->impl) & 0x20); 
}
inline bool wdc65c816_is_index_16bit(wdc65c816_c_t* cpu) { 
    return !cpu->emulation && !(fam65xx_p(&cpu->impl) & 0x10); 
}
inline bool wdc65c816_is_native_mode(wdc65c816_c_t* cpu) { return !cpu->emulation; }

// Long instruction tracking
inline uint32_t wdc65c816_get_long_count(wdc65c816_c_t* cpu) { return cpu->long_count; }
inline void wdc65c816_reset_long_count(wdc65c816_c_t* cpu) { cpu->long_count = 0; }

// 24-bit address helpers
inline uint32_t wdc65c816_make_long_address(uint8_t bank, uint16_t addr) { 
    return (((uint32_t)bank) << 16) | addr; 
}

#ifdef __cplusplus
}
#endif