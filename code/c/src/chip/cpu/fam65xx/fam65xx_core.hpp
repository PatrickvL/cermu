#pragma once
/*
 * fam65xx_core.hpp - Unified CPU Template with Feature Composition
 *
 * This file provides the main unified CPU template that composes all shared feature
 * components based on processor traits. This eliminates duplication by implementing
 * each feature once and selectively enabling them at compile time.
 *
 * DESIGN PRINCIPLES:
 * - Single CPU implementation with feature composition
 * - Compile-time feature selection via processor traits
 * - Zero runtime overhead for disabled features
 * - Complete backward compatibility with existing APIs
 * - Shared feature components mixed and matched per processor
 *
 * FEATURE COMPOSITION:
 * - Base 6502 functionality (always present)
 * - I/O Port Feature (MOS 6510 and derivatives)
 * - Bit Manipulation Feature (Rockwell 65C02 and derivatives)
 * - 16-bit Mode Feature (WDC 65C816 and derivatives)
 * - CMOS Enhancements (65C02 family)
 * - Illegal Opcodes (NMOS processors only)
 *
 * USAGE PATTERN:
 *   using MOS6502CPU = UnifiedCPU<MOS6502Tag>;
 *   using MOS6510CPU = UnifiedCPU<MOS6510Tag>;  // Gets I/O port automatically
 *   using WDC65C816CPU = UnifiedCPU<WDC65C816Tag>; // Gets 16-bit mode automatically
 */

#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include "fam65xx_tables.hpp"  // Include for fam65xx_init_opcode_table
#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#endif

