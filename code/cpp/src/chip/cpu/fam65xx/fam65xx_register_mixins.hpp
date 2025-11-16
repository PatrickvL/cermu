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
    
    // === Methods moved to fam65xx_t base class to eliminate duplication ===
    // init_registers(), get(), set(), inc(), dec(), load() methods are now in fam65xx_t
    
    // === 65C816 compatibility methods removed - now in main fam65xx_t class ===
    // Functions moved to fam65xx_t for constexpr wide register detection
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
    
    // === Methods moved to fam65xx_t base class to eliminate duplication ===
    // init_registers(), get(), set(), inc(), dec(), load() methods are now in fam65xx_t    
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