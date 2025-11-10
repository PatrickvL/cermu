#pragma once
/*
 * fam65xx_register_mixins.hpp - Register Layout Mixins for MOS 65xx Family
 *
 * This file contains two different register layout mixins:
 * 1. narrow_registers_mixin_t - For 8-bit CPUs (6502, 6510, 65C02, etc.)
 * 2. wide_registers_mixin_t - For 65C816 with 16-bit register support
 *
 * Both mixins provide identical interfaces so code can work with either CPU type.
 * The mixins handle register width, memory operations, and banking transparently.
 */

#include <cstdint>
#include <cstring>
#include <type_traits>
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.h"

namespace fam65xx {

// ============================================================================
// EMPTY BASES FOR DISABLED FEATURES
// ============================================================================

struct empty_register_mixin_t {};

// ============================================================================
// NARROW REGISTERS MIXIN (8-bit CPUs: 6502, 6510, 65C02, etc.)
// ============================================================================

template<const CPUTraits& Traits>
struct narrow_registers_mixin_t {
    // Data type alias - always 8-bit for narrow CPUs
    using data_t = uint8_t;
    
    // Register array - standard 8-bit layout
    union {
        uint8_t reg8[REG_COUNT];        // 8-bit register access
        uint16_t reg16[REG_COUNT / 2];  // 16-bit pair access (little-endian)
    };
    
    // Initialize registers
    void init_registers() {
        memset(&reg8, 0, sizeof(reg8));
    }
    
    // === 8-bit register accessors ===
    inline uint8_t get(reg8_t reg) const {
        return reg8[reg];
    }
    
    inline void set(reg8_t reg, uint8_t value) {
        reg8[reg] = value;
    }
    
    inline void inc(reg8_t reg) {
        reg8[reg]++;
    }
    
    inline void dec(reg8_t reg) {
        reg8[reg]--;
    }
    
    // === 16-bit register accessors ===
    inline uint16_t get(reg16_t reg) const {
        return reg16[reg];
    }
    
    inline void set(reg16_t reg, uint16_t value) {
        reg16[reg] = value;
    }
    
    inline void inc(reg16_t reg) {
        reg16[reg]++;
    }
    
    inline void dec(reg16_t reg) {
        reg16[reg]--;
    }
    
    // === Memory operation helpers ===
    inline data_t get_accumulator() const {
        return get(REG_A);
    }
    
    inline void set_accumulator(data_t value) {
        set(REG_A, static_cast<uint8_t>(value));
    }
    
    inline data_t get_x_register() const {
        return get(REG_X);
    }
    
    inline void set_x_register(data_t value) {
        set(REG_X, static_cast<uint8_t>(value));
    }
    
    inline data_t get_y_register() const {
        return get(REG_Y);
    }
    
    inline void set_y_register(data_t value) {
        set(REG_Y, static_cast<uint8_t>(value));
    }
    
    inline uint16_t get_stack_pointer() const {
        return get(REG_SP);
    }
    
    inline void set_stack_pointer(uint16_t value) {
        set(REG_SP, value);
    }
    
    // === 65C816 compatibility stubs (no-ops for 8-bit CPUs) ===
    inline bool get_emulation_mode() const { return true; }
    inline void set_emulation_mode(bool /*mode*/) { /* no-op */ }
    inline uint8_t get_dbr() const { return 0x00; }
    inline void set_dbr(uint8_t /*value*/) { /* no-op */ }
    inline uint8_t get_pbr() const { return 0x00; }
    inline void set_pbr(uint8_t /*value*/) { /* no-op */ }
    inline uint16_t get_d() const { return 0x0000; }
    inline void set_d(uint16_t /*value*/) { /* no-op */ }
    
    // === Address calculation (16-bit only) ===
    inline uint32_t calc_effective_address(uint16_t addr) const {
        return addr; // No banking in 8-bit CPUs
    }
    
    // === Load function for bus operations ===
    inline void load(reg8_t reg, bus_state_t pins) {
        reg8[reg] = FAM65XX_GET_DATA(pins);
    }
};

// ============================================================================
// WIDE REGISTERS MIXIN (65C816)
// ============================================================================

template<const CPUTraits& Traits>
struct wide_registers_mixin_t {
    // Data type alias - 16-bit for wide CPUs
    using data_t = uint16_t;
    
