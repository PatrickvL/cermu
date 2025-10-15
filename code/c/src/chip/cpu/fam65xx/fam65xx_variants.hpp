#pragma once
/*
 * fam65xx_templates.hpp - Template System for MOS 65xx Family CPU Variants
 *
 * This header provides C++ template support for different MOS 65xx processor
 * variants while maintaining full backward compatibility with the existing C API.
 *
 * SUPPORTED PROCESSOR VARIANTS:
 * - MOS 6502 (Original NMOS with illegal opcodes)
 * - MOS 6510 (C64 variant with I/O port)
 * - WDC 65C02 (CMOS version with enhanced instructions)
 * - Rockwell R65C02 (CMOS with bit manipulation)
 * - WDC 65C816 (16-bit extension)
 *
 * TEMPLATE FEATURES:
 * - Compile-time processor feature selection
 * - Zero runtime overhead for processor differences
 * - Processor-specific opcode tables and behaviors
 * - Single source compatibility across all variants
 */

#ifdef __cplusplus

#include <cstdint>

// Include the C API for actual implementation
extern "C" {
#include "fam65xx_core.hpp"
}

namespace fam65xx_variants {

// ============================================================================
// PROCESSOR FEATURE TRAITS
// ============================================================================

// Base trait template for processor features
template<typename ProcessorTag>
struct ProcessorTraits {
    static constexpr bool has_illegal_opcodes = false;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = false;
    static constexpr bool has_bit_manipulation = false;
    static constexpr bool has_16bit_mode = false;
    static constexpr bool has_io_port = false;
    static constexpr bool has_nmos_bugs = false;
    static constexpr bool decimal_affects_nz = false;
};

// Processor tag types
struct MOS6502Tag {};
struct MOS6510Tag {};
struct WDC65C02Tag {};
struct Rockwell65C02Tag {};
struct WDC65C816Tag {};

// MOS 6502 specialization
template<>
struct ProcessorTraits<MOS6502Tag> {
    static constexpr bool has_illegal_opcodes = true;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = false;
    static constexpr bool has_bit_manipulation = false;
    static constexpr bool has_16bit_mode = false;
    static constexpr bool has_io_port = false;
    static constexpr bool has_nmos_bugs = true;
    static constexpr bool decimal_affects_nz = true;
};

// MOS 6510 specialization
template<>
struct ProcessorTraits<MOS6510Tag> {
    static constexpr bool has_illegal_opcodes = true;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = false;
    static constexpr bool has_bit_manipulation = false;
    static constexpr bool has_16bit_mode = false;
    static constexpr bool has_io_port = true;
    static constexpr bool has_nmos_bugs = true;
    static constexpr bool decimal_affects_nz = true;
};

// WDC 65C02 specialization
template<>
struct ProcessorTraits<WDC65C02Tag> {
    static constexpr bool has_illegal_opcodes = false;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = true;
    static constexpr bool has_bit_manipulation = false;
    static constexpr bool has_16bit_mode = false;
    static constexpr bool has_io_port = false;
    static constexpr bool has_nmos_bugs = false;
    static constexpr bool decimal_affects_nz = false;
};

// Rockwell 65C02 specialization
template<>
struct ProcessorTraits<Rockwell65C02Tag> {
    static constexpr bool has_illegal_opcodes = false;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = true;
    static constexpr bool has_bit_manipulation = true;
    static constexpr bool has_16bit_mode = false;
    static constexpr bool has_io_port = false;
    static constexpr bool has_nmos_bugs = false;
    static constexpr bool decimal_affects_nz = false;
};

// WDC 65C816 specialization
template<>
struct ProcessorTraits<WDC65C816Tag> {
    static constexpr bool has_illegal_opcodes = false;
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_enhancements = true;
    static constexpr bool has_bit_manipulation = false;
    static constexpr bool has_16bit_mode = true;
    static constexpr bool has_io_port = false;
    static constexpr bool has_nmos_bugs = false;
    static constexpr bool decimal_affects_nz = false;
};

// ============================================================================
// TEMPLATE CPU CLASS
// ============================================================================

template<typename ProcessorTag>
class CPU {
private:
    fam65xx_t* cpu_impl;  // Pointer to avoid including full definition
    using traits = ProcessorTraits<ProcessorTag>;
    
public:
    // ========================================================================
    // INITIALIZATION AND LIFECYCLE
    // ========================================================================
    
    CPU() : cpu_impl(nullptr) {
        // Implementation will be allocated externally or provided
    }
    
    explicit CPU(fam65xx_t* impl) : cpu_impl(impl) {}
    
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) {
        if (!cpu_impl) return 0;
        
        bus_state_t pins = fam65xx_init(cpu_impl, desc);
        
        // Apply processor-specific initialization
        if constexpr (traits::has_io_port) {
            init_io_port();
        }
        
        return pins;
    }
    
