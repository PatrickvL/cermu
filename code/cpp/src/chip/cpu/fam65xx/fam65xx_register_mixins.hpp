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
    
    // Use register types for 8-bit CPUs
    using reg8_t = uint8_t;
    using reg16_t = uint8_t;
    
    // Register array - standard 8-bit layout
    union {
        uint8_t reg8[REG_COUNT_8BIT];        // 8-bit register access
        uint16_t reg16[REG_COUNT_8BIT / 2];  // 16-bit pair access (little-endian)
    };
    
    // Initialize registers
    void init_registers() {
        memset(&reg8, 0, sizeof(reg8));
    }
    
    // === 8-bit register accessors ===
    inline uint8_t get(uint8_t reg) const {
        return reg8[reg];
    }
    
    inline void set(uint8_t reg, uint8_t value) {
        reg8[reg] = value;
    }
    
    inline void inc(uint8_t reg) {
        reg8[reg]++;
    }
    
    inline void dec(uint8_t reg) {
        reg8[reg]--;
    }
    
    // === Explicit 16-bit accessors ===
    inline uint16_t get16(uint8_t reg_pair) const {
        return reg16[reg_pair];
    }
    
    inline void set16(uint8_t reg_pair, uint16_t value) {
        reg16[reg_pair] = value;
    }
    
    inline void inc16(uint8_t reg_pair) {
        reg16[reg_pair]++;
    }
    
    inline void dec16(uint8_t reg_pair) {
        reg16[reg_pair]--;
    }
    
    // === Memory operation helpers ===
    inline data_t get_accumulator() const {
        return get(REG_A);
    }
    
    inline void set_accumulator(data_t value) {
        set(REG_A, value);
    }
    
    inline data_t get_x_register() const {
        return get(REG_X);
    }
    
    inline void set_x_register(data_t value) {
        set(REG_X, value);
    }
    
    inline data_t get_y_register() const {
        return get(REG_Y);
    }
    
    inline void set_y_register(data_t value) {
        set(REG_Y, value);
    }
    
    inline uint16_t get_stack_pointer() const {
        return get16(REG_SP);
    }
    
    inline void set_stack_pointer(uint16_t value) {
        set16(REG_SP, value);
    }
    
    // Note: 65C816 compatibility - 8-bit CPUs don't have these registers
    // Use conditional compilation in operations that need 65C816 features
    
    // === Address calculation (16-bit only) ===
    inline uint32_t calc_effective_address(uint16_t addr) const {
        return addr; // No banking in 8-bit CPUs
    }
    
    // === Load function for bus operations ===
    inline void load(uint8_t reg, bus_state_t pins) {
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
    
    // Use register types for 16-bit CPUs
    using reg8_t = uint8_t;
    using reg16_t = uint8_t;
    
    // Register array with proper 8/16-bit alignment - matches narrow_registers_mixin_t structure
    union {
        uint8_t reg8[REG_COUNT_16BIT];        // 8-bit register access
        uint16_t reg16[REG_COUNT_16BIT / 2];  // 16-bit pair access (little-endian)
    };
    
    // 65C816 control state - separate from register array for cleaner design
    bool emulation_mode;    // Emulation mode flag
    
    // Initialize registers
    void init_registers() {
        memset(&reg8, 0, sizeof(reg8));
        emulation_mode = true; // Start in emulation mode
        set(REG_SPL, 0xFF);
        set(REG_SPH, 0x01); // Initialize stack pointer
    }
    
    // === Helper: Check if register is in 16-bit mode ===
    inline bool is_accumulator_16bit() const {
        return !emulation_mode && !(get(REG_P) & FLAG_M);
    }
    
    inline bool is_index_16bit() const {
        return !emulation_mode && !(get(REG_P) & FLAG_X);
    }
    
    // === 8-bit register accessors ===
    inline uint8_t get(uint8_t reg) const {
        return reg8[reg];
    }
    
    inline void set(uint8_t reg, uint8_t value) {
        reg8[reg] = value;
    }
    
    inline void inc(uint8_t reg) {
        reg8[reg]++;
    }
    
    inline void dec(uint8_t reg) {
        reg8[reg]--;
    }
    
    // === Explicit 16-bit accessors ===
    inline uint16_t get16(uint8_t reg_pair) const {
        return reg16[reg_pair];
    }
    
    inline void set16(uint8_t reg_pair, uint16_t value) {
        reg16[reg_pair] = value;
    }
    
    inline void inc16(uint8_t reg_pair) {
        reg16[reg_pair]++;
    }
    
    inline void dec16(uint8_t reg_pair) {
        reg16[reg_pair]--;
    }
    
    // === Memory operation helpers (context-aware) ===
    inline data_t get_accumulator() const {
        if (is_accumulator_16bit()) {
            return get(REG_A) | (get(REG_AH) << 8);
        } else {
            return get(REG_A);
        }
    }
    
    inline void set_accumulator(data_t value) {
        if (is_accumulator_16bit()) {
            set(REG_A, static_cast<uint8_t>(value & 0xFF));
            set(REG_AH, static_cast<uint8_t>((value >> 8) & 0xFF));
        } else {
            set(REG_A, static_cast<uint8_t>(value & 0xFF));
        }
    }
    
    inline data_t get_x_register() const {
        if (is_index_16bit()) {
            return get(REG_X) | (get(REG_XH) << 8);
        } else {
            return get(REG_X);
        }
    }
    
    inline void set_x_register(data_t value) {
        if (is_index_16bit()) {
            set(REG_X, static_cast<uint8_t>(value & 0xFF));
            set(REG_XH, static_cast<uint8_t>((value >> 8) & 0xFF));
        } else {
            set(REG_X, static_cast<uint8_t>(value & 0xFF));
        }
    }
    
    inline data_t get_y_register() const {
        if (is_index_16bit()) {
            return get(REG_Y) | (get(REG_YH) << 8);
        } else {
            return get(REG_Y);
        }
    }
    
    inline void set_y_register(data_t value) {
        if (is_index_16bit()) {
            set(REG_Y, static_cast<uint8_t>(value & 0xFF));
            set(REG_YH, static_cast<uint8_t>((value >> 8) & 0xFF));
        } else {
            set(REG_Y, static_cast<uint8_t>(value & 0xFF));
        }
    }
    
    inline uint16_t get_stack_pointer() const {
        return get(REG_SPL) | (get(REG_SPH) << 8);
    }
    
    inline void set_stack_pointer(uint16_t value) {
        set(REG_SPL, static_cast<uint8_t>(value & 0xFF));
        set(REG_SPH, static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    // === 65C816 extended register access ===
    inline bool get_emulation_mode() const {
        return emulation_mode;
    }
    
    inline void set_emulation_mode(bool mode) {
        emulation_mode = mode;
        if (mode) {
            // Force 8-bit modes in emulation mode
            set(REG_P, get(REG_P) | (FLAG_M | FLAG_X));
            // Clear high bytes of accumulator and index registers
            set(REG_AH, 0);
            set(REG_XH, 0);
            set(REG_YH, 0);
            // Force stack pointer to page 1
            set(REG_SPH, 0x01);
        }
    }
    
    // Note: Use regular get(REG_*)/set(REG_*, value) functions instead of helper functions
    // This provides direct access without additional function call overhead
    
    // === 16-bit accumulator operations (respects M flag) ===
    inline uint16_t get_accumulator_16() const {
        return get_accumulator();
    }
    
    inline void set_accumulator_16(uint16_t value) {
        set_accumulator(value);
    }
    
    // === 16-bit X register operations (respects X flag) ===
    inline uint16_t get_x_16() const {
        return get_x_register();
    }
    
    inline void set_x_16(uint16_t value) {
        set_x_register(value);
    }
    
    // === 16-bit Y register operations (respects X flag) ===
    inline uint16_t get_y_16() const {
        return get_y_register();
    }
    
    inline void set_y_16(uint16_t value) {
        set_y_register(value);
    }
    
    // === Address calculation (24-bit with banking) ===
    inline uint32_t calc_effective_address(uint16_t addr) const {
        return (get(REG_DBR) << 16) | addr;
    }
    
    inline uint32_t calc_program_address(uint16_t addr) const {
        return (get(REG_PBR) << 16) | addr;
    }
    
    // === 24-bit address formation (for 65C816 addressing) ===
    inline uint32_t get_full_address(uint16_t offset = 0) const {
        uint16_t pc = get(REG_PCL) | (get(REG_PCH) << 8);
        return (get(REG_PBR) << 16) | (pc + offset);
    }
    
    inline uint32_t get_data_address(uint16_t offset) const {
        return (get(REG_DBR) << 16) | offset;
    }
    
    inline uint32_t get_direct_address(uint8_t offset) const {
        uint16_t d_reg = get16(REG_D);  // Use the proper 16-bit D register access
        return (get(REG_DBR) << 16) |
               ((d_reg + offset) & 0xFFFF);
    }
    
    // === Load function for bus operations ===
    inline void load(uint8_t reg, bus_state_t pins) {
        reg8[reg] = FAM65XX_GET_DATA(pins);
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