#pragma once
/*
 * fam65xx_processor_traits.hpp - CPU Traits System for MOS 65xx Family
 *
 * This file provides the CPUTraits system for compile-time processor 
 * identification and capability detection. It replaces the legacy ProcessorTraits
 * system with a more flexible struct-based approach.
 */

#include <cstdint>
#include <type_traits>

#ifdef __cplusplus

namespace fam65xx {

// ============================================================================
// CPU Core Feature Flags (only CPU execution behavior)
// ============================================================================

// CPUCoreFlags namespace - provides global access to constants for operations files
namespace CPUCoreFlags {
    // === Instruction Set (bits 0-7) ===
    constexpr uint32_t ILLEGAL_OPCODES      = 1 << 0;   // NMOS undocumented opcodes work
    constexpr uint32_t CMOS_BASE            = 1 << 1;   // CMOS instruction set (BRA, STZ, etc.)
    constexpr uint32_t ROCKWELL_BITS        = 1 << 2;   // RMB/SMB/BBR/BBS instructions
    constexpr uint32_t WAI_STP              = 1 << 3;   // WAI and STP instructions
    constexpr uint32_t CE02_EXTENDED        = 1 << 4;   // 65CE02 extensions (Z register, etc.)
    constexpr uint32_t C816_16BIT           = 1 << 5;   // 65C816 16-bit mode
    constexpr uint32_t HUC6280_EXTENDED     = 1 << 6;   // HuC6280 unique instructions
    // Bit 7 reserved
    
    // === Hardware Bugs (bits 8-11) ===
    constexpr uint32_t JMP_INDIRECT_BUG     = 1 << 8;   // JMP ($xxFF) wraps within page
    constexpr uint32_t RMW_DUMMY_WRITE      = 1 << 9;   // RMW writes original (vs dummy read)
    // Bits 10-11 reserved for other quirks
    
    // === Decimal Mode (bits 12-15) ===
    constexpr uint32_t HAS_DECIMAL_MODE     = 1 << 12;  // BCD mode exists (2A03 lacks this)
    constexpr uint32_t BCD_NMOS_FLAGS       = 1 << 13;  // N,V,Z from binary (NMOS) vs BCD (CMOS)
    constexpr uint32_t BCD_EXTRA_CYCLE      = 1 << 14;  // CMOS takes extra cycle in decimal
    // Bit 15 reserved
    
    // === Memory & Banking (bits 16-19) ===
    constexpr uint32_t HAS_IO_PORT          = 1 << 16;  // Memory-mapped I/O port
    constexpr uint32_t HAS_BANKING          = 1 << 17;  // Banking/MMU present
    // Bits 18-19 reserved
    
    // === Interrupts (bits 20-23) ===
    constexpr uint32_t NO_NMI_LINE          = 1 << 20;  // NMI disabled (7501)
    constexpr uint32_t NO_IRQ_LINE          = 1 << 21;  // IRQ disabled (6507)
    // Bits 22-23 reserved
    
    // === Timing (bits 24-27) ===
    constexpr uint32_t OPTIMIZED_CYCLES     = 1 << 24;  // 65CE02 removed dummy cycles
    constexpr uint32_t VARIABLE_CLOCK       = 1 << 25;  // Can switch speeds (8502, HuC6280)
    // Bits 26-27 reserved
}

// Legacy enum for backward compatibility during transition
enum CPUCoreFlags_Enum : uint32_t {
    ILLEGAL_OPCODES      = CPUCoreFlags::ILLEGAL_OPCODES,
    CMOS_BASE            = CPUCoreFlags::CMOS_BASE,
    ROCKWELL_BITS        = CPUCoreFlags::ROCKWELL_BITS,
    WAI_STP              = CPUCoreFlags::WAI_STP,
    CE02_EXTENDED        = CPUCoreFlags::CE02_EXTENDED,
    C816_16BIT           = CPUCoreFlags::C816_16BIT,
    HUC6280_EXTENDED     = CPUCoreFlags::HUC6280_EXTENDED,
    JMP_INDIRECT_BUG     = CPUCoreFlags::JMP_INDIRECT_BUG,
    RMW_DUMMY_WRITE      = CPUCoreFlags::RMW_DUMMY_WRITE,
    HAS_DECIMAL_MODE     = CPUCoreFlags::HAS_DECIMAL_MODE,
    BCD_NMOS_FLAGS       = CPUCoreFlags::BCD_NMOS_FLAGS,
    BCD_EXTRA_CYCLE      = CPUCoreFlags::BCD_EXTRA_CYCLE,
    HAS_IO_PORT          = CPUCoreFlags::HAS_IO_PORT,
    HAS_BANKING          = CPUCoreFlags::HAS_BANKING,
    NO_NMI_LINE          = CPUCoreFlags::NO_NMI_LINE,
    NO_IRQ_LINE          = CPUCoreFlags::NO_IRQ_LINE,
    OPTIMIZED_CYCLES     = CPUCoreFlags::OPTIMIZED_CYCLES,
    VARIABLE_CLOCK       = CPUCoreFlags::VARIABLE_CLOCK,
};

// ============================================================================
// Banking/MMU Type (mutually exclusive)
// ============================================================================

enum class BankingType : uint8_t {
    NONE = 0,           // Standard 64KB flat addressing
    MOS6509,            // 6509 banking (indirect-Y indexed)
    HUC6280,            // HuC6280 8-bank mapper
    CSG4510,            // 4510 MAP instruction
    // Future: Add more as needed
};

// ============================================================================
// Peripheral Configuration (separate from CPU core)
// ============================================================================

enum class SoundChip : uint8_t {
    NONE = 0,
    RICOH_APU,          // 2A03/2A07 5-channel APU
    HUC6280_PSG,        // HuC6280 6-channel PSG
    // Future: Others if needed
};

enum class DMAController : uint8_t {
    NONE = 0,
    CSG4510_DMA,        // 4510 integrated DMA
    RICOH_5A22_DMA,     // 5A22 SNES DMA/HDMA
    // Future: Others
};

struct PeripheralConfig {
    SoundChip sound;
    DMAController dma;
    bool has_timer;     // Simple flag for integrated timer
    