    // Extended register layout for 65C816
    struct alignas(8) {
        // Base 8-bit registers (compatible layout)
        union {
            uint8_t reg8[REG_COUNT];        // 8-bit register access
            uint16_t reg16[REG_COUNT / 2];  // 16-bit pair access
        };
        
        // Extended 65C816 registers
        uint16_t A_full;        // Full 16-bit accumulator
        uint16_t X_full;        // Full 16-bit X register  
        uint16_t Y_full;        // Full 16-bit Y register
        uint16_t SP_full;       // Full 16-bit stack pointer
        uint16_t D;             // Direct Page register
        uint8_t DBR;            // Data Bank register
        uint8_t PBR;            // Program Bank register
        bool emulation_mode;    // Emulation mode flag
        uint8_t _padding[5];    // Align to 8 bytes
    } wide_state;
    
    // Initialize registers
    void init_registers() {
        memset(&wide_state, 0, sizeof(wide_state));
        wide_state.emulation_mode = true; // Start in emulation mode
        wide_state.SP_full = 0x01FF;      // Initialize stack pointer
        
        // Sync 8-bit views with extended registers
        sync_registers_to_8bit();
    }
    
    // === Helper: Check if register is in 16-bit mode ===
    inline bool is_accumulator_16bit() const {
        return !wide_state.emulation_mode && !(get(REG_P) & FLAG_M);
    }
    
    inline bool is_index_16bit() const {
        return !wide_state.emulation_mode && !(get(REG_P) & FLAG_X);
    }
    
    // === Synchronization between 8-bit and 16-bit views ===
    inline void sync_registers_to_8bit() {
        // Sync accumulator
        wide_state.reg8[REG_A] = wide_state.A_full & 0xFF;
        
        // Sync index registers
        wide_state.reg8[REG_X] = wide_state.X_full & 0xFF;
        wide_state.reg8[REG_Y] = wide_state.Y_full & 0xFF;
        
        // Sync stack pointer
        wide_state.reg16[REG_SP] = wide_state.SP_full;
    }
    
    inline void sync_registers_from_8bit() {
        // When in 8-bit mode, only update low bytes
        if (!is_accumulator_16bit()) {
            wide_state.A_full = (wide_state.A_full & 0xFF00) | wide_state.reg8[REG_A];
        }
        
        if (!is_index_16bit()) {
            wide_state.X_full = (wide_state.X_full & 0xFF00) | wide_state.reg8[REG_X];
            wide_state.Y_full = (wide_state.Y_full & 0xFF00) | wide_state.reg8[REG_Y];
            wide_state.SP_full = (wide_state.SP_full & 0xFF00) | (wide_state.reg16[REG_SP] & 0xFF);
        }
    }
    
    // === 8-bit register accessors ===
    inline uint8_t get(reg8_t reg) const {
        return wide_state.reg8[reg];
    }
    
    inline void set(reg8_t reg, uint8_t value) {
        wide_state.reg8[reg] = value;
        
        // Sync extended registers when 8-bit registers change
        if (reg == REG_A && !is_accumulator_16bit()) {
            wide_state.A_full = (wide_state.A_full & 0xFF00) | value;
        } else if (reg == REG_X && !is_index_16bit()) {
            wide_state.X_full = (wide_state.X_full & 0xFF00) | value;
        } else if (reg == REG_Y && !is_index_16bit()) {
            wide_state.Y_full = (wide_state.Y_full & 0xFF00) | value;
        }
    }
    
    inline void inc(reg8_t reg) {
        set(reg, get(reg) + 1);
    }
    
    inline void dec(reg8_t reg) {
        set(reg, get(reg) - 1);
    }
    
    // === 16-bit register accessors ===
    inline uint16_t get(reg16_t reg) const {
        if (reg == REG_SP) {
            return wide_state.SP_full;
        }
        return wide_state.reg16[reg];
    }
    
    inline void set(reg16_t reg, uint16_t value) {
        if (reg == REG_SP) {
            wide_state.SP_full = value;
            wide_state.reg16[REG_SP] = value;
        } else {
            wide_state.reg16[reg] = value;
        }
    }
    
    inline void inc(reg16_t reg) {
        set(reg, get(reg) + 1);
    }
    
    inline void dec(reg16_t reg) {
        set(reg, get(reg) - 1);
    }
    
