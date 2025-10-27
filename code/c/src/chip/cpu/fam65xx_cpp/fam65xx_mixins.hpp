#pragma once
/*
 * fam65xx_mixins.hpp - Conditional Feature Mixins for MOS 65xx Family
 *
 * This file contains mixin classes that add processor-specific data and functionality
 * only when the corresponding feature is enabled via ProcessorTraits. Empty base
 * optimization ensures zero overhead when features are disabled.
 */

#include <cstdint>
#include <type_traits>
#include "fam65xx_processor_traits.hpp"
#include "nes6502.h"

#ifdef __cplusplus

namespace fam65xx_cpp {

// ============================================================================
// EMPTY BASE FOR DISABLED FEATURES
// ============================================================================

// Empty mixin used when feature is not present (EBO eliminates overhead)
struct empty_mixin_t {};

// ============================================================================
// I/O PORT MIXIN (6510-style processors)
// ============================================================================

// I/O Port functionality for processors like MOS 6510 (C64/C128)
template<typename ProcessorTag>
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
// BCD ARITHMETIC MIXIN
// ============================================================================

// Binary Coded Decimal arithmetic support (disabled on NES 6502)
template<typename ProcessorTag>
struct bcd_mixin_t {
    // Note: No additional state needed - BCD is algorithmic
    
    // Add with Carry in BCD mode
    uint8_t adc_bcd(uint8_t a, uint8_t b, bool carry_in, bool& carry_out, bool& overflow) {
        uint16_t al = (a & 0x0F) + (b & 0x0F) + (carry_in ? 1 : 0);
        if (al > 0x09) al += 0x06;
        
        uint16_t ah = (a >> 4) + (b >> 4) + (al > 0x0F ? 1 : 0);
        if (ah > 0x09) ah += 0x06;
        
        carry_out = (ah > 0x0F);
        
        // V flag behavior differs between NMOS and CMOS
        if constexpr (has_nmos_bugs<ProcessorTag>()) {
            // NMOS: V flag reflects binary operation result  
            uint16_t binary_result = a + b + (carry_in ? 1 : 0);
            overflow = ((a ^ binary_result) & (b ^ binary_result) & 0x80) != 0;
        } else {
            // CMOS: V flag undefined in BCD mode
            overflow = false;
        }
        
        return ((ah & 0x0F) << 4) | (al & 0x0F);
    }
    
    // BCD addition helper matching old implementation signature
    void bcd_addition_helper(uint8_t a_old, uint8_t operand, uint8_t carry_in, uint8_t* bcd_result, uint8_t* bcd_flags) {
        bool carry_out, overflow;
        *bcd_result = adc_bcd(a_old, operand, carry_in != 0, carry_out, overflow);
        
        // Generate flags matching old implementation
        *bcd_flags = (*bcd_result & 0x80) |                    // N flag
                    (*bcd_result == 0 ? 0x02 : 0) |           // Z flag (FLAG_Z = 0x02)
                    (carry_out ? 0x01 : 0) |                  // C flag (FLAG_C = 0x01)
                    (overflow ? 0x40 : 0);                    // V flag (FLAG_V = 0x40)
    }
    
    // Subtract with Borrow in BCD mode  
    uint8_t sbc_bcd(uint8_t a, uint8_t b, bool borrow_in, bool& carry_out, bool& overflow) {
        uint16_t al = (a & 0x0F) - (b & 0x0F) - (borrow_in ? 0 : 1);
        if (al & 0x10) al -= 0x06;
        
        uint16_t ah = (a >> 4) - (b >> 4) - ((al & 0x10) ? 1 : 0);
        if (ah & 0x10) ah -= 0x06;
        
        carry_out = !(ah & 0x10);
        
        // V flag behavior differs between NMOS and CMOS
        if constexpr (has_nmos_bugs<ProcessorTag>()) {
            // NMOS: V flag reflects binary operation result
            uint16_t binary_result = a - b - (borrow_in ? 0 : 1);
            overflow = ((a ^ b) & (a ^ binary_result) & 0x80) != 0;
        } else {
            // CMOS: V flag undefined in BCD mode  
            overflow = false;
        }
        
        return ((ah & 0x0F) << 4) | (al & 0x0F);
    }
};

// ============================================================================
// 65C02 ENHANCED STATE MIXIN
// ============================================================================

// Additional state for 65C02 processors (WAI/STP instructions)
template<typename ProcessorTag>
struct cmos_state_mixin_t {
    // Note: The base fam65xx_t already has wait_for_interrupt and stopped
    // This mixin could add additional CMOS-specific state if needed
    
    // For now, this is empty but provides extension point
    // Future: Could add timing state, enhanced addressing mode state, etc.
};

// ============================================================================
// 16-BIT MODE MIXIN (65C816)
// ============================================================================

// Extended state for 65C816 16-bit processor
template<typename ProcessorTag>
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
    
    // Mode checking helpers - moved to main class due to register access needs
};

// ============================================================================
// APU MIXIN (NES 6502 Audio Processing Unit)
// ============================================================================

// APU functionality for NES 6502 processors with integrated audio
template<typename ProcessorTag>
struct apu_mixin_t {
    // APU instance (aligned for performance)
    struct alignas(8) {
        nes6502_apu::APU* apu_instance;
        bool is_pal;
        uint8_t _padding[6]; // Align to 8 bytes
    } apu_state;
    
    // Initialize APU
    void init_apu() {
        apu_state.is_pal = false; // Default to NTSC
        apu_state.apu_instance = new nes6502_apu::APU(apu_state.is_pal);
    }
    
    // Cleanup APU
    void destroy_apu() {
        delete apu_state.apu_instance;
        apu_state.apu_instance = nullptr;
    }
    
    // APU register write handler ($4000-$4017)
    bool write_apu_register(uint16_t addr, uint8_t value) {
        if (addr >= 0x4000 && addr <= 0x4017) {
            if (apu_state.apu_instance) {
                apu_state.apu_instance->write(addr, value);
            }
            return true; // Handled
        }
        return false; // Not APU register
    }
    
    // APU register read handler ($4015)
    bool read_apu_register(uint16_t addr, uint8_t& value) {
        if (addr == 0x4015) {
            if (apu_state.apu_instance) {
                value = apu_state.apu_instance->read(addr);
            } else {
                value = 0;
            }
            return true; // Handled
        }
        return false; // Not APU register
    }
    
    // Clock APU (called every CPU cycle)
    void clock_apu() {
        if (apu_state.apu_instance) {
            apu_state.apu_instance->clock();
        }
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
};

// ============================================================================
// CONDITIONAL MIXIN TYPE SELECTION
// ============================================================================

// Use std::conditional to include mixins only when features are present
template<typename ProcessorTag>
using io_port_base_t = std::conditional_t<
    has_io_port<ProcessorTag>(),
    io_port_mixin_t<ProcessorTag>,
    empty_mixin_t
>;

// BCD and CMOS state functionality merged into main CPU class
// Only I/O port mixin remains active

template<typename ProcessorTag>
using bcd_base_t = empty_mixin_t;  // BCD merged into main class

template<typename ProcessorTag>
using cmos_state_base_t = empty_mixin_t;  // CMOS state merged into main class

template<typename ProcessorTag>
using wide_registers_base_t = empty_mixin_t;  // Wide registers disabled for now

template<typename ProcessorTag>
using apu_base_t = std::conditional_t<
    has_apu<ProcessorTag>(),
    apu_mixin_t<ProcessorTag>,
    empty_mixin_t
>;

} // namespace fam65xx_cpp

#endif // __cplusplus