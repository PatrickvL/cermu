#pragma once
/*
 * fam65xx_processor_traits.hpp - Unified Processor Traits System
 *
 * This system provides a single source of truth for all processor characteristics
 * using bitwise feature composition with semantic convenience functions.
 *
 * DESIGN PRINCIPLES:
 * - Single source of truth: only ProcessorFeatures bitwise enum
 * - All queries go through has_feature<ProcessorTag>(ProcessorFeatures::FEATURE)
 * - Consistent API - no duplication between bitwise and boolean approaches
 * - Zero runtime overhead through constexpr
 * - Easy extension through feature composition
 */

#include <cstdint>
#include <type_traits>

#ifdef __cplusplus

namespace fam65xx_core {

// ============================================================================
// FEATURE FLAGS - Single Source of Truth
// ============================================================================

enum class ProcessorFeatures : uint32_t {
    MOS6502_BASE        =  0 << 1,  // Basic 6502 instruction set
    ILLEGAL_OPCODES     =  1 << 1,  // NMOS illegal/undocumented opcodes
    CMOS_ENHANCEMENTS   =  2 << 1,  // 65C02 enhanced instructions
    BIT_MANIPULATION    =  3 << 1,  // Rockwell bit manipulation
    WIDE_REGISTERS      =  4 << 1,  // 65C816 16-bit register modes
    IO_PORT             =  5 << 1,  // 6510-style I/O port
    BANK_SWITCHING      =  6 << 1,  // 65C816 bank switching
    NMOS_BUGS           =  7 << 1,  // NMOS hardware bugs
    DECIMAL_MODE        =  8 << 1,  // BCD arithmetic support
    DECIMAL_AFFECTS_NZ  =  9 << 1,  // NMOS decimal N/Z behavior
    ENHANCED_ADDRESSING = 10 << 1,  // 65C02+ addressing modes
    LONG_ADDRESSING     = 11 << 1,  // 65C816 24-bit addressing
    EMULATION_MODE      = 12 << 1,  // 65C816 emulation mode
    NATIVE_MODE         = 13 << 1,  // 65C816 native mode
    WAIT_STATES         = 14 << 1,  // WAI/STP instruction support
    COPROCESSOR         = 15 << 1,  // COP instruction support
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
// UNIFIED PROCESSOR TRAIT SPECIALIZATIONS - Only Features + Metadata
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
        ProcessorFeatures::DECIMAL_AFFECTS_NZ;
    
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
        ProcessorFeatures::IO_PORT;
    
    static constexpr const char* name = "MOS 6510";
    static constexpr const char* family = "NMOS 6502";
    static constexpr uint16_t year_introduced = 1982;
};

// NES 6502 - 6502 without decimal mode
template<>
struct ProcessorTraits<NES6502Tag> {
    static constexpr ProcessorFeatures features = 
        ProcessorFeatures::MOS6502_BASE |
        ProcessorFeatures::ILLEGAL_OPCODES |
        ProcessorFeatures::NMOS_BUGS;
    
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
        ProcessorFeatures::WAIT_STATES;
    
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
        ProcessorFeatures::WAIT_STATES;
    
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
        ProcessorFeatures::COPROCESSOR;
    
    static constexpr const char* name = "WDC 65C816";
    static constexpr const char* family = "16-bit 65816";
    static constexpr uint16_t year_introduced = 1984;
};

// ============================================================================
// UNIFIED FEATURE QUERY API - Single Consistent Interface
// ============================================================================

// Primary feature query function - all other queries should use this
template<typename ProcessorTag>
constexpr bool has_feature(ProcessorFeatures feature) {
    return (ProcessorTraits<ProcessorTag>::features & feature) == feature;
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

// ============================================================================
// SEMANTIC CONVENIENCE FUNCTIONS - Built on has_feature()
// ============================================================================

// These replace the individual boolean traits but use the unified API

template<typename ProcessorTag>
constexpr bool has_illegal_opcodes() {
    return has_feature<ProcessorTag>(ProcessorFeatures::ILLEGAL_OPCODES);
}

template<typename ProcessorTag>
constexpr bool has_decimal_mode() {
    return has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE);
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
constexpr bool has_16bit_mode() {
    return has_feature<ProcessorTag>(ProcessorFeatures::WIDE_REGISTERS);
}

template<typename ProcessorTag>
constexpr bool has_io_port() {
    return has_feature<ProcessorTag>(ProcessorFeatures::IO_PORT);
}

template<typename ProcessorTag>
constexpr bool has_nmos_bugs() {
    return has_feature<ProcessorTag>(ProcessorFeatures::NMOS_BUGS);
}

template<typename ProcessorTag>
constexpr bool decimal_affects_nz() {
    return has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_AFFECTS_NZ);
}

// Processor classification
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

// ============================================================================
// METADATA ACCESS FUNCTIONS
// ============================================================================

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

} // namespace fam65xx_core

#endif // __cplusplus