    // === Memory operation helpers (context-aware) ===
    inline data_t get_accumulator() const {
        return is_accumulator_16bit() ? wide_state.A_full : (wide_state.A_full & 0xFF);
    }
    
    inline void set_accumulator(data_t value) {
        if (is_accumulator_16bit()) {
            wide_state.A_full = value;
            wide_state.reg8[REG_A] = value & 0xFF;
        } else {
            wide_state.A_full = (wide_state.A_full & 0xFF00) | (value & 0xFF);
            wide_state.reg8[REG_A] = value & 0xFF;
        }
    }
    
    inline data_t get_x_register() const {
        return is_index_16bit() ? wide_state.X_full : (wide_state.X_full & 0xFF);
    }
    
    inline void set_x_register(data_t value) {
        if (is_index_16bit()) {
            wide_state.X_full = value;
            wide_state.reg8[REG_X] = value & 0xFF;
        } else {
            wide_state.X_full = (wide_state.X_full & 0xFF00) | (value & 0xFF);
            wide_state.reg8[REG_X] = value & 0xFF;
        }
    }
    
    inline data_t get_y_register() const {
        return is_index_16bit() ? wide_state.Y_full : (wide_state.Y_full & 0xFF);
    }
    
    inline void set_y_register(data_t value) {
        if (is_index_16bit()) {
            wide_state.Y_full = value;
            wide_state.reg8[REG_Y] = value & 0xFF;
        } else {
            wide_state.Y_full = (wide_state.Y_full & 0xFF00) | (value & 0xFF);
            wide_state.reg8[REG_Y] = value & 0xFF;
        }
    }
    
    inline uint16_t get_stack_pointer() const {
        return wide_state.SP_full;
    }
    
    inline void set_stack_pointer(uint16_t value) {
        wide_state.SP_full = value;
        wide_state.reg16[REG_SP] = value;
    }
    
    // === 65C816 extended register access ===
    inline bool get_emulation_mode() const {
        return wide_state.emulation_mode;
    }
    
    inline void set_emulation_mode(bool mode) {
        wide_state.emulation_mode = mode;
        if (mode) {
            // Force 8-bit modes in emulation mode
            set(REG_P, get(REG_P) | (FLAG_M | FLAG_X));
        }
    }
    
    inline uint8_t get_dbr() const { return wide_state.DBR; }
    inline void set_dbr(uint8_t value) { wide_state.DBR = value; }
    inline uint8_t get_pbr() const { return wide_state.PBR; }
    inline void set_pbr(uint8_t value) { wide_state.PBR = value; }
    inline uint16_t get_d() const { return wide_state.D; }
    inline void set_d(uint16_t value) { wide_state.D = value; }
    
    // === 16-bit specific accessors ===
    inline uint16_t get_accumulator_full() const { return wide_state.A_full; }
    inline void set_accumulator_full(uint16_t value) { 
        wide_state.A_full = value;
        sync_registers_to_8bit();
    }
    
    inline uint16_t get_x_full() const { return wide_state.X_full; }
    inline void set_x_full(uint16_t value) { 
        wide_state.X_full = value;
        sync_registers_to_8bit();
    }
    
    inline uint16_t get_y_full() const { return wide_state.Y_full; }
    inline void set_y_full(uint16_t value) { 
        wide_state.Y_full = value;
        sync_registers_to_8bit();
    }
    
    // === Address calculation (24-bit with banking) ===
    inline uint32_t calc_effective_address(uint16_t addr) const {
        return (static_cast<uint32_t>(wide_state.DBR) << 16) | addr;
    }
    
    inline uint32_t calc_program_address(uint16_t addr) const {
        return (static_cast<uint32_t>(wide_state.PBR) << 16) | addr;
    }
    
    // === Load function for bus operations ===
    inline void load(reg8_t reg, bus_state_t pins) {
        wide_state.reg8[reg] = FAM65XX_GET_DATA(pins);
        sync_registers_from_8bit();
    }
};

// ============================================================================
// CONDITIONAL REGISTER MIXIN TYPE SELECTION
// ============================================================================

template<const CPUTraits& Traits>
using register_base_t = std::conditional_t<
    Traits.has(CPUCoreFlags::C816_16BIT),
    wide_registers_mixin_t<Traits>,
    narrow_registers_mixin_t<Traits>
>;

} // namespace fam65xx