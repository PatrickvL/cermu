#pragma once
/*
 * fam65xx_processor_traits.hpp - Processor Feature Definitions and Detection
 *
 * This file contains the ProcessorFeatures enum, processor tags, ProcessorTraits
 * specializations, and feature detection helper functions. It provides compile-time
 * processor identification and capability detection for the template system.
 */

#include <cstdint>
#include <type_traits>

#ifdef __cplusplus

namespace fam65xx_cpp {

// ============================================================================
// FEATURE FLAGS - Single Source of Truth
// ============================================================================

enum class ProcessorFeatures : uint32_t {
    MOS6502_BASE        = 1 <<  0,  // Basic 6502 instruction set
    ILLEGAL_OPCODES     = 1 <<  1,  // NMOS illegal/undocumented opcodes
    CMOS_ENHANCEMENTS   = 1 <<  2,  // 65C02 enhanced instructions
    BIT_MANIPULATION    = 1 <<  3,  // Rockwell bit manipulation
    WIDE_REGISTERS      = 1 <<  4,  // 65C816 16-bit register modes
    IO_PORT             = 1 <<  5,  // 6510-style I/O port
    BANK_SWITCHING      = 1 <<  6,  // 65C816 bank switching
    NMOS_BUGS           = 1 <<  7,  // NMOS hardware bugs
    DECIMAL_MODE        = 1 <<  8,  // BCD arithmetic support
    DECIMAL_AFFECTS_NZ  = 1 <<  9,  // NMOS decimal N/Z behavior
    ENHANCED_ADDRESSING = 1 << 10,  // 65C02+ addressing modes
    LONG_ADDRESSING     = 1 << 11,  // 65C816 24-bit addressing
    EMULATION_MODE      = 1 << 12,  // 65C816 emulation mode
    NATIVE_MODE         = 1 << 13,  // 65C816 native mode
    WAIT_STATES         = 1 << 14,  // WAI/STP instruction support
    COPROCESSOR         = 1 << 15,  // COP instruction support
    AUDIO_PROCESSING    = 1 << 16,  // Integrated APU (Audio Processing Unit)
    
