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

namespace fam65xx {

// ============================================================================
// CPU Core Feature Flags (only CPU execution behavior)
// ============================================================================

// CPUCoreFlags namespace - provides global access to constants for operations files
namespace CPUCoreFlags {
    // === Instruction Set (bits 0-7) ===
    constexpr uint32_t ILLEGAL_OPCODES      = 1 << 0;   // NMOS undocumented opcodes work ✓ IMPLEMENTED
    constexpr uint32_t CMOS_BASE            = 1 << 1;   // CMOS instruction set (BRA, STZ, etc.) ✓ IMPLEMENTED
    constexpr uint32_t ROCKWELL_BITS        = 1 << 2;   // RMB/SMB/BBR/BBS instructions ✓ IMPLEMENTED
    constexpr uint32_t WAI_STP              = 1 << 3;   // WAI and STP instructions ✓ IMPLEMENTED
    constexpr uint32_t CE02_EXTENDED        = 1 << 4;   // TODO: 65CE02 extensions (Z register, etc.)
    constexpr uint32_t C816_16BIT           = 1 << 5;   // 65C816 16-bit mode ✓ IMPLEMENTED
    constexpr uint32_t HUC6280_EXTENDED     = 1 << 6;   // TODO: HuC6280 unique instructions
    // Bit 7 reserved
    
    // === Hardware Bugs (bits 8-11) ===
    constexpr uint32_t JMP_INDIRECT_BUG     = 1 << 8;   // JMP ($xxFF) wraps within page ✓ IMPLEMENTED
    constexpr uint32_t RMW_DUMMY_WRITE      = 1 << 9;   // ✓ IMPLEMENTED: RMW writes original (vs dummy read)
    // Bits 10-11 reserved for other quirks
    
    // === Decimal Mode (bits 12-15) ===
    constexpr uint32_t HAS_DECIMAL_MODE     = 1 << 12;  // BCD mode exists (2A03 lacks this) ✓ IMPLEMENTED
    constexpr uint32_t BCD_NMOS_FLAGS       = 1 << 13;  // N,V,Z from binary (NMOS) vs BCD (CMOS) ✓ IMPLEMENTED
    constexpr uint32_t BCD_EXTRA_CYCLE      = 1 << 14;  // CMOS takes extra cycle in decimal ✓ IMPLEMENTED
    // Bit 15 reserved
    
    // === Memory & Banking (bits 16-19) ===
    constexpr uint32_t HAS_IO_PORT          = 1 << 16;  // Memory-mapped I/O port ✓ IMPLEMENTED
    constexpr uint32_t HAS_BANKING          = 1 << 17;  // TODO: Banking/MMU present
    // Bits 18-19 reserved
    
    // === Interrupts (bits 20-23) ===
    constexpr uint32_t NO_NMI_LINE          = 1 << 20;  // ✓ IMPLEMENTED: NMI disabled (7501)
    constexpr uint32_t NO_IRQ_LINE          = 1 << 21;  // ✓ IMPLEMENTED: IRQ disabled (6507)
    // Bits 22-23 reserved
    
    // === Timing (bits 24-27) ===
    constexpr uint32_t OPTIMIZED_CYCLES     = 1 << 24;  // ✓ IMPLEMENTED: 65CE02 removed dummy cycles - implemented in flags.inc.hpp
    constexpr uint32_t VARIABLE_CLOCK       = 1 << 25;  // TODO: Can switch speeds (8502, HuC6280)
    constexpr uint32_t ACCURATE_INTERNAL_CYCLES = 1 << 26;  // Simulate internal dummy cycles for accuracy
    constexpr uint32_t UPDATE_BUS_LINES     = 1 << 27;  // Update bus pins during simulation
}


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
    const char* vendor;         // Manufacturer name (e.g., "MOS Technology", "Ricoh", "WDC")
    const char* chip_id;        // Chip identifier (e.g., "6502", "6510", "2A03")
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
        return ~(~0ull << address_bits);
    }
    
    // Feature detection methods (for template compatibility)
    constexpr bool has_illegal_opcodes() const {
        return has(CPUCoreFlags::ILLEGAL_OPCODES);
    }
    
    // String identification methods
    constexpr const char* get_vendor() const {
        return vendor;
    }
    
    constexpr const char* get_chip_id() const {
        return chip_id;
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
    
    // Additional traits for phi2_access template method
    constexpr bool accurate_internal_cycles() const {
        return has(CPUCoreFlags::ACCURATE_INTERNAL_CYCLES);
    }
    
    constexpr bool update_bus_lines() const {
        return has(CPUCoreFlags::UPDATE_BUS_LINES);
    }
    
    // Equality operator for constexpr comparisons
    // Since CPUTraits instances use string literals, we can use pointer comparison
    constexpr bool operator==(const CPUTraits& other) const {
        return vendor == other.vendor &&
               chip_id == other.chip_id &&
               core_flags == other.core_flags &&
               address_bits == other.address_bits &&
               io_port_mask == other.io_port_mask &&
               banking == other.banking &&
               peripheral.sound == other.peripheral.sound &&
               peripheral.dma == other.peripheral.dma &&
               peripheral.has_timer == other.peripheral.has_timer;
    }
    
    constexpr bool operator!=(const CPUTraits& other) const {
        return !(*this == other);
    }
};

// ============================================================================
// Common Flag Combinations (DRY principle)
// ============================================================================