    constexpr bool has_sound() const { return sound != SoundChip::NONE; }
    constexpr bool has_dma() const { return dma != DMAController::NONE; }
};

// ============================================================================
// Complete CPU Trait Structure
// ============================================================================

struct CPUTraits {
    uint32_t core_flags;        // CPUCoreFlags combination
    uint8_t address_bits;       // Address bus width (13, 16, 20, 21, 24)
    uint8_t io_port_mask;       // Bitmask of available I/O pins
    BankingType banking;        // Type of banking/MMU
    PeripheralConfig peripheral; // Integrated peripherals
    
    // === Helper Methods ===
    
    constexpr bool has(uint32_t flag) const {
        return (core_flags & flag) != 0;
    }
    
    constexpr bool is_nmos() const {
        return !has(CPUCoreFlags::CMOS_BASE);
    }
    
    constexpr bool is_cmos() const {
        return has(CPUCoreFlags::CMOS_BASE);
    }
    
    constexpr bool has_extended_instructions() const {
        return (core_flags & 0x7C) != 0;  // Bits 2-6
    }
    
    constexpr bool has_io_port() const {
        return io_port_mask != 0;
    }
    
    constexpr bool io_pin_exists(uint8_t pin) const {
        return (io_port_mask & (1 << pin)) != 0;
    }
    
    constexpr bool has_banking() const {
        return banking != BankingType::NONE;
    }
    
    constexpr uint32_t address_mask() const {
        return (1u << address_bits) - 1;
    }
    
    // Feature detection methods (for template compatibility)
    constexpr bool has_illegal_opcodes() const {
        return has(CPUCoreFlags::ILLEGAL_OPCODES);
    }
    
    constexpr bool has_cmos_enhancements() const {
        return has(CPUCoreFlags::CMOS_BASE);
    }
    
    constexpr bool has_wide_registers() const {
        return has(CPUCoreFlags::C816_16BIT);
    }
    
    constexpr bool has_bcd() const {
        return has(CPUCoreFlags::HAS_DECIMAL_MODE);
    }
    
    constexpr bool has_nmos_bugs() const {
        return has(CPUCoreFlags::JMP_INDIRECT_BUG) || has(CPUCoreFlags::RMW_DUMMY_WRITE);
    }
    
    constexpr bool has_apu() const {
        return peripheral.sound == SoundChip::RICOH_APU;
    }
    
    constexpr bool has_bit_manipulation() const {
        return has(CPUCoreFlags::ROCKWELL_BITS);
    }
};

// ============================================================================
// Common Flag Combinations (DRY principle)
// ============================================================================

namespace CoreFlags {
    // NMOS common flags
    constexpr uint32_t NMOS_BASE = 
        CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG | CPUCoreFlags::RMW_DUMMY_WRITE |
        CPUCoreFlags::HAS_DECIMAL_MODE | CPUCoreFlags::BCD_NMOS_FLAGS;
    
    // CMOS common flags
    constexpr uint32_t CMOS_BASE_FLAGS = 
        CPUCoreFlags::CMOS_BASE | CPUCoreFlags::HAS_DECIMAL_MODE | CPUCoreFlags::BCD_EXTRA_CYCLE;
    
    // CMOS with Rockwell extensions
    constexpr uint32_t ROCKWELL_BASE = 
        CMOS_BASE_FLAGS | CPUCoreFlags::ROCKWELL_BITS;
    
