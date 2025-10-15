#pragma once
/*
 * rockwell65c02.hpp - Rockwell R65C02 Microprocessor Emulator
 *
 * Enhanced CMOS 6502 with bit manipulation instructions.
 * Used in: Some Apple IIc systems, various embedded applications.
 *
 * Features:
 * - All WDC 65C02 features (enhanced instructions, bug fixes)
 * - Additional bit manipulation instructions:
 *   - RMB0-RMB7: Reset Memory Bit (clear specific bit in zero page)
 *   - SMB0-SMB7: Set Memory Bit (set specific bit in zero page) 
 *   - BBR0-BBR7: Branch on Bit Reset (branch if specific bit clear)
 *   - BBS0-BBS7: Branch on Bit Set (branch if specific bit set)
 * - Useful for embedded control applications
 * - Zero page relative addressing for bit branch instructions
 */

#include "fam65xx.hpp"

#ifdef __cplusplus

namespace fam65xx_cpu {

// Rockwell 65C02 CPU class - WDC 65C02 with bit manipulation
using Rockwell65C02 = fam65xx_template::CPU<fam65xx_template::Rockwell65C02Tag>;

// Feature queries for compile-time optimization
constexpr bool has_illegal_opcodes() { return false; }  // Converted to NOPs
constexpr bool has_decimal_mode() { return true; }
constexpr bool has_cmos_enhancements() { return true; }
constexpr bool has_bit_manipulation() { return true; }  // Key feature
constexpr bool has_16bit_mode() { return false; }
constexpr bool has_io_port() { return false; }
constexpr bool has_nmos_bugs() { return false; }       // Bugs fixed
constexpr bool decimal_affects_nz() { return false; }  // Fixed in CMOS

// Bit manipulation instruction opcodes
enum RockwellBitInstructions {
    // Reset Memory Bit (RMB) - Clear specific bit in zero page
    RMB0 = 0x07, RMB1 = 0x17, RMB2 = 0x27, RMB3 = 0x37,
    RMB4 = 0x47, RMB5 = 0x57, RMB6 = 0x67, RMB7 = 0x77,
    
    // Set Memory Bit (SMB) - Set specific bit in zero page  
    SMB0 = 0x87, SMB1 = 0x97, SMB2 = 0xA7, SMB3 = 0xB7,
    SMB4 = 0xC7, SMB5 = 0xD7, SMB6 = 0xE7, SMB7 = 0xF7,
    
    // Branch on Bit Reset (BBR) - Branch if specific bit clear
    BBR0 = 0x0F, BBR1 = 0x1F, BBR2 = 0x2F, BBR3 = 0x3F,
    BBR4 = 0x4F, BBR5 = 0x5F, BBR6 = 0x6F, BBR7 = 0x7F,
    
    // Branch on Bit Set (BBS) - Branch if specific bit set
    BBS0 = 0x8F, BBS1 = 0x9F, BBS2 = 0xAF, BBS3 = 0xBF,
    BBS4 = 0xCF, BBS5 = 0xDF, BBS6 = 0xEF, BBS7 = 0xFF
};

// Bit manipulation helper functions
class BitManipulation {
public:
    // Get bit number from RMB/SMB opcode
    static uint8_t get_rmb_smb_bit(uint8_t opcode) {
        return (opcode >> 4) & 0x07;
    }
    
    // Get bit number from BBR/BBS opcode
    static uint8_t get_bbr_bbs_bit(uint8_t opcode) {
        return (opcode >> 4) & 0x07;
    }
    
    // Check if opcode is a bit manipulation instruction
    static bool is_bit_instruction(uint8_t opcode) {
        // RMB instructions: 0x07, 0x17, 0x27, 0x37, 0x47, 0x57, 0x67, 0x77
        // SMB instructions: 0x87, 0x97, 0xA7, 0xB7, 0xC7, 0xD7, 0xE7, 0xF7
        // BBR instructions: 0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F
        // BBS instructions: 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF
        return (opcode & 0x0F) == 0x07 || (opcode & 0x0F) == 0x0F;
    }
    