    // ========================================================================
    // BRIDGE VALUES - Temporary mappings to CPUCoreFlags during migration
    // ========================================================================
    ILLEGAL_OPCODES_BRIDGE     = 1 << 17,  // Maps to CPUCoreFlags::ILLEGAL_OPCODES
    CMOS_BASE_BRIDGE          = 1 << 18,  // Maps to CPUCoreFlags::CMOS_BASE
    ROCKWELL_BITS_BRIDGE      = 1 << 19,  // Maps to CPUCoreFlags::ROCKWELL_BITS
    WAI_STP_BRIDGE            = 1 << 20,  // Maps to CPUCoreFlags::WAI_STP
    C816_16BIT_BRIDGE         = 1 << 21,  // Maps to CPUCoreFlags::C816_16BIT
    JMP_INDIRECT_BUG_BRIDGE   = 1 << 22,  // Maps to CPUCoreFlags::JMP_INDIRECT_BUG
    RMW_DUMMY_WRITE_BRIDGE    = 1 << 23,  // Maps to CPUCoreFlags::RMW_DUMMY_WRITE
    HAS_DECIMAL_MODE_BRIDGE   = 1 << 24,  // Maps to CPUCoreFlags::HAS_DECIMAL_MODE
    BCD_NMOS_FLAGS_BRIDGE     = 1 << 25,  // Maps to CPUCoreFlags::BCD_NMOS_FLAGS
    BCD_EXTRA_CYCLE_BRIDGE    = 1 << 26,  // Maps to CPUCoreFlags::BCD_EXTRA_CYCLE
    HAS_IO_PORT_BRIDGE        = 1 << 27,  // Maps to CPUCoreFlags::HAS_IO_PORT
    HAS_BANKING_BRIDGE        = 1 << 28,  // Maps to CPUCoreFlags::HAS_BANKING
};

// Bitwise operators
constexpr ProcessorFeatures operator|(ProcessorFeatures a, ProcessorFeatures b) {
    return static_cast<ProcessorFeatures>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr ProcessorFeatures operator&(ProcessorFeatures a, ProcessorFeatures b) {
    return static_cast<ProcessorFeatures>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr ProcessorFeatures operator^(ProcessorFeatures a, ProcessorFeatures b) {
    return static_cast<ProcessorFeatures>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

constexpr ProcessorFeatures operator~(ProcessorFeatures a) {
    return static_cast<ProcessorFeatures>(~static_cast<uint32_t>(a));
}

// ============================================================================
// PROCESSOR TAGS
// ============================================================================

struct MOS6502Tag {};
struct MOS6510Tag {};
struct NES6502Tag {};
struct WDC65C02Tag {};
struct Rockwell65C02Tag {};
struct WDC65C816Tag {};

// ============================================================================
// BRIDGE PROCESSOR TAG ALIASES (for incremental migration)
// ============================================================================

// These aliases allow gradual migration from ProcessorTag to CPUTraits
using MOS6502Tag_BRIDGE = MOS6502Tag;
using MOS6510Tag_BRIDGE = MOS6510Tag;
using NES6502Tag_BRIDGE = NES6502Tag;
using WDC65C02Tag_BRIDGE = WDC65C02Tag;
using Rockwell65C02Tag_BRIDGE = Rockwell65C02Tag;
using WDC65C816Tag_BRIDGE = WDC65C816Tag;

// ============================================================================
// UNIFIED PROCESSOR TRAIT SPECIALIZATIONS
// ============================================================================

template<typename ProcessorTag>
struct ProcessorTraits {
    static constexpr ProcessorFeatures features = ProcessorFeatures::MOS6502_BASE;
    static constexpr const char* name = "Unknown Processor";
    static constexpr const char* family = "65xx";
    static constexpr uint16_t year_introduced = 1975;
};

// MOS 6502 - Original NMOS processor
template<>
struct ProcessorTraits<MOS6502Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::ILLEGAL_OPCODES |
        ProcessorFeatures::NMOS_BUGS |
        ProcessorFeatures::DECIMAL_MODE |
        ProcessorFeatures::DECIMAL_AFFECTS_NZ |
        // Bridge flags for migration compatibility
        ProcessorFeatures::ILLEGAL_OPCODES_BRIDGE |
        ProcessorFeatures::NMOS_BUGS_BRIDGE |
        ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE;
    
    static constexpr const char* name = "MOS 6502";
    static constexpr const char* family = "NMOS 6502";
    static constexpr uint16_t year_introduced = 1975;
};

// MOS 6510 - 6502 with I/O port
template<>
struct ProcessorTraits<MOS6510Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::ILLEGAL_OPCODES |
        ProcessorFeatures::NMOS_BUGS |
        ProcessorFeatures::DECIMAL_MODE |
        ProcessorFeatures::DECIMAL_AFFECTS_NZ |
        ProcessorFeatures::IO_PORT |
        // Bridge flags for migration compatibility
        ProcessorFeatures::ILLEGAL_OPCODES_BRIDGE |
        ProcessorFeatures::NMOS_BUGS_BRIDGE |
        ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE |
        ProcessorFeatures::HAS_IO_PORT_BRIDGE;
    
    static constexpr const char* name = "MOS 6510";
    static constexpr const char* family = "NMOS 6502";
    static constexpr uint16_t year_introduced = 1982;
};

// NES 6502 - 6502 without decimal mode but with integrated APU
template<>
struct ProcessorTraits<NES6502Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::ILLEGAL_OPCODES |
        ProcessorFeatures::NMOS_BUGS |
        ProcessorFeatures::AUDIO_PROCESSING |
        // Bridge flags for migration compatibility
        ProcessorFeatures::ILLEGAL_OPCODES_BRIDGE |
        ProcessorFeatures::NMOS_BUGS_BRIDGE |
        ProcessorFeatures::AUDIO_PROCESSING_BRIDGE;
    
    static constexpr const char* name = "Ricoh 2A03";
    static constexpr const char* family = "NES 6502";
    static constexpr uint16_t year_introduced = 1983;
};

// WDC 65C02 - CMOS with enhancements
template<>
struct ProcessorTraits<WDC65C02Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::CMOS_ENHANCEMENTS |
        ProcessorFeatures::ENHANCED_ADDRESSING |
        ProcessorFeatures::DECIMAL_MODE |
        ProcessorFeatures::WAIT_STATES |
        // Bridge flags for migration compatibility
        ProcessorFeatures::CMOS_BASE_BRIDGE |
        ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE;
    