    // Modern WDC (Rockwell + WAI/STP)
    constexpr uint32_t WDC_MODERN = 
        ROCKWELL_BASE | CPUCoreFlags::WAI_STP;
}

// ============================================================================
// Predefined CPU Variants
// ============================================================================

// --- NMOS Family ---

constexpr CPUTraits MOS6502 = {
    .core_flags = CoreFlags::NMOS_BASE,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6507 = {
    .core_flags = CoreFlags::NMOS_BASE | CPUCoreFlags::NO_IRQ_LINE,
    .address_bits = 13,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6504 = {
    .core_flags = CoreFlags::NMOS_BASE,
    .address_bits = 13,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6510 = {
    .core_flags = CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT,
    .address_bits = 16,
    .io_port_mask = 0x3F,  // Pins 0-5
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6510T = MOS6510;  // Identical

constexpr CPUTraits CSG7501 = {
    .core_flags = CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT | CPUCoreFlags::NO_NMI_LINE,
    .address_bits = 16,
    .io_port_mask = 0x5F,  // Pins 0-4, 6 (no pin 5)
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits CSG8502 = {
    .core_flags = CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT | CPUCoreFlags::VARIABLE_CLOCK,
    .address_bits = 16,
    .io_port_mask = 0x7F,  // Pins 0-6 (no pin 7)
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6509 = {
    .core_flags = CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_BANKING,
    .address_bits = 20,
    .io_port_mask = 0x00,
    .banking = BankingType::MOS6509,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits RICOH_2A03 = {
    .core_flags = CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG | CPUCoreFlags::RMW_DUMMY_WRITE,
    // NO HAS_DECIMAL_MODE!
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::RICOH_APU, DMAController::NONE, false}
};

constexpr CPUTraits RICOH_2A07 = RICOH_2A03;  // PAL version

// --- CMOS Family ---

constexpr CPUTraits WDC_65C02_EARLY = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits WDC_65SC02 = WDC_65C02_EARLY;
constexpr CPUTraits GTE_G65SC02 = WDC_65C02_EARLY;
constexpr CPUTraits SYNERTEK_65C02 = WDC_65C02_EARLY;

constexpr CPUTraits ROCKWELL_R65C02 = {
    .core_flags = CoreFlags::ROCKWELL_BASE,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits WDC_W65C02S = {
    .core_flags = CoreFlags::WDC_MODERN,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

// --- Advanced 8-bit Family ---

constexpr CPUTraits CSG_65CE02 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED | CPUCoreFlags::OPTIMIZED_CYCLES,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits CSG_4510 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED | 
                  CPUCoreFlags::OPTIMIZED_CYCLES | CPUCoreFlags::HAS_BANKING,
    .address_bits = 20,
    .io_port_mask = 0x00,
    .banking = BankingType::CSG4510,
    .peripheral = {SoundChip::NONE, DMAController::CSG4510_DMA, false}
};

constexpr CPUTraits HUDSON_HUC6280 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::HUC6280_EXTENDED | 
                  CPUCoreFlags::VARIABLE_CLOCK | CPUCoreFlags::HAS_BANKING,
    .address_bits = 21,
    .io_port_mask = 0x00,
    .banking = BankingType::HUC6280,
    .peripheral = {SoundChip::HUC6280_PSG, DMAController::NONE, true}
};

// --- 16-bit Family ---

constexpr CPUTraits WDC_65C816 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,
    .address_bits = 24,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits RICOH_5A22 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,
    .address_bits = 24,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::RICOH_5A22_DMA, false}
};

// ============================================================================
// Template Helper Functions (for compatibility with existing code)
// ============================================================================

// Helper functions that can be used with CPUTraits instances
template<const CPUTraits& Traits>
constexpr bool has_io_port() {
    return Traits.has_io_port();
}

template<const CPUTraits& Traits>
constexpr bool has_bcd() {
    return Traits.has_bcd();
}

template<const CPUTraits& Traits>
constexpr bool has_illegal_opcodes() {
    return Traits.has_illegal_opcodes();
}

template<const CPUTraits& Traits>
constexpr bool has_cmos_enhancements() {
    return Traits.has_cmos_enhancements();
}

template<const CPUTraits& Traits>
constexpr bool has_bit_manipulation() {
    return Traits.has_bit_manipulation();
}

template<const CPUTraits& Traits>
constexpr bool has_wide_registers() {
    return Traits.has_wide_registers();
}

template<const CPUTraits& Traits>
constexpr bool has_nmos_bugs() {
    return Traits.has_nmos_bugs();
}

template<const CPUTraits& Traits>
constexpr bool has_apu() {
    return Traits.has_apu();
}

} // namespace fam65xx

#endif // __cplusplus