// Forward declare API functions used in the template
#ifdef __cplusplus
extern "C" {
#endif
bus_state_t fam65xx_init(fam65xx_t* cpu, const fam65xx_desc_t* desc);
bus_state_t fam65xx_reset(fam65xx_t* cpu, bus_state_t pins);
bus_state_t fam65xx_tick(fam65xx_t* cpu, bus_state_t pins);
bool fam65xx_opdone(fam65xx_t* cpu);
bus_state_t fam65xx_bootstrap(fam65xx_t* cpu, bus_state_t pins);
void fam65xx_set_a(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_x(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_y(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_s(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_p(fam65xx_t* cpu, uint8_t v);
void fam65xx_set_pc(fam65xx_t* cpu, uint16_t v);
uint8_t fam65xx_a(fam65xx_t* cpu);
uint8_t fam65xx_x(fam65xx_t* cpu);
uint8_t fam65xx_y(fam65xx_t* cpu);
uint8_t fam65xx_s(fam65xx_t* cpu);
uint8_t fam65xx_p(fam65xx_t* cpu);
uint16_t fam65xx_pc(fam65xx_t* cpu);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace fam65xx_core {

// ============================================================================
// UNIFIED CPU TEMPLATE WITH AUTOMATIC OPCODE TABLE INITIALIZATION
// ============================================================================

template<typename ProcessorTag>
class fam65xx_cpu_template_t {
private:
    // Core CPU state (always present)
    fam65xx_t cpu_impl;
    
    // Processor trait shortcuts
    using traits = ProcessorTraits<ProcessorTag>;
    static constexpr auto features = traits::features;
    
    // Feature availability checks
    static constexpr bool has_io_port = has_feature<ProcessorTag>(ProcessorFeatures::IO_PORT);
    static constexpr bool has_bit_ops = has_feature<ProcessorTag>(ProcessorFeatures::BIT_MANIPULATION);
    static constexpr bool has_16bit = has_feature<ProcessorTag>(ProcessorFeatures::WIDE_REGISTERS);
    static constexpr bool has_cmos = has_feature<ProcessorTag>(ProcessorFeatures::CMOS_ENHANCEMENTS);
    static constexpr bool has_illegal = has_feature<ProcessorTag>(ProcessorFeatures::ILLEGAL_OPCODES);
    
public:
    // ========================================================================
    // CONSTRUCTOR AND LIFECYCLE
    // ========================================================================
    
    fam65xx_cpu_template_t() {
        // Zero-initialize the core implementation
        #ifdef __cplusplus
        std::memset(&cpu_impl, 0, sizeof(fam65xx_t));
        #else
        memset(&cpu_impl, 0, sizeof(fam65xx_t));
        #endif
        
        // Automatically populate opcode table using ProcessorTraits
        fam65xx_init_opcode_table<ProcessorTag>(&cpu_impl);
    }
    
    // Copy constructor
    fam65xx_cpu_template_t(const fam65xx_cpu_template_t& other) : cpu_impl(other.cpu_impl) {
        // CPU implementation is copied, no additional feature components to copy
    }
    
    // Assignment operator
    fam65xx_cpu_template_t& operator=(const fam65xx_cpu_template_t& other) {
        if (this != &other) {
            cpu_impl = other.cpu_impl;
        }
        return *this;
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) {
        return fam65xx_init(&cpu_impl, desc);
    }
    
    bus_state_t reset(bus_state_t pins) {
        return fam65xx_reset(&cpu_impl, pins);
    }
    
    bus_state_t bootstrap(bus_state_t pins) {
        return fam65xx_bootstrap(&cpu_impl, pins);
    }
    
    // ========================================================================
    // EXECUTION
    // ========================================================================
    
    bus_state_t tick(bus_state_t pins) {
        return fam65xx_tick(&cpu_impl, pins);
    }
    
    bool opdone() const {
        return fam65xx_opdone(const_cast<fam65xx_t*>(&cpu_impl));
    }
    
    // ========================================================================
    // REGISTER ACCESS (8-bit standard interface)
    // ========================================================================
    
    void set_a(uint8_t v) { fam65xx_set_a(&cpu_impl, v); }
    void set_x(uint8_t v) { fam65xx_set_x(&cpu_impl, v); }
    void set_y(uint8_t v) { fam65xx_set_y(&cpu_impl, v); }
    void set_s(uint8_t v) { fam65xx_set_s(&cpu_impl, v); }
    void set_p(uint8_t v) {
        // Special handling for processors without decimal mode
        if constexpr (!has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE)) {
            v &= ~FLAG_D; // Clear decimal flag for NES 6502
        }
        fam65xx_set_p(&cpu_impl, v);
    }
    
    void set_pc(uint16_t v) { 
        fam65xx_set_pc(&cpu_impl, v);
    }
    
    uint8_t a() const { return fam65xx_a(const_cast<fam65xx_t*>(&cpu_impl)); }
    uint8_t x() const { return fam65xx_x(const_cast<fam65xx_t*>(&cpu_impl)); }
    uint8_t y() const { return fam65xx_y(const_cast<fam65xx_t*>(&cpu_impl)); }
    uint8_t s() const { return fam65xx_s(const_cast<fam65xx_t*>(&cpu_impl)); }
    uint16_t pc() const { return fam65xx_pc(const_cast<fam65xx_t*>(&cpu_impl)); }
    
    uint8_t p() const {
        uint8_t flags = fam65xx_p(const_cast<fam65xx_t*>(&cpu_impl));
        // Special handling for processors without decimal mode
        if constexpr (!has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE)) {
            flags &= ~FLAG_D; // Always clear decimal flag for NES 6502
        }
        return flags;
    }
    
    // ========================================================================
    // FEATURE QUERIES (always available)
    // ========================================================================
    
    static constexpr bool has_illegal_opcodes() { return has_illegal; }
    static constexpr bool has_decimal_mode() { return has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE); }
    static constexpr bool has_cmos_enhancements() { return has_cmos; }
    static constexpr bool has_bit_manipulation() { return has_bit_ops; }
    static constexpr bool has_16bit_mode() { return has_16bit; }
    static constexpr bool has_io_port_feature() { return has_io_port; }  // Renamed to avoid conflict
    static constexpr bool has_nmos_bugs() { return has_feature<ProcessorTag>(ProcessorFeatures::NMOS_BUGS); }
    
    // Get processor name for debugging
    static const char* get_processor_name() {
        return ProcessorTraits<ProcessorTag>::name;
    }
    
    // Get feature flags
    static constexpr ProcessorFeatures get_features() {
        return ProcessorTraits<ProcessorTag>::features;
    }
    
    // ========================================================================
    // DIRECT ACCESS (for backward compatibility and advanced usage)
    // ========================================================================
    
    fam65xx_t* get_impl() { return &cpu_impl; }
    const fam65xx_t* get_impl() const { return &cpu_impl; }
    
    // Direct CPU access for C compatibility
    fam65xx_t* get_cpu() { return &cpu_impl; }
    const fam65xx_t* get_cpu() const { return &cpu_impl; }
};

// ============================================================================
// TYPE ALIASES FOR ALL PROCESSOR VARIANTS (replaces fam65xx_processor_wrapper_t)
// ============================================================================

// Primary processor type aliases (with automatic opcode table initialization)
using mos6502_cpu_t = fam65xx_cpu_template_t<MOS6502Tag>;
using mos6510_cpu_t = fam65xx_cpu_template_t<MOS6510Tag>;
using nes6502_cpu_t = fam65xx_cpu_template_t<NES6502Tag>;
using wdc65c02_cpu_t = fam65xx_cpu_template_t<WDC65C02Tag>;
using rockwell65c02_cpu_t = fam65xx_cpu_template_t<Rockwell65C02Tag>;
using wdc65c816_cpu_t = fam65xx_cpu_template_t<WDC65C816Tag>;


// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

template<typename ProcessorTag>
constexpr fam65xx_cpu_template_t<ProcessorTag> make_cpu() {
    return fam65xx_cpu_template_t<ProcessorTag>{};
}

// Convenience factory functions
inline mos6502_cpu_t make_mos6502() { return make_cpu<MOS6502Tag>(); }
inline mos6510_cpu_t make_mos6510() { return make_cpu<MOS6510Tag>(); }
inline nes6502_cpu_t make_nes6502() { return make_cpu<NES6502Tag>(); }
inline wdc65c02_cpu_t make_wdc65c02() { return make_cpu<WDC65C02Tag>(); }
inline rockwell65c02_cpu_t make_rockwell65c02() { return make_cpu<Rockwell65C02Tag>(); }
inline wdc65c816_cpu_t make_wdc65c816() { return make_cpu<WDC65C816Tag>(); }

// Factory with memory callbacks
template<typename ProcessorTag>
fam65xx_cpu_template_t<ProcessorTag> make_cpu_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    fam65xx_cpu_template_t<ProcessorTag> cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_core

#endif // __cplusplus