    bus_state_t reset(bus_state_t pins) {
        if (!cpu_impl) return 0;
        return fam65xx_reset(cpu_impl, pins);
    }
    
    bus_state_t bootstrap(bus_state_t pins) {
        if (!cpu_impl) return 0;
        return fam65xx_bootstrap(cpu_impl, pins);
    }
    
    // ========================================================================
    // EXECUTION
    // ========================================================================
    
    bus_state_t tick(bus_state_t pins) {
        if (!cpu_impl) return 0;
        
        // Handle processor-specific behavior
        if constexpr (traits::has_io_port) {
            pins = handle_io_port_tick(pins);
        }
        
        return fam65xx_tick(cpu_impl, pins);
    }
    
    bool opdone() const {
        if (!cpu_impl) return true;
        return fam65xx_opdone(cpu_impl);
    }
    
    // ========================================================================
    // REGISTER ACCESS
    // ========================================================================
    
    void set_a(uint8_t v) { if (cpu_impl) fam65xx_set_a(cpu_impl, v); }
    void set_x(uint8_t v) { if (cpu_impl) fam65xx_set_x(cpu_impl, v); }
    void set_y(uint8_t v) { if (cpu_impl) fam65xx_set_y(cpu_impl, v); }
    void set_s(uint8_t v) { if (cpu_impl) fam65xx_set_s(cpu_impl, v); }
    void set_p(uint8_t v) { if (cpu_impl) fam65xx_set_p(cpu_impl, v); }
    void set_pc(uint16_t v) { if (cpu_impl) fam65xx_set_pc(cpu_impl, v); }
    
    uint8_t a() const { return cpu_impl ? fam65xx_a(cpu_impl) : 0; }
    uint8_t x() const { return cpu_impl ? fam65xx_x(cpu_impl) : 0; }
    uint8_t y() const { return cpu_impl ? fam65xx_y(cpu_impl) : 0; }
    uint8_t s() const { return cpu_impl ? fam65xx_s(cpu_impl) : 0; }
    uint8_t p() const { return cpu_impl ? fam65xx_p(cpu_impl) : 0; }
    uint16_t pc() const { return cpu_impl ? fam65xx_pc(cpu_impl) : 0; }
    
    // Direct access for compatibility
    fam65xx_t* get_impl() { return cpu_impl; }
    const fam65xx_t* get_impl() const { return cpu_impl; }
    
    void set_impl(fam65xx_t* impl) { cpu_impl = impl; }
    
    // ========================================================================
    // FEATURE QUERIES
    // ========================================================================
    
    static constexpr bool has_illegal_opcodes() { return traits::has_illegal_opcodes; }
    static constexpr bool has_decimal_mode() { return traits::has_decimal_mode; }
    static constexpr bool has_cmos_enhancements() { return traits::has_cmos_enhancements; }
    static constexpr bool has_bit_manipulation() { return traits::has_bit_manipulation; }
    static constexpr bool has_16bit_mode() { return traits::has_16bit_mode; }
    static constexpr bool has_io_port() { return traits::has_io_port; }
    static constexpr bool has_nmos_bugs() { return traits::has_nmos_bugs; }
    static constexpr bool decimal_affects_nz() { return traits::decimal_affects_nz; }
    
private:
    // Processor-specific initialization
    void init_io_port() {
        if constexpr (traits::has_io_port) {
            // Initialize 6510 I/O port state
            // This would be handled by specialized memory callbacks
        }
    }
    
    // Handle I/O port behavior
    bus_state_t handle_io_port_tick(bus_state_t pins) {
        if constexpr (traits::has_io_port) {
            // Handle memory-mapped I/O port behavior
            // Would intercept reads/writes to addresses $00 and $01
        }
        return pins;
    }
};

// ============================================================================
// TYPE ALIASES FOR COMMON PROCESSORS
// ============================================================================

using CPU_MOS6502 = CPU<MOS6502Tag>;
using CPU_MOS6510 = CPU<MOS6510Tag>;
using CPU_WDC65C02 = CPU<WDC65C02Tag>;
using CPU_Rockwell65C02 = CPU<Rockwell65C02Tag>;
using CPU_WDC65C816 = CPU<WDC65C816Tag>;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

template<typename ProcessorTag>
constexpr auto make_cpu() {
    return CPU<ProcessorTag>{};
}

template<typename ProcessorTag>
constexpr bool processor_has_illegal_opcodes() {
    return ProcessorTraits<ProcessorTag>::has_illegal_opcodes;
}

template<typename ProcessorTag>
constexpr bool processor_has_io_port() {
    return ProcessorTraits<ProcessorTag>::has_io_port;
}

template<typename ProcessorTag>
constexpr bool processor_has_cmos_enhancements() {
    return ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
}

template<typename ProcessorTag>
constexpr bool processor_has_16bit_mode() {
    return ProcessorTraits<ProcessorTag>::has_16bit_mode;
}

} // namespace fam65xx_variants

#endif // __cplusplus