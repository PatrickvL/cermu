#pragma once
#ifndef FAM65XX_PROCESSOR_WRAPPERS_HPP_INCLUDED
#define FAM65XX_PROCESSOR_WRAPPERS_HPP_INCLUDED

/*
 * fam65xx_processor_wrappers.hpp - Processor-Specific C++ Wrapper Types
 *
 * This file defines concrete processor types that automatically initialize
 * their opcode tables based on ProcessorTraits. Each wrapper contains a
 * fam65xx_t instance and handles processor-specific initialization.
 *
 * Pattern:
 * - C wrappers contain concrete processor instances (mos6502_cpu_t, mos6510_cpu_t, etc.)
 * - Each processor type automatically calls fam65xx_init_opcode_table<ProcessorTag> in constructor
 * - No manual opcode table initialization needed in C code
 */

#ifdef __cplusplus

#include "fam65xx.hpp"
#include "fam65xx_processor_traits.hpp"

// ============================================================================
// PROCESSOR-SPECIFIC WRAPPER TYPES
// ============================================================================

// Base template for processor wrappers
template<typename ProcessorTag>
class fam65xx_processor_wrapper_t {
private:
    fam65xx_t cpu_impl;

public:
    // Constructor automatically initializes opcode table based on ProcessorTraits
    fam65xx_processor_wrapper_t() {
        // Initialize CPU state
        memset(&cpu_impl, 0, sizeof(fam65xx_t));
        
        // Automatically populate opcode table using ProcessorTraits
        fam65xx_init_opcode_table<ProcessorTag>(&cpu_impl);
    }
    
    // Provide access to the underlying CPU implementation
    fam65xx_t* get_cpu() { return &cpu_impl; }
    const fam65xx_t* get_cpu() const { return &cpu_impl; }
    
    // Delegate common operations
    bus_state_t init(const fam65xx_desc_t* desc) {
        return fam65xx_init(&cpu_impl, desc);
    }
    
    bus_state_t reset(bus_state_t pins) {
        return fam65xx_reset(&cpu_impl, pins);
    }
    
    bus_state_t tick(bus_state_t pins) {
        return fam65xx_tick(&cpu_impl, pins);
    }
    
    bool opdone() const {
        return fam65xx_opdone(const_cast<fam65xx_t*>(&cpu_impl));
    }
    
    bus_state_t bootstrap(bus_state_t pins) {
        return fam65xx_bootstrap(&cpu_impl, pins);
    }
};

// ============================================================================
// CONCRETE PROCESSOR TYPES
// ============================================================================

// MOS 6502 - Original NMOS processor
using mos6502_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::MOS6502Tag>;

// MOS 6510 - 6502 with I/O port (C64)
using mos6510_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::MOS6510Tag>;

// NES 6502 - 6502 variant without decimal mode
using nes6502_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::NES6502Tag>;

// WDC 65C02 - CMOS with enhancements
using wdc65c02_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::WDC65C02Tag>;

// Rockwell 65C02 - CMOS with bit manipulation
using rockwell65c02_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::Rockwell65C02Tag>;

// WDC 65C816 - 16-bit extension
using wdc65c816_cpu_t = fam65xx_processor_wrapper_t<fam65xx_core::WDC65C816Tag>;

#endif // __cplusplus

#endif /* FAM65XX_PROCESSOR_WRAPPERS_HPP_INCLUDED */