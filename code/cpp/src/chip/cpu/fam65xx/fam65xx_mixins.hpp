#pragma once
/*
 * fam65xx_mixins.hpp - Conditional Feature Mixins for MOS 65xx Family
 *
 * This file contains mixin classes that add processor-specific data and functionality
 * only when the corresponding feature is enabled via CPUTraits. Empty base
 * optimization ensures zero overhead when features are disabled.
 */

#include <cstdint>
#include <type_traits>
#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.h"  // For bus_state_t
#include "nes6502.h"        // For nes6502_apu::APU class

namespace fam65xx {

// ============================================================================
// EMPTY BASES FOR DISABLED FEATURES (processor-specific to avoid collisions)
// ============================================================================

// Separate empty types per feature to prevent duplicate base class errors
struct empty_io_port_mixin_t {};
struct empty_wide_mixin_t {};
struct empty_apu_mixin_t {};

// ============================================================================
// I/O PORT MIXIN (6510-style processors)
// ============================================================================

// I/O Port functionality for processors like MOS 6510 (C64/C128)
template<const CPUTraits& Traits>
struct io_port_mixin_t {
    // I/O Port registers (aligned to 4-byte boundary)
    struct alignas(4) {
        uint8_t direction;  // Data Direction Register (DDR) at $00
        uint8_t data;       // Port data register at $01  
        uint8_t input;      // External input pin state
        uint8_t _padding;   // Align to 4 bytes
    } io_port;
    
    // Initialize I/O port to C64 defaults
    void init_io_port() {
        io_port.direction = 0x2F;  // C64 default: bits 0,1,2,3,5 output
        io_port.data = 0x37;       // C64 default: HIRAM/LORAM/CHAREN config
        io_port.input = 0x17;      // Default input state (cassette sense)
    }
    
    // Write to Data Direction Register ($00)
    void write_io_ddr(uint8_t value) {
        io_port.direction = value;
    }
    
    // Write to Port data register ($01)
    void write_io_data(uint8_t value) {
        io_port.data = value;
    }
    
    // Read from Port (combines output and input based on direction)
    uint8_t read_io_port() const {
        return (io_port.data & io_port.direction) | 
               (io_port.input & ~io_port.direction);
    }
    
    // Update external input pins (for cartridge/tape interface)
    void set_io_input(uint8_t value) {
        io_port.input = value;
    }
};

// ============================================================================
// 16-BIT MODE MIXIN (65C816)
// ============================================================================

// Extended state for 65C816 16-bit processor
template<const CPUTraits& Traits>
struct wide_registers_mixin_t {
    // Extended registers (aligned to 8-byte boundary for performance)
    struct alignas(8) {
        uint16_t A_full;    // Full 16-bit accumulator (C = A_full >> 8)
        uint16_t X_full;    // Full 16-bit X index register
        uint16_t Y_full;    // Full 16-bit Y index register
        uint16_t D;         // Direct Page register
        uint8_t DBR;        // Data Bank register
        uint8_t PBR;        // Program Bank register
        bool emulation_mode; // Emulation mode flag (separate from E flag)
        uint8_t _padding;   // Align to 8 bytes
    } wide_state;
    
    // Initialize 16-bit state
    void init_wide_registers() {
        wide_state.A_full = 0x0000;
        wide_state.X_full = 0x0000;
        wide_state.Y_full = 0x0000;
        wide_state.D = 0x0000;
        wide_state.DBR = 0x00;
        wide_state.PBR = 0x00;
        wide_state.emulation_mode = true; // Start in emulation mode
    }
    
    // Accessor methods for 16-bit registers
    uint16_t get_accumulator() const { return wide_state.A_full; }
    uint16_t get_x_full() const { return wide_state.X_full; }
    uint16_t get_y_full() const { return wide_state.Y_full; }
    uint16_t get_d() const { return wide_state.D; }
    uint8_t get_dbr() const { return wide_state.DBR; }
    uint8_t get_pbr() const { return wide_state.PBR; }
    bool get_emulation_mode() const { 
        // For 65C816, emulation mode is determined by the E flag
        // In emulation mode (E=1), the CPU behaves like a 6502
        // In native mode (E=0), the CPU uses 16-bit capabilities
        return wide_state.emulation_mode; 
    }
    