namespace CoreFlags {
    // NMOS common flags
    constexpr uint32_t NMOS_BASE = 
        CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG | CPUCoreFlags::RMW_DUMMY_WRITE |
        CPUCoreFlags::HAS_DECIMAL_MODE | CPUCoreFlags::BCD_NMOS_FLAGS |
        CPUCoreFlags::ACCURATE_INTERNAL_CYCLES | CPUCoreFlags::UPDATE_BUS_LINES;
    
    // CMOS common flags
    constexpr uint32_t CMOS_BASE_FLAGS =
        CPUCoreFlags::CMOS_BASE | CPUCoreFlags::HAS_DECIMAL_MODE | CPUCoreFlags::BCD_EXTRA_CYCLE |
        CPUCoreFlags::ACCURATE_INTERNAL_CYCLES | CPUCoreFlags::UPDATE_BUS_LINES;
    
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
    "MOS Technology",                                                   // vendor
    "6502",                                                             // chip_id
    CoreFlags::NMOS_BASE,                                               // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits MOS6504 = {
    "MOS Technology",                                                   // vendor
    "6504",                                                             // chip_id
    CoreFlags::NMOS_BASE,                                               // core_flags
    13,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits MOS6507 = {
    "MOS Technology",                                                   // vendor
    "6507",                                                             // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::NO_IRQ_LINE,                   // core_flags
    13,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits MOS6509 = {
    "MOS Technology",                                                   // vendor
    "6509",                                                             // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_BANKING,                   // core_flags
    20,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::MOS6509,                                               // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits MOS6510 = {
    "MOS Technology",                                                   // vendor
    "6510",                                                             // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT,                   // core_flags
    16,                                                                 // address_bits
    0x3F,                                                               // io_port_mask (Pins 0-5)
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits MOS6510T = MOS6510;  // Identical

constexpr CPUTraits CSG7501 = {
    "Commodore",                                                        // vendor
    "7501",                                                             // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT | CPUCoreFlags::NO_NMI_LINE, // core_flags
    16,                                                                 // address_bits
    0x5F,                                                               // io_port_mask (Pins 0-4, 6 - no pin 5)
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits CSG8502 = {
    "Commodore",                                                        // vendor
    "CSG8502",                                                          // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT | CPUCoreFlags::VARIABLE_CLOCK, // core_flags
    16,                                                                 // address_bits
    0x7F,                                                               // io_port_mask (Pins 0-6, no pin 7)
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits RICOH_2A03 = {
    "Ricoh",                                                            // vendor
    "2A03",                                                             // chip_id
    CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG | CPUCoreFlags::RMW_DUMMY_WRITE, // core_flags (NO HAS_DECIMAL_MODE!)
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::RICOH_APU, DMAController::NONE, false}                  // peripheral
};

constexpr CPUTraits RICOH_2A07 = RICOH_2A03;  // PAL version

// --- CMOS Family ---

constexpr CPUTraits WDC_65C02_EARLY = {
    "WDC",                                                              // vendor
    "65C02",                                                            // chip_id
    CoreFlags::CMOS_BASE_FLAGS,                                         // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits WDC_65SC02 = WDC_65C02_EARLY;
constexpr CPUTraits GTE_G65SC02 = WDC_65C02_EARLY;

constexpr CPUTraits SYNERTEK_65C02 = {
    "Synertek",                                                         // vendor
    "65C02",                                                            // chip_id
    CPUCoreFlags::CMOS_BASE | CPUCoreFlags::HAS_DECIMAL_MODE | CPUCoreFlags::BCD_EXTRA_CYCLE, // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits ROCKWELL_R65C02 = {
    "Rockwell",                                                         // vendor
    "R65C02",                                                           // chip_id
    CoreFlags::ROCKWELL_BASE,                                           // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits WDC_W65C02S = {
    "WDC",                                                              // vendor
    "W65C02S",                                                          // chip_id
    CoreFlags::WDC_MODERN,                                              // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

// --- Advanced 8-bit Family ---

constexpr CPUTraits CSG_65CE02 = {
    "Commodore",                                                        // vendor
    "65CE02",                                                           // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED | CPUCoreFlags::OPTIMIZED_CYCLES, // core_flags
    16,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits CSG_4510 = {
    "Commodore",                                                        // vendor
    "4510",                                                             // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED | CPUCoreFlags::OPTIMIZED_CYCLES | CPUCoreFlags::HAS_BANKING, // core_flags
    20,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::CSG4510,                                               // banking
    {SoundChip::NONE, DMAController::CSG4510_DMA, false}                // peripheral
};

constexpr CPUTraits HUDSON_HUC6280 = {
    "Hudson Soft",                                                      // vendor
    "HuC6280",                                                          // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::HUC6280_EXTENDED | CPUCoreFlags::VARIABLE_CLOCK | CPUCoreFlags::HAS_BANKING, // core_flags
    21,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::HUC6280,                                               // banking
    {SoundChip::HUC6280_PSG, DMAController::NONE, true}                 // peripheral
};

// --- 16-bit Family ---

constexpr CPUTraits WDC_65C816 = {
    "WDC",                                                              // vendor
    "65C816",                                                           // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,              // core_flags
    24,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::NONE, false}                       // peripheral
};

constexpr CPUTraits RICOH_5A22 = {
    "Ricoh",                                                            // vendor
    "5A22",                                                             // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,              // core_flags
    24,                                                                 // address_bits
    0x00,                                                               // io_port_mask
    BankingType::NONE,                                                  // banking
    {SoundChip::NONE, DMAController::RICOH_5A22_DMA, false}             // peripheral
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