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
#include "fam65xx_feat_io_port.hpp"
#include "fam65xx_feat_bit_ops.hpp"
#include "fam65xx_feat_16bit_mode.hpp"
#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include <cstdint>
#include <cstring>

#ifdef __cplusplus

namespace fam65xx_core {

// ============================================================================
// UNIFIED CPU TEMPLATE
// ============================================================================

template<typename ProcessorTag>
class UnifiedCPU {
private:
    // Core CPU state (always present)
    fam65xx_t cpu_impl;
    
    // Feature components (conditionally instantiated)
    fam65xx_features::IOPortFeature<ProcessorTag> io_port_feature;
    fam65xx_features::BitManipulationFeature<ProcessorTag> bit_ops_feature;
    fam65xx_features::SixteenBitModeFeature<ProcessorTag> sixteenbit_feature;
    
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
    
    UnifiedCPU() {
        // Zero-initialize the core implementation
        std::memset(&cpu_impl, 0, sizeof(fam65xx_t));
    }
    
    // Copy constructor
    UnifiedCPU(const UnifiedCPU& other) : cpu_impl(other.cpu_impl) {
        if constexpr (has_io_port) {
            io_port_feature = other.io_port_feature;
        }
        if constexpr (has_bit_ops) {
            bit_ops_feature = other.bit_ops_feature;
        }
        if constexpr (has_16bit) {
            sixteenbit_feature = other.sixteenbit_feature;
        }
    }
    
    // Assignment operator
    UnifiedCPU& operator=(const UnifiedCPU& other) {
        if (this != &other) {
            cpu_impl = other.cpu_impl;
            if constexpr (has_io_port) {
                io_port_feature = other.io_port_feature;
            }
            if constexpr (has_bit_ops) {
                bit_ops_feature = other.bit_ops_feature;
            }
            if constexpr (has_16bit) {
                sixteenbit_feature = other.sixteenbit_feature;
            }
        }
        return *this;
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bus_state_t init(const fam65xx_desc_t* desc = nullptr) {
        bus_state_t pins = fam65xx_init(&cpu_impl, desc);
        
        // Initialize feature components
        if constexpr (has_io_port) {
            io_port_feature.reset();
        }
        if constexpr (has_bit_ops) {
            bit_ops_feature.reset();
        }
        if constexpr (has_16bit) {
            sixteenbit_feature.reset();
        }
        
        return pins;
    }
    
    bus_state_t reset(bus_state_t pins) {
        pins = fam65xx_reset(&cpu_impl, pins);
        
        // Reset feature components
        if constexpr (has_io_port) {
            io_port_feature.reset();
        }
        if constexpr (has_bit_ops) {
            bit_ops_feature.reset();
        }
        if constexpr (has_16bit) {
            sixteenbit_feature.reset();
        }
        
        return pins;
    }
    
    bus_state_t bootstrap(bus_state_t pins) {
        return fam65xx_bootstrap(&cpu_impl, pins);
    }
    
    // ========================================================================
    // EXECUTION
    // ========================================================================
    
    bus_state_t tick(bus_state_t pins) {
        // Pre-tick feature processing
        if constexpr (has_io_port) {
            pins = io_port_feature.handle_tick(pins);
        }
        if constexpr (has_16bit) {
            pins = sixteenbit_feature.handle_tick(pins);
        }
        
        // Core CPU tick
        pins = fam65xx_tick(&cpu_impl, pins);
        
        // Post-tick feature processing
        if (opdone()) {
            // Track instruction usage in features
            uint8_t opcode = cpu_impl.reg8[REG_IR]; // Get last executed opcode
            
            if constexpr (has_bit_ops) {
                bit_ops_feature.track_instruction(opcode);
            }
            if constexpr (has_16bit) {
                sixteenbit_feature.track_instruction(opcode);
                sixteenbit_feature.update_register_widths(&cpu_impl);
            }
        }
        
        return pins;
    }
    
    bool opdone() const {
        return fam65xx_opdone(const_cast<fam65xx_t*>(&cpu_impl));
    }
    
    // ========================================================================
    // REGISTER ACCESS (8-bit standard interface)
    // ========================================================================
    
    void set_a(uint8_t v) { 
        fam65xx_set_a(&cpu_impl, v);
        // Sync with 16-bit register if available
        if constexpr (has_16bit) {
            sixteenbit_feature.set_a_16((sixteenbit_feature.get_a_16() & 0xFF00) | v);
        }
    }
    
    void set_x(uint8_t v) { 
        fam65xx_set_x(&cpu_impl, v);
        if constexpr (has_16bit) {
            sixteenbit_feature.set_x_16((sixteenbit_feature.get_x_16() & 0xFF00) | v);
        }
    }
    
    void set_y(uint8_t v) { 
        fam65xx_set_y(&cpu_impl, v);
        if constexpr (has_16bit) {
            sixteenbit_feature.set_y_16((sixteenbit_feature.get_y_16() & 0xFF00) | v);
        }
    }
    
    void set_s(uint8_t v) { 
        fam65xx_set_s(&cpu_impl, v);
        if constexpr (has_16bit) {
            sixteenbit_feature.set_s_16((sixteenbit_feature.get_s_16() & 0xFF00) | v);
        }
    }
    
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
    // I/O PORT FEATURE INTERFACE (only available if processor has I/O port)
    // ========================================================================
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), void>
    set_io_ddr(uint8_t value) {
        io_port_feature.set_ddr(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), void>
    set_io_data(uint8_t value) {
        io_port_feature.set_data(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), void>
    set_io_external(uint8_t value) {
        io_port_feature.set_external(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), uint8_t>
    get_io_ddr() const {
        return io_port_feature.get_ddr();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), uint8_t>
    get_io_data() const {
        return io_port_feature.get_data();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), uint8_t>
    get_io_external() const {
        return io_port_feature.get_external();
    }
    
    // I/O port callbacks
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), void>
    set_io_callbacks(const fam65xx_features::IOPortCallbacks& callbacks) {
        io_port_feature.set_callbacks(callbacks);
    }
    
    // Memory banking helpers (C64-specific)
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), bool>
    is_basic_rom_enabled() const {
        return io_port_feature.is_basic_rom_enabled();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), bool>
    is_kernal_rom_enabled() const {
        return io_port_feature.is_kernal_rom_enabled();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), bool>
    is_char_rom_enabled() const {
        return io_port_feature.is_char_rom_enabled();
    }
    
    // ========================================================================
    // BIT MANIPULATION FEATURE INTERFACE (only available if processor has bit ops)
    // ========================================================================
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), uint32_t>
    get_bit_instruction_count() const {
        return bit_ops_feature.get_bit_instruction_count();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), void>
    reset_bit_instruction_count() {
        bit_ops_feature.reset_bit_instruction_count();
    }
    