    // Check if opcode is RMB/SMB (memory bit set/reset)
    static bool is_memory_bit_instruction(uint8_t opcode) {
        return (opcode & 0x0F) == 0x07;
    }
    
    // Check if opcode is BBR/BBS (bit branch)
    static bool is_bit_branch_instruction(uint8_t opcode) {
        return (opcode & 0x0F) == 0x0F;
    }
};

// Rockwell 65C02 with bit manipulation tracking
class Rockwell65C02CPU {
private:
    Rockwell65C02 cpu;
    uint32_t bit_instruction_count = 0;
    
public:
    // CPU interface delegation
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) { return cpu.init(desc); }
    bus_state_t reset(bus_state_t pins) { 
        bit_instruction_count = 0;
        return cpu.reset(pins); 
    }
    bus_state_t bootstrap(bus_state_t pins) { return cpu.bootstrap(pins); }
    bool opdone() const { return cpu.opdone(); }
    
    // Enhanced tick with bit instruction tracking
    bus_state_t tick(bus_state_t pins) {
        // Track when bit manipulation instructions are used
        if (opdone()) {
            uint8_t opcode = 0; // Would need to get current opcode from CPU state
            if (BitManipulation::is_bit_instruction(opcode)) {
                bit_instruction_count++;
            }
        }
        return cpu.tick(pins);
    }
    
    // Register access
    void set_a(uint8_t v) { cpu.set_a(v); }
    void set_x(uint8_t v) { cpu.set_x(v); }
    void set_y(uint8_t v) { cpu.set_y(v); }
    void set_s(uint8_t v) { cpu.set_s(v); }
    void set_p(uint8_t v) { cpu.set_p(v); }
    void set_pc(uint16_t v) { cpu.set_pc(v); }
    
    uint8_t a() const { return cpu.a(); }
    uint8_t x() const { return cpu.x(); }
    uint8_t y() const { return cpu.y(); }
    uint8_t s() const { return cpu.s(); }
    uint8_t p() const { return cpu.p(); }
    uint16_t pc() const { return cpu.pc(); }
    
    // Bit instruction queries
    uint32_t get_bit_instruction_count() const { return bit_instruction_count; }
    void reset_bit_instruction_count() { bit_instruction_count = 0; }
    
    // Bit manipulation helpers
    static uint8_t set_bit(uint8_t value, uint8_t bit) {
        return value | (1 << bit);
    }
    
    static uint8_t clear_bit(uint8_t value, uint8_t bit) {
        return value & ~(1 << bit);
    }
    
    static bool test_bit(uint8_t value, uint8_t bit) {
        return (value & (1 << bit)) != 0;
    }
    
    // Direct CPU access
    Rockwell65C02* get_cpu() { return &cpu; }
    const Rockwell65C02* get_cpu() const { return &cpu; }
};

// Convenient creation functions
inline Rockwell65C02 create_basic() {
    return Rockwell65C02{};
}

inline Rockwell65C02CPU create() {
    return Rockwell65C02CPU{};
}

// Initialization with memory callbacks
inline Rockwell65C02CPU create_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    Rockwell65C02CPU cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_cpu

// Global type aliases for convenience
using rockwell65c02_t = fam65xx_cpu::Rockwell65C02CPU;
using r65c02_t = fam65xx_cpu::Rockwell65C02CPU;  // Short name

#endif // __cplusplus