    void set_accumulator(uint16_t value) { wide_state.A_full = value; }
    void set_x_full(uint16_t value) { wide_state.X_full = value; }
    void set_y_full(uint16_t value) { wide_state.Y_full = value; }
    void set_d(uint16_t value) { wide_state.D = value; }
    void set_dbr(uint8_t value) { wide_state.DBR = value; }
    void set_pbr(uint8_t value) { wide_state.PBR = value; }
    void set_emulation_mode(bool mode) { wide_state.emulation_mode = mode; }
};

// ============================================================================
// APU MIXIN (NES 6502 Audio Processing Unit)
// ============================================================================

// APU functionality for NES 6502 processors with integrated audio
template<const CPUTraits& Traits>
struct apu_mixin_t {
    // APU instance (aligned for performance)
    struct alignas(8) {
        nes6502_apu::APU* apu_instance;
        bool is_pal;
        bool processor_tests_mode;  // Disable memory-mapped I/O for ProcessorTests compatibility
        uint8_t _padding[5]; // Align to 8 bytes
    } apu_state;
    
    // Initialize APU
    void init_apu() {
        apu_state.is_pal = false; // Default to NTSC
        apu_state.processor_tests_mode = false; // Default to normal APU mode
        apu_state.apu_instance = new nes6502_apu::APU(apu_state.is_pal);
    }
    
    // Cleanup APU
    void destroy_apu() {
        delete apu_state.apu_instance;
        apu_state.apu_instance = nullptr;
    }
    
    // APU register write handler ($4000-$4017) - ProcessorTests compatible
    bool write_apu_register(uint16_t addr, uint8_t value) {
        // In ProcessorTests mode, don't intercept memory-mapped I/O
        if (addr >= 0x4000 && addr <= 0x4017) {
            if (apu_state.processor_tests_mode) {
                return false; // Not handled - allow normal memory access
            }
            
            if (apu_state.apu_instance) {
                // Create a bus state with the address and data set
                bus_state_t bus_state = 0;
                FAM65XX_SET_ADDR(bus_state, addr);
                FAM65XX_SET_DATA(bus_state, value);
                apu_state.apu_instance->write(addr, value, bus_state);
            }
            return true; // Handled
        }
        return false; // Not APU register
    }
    
    // APU register read handler ($4015) - ProcessorTests compatible
    bool read_apu_register(uint16_t addr, uint8_t& value) {
        // In ProcessorTests mode, don't intercept memory-mapped I/O
        if (addr == 0x4015) {
            if (apu_state.processor_tests_mode) {
                return false; // Not handled - allow normal memory access
            }
            
            if (apu_state.apu_instance) {
                // Create a bus state with the address set
                bus_state_t bus_state = 0;
                FAM65XX_SET_ADDR(bus_state, addr);
                bus_state = apu_state.apu_instance->read(addr, bus_state);
                value = FAM65XX_GET_DATA(bus_state);
            } else {
                value = 0;
            }
            return true; // Handled
        }
        return false; // Not APU register
    }
    
    // Clock APU (called every CPU cycle)  
    bus_state_t clock_apu(bus_state_t bus_state) {
        if (apu_state.apu_instance) {
            return apu_state.apu_instance->tick(bus_state);
        }
        return bus_state;
    }
    
    // Generate audio sample
    float generate_audio_sample() {
        if (apu_state.apu_instance) {
            return apu_state.apu_instance->sample();
        }
        return 0.0f;
    }
    
    // DMC DMA handling
    bool apu_needs_dma() const {
        return apu_state.apu_instance && apu_state.apu_instance->dmc_needs_sample();
    }
    
    uint16_t apu_dma_address() const {
        return apu_state.apu_instance ? apu_state.apu_instance->dmc_sample_address() : 0;
    }
    
    void apu_load_dma_sample(uint8_t data) {
        if (apu_state.apu_instance) {
            apu_state.apu_instance->dmc_load_sample(data);
        }
    }
    
    // APU IRQ status
    bool apu_irq() const {
        return apu_state.apu_instance && apu_state.apu_instance->irq();
    }
    
    // Set PAL/NTSC mode
    void set_apu_region(bool is_pal_region) {
        apu_state.is_pal = is_pal_region;
        if (apu_state.apu_instance) {
            destroy_apu();
            init_apu();
        }
    }
    
    // Enable/disable ProcessorTests compatibility mode
    void set_processor_tests_mode(bool enable) {
        apu_state.processor_tests_mode = enable;
    }
};

// ============================================================================
// CONDITIONAL MIXIN TYPE SELECTION
// ============================================================================

// Use std::conditional to include mixins only when features are present
template<const CPUTraits& Traits>
using io_port_base_t = std::conditional_t<
    Traits.has_io_port(),
    io_port_mixin_t<Traits>,
    empty_io_port_mixin_t
>;

template<const CPUTraits& Traits>
using wide_registers_base_t = std::conditional_t<
    Traits.has(CPUCoreFlags::C816_16BIT),
    wide_registers_mixin_t<Traits>,
    empty_wide_mixin_t
>;

template<const CPUTraits& Traits>
using apu_base_t = std::conditional_t<
    Traits.peripheral.has_sound(),
    apu_mixin_t<Traits>,
    empty_apu_mixin_t
>;

} // namespace fam65xx