    // Static bit manipulation utilities
    template<typename T = ProcessorTag>
    static std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), uint8_t>
    set_bit(uint8_t value, uint8_t bit) {
        return fam65xx_features::BitManipulationFeature<T>::set_bit(value, bit);
    }
    
    template<typename T = ProcessorTag>
    static std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), uint8_t>
    clear_bit(uint8_t value, uint8_t bit) {
        return fam65xx_features::BitManipulationFeature<T>::clear_bit(value, bit);
    }
    
    template<typename T = ProcessorTag>
    static std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), bool>
    test_bit(uint8_t value, uint8_t bit) {
        return fam65xx_features::BitManipulationFeature<T>::test_bit(value, bit);
    }
    
    // ========================================================================
    // 16-BIT MODE FEATURE INTERFACE (only available if processor has 16-bit mode)
    // ========================================================================
    
    // 16-bit register access
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_a_16(uint16_t value) {
        sixteenbit_feature.set_a_16(value);
        fam65xx_set_a(&cpu_impl, value & 0xFF); // Sync 8-bit register
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_x_16(uint16_t value) {
        sixteenbit_feature.set_x_16(value);
        fam65xx_set_x(&cpu_impl, value & 0xFF);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_y_16(uint16_t value) {
        sixteenbit_feature.set_y_16(value);
        fam65xx_set_y(&cpu_impl, value & 0xFF);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint16_t>
    get_a_16() const {
        return sixteenbit_feature.get_a_16();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint16_t>
    get_x_16() const {
        return sixteenbit_feature.get_x_16();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint16_t>
    get_y_16() const {
        return sixteenbit_feature.get_y_16();
    }
    
    // Bank registers
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_dp(uint16_t value) {
        sixteenbit_feature.set_dp(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_dbr(uint8_t value) {
        sixteenbit_feature.set_dbr(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_pbr(uint8_t value) {
        sixteenbit_feature.set_pbr(value);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint16_t>
    get_dp() const {
        return sixteenbit_feature.get_dp();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint8_t>
    get_dbr() const {
        return sixteenbit_feature.get_dbr();
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint8_t>
    get_pbr() const {
        return sixteenbit_feature.get_pbr();
    }
    
    // Mode control
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), void>
    set_emulation_mode(bool emulation) {
        sixteenbit_feature.set_emulation_mode(emulation);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), bool>
    get_emulation_mode() const {
        return sixteenbit_feature.get_emulation_mode();
    }
    
    // Mode queries
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), bool>
    is_accumulator_16bit() const {
        return sixteenbit_feature.is_accumulator_16bit(p());
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), bool>
    is_index_16bit() const {
        return sixteenbit_feature.is_index_16bit(p());
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), bool>
    is_native_mode() const {
        return sixteenbit_feature.is_native_mode();
    }
    
    // 24-bit addressing
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint32_t>
    make_long_address(uint8_t bank, uint16_t addr) const {
        return sixteenbit_feature.make_long_address(bank, addr);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint32_t>
    make_data_address(uint16_t addr) const {
        return sixteenbit_feature.make_data_address(addr);
    }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), uint32_t>
    make_program_address(uint16_t addr) const {
        return sixteenbit_feature.make_program_address(addr);
    }
    
    // ========================================================================
    // FEATURE QUERIES (always available)
    // ========================================================================
    
    static constexpr bool has_illegal_opcodes() { return has_illegal; }
    static constexpr bool has_decimal_mode() { return has_feature<ProcessorTag>(ProcessorFeatures::DECIMAL_MODE); }
    static constexpr bool has_cmos_enhancements() { return has_cmos; }
    static constexpr bool has_bit_manipulation() { return has_bit_ops; }
    static constexpr bool has_16bit_mode() { return has_16bit; }
    static constexpr bool has_io_port() { return has_io_port; }
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
    
    // Feature component access (only if enabled)
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::IO_PORT), fam65xx_features::IOPortFeature<T>&>
    get_io_port_feature() { return io_port_feature; }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::BIT_MANIPULATION), fam65xx_features::BitManipulationFeature<T>&>
    get_bit_ops_feature() { return bit_ops_feature; }
    
    template<typename T = ProcessorTag>
    std::enable_if_t<has_feature<T>(ProcessorFeatures::WIDE_REGISTERS), fam65xx_features::SixteenBitModeFeature<T>&>
    get_16bit_feature() { return sixteenbit_feature; }
    
    // ========================================================================
    // MEMORY INTERFACE (delegate to implementations)
    // ========================================================================
    
    // Handle memory-mapped I/O reads (called by memory system)
    uint8_t handle_memory_read(uint16_t addr, uint8_t bus_data) {
        if constexpr (has_io_port) {
            if (addr <= 0x0001) {
                return io_port_feature.read_register(addr);
            }
        }
        
        // Default: use provided bus data
        return bus_data;
    }
    
    // Handle memory-mapped I/O writes (called by memory system) 
    void handle_memory_write(uint16_t addr, uint8_t data) {
        if constexpr (has_io_port) {
            if (addr <= 0x0001) {
                io_port_feature.write_register(addr, data);
                return;
            }
        }
        
        // Other address ranges handled by memory system
    }
};

// ============================================================================
// TYPE ALIASES FOR ALL PROCESSOR VARIANTS
// ============================================================================

using CPU_MOS6502 = UnifiedCPU<MOS6502Tag>;
using CPU_MOS6510 = UnifiedCPU<MOS6510Tag>;
using CPU_NES6502 = UnifiedCPU<NES6502Tag>;
using CPU_WDC65C02 = UnifiedCPU<WDC65C02Tag>;
using CPU_Rockwell65C02 = UnifiedCPU<Rockwell65C02Tag>;
using CPU_WDC65C816 = UnifiedCPU<WDC65C816Tag>;

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

template<typename ProcessorTag>
constexpr UnifiedCPU<ProcessorTag> make_cpu() {
    return UnifiedCPU<ProcessorTag>{};
}

// Convenience factory functions
inline CPU_MOS6502 make_mos6502() { return make_cpu<MOS6502Tag>(); }
inline CPU_MOS6510 make_mos6510() { return make_cpu<MOS6510Tag>(); }
inline CPU_NES6502 make_nes6502() { return make_cpu<NES6502Tag>(); }
inline CPU_WDC65C02 make_wdc65c02() { return make_cpu<WDC65C02Tag>(); }
inline CPU_Rockwell65C02 make_rockwell65c02() { return make_cpu<Rockwell65C02Tag>(); }
inline CPU_WDC65C816 make_wdc65c816() { return make_cpu<WDC65C816Tag>(); }

// Factory with memory callbacks
template<typename ProcessorTag>
UnifiedCPU<ProcessorTag> make_cpu_with_memory(
    uint8_t (*read_fn)(void*, uint16_t, uint8_t),
    void (*write_fn)(void*, uint16_t, uint8_t),
    void* user_data = nullptr
) {
    fam65xx_desc_t desc = {};
    desc.mem_read = read_fn;
    desc.mem_write = write_fn;
    desc.mem_user_data = user_data;
    
    UnifiedCPU<ProcessorTag> cpu;
    cpu.init(&desc);
    return cpu;
}

} // namespace fam65xx_core

#endif // __cplusplus