    static constexpr const char* name = "WDC 65C02";
    static constexpr const char* family = "CMOS 65C02";
    static constexpr uint16_t year_introduced = 1982;
};

// Rockwell 65C02 - CMOS with bit manipulation
template<>
struct ProcessorTraits<Rockwell65C02Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::CMOS_ENHANCEMENTS |
        ProcessorFeatures::BIT_MANIPULATION |
        ProcessorFeatures::ENHANCED_ADDRESSING |
        ProcessorFeatures::DECIMAL_MODE |
        ProcessorFeatures::WAIT_STATES |
        // Bridge flags for migration compatibility
        ProcessorFeatures::CMOS_BASE_BRIDGE |
        ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE;
    
    static constexpr const char* name = "Rockwell R65C02";
    static constexpr const char* family = "CMOS 65C02";
    static constexpr uint16_t year_introduced = 1983;
};

// WDC 65C816 - 16-bit extension
template<>
struct ProcessorTraits<WDC65C816Tag> {
    static constexpr ProcessorFeatures features =
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::CMOS_ENHANCEMENTS |
        ProcessorFeatures::WIDE_REGISTERS |
        ProcessorFeatures::BANK_SWITCHING |
        ProcessorFeatures::ENHANCED_ADDRESSING |
        ProcessorFeatures::LONG_ADDRESSING |
        ProcessorFeatures::DECIMAL_MODE |
        ProcessorFeatures::EMULATION_MODE |
        ProcessorFeatures::NATIVE_MODE |
        ProcessorFeatures::WAIT_STATES |
        ProcessorFeatures::COPROCESSOR |
        // Bridge flags for migration compatibility
        ProcessorFeatures::CMOS_BASE_BRIDGE |
        ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE |
        ProcessorFeatures::WIDE_REGISTERS_BRIDGE;
    
    static constexpr const char* name = "WDC 65C816";
    static constexpr const char* family = "16-bit 65816";
    static constexpr uint16_t year_introduced = 1984;
};

// ============================================================================
// FEATURE DETECTION HELPERS
// ============================================================================

// Primary feature query function - all other queries should use this
template<typename ProcessorTag>
constexpr bool has_feature(ProcessorFeatures feature) {
    return (ProcessorTraits<ProcessorTag>::features & feature) == feature;
}

// Individual feature detection functions for cleaner template conditions
template<typename ProcessorTag>
constexpr bool has_io_port() {
    return has_feature<ProcessorTag>(ProcessorFeatures::IO_PORT);
}

template<typename ProcessorTag>  
constexpr bool has_bcd() {
    return has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE);
}

template<typename ProcessorTag>
constexpr bool has_illegal_opcodes() {
    return has_feature<ProcessorTag>(ProcessorFeatures::ILLEGAL_OPCODES);
}

template<typename ProcessorTag>
constexpr bool has_cmos_enhancements() {
    return has_feature<ProcessorTag>(ProcessorFeatures::CMOS_ENHANCEMENTS);
}

template<typename ProcessorTag>
constexpr bool has_bit_manipulation() {
    return has_feature<ProcessorTag>(ProcessorFeatures::BIT_MANIPULATION);
}

template<typename ProcessorTag>
constexpr bool has_wide_registers() {
    return has_feature<ProcessorTag>(ProcessorFeatures::WIDE_REGISTERS);
}

