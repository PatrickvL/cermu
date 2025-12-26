#pragma once
/*
 * fam65xx_mixins.hpp - Conditional Feature Mixins for MOS 65xx Family
 *
 * This file contains mixin classes that add processor-specific data and
 * functionality only when the corresponding feature is enabled via CPUTraits.
 * Empty base optimization ensures zero overhead when features are disabled.
 */

#include "fam65xx_processor_traits.hpp"
#include "fam65xx_types.h" // For bus_state_t
#include "nes6502.h"       // For nes6502_apu::APU class
#include <cstdint>
#include <cstdio>          // For printf in debug output
#include <type_traits>

namespace fam65xx {

// ============================================================================
// EMPTY BASES FOR DISABLED FEATURES (processor-specific to avoid collisions)
// ============================================================================

// Separate empty types per feature to prevent duplicate base class errors
struct empty_io_port_mixin_t {};
struct empty_apu_mixin_t {};

// ============================================================================
// I/O PORT MIXIN (6510-style processors)
// ============================================================================

// I/O Port functionality for processors like MOS 6510 (C64/C128)
template <const CPUTraits &Traits> struct io_port_mixin_t {
  // I/O Port registers (aligned to 4-byte boundary)
  struct alignas(4) {
    uint8_t direction; // Data Direction Register (DDR) at $00
    uint8_t data;      // Port data register at $01
    uint8_t input;     // External input pin state
    uint8_t _padding;  // Align to 4 bytes
  } io_port;

  // Chip descriptor pointer - needed to call bank_change callback
  chip_descriptor_t* descriptor = nullptr;
  void* chip_instance = nullptr;

  // Initialize I/O port to C64 defaults
  void init_io_port() {
    io_port.direction = 0x2F; // C64 default: bits 0,1,2,3,5 output
    io_port.data = 0x17;      // C64 default: LORAM=1, HIRAM=1, CHAREN=1 (mode $17 for BASIC/KERNAL + I/O)
    io_port.input = 0x17;     // Default input state (cassette sense)
  }

  // Write to Data Direction Register ($00)
  void write_io_ddr(uint8_t value) {
    uint8_t old_direction = io_port.direction;
    io_port.direction = value;
    // If DDR changes affect banking bits (0-2), notify via descriptor's bank_change
    if (((old_direction ^ value) & 0x07) && descriptor && descriptor->bank_change) {
      uint8_t banking_bits = io_port.data & io_port.direction & 0x07;
      descriptor->bank_change(chip_instance, banking_bits);
    }
  }

  // Write to Port data register ($01)
  void write_io_data(uint8_t value) {
    uint8_t old_data = io_port.data;
    io_port.data = value;
    // If banking bits (0-2) changed, notify via descriptor's bank_change
    if ((old_data ^ value) & 0x07) {
      uint8_t banking_bits = value & io_port.direction & 0x07;
      if (descriptor && descriptor->bank_change) {
        descriptor->bank_change(chip_instance, banking_bits);
      }
    }
  }

  // Read from Port (combines output and input based on direction)
  uint8_t read_io_port() const {
    return (io_port.data & io_port.direction) |
           (io_port.input & ~io_port.direction);
  }

  // Update external input pins (for cartridge/tape interface)
  void set_io_input(uint8_t value) { io_port.input = value; }
};

// Note: 16-bit register support is now handled by fam65xx_register_mixins.hpp
// The register layout mixins provide both narrow and wide register support
// with automatic mode switching based on M and X flags.

// ============================================================================
// APU MIXIN (NES 6502 Audio Processing Unit)
// ============================================================================

// APU functionality for NES 6502 processors with integrated audio
template <const CPUTraits &Traits> struct apu_mixin_t {
  // APU instance (aligned for performance)
  struct alignas(8) {
    nes6502_apu::APU *apu_instance;
    bool is_pal;
    uint8_t _padding[6];       // Align to 8 bytes
  } apu_state;

  // Initialize APU
  void init_apu() {
    apu_state.apu_instance = nullptr;       // CRITICAL: Initialize pointer first to prevent access violation
    apu_state.is_pal = false;               // Default to NTSC
    destroy_apu(); // Ensure no existing instance as we're creating a new one,
                   // potentially with different region
    apu_state.apu_instance = new nes6502_apu::APU(apu_state.is_pal);
  }

  // Cleanup APU
  void destroy_apu() {
    if (apu_state.apu_instance != nullptr) {
      delete apu_state.apu_instance;
      apu_state.apu_instance = nullptr;
    }
  }

  // APU register write handler ($4000-$4017)
  bool write_apu_register(uint16_t addr, uint8_t value) {
    if (addr >= 0x4000 && addr <= 0x4017) {
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

  // APU register read handler ($4015)
  bool read_apu_register(uint16_t addr, uint8_t &value) {
    if (addr == 0x4015) {
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
    return apu_state.apu_instance ? apu_state.apu_instance->dmc_sample_address()
                                  : 0;
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
    if (apu_state.is_pal == is_pal_region)
      return;

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
template <const CPUTraits &Traits>
using io_port_base_t =
    std::conditional_t<Traits.has_io_port(), io_port_mixin_t<Traits>,
                       empty_io_port_mixin_t>;

// Note: Register layout selection is now handled by register_base_t in
// fam65xx_register_mixins.hpp

template <const CPUTraits &Traits>
using apu_base_t = std::conditional_t<Traits.peripheral.has_sound(),
                                      apu_mixin_t<Traits>, empty_apu_mixin_t>;

} // namespace fam65xx