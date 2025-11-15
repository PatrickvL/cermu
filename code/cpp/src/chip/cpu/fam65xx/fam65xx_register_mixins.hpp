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
        uint8_t reg8[REG_COUNT_8BIT];        // 8-bit register access
        uint16_t reg16[REG_COUNT_8BIT / 2];  // 16-bit pair access (little-endian)
    };
    
    // Initialize registers
    void init_registers() {
        memset(&reg8, 0, sizeof(reg8));

        // Initialize P register like a 6502 (only standard 6502 flags)
        set(REG_P, FLAG_U | FLAG_I);  // Only unused bit and interrupt disable (6502-compatible)
        
        /* Initialize register layout:
        * SP = 0x01FF (stack starts at top of page 1)
        */
        this->set(REG_SP, 0x01FF); /* Stack pointer (page 1, starts at 0xFF) */
    }
    
    // === Type-safe 8-bit register accessors ===
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
    
    // === Type-safe 16-bit register accessors ===
    inline uint16_t get(reg16_t reg_pair) const {
        return reg16[reg_pair];
    }
    
    inline void set(reg16_t reg_pair, uint16_t value) {
        reg16[reg_pair] = value;
    }
    
    inline void inc(reg16_t reg_pair) {
        reg16[reg_pair]++;
    }
    
    inline void dec(reg16_t reg_pair) {
        reg16[reg_pair]--;
    }
    
    // === Load function for bus operations ===
    inline void load(reg8_t data_reg, bus_state_t pins) {
        reg8[data_reg] = FAM65XX_GET_DATA(pins);
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
    
    // === 65C816 compatibility methods removed - now in main fam65xx_t class ===
    // Functions moved to fam65xx_t for constexpr wide register detection
    
    template<reg8_t reg_type>
    inline bool is_register_16bit() const {
        return false;  // Always 8-bit for narrow CPUs
    }
};

// ============================================================================
// WIDE REGISTERS MIXIN (65C816)
// ============================================================================

template<const CPUTraits& Traits>
struct wide_registers_mixin_t {
    // Data type alias - 16-bit for wide CPUs
    using data_t = uint16_t;
    
    // Register array with proper 8/16-bit alignment - matches narrow_registers_mixin_t structure
    union {
        uint8_t reg8[REG_COUNT_16BIT];        // 8-bit register access
        uint16_t reg16[REG_COUNT_16BIT / 2];  // 16-bit pair access (little-endian)
    };
    
    // Initialize registers
    void init_registers() {
        memset(&reg8, 0, sizeof(reg8));
        
        // Initialize P register like a 6502 (only standard 6502 flags)
        set(REG_P, FLAG_U | FLAG_I);  // Only unused bit and interrupt disable (6502-compatible)
        
        // Initialize emulation mode in high byte of P register using FLAG_E
        set(REG_PH, static_cast<uint8_t>(FLAG_E >> 8));  // Set emulation mode bit in high byte
        
        // Initialize stack pointer to page 1 (6502 compatible)
        set(REG_SP, 0x01FF);
        
        // Initialize 65C816-specific registers to 6502-compatible values
        set(REG_D, 0x0000);    // Direct Page register = $0000 (behaves like Zero Page)
        set(REG_DBR, 0x00);    // Data Bank Register = $00
        set(REG_PBR, 0x00);    // Program Bank Register = $00
        set(REG_ZBR, 0x00);    // Zero Bank Register = $00
    }   
    
    // === 65C816 extended register access methods removed - now in main fam65xx_t class ===
    // Functions moved to fam65xx_t for constexpr wide register detection

    // Template version for compile-time register type selection
    template<reg8_t reg_type>
    inline bool is_register_16bit() const {
        // Note: This will be overridden in the main fam65xx_t class
        // For now, return false since the actual logic is in fam65xx_t
        return false;  // Will be overridden by fam65xx_t implementation
    }
    
    // Fast XCE (Exchange Carry with Emulation) operation helper
    inline void exchange_carry_emulation() {
        uint16_t p_reg = get(REG_P_16);
        bool old_carry = (p_reg & FLAG_C) != 0;
        bool old_emulation = (p_reg & FLAG_E) != 0;
        
        // Set carry to old emulation state
        if (old_emulation) {
            p_reg |= FLAG_C;
        } else {
            p_reg &= ~FLAG_C;
        }
        
        // Set emulation to old carry state
        if (old_carry) {
            p_reg |= FLAG_E;
        } else {
            p_reg &= ~FLAG_E;
        }
        
        set(REG_P_16, p_reg);
        
        // Handle emulation mode side effects
        if (old_carry) {  // Switching to emulation mode
            set(REG_SPH, 0x01);  // Force stack to page 1
            set(REG_P, get(REG_P) & ~(FLAG_M | FLAG_X));  // Hide M/X flags
        }
    }
    
    // === Type-safe 8-bit register accessors ===
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
    
    // === Type-safe 16-bit register accessors ===
    inline uint16_t get(reg16_t reg_pair) const {
        return reg16[reg_pair];
    }
    
    inline void set(reg16_t reg_pair, uint16_t value) {
        reg16[reg_pair] = value;
    }
    
    inline void inc(reg16_t reg_pair) {
        reg16[reg_pair]++;
    }
    
    inline void dec(reg16_t reg_pair) {
        reg16[reg_pair]--;
    }
    
    // === Load function for bus operations ===
    inline void load(reg8_t data_reg, bus_state_t pins) {
        reg8[data_reg] = FAM65XX_GET_DATA(pins);
    }
    
    // === Memory operation helpers (simplified - width logic moved to fam65xx_t) ===
    // Note: These will be overridden in fam65xx_t with proper 16-bit width detection
    inline data_t get_accumulator() const {
        return get(REG_A);  // Always 8-bit in mixin, overridden in fam65xx_t
    }
    
    inline void set_accumulator(data_t value) {
        set(REG_A, static_cast<uint8_t>(value & 0xFF));  // Always 8-bit in mixin, overridden in fam65xx_t
    }
    
    inline data_t get_x_register() const {
        return get(REG_X);  // Always 8-bit in mixin, overridden in fam65xx_t
    }
    
    inline void set_x_register(data_t value) {
        set(REG_X, static_cast<uint8_t>(value & 0xFF));  // Always 8-bit in mixin, overridden in fam65xx_t
    }
    
    inline data_t get_y_register() const {
        return get(REG_Y);  // Always 8-bit in mixin, overridden in fam65xx_t
    }
    
    inline void set_y_register(data_t value) {
        set(REG_Y, static_cast<uint8_t>(value & 0xFF));  // Always 8-bit in mixin, overridden in fam65xx_t
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