template<typename ProcessorTag>
constexpr bool has_nmos_bugs() {
    return has_feature<ProcessorTag>(ProcessorFeatures::NMOS_BUGS);
}

template<typename ProcessorTag>
constexpr bool has_apu() {
    return has_feature<ProcessorTag>(ProcessorFeatures::AUDIO_PROCESSING);
}

// Multiple feature queries
template<typename ProcessorTag>
constexpr bool has_any_feature(ProcessorFeatures features) {
    return (ProcessorTraits<ProcessorTag>::features & features) != static_cast<ProcessorFeatures>(0);
}

template<typename ProcessorTag>
constexpr bool has_all_features(ProcessorFeatures features) {
    return (ProcessorTraits<ProcessorTag>::features & features) == features;
}

// Get complete feature set
template<typename ProcessorTag>
constexpr ProcessorFeatures get_features() {
    return ProcessorTraits<ProcessorTag>::features;
}

// Processor classification helpers
template<typename ProcessorTag>
constexpr bool is_nmos() {
    return has_feature<ProcessorTag>(ProcessorFeatures::NMOS_BUGS);
}

template<typename ProcessorTag>
constexpr bool is_cmos() {
    return has_feature<ProcessorTag>(ProcessorFeatures::CMOS_ENHANCEMENTS);
}

template<typename ProcessorTag>
constexpr bool is_16bit() {
    return has_feature<ProcessorTag>(ProcessorFeatures::WIDE_REGISTERS);
}

// Metadata access functions
template<typename ProcessorTag>
constexpr const char* get_processor_name() {
    return ProcessorTraits<ProcessorTag>::name;
}

template<typename ProcessorTag>
constexpr const char* get_processor_family() {
    return ProcessorTraits<ProcessorTag>::family;
}

template<typename ProcessorTag>
constexpr uint16_t get_year_introduced() {
    return ProcessorTraits<ProcessorTag>::year_introduced;
}

// ============================================================================
// BRIDGE FEATURE DETECTION FUNCTIONS (for incremental migration)
// ============================================================================

// Bridge functions that map ProcessorTraits to CPUTraits flags
template<typename ProcessorTag>
constexpr bool has_illegal_opcodes_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::ILLEGAL_OPCODES_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_cmos_base_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::CMOS_BASE_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_decimal_mode_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::HAS_DECIMAL_MODE_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_nmos_bugs_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::NMOS_BUGS_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_io_port_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::HAS_IO_PORT_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_wide_registers_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::WIDE_REGISTERS_BRIDGE);
}

template<typename ProcessorTag>
constexpr bool has_apu_bridge() {
    return has_feature<ProcessorTag>(ProcessorFeatures::AUDIO_PROCESSING_BRIDGE);
}

// Helper to convert ProcessorTag to CPUTraits (for future migration)
template<typename ProcessorTag>
constexpr CPUTraits get_cpu_traits_for_processor();

// Specializations will be added in later phases
template<> constexpr CPUTraits get_cpu_traits_for_processor<MOS6502Tag>() { return MOS6502; }
template<> constexpr CPUTraits get_cpu_traits_for_processor<MOS6510Tag>() { return MOS6510; }
template<> constexpr CPUTraits get_cpu_traits_for_processor<NES6502Tag>() { return RICOH_2A03; }
template<> constexpr CPUTraits get_cpu_traits_for_processor<WDC65C02Tag>() { return WDC_65C02_EARLY; }
template<> constexpr CPUTraits get_cpu_traits_for_processor<Rockwell65C02Tag>() { return ROCKWELL_R65C02; }
template<> constexpr CPUTraits get_cpu_traits_for_processor<WDC65C816Tag>() { return WDC_65C816; }

// ============================================================================
// Refined CPU Traits System - Core vs Peripherals Separated
// ============================================================================

// ============================================================================
// CPU Core Feature Flags (only CPU execution behavior)
// ============================================================================

