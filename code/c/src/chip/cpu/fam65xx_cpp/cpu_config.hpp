#ifndef CPU_CONFIG_HPP
#define CPU_CONFIG_HPP

#include <cstdint>

namespace fam65xx_cpp {

// CPU variant enumeration
enum class CpuVariant {
    NMOS_6502,
    NMOS_6510,
    CMOS_65C02,
    WDC_65C816
};

} // namespace fam65xx_cpp

/**
 * Template Helper Class: CPU Configuration
 * 
 * Provides compile-time CPU variant configuration with feature detection.
 * Each configuration template enables/disables specific CPU features,
 * allowing the compiler to eliminate unused code paths entirely.
 */

// Template-based CPU variant configuration - compile-time feature detection
template<fam65xx_cpp::CpuVariant Variant, bool HasDecimalMode, bool HasIOPorts, bool HasSyncPin, bool HasSOPin,
         bool HasAECPin, bool HasBEPin, bool HasAbortPin, bool HasVPPin,
         bool HasMLPin, bool HasBankPins, bool HasIllegalOpcodes, bool HasCMOSFixes,
         uint8_t AddressLines, bool RDYAffectsWrites, bool HasWAI, bool HasSTP>
struct cpu_config {
    static constexpr fam65xx_cpp::CpuVariant cpu_variant = Variant;
    static constexpr bool has_decimal_mode = HasDecimalMode;
    static constexpr bool has_io_ports = HasIOPorts;
    static constexpr bool has_sync_pin = HasSyncPin;
    static constexpr bool has_so_pin = HasSOPin;
    static constexpr bool has_aec_pin = HasAECPin;
    static constexpr bool has_be_pin = HasBEPin;
    static constexpr bool has_abort_pin = HasAbortPin;
    static constexpr bool has_vp_pin = HasVPPin;
    static constexpr bool has_ml_pin = HasMLPin;
    static constexpr bool has_bank_pins = HasBankPins;
    static constexpr bool has_illegal_opcodes = HasIllegalOpcodes;
    static constexpr bool has_cmos_fixes = HasCMOSFixes;
    static constexpr uint8_t address_lines = AddressLines;
    static constexpr bool rdy_affects_writes = RDYAffectsWrites;
    static constexpr bool wai_instruction = HasWAI;
    static constexpr bool stp_instruction = HasSTP;
    static constexpr uint16_t address_mask = (1 << AddressLines) - 1;
};

// Pre-defined CPU variant configurations
// Parameters: variant, decimal, io_ports, sync, so, aec, be, abort, vp, ml, bank, illegal, cmos, addr_lines, rdy_writes, wai, stp

using config_6502 = cpu_config<fam65xx_cpp::CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false,
                              false, false, true, false, 16, false, false, false>;

using config_65c02 = cpu_config<fam65xx_cpp::CpuVariant::CMOS_65C02, true, false, true, true, false, true, false, true,
                               true, false, false, true, 16, true, true, true>;

using config_6510 = cpu_config<fam65xx_cpp::CpuVariant::NMOS_6510, true, true, true, true, true, false, false, false,
                              false, false, true, false, 16, false, false, false>;

using config_6507 = cpu_config<fam65xx_cpp::CpuVariant::NMOS_6502, true, false, false, false, false, false, false, false,
                              false, false, true, false, 13, false, false, false>;

using config_65c816 = cpu_config<fam65xx_cpp::CpuVariant::WDC_65C816, true, false, true, true, false, true, true, true,
                                true, true, false, true, 16, true, true, true>;

/**
 * Template Helper Class: CPU Pin Configuration
 * 
 * Generates compile-time optimized pin masks for each CPU variant.
 * Used to minimize runtime pin checking overhead by pre-computing
 * which pins are relevant for each variant.
 */
template<typename Config>
struct cpu_pin_config {
    // Combined input pin check - compile-time optimized pin set
    // Maps to actual bit positions from system_lines.h
    static constexpr uint64_t variant_input_pins() {
        return (1ULL << 32) |                                   // BUS_RES_BIT (always present)
               (1ULL << 33) |                                   // BUS_IRQ_BIT (always present)
               (1ULL << 34) |                                   // BUS_NMI_BIT (always present)
               (1ULL << 35) |                                   // BUS_RDY_BIT (always present)
               (Config::has_so_pin ? (1ULL << 36) : 0) |        // BUS_SO_BIT
               (Config::has_aec_pin ? (1ULL << 37) : 0) |       // BUS_AEC_BIT
               (Config::has_be_pin ? (1ULL << 38) : 0) |        // BUS_BE_BIT
               (Config::has_abort_pin ? (1ULL << 39) : 0);      // BUS_ABORT_BIT
    }
    
    // Output pin configuration - maps to actual bit positions from system_lines.h
    static constexpr uint64_t variant_output_pins() {
        return (1ULL << 48) |                                   // BUS_RW_BIT (always present)
               (Config::has_sync_pin ? (1ULL << 49) : 0) |      // BUS_SYNC_BIT
               (1ULL << 52) |                                   // BUS_BA_BIT (always present)
               (Config::has_vp_pin ? (1ULL << 53) : 0) |        // BUS_VP_BIT
               (Config::has_ml_pin ? (1ULL << 54) : 0);         // BUS_ML_BIT
    }
    
    // Special state masks - compile-time conditional
    static constexpr uint16_t variant_special_states() {
        constexpr uint16_t BASE_STATES = 0x0001 | 0x0002 | 0x0004 | 0x0080 | 0x0100 | 0x0200; // Basic states
        return BASE_STATES |
               (Config::wai_instruction ? 0x2000 : 0) |         // STATE_WAI_MODE
               (Config::stp_instruction ? 0x4000 : 0);          // STATE_STP_MODE
    }
    
    // Compile-time constants for optimization
    static constexpr uint64_t INPUT_PINS = variant_input_pins();
    static constexpr uint64_t OUTPUT_PINS = variant_output_pins();
    static constexpr uint16_t SPECIAL_STATES = variant_special_states();
};

/**
 * Template Helper Class: CPU Feature Detection
 * 
 * Provides compile-time feature detection and capability queries.
 * Used by other helper classes to enable/disable functionality
 * based on CPU variant capabilities.
 */
template<typename Config>
struct cpu_features {
    // Instruction set capabilities
    static constexpr bool supports_illegal_opcodes() { return Config::has_illegal_opcodes; }
    static constexpr bool supports_decimal_mode() { return Config::has_decimal_mode; }
    static constexpr bool supports_wai_stp() { return Config::wai_instruction || Config::stp_instruction; }
    
    // Hardware capabilities  
    static constexpr bool has_memory_banking() { return Config::has_bank_pins; }
    static constexpr bool has_io_processing() { return Config::has_io_ports; }
    static constexpr bool has_advanced_pins() { 
        return Config::has_sync_pin || Config::has_so_pin || Config::has_be_pin;
    }
    
    // Bus behavior
    static constexpr bool rdy_blocks_writes() { return Config::rdy_affects_writes; }
    static constexpr uint16_t max_address() { return Config::address_mask; }
    
    // Performance hints for optimization
    static constexpr bool needs_pin_processing() { return has_advanced_pins(); }
    static constexpr bool needs_special_states() { return supports_wai_stp(); }
    static constexpr bool is_minimal_variant() { 
        return !Config::has_io_ports && !has_advanced_pins() && !supports_illegal_opcodes();
    }
};

#endif // CPU_CONFIG_HPP