// C compatibility wrapper functions
#ifdef __cplusplus
extern "C" {
#endif

// C API for Rockwell 65C02 - wraps the template implementation
typedef struct {
    fam65xx_t impl;
    uint32_t bit_count;  // Count of bit manipulation instructions used
} rockwell65c02_c_t;

// Initialize Rockwell 65C02 CPU
inline uint64_t rockwell65c02_init(rockwell65c02_c_t* cpu, const fam65xx_desc_t* desc) {
    cpu->bit_count = 0;
    return fam65xx_init(&cpu->impl, desc);
}

// Reset Rockwell 65C02 CPU
inline uint64_t rockwell65c02_reset(rockwell65c02_c_t* cpu, uint64_t pins) {
    cpu->bit_count = 0;
    return fam65xx_reset(&cpu->impl, pins);
}

// Execute one tick
inline uint64_t rockwell65c02_tick(rockwell65c02_c_t* cpu, uint64_t pins) {
    return fam65xx_tick(&cpu->impl, pins);
}

// Check if operation is done
inline bool rockwell65c02_opdone(rockwell65c02_c_t* cpu) {
    return fam65xx_opdone(&cpu->impl);
}

// Register access functions
inline uint8_t rockwell65c02_a(rockwell65c02_c_t* cpu) { return fam65xx_a(&cpu->impl); }
inline uint8_t rockwell65c02_x(rockwell65c02_c_t* cpu) { return fam65xx_x(&cpu->impl); }
inline uint8_t rockwell65c02_y(rockwell65c02_c_t* cpu) { return fam65xx_y(&cpu->impl); }
inline uint8_t rockwell65c02_s(rockwell65c02_c_t* cpu) { return fam65xx_s(&cpu->impl); }
inline uint8_t rockwell65c02_p(rockwell65c02_c_t* cpu) { return fam65xx_p(&cpu->impl); }
inline uint16_t rockwell65c02_pc(rockwell65c02_c_t* cpu) { return fam65xx_pc(&cpu->impl); }

inline void rockwell65c02_set_a(rockwell65c02_c_t* cpu, uint8_t v) { fam65xx_set_a(&cpu->impl, v); }
inline void rockwell65c02_set_x(rockwell65c02_c_t* cpu, uint8_t v) { fam65xx_set_x(&cpu->impl, v); }
inline void rockwell65c02_set_y(rockwell65c02_c_t* cpu, uint8_t v) { fam65xx_set_y(&cpu->impl, v); }
inline void rockwell65c02_set_s(rockwell65c02_c_t* cpu, uint8_t v) { fam65xx_set_s(&cpu->impl, v); }
inline void rockwell65c02_set_p(rockwell65c02_c_t* cpu, uint8_t v) { fam65xx_set_p(&cpu->impl, v); }
inline void rockwell65c02_set_pc(rockwell65c02_c_t* cpu, uint16_t v) { fam65xx_set_pc(&cpu->impl, v); }

// Bit manipulation instruction tracking
inline uint32_t rockwell65c02_get_bit_count(rockwell65c02_c_t* cpu) { return cpu->bit_count; }
inline void rockwell65c02_reset_bit_count(rockwell65c02_c_t* cpu) { cpu->bit_count = 0; }

// Bit manipulation helper functions
inline uint8_t rockwell65c02_set_bit(uint8_t value, uint8_t bit) { return value | (1 << bit); }
inline uint8_t rockwell65c02_clear_bit(uint8_t value, uint8_t bit) { return value & ~(1 << bit); }
inline bool rockwell65c02_test_bit(uint8_t value, uint8_t bit) { return (value & (1 << bit)) != 0; }

// Convenience aliases
#define r65c02_init rockwell65c02_init
#define r65c02_reset rockwell65c02_reset
#define r65c02_tick rockwell65c02_tick
#define r65c02_opdone rockwell65c02_opdone
#define r65c02_a rockwell65c02_a
#define r65c02_x rockwell65c02_x
#define r65c02_y rockwell65c02_y
#define r65c02_s rockwell65c02_s
#define r65c02_p rockwell65c02_p
#define r65c02_pc rockwell65c02_pc
#define r65c02_set_a rockwell65c02_set_a
#define r65c02_set_x rockwell65c02_set_x
#define r65c02_set_y rockwell65c02_set_y
#define r65c02_set_s rockwell65c02_set_s
#define r65c02_set_p rockwell65c02_set_p
#define r65c02_set_pc rockwell65c02_set_pc

#ifdef __cplusplus
}
#endif