enum CPUCoreFlags : uint32_t {
    // === Instruction Set (bits 0-7) ===
    ILLEGAL_OPCODES      = 1 << 0,   // NMOS undocumented opcodes work
    CMOS_BASE            = 1 << 1,   // CMOS instruction set (BRA, STZ, etc.)
    ROCKWELL_BITS        = 1 << 2,   // RMB/SMB/BBR/BBS instructions
    WAI_STP              = 1 << 3,   // WAI and STP instructions
    CE02_EXTENDED        = 1 << 4,   // 65CE02 extensions (Z register, etc.)
    C816_16BIT           = 1 << 5,   // 65C816 16-bit mode
    HUC6280_EXTENDED     = 1 << 6,   // HuC6280 unique instructions
    // Bit 7 reserved
    
    // === Hardware Bugs (bits 8-11) ===
    JMP_INDIRECT_BUG     = 1 << 8,   // JMP ($xxFF) wraps within page
    RMW_DUMMY_WRITE      = 1 << 9,   // RMW writes original (vs dummy read)
    // Bits 10-11 reserved for other quirks
    
    // === Decimal Mode (bits 12-15) ===
    HAS_DECIMAL_MODE     = 1 << 12,  // BCD mode exists (2A03 lacks this)
    BCD_NMOS_FLAGS       = 1 << 13,  // N,V,Z from binary (NMOS) vs BCD (CMOS)
    BCD_EXTRA_CYCLE      = 1 << 14,  // CMOS takes extra cycle in decimal
    // Bit 15 reserved
    
    // === Memory & Banking (bits 16-19) ===
    HAS_IO_PORT          = 1 << 16,  // Memory-mapped I/O port
    HAS_BANKING          = 1 << 17,  // Banking/MMU present
    // Bits 18-19 reserved
    
    // === Interrupts (bits 20-23) ===
    NO_NMI_LINE          = 1 << 20,  // NMI disabled (7501)
    NO_IRQ_LINE          = 1 << 21,  // IRQ disabled (6507)
    // Bits 22-23 reserved
    
    // === Timing (bits 24-27) ===
    OPTIMIZED_CYCLES     = 1 << 24,  // 65CE02 removed dummy cycles
    VARIABLE_CLOCK       = 1 << 25,  // Can switch speeds (8502, HuC6280)
    // Bits 26-27 reserved
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
    
    constexpr bool has(CPUCoreFlags flag) const {
        return (core_flags & flag) != 0;
    }
    
    constexpr bool is_nmos() const {
        return !has(CMOS_BASE);
    }
    
    constexpr bool is_cmos() const {
        return has(CMOS_BASE);
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
};

// ============================================================================
// Common Flag Combinations (DRY principle)
// ============================================================================

namespace CoreFlags {
    // NMOS common flags
    constexpr uint32_t NMOS_BASE = 
        ILLEGAL_OPCODES | JMP_INDIRECT_BUG | RMW_DUMMY_WRITE |
        HAS_DECIMAL_MODE | BCD_NMOS_FLAGS;
    
    // CMOS common flags
    constexpr uint32_t CMOS_BASE_FLAGS = 
        CMOS_BASE | HAS_DECIMAL_MODE | BCD_EXTRA_CYCLE;
    
    // CMOS with Rockwell extensions
    constexpr uint32_t ROCKWELL_BASE = 
        CMOS_BASE_FLAGS | ROCKWELL_BITS;
    
    // Modern WDC (Rockwell + WAI/STP)
    constexpr uint32_t WDC_MODERN = 
        ROCKWELL_BASE | WAI_STP;
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
    .core_flags = CoreFlags::NMOS_BASE | NO_IRQ_LINE,
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
    .core_flags = CoreFlags::NMOS_BASE | HAS_IO_PORT,
    .address_bits = 16,
    .io_port_mask = 0x3F,  // Pins 0-5
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6510T = MOS6510;  // Identical

constexpr CPUTraits CSG7501 = {
    .core_flags = CoreFlags::NMOS_BASE | HAS_IO_PORT | NO_NMI_LINE,
    .address_bits = 16,
    .io_port_mask = 0x5F,  // Pins 0-4, 6 (no pin 5)
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits CSG8502 = {
    .core_flags = CoreFlags::NMOS_BASE | HAS_IO_PORT | VARIABLE_CLOCK,
    .address_bits = 16,
    .io_port_mask = 0x7F,  // Pins 0-6 (no pin 7)
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits MOS6509 = {
    .core_flags = CoreFlags::NMOS_BASE | HAS_BANKING,
    .address_bits = 20,
    .io_port_mask = 0x00,
    .banking = BankingType::MOS6509,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits RICOH_2A03 = {
    .core_flags = ILLEGAL_OPCODES | JMP_INDIRECT_BUG | RMW_DUMMY_WRITE,
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
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CE02_EXTENDED | OPTIMIZED_CYCLES,
    .address_bits = 16,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits CSG_4510 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | CE02_EXTENDED | 
                  OPTIMIZED_CYCLES | HAS_BANKING,
    .address_bits = 20,
    .io_port_mask = 0x00,
    .banking = BankingType::CSG4510,
    .peripheral = {SoundChip::NONE, DMAController::CSG4510_DMA, false}
};

constexpr CPUTraits HUDSON_HUC6280 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | HUC6280_EXTENDED | 
                  VARIABLE_CLOCK | HAS_BANKING,
    .address_bits = 21,
    .io_port_mask = 0x00,
    .banking = BankingType::HUC6280,
    .peripheral = {SoundChip::HUC6280_PSG, DMAController::NONE, true}
};

// --- 16-bit Family ---

constexpr CPUTraits WDC_65C816 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | C816_16BIT,
    .address_bits = 24,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::NONE, false}
};

constexpr CPUTraits RICOH_5A22 = {
    .core_flags = CoreFlags::CMOS_BASE_FLAGS | C816_16BIT,
    .address_bits = 24,
    .io_port_mask = 0x00,
    .banking = BankingType::NONE,
    .peripheral = {SoundChip::NONE, DMAController::RICOH_5A22_DMA, false}
};

/* ============================================================================
// Usage Example
// ============================================================================

template<CPUTraits Traits>
class CPU6502Core {
public:
    void execute() {
        // Core CPU execution based on traits
        if constexpr (Traits.has(ILLEGAL_OPCODES)) {
            // Handle illegal opcodes
        }
        
        if constexpr (Traits.has(ROCKWELL_BITS)) {
            // Handle bit manipulation instructions
        }
        
        // Peripherals are separate concerns
        if constexpr (Traits.peripheral.sound == SoundChip::RICOH_APU) {
            // APU is a separate subsystem, not part of CPU core
            // apu.tick();
        }
    }
    
    uint8_t read_memory(uint16_t addr) {
        // Handle I/O port
        if constexpr (Traits.has_io_port()) {
            if (addr <= 0x0001) {
                return read_io_port(addr);
            }
        }
        
        // Apply banking
        if constexpr (Traits.has_banking()) {
            addr = apply_banking(addr);
        }
        
        // Apply address mask
        addr &= Traits.address_mask();
        
        return memory[addr];
    }

private:
    uint32_t apply_banking(uint16_t addr) {
        if constexpr (Traits.banking == BankingType::MOS6509) {
            // 6509 banking logic
            return ((uint32_t)bank_register << 16) | addr;
        } else if constexpr (Traits.banking == BankingType::HUC6280) {
            // HuC6280 8-bank mapper
            uint8_t bank = mpr[addr >> 13];
            return ((uint32_t)bank << 13) | (addr & 0x1FFF);
        } else if constexpr (Traits.banking == BankingType::CSG4510) {
            // 4510 MAP instruction
            return map_translate(addr);
        }
        return addr;
    }
    
    uint8_t read_io_port(uint16_t addr);
    uint32_t map_translate(uint16_t addr);
    
    uint8_t bank_register;
    uint8_t mpr[8];  // HuC6280 mapper registers
    uint8_t memory[0x1000000];  // Max 24-bit addressing
};

*/

} // namespace fam65xx_cpp

#endif // __cplusplus