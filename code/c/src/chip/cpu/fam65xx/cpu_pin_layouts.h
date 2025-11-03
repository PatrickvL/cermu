/*
 * cpu_pin_layouts.h - CPU trait-specific pin layout definitions
 * 
 * This header defines pin layouts for different CPU variants based on their
 * specific features and capabilities using CPU traits.
 */

#ifndef CPU_PIN_LAYOUTS_H
#define CPU_PIN_LAYOUTS_H

#include "chip_visualization.h"
#include "fam65xx_processor_traits.hpp"
#include "../../core/system_lines.h"

// ============================================================================
// CPU TRAIT-BASED PIN LAYOUT FACTORY
// ============================================================================

// Template function to create pin layout based on CPU traits
template<const fam65xx::CPUTraits& Traits>
PinLayout create_cpu_pin_layout();

// Specialized layouts for different CPU variants
template<>
PinLayout create_cpu_pin_layout<fam65xx::MOS6502>();

template<>
PinLayout create_cpu_pin_layout<fam65xx::MOS6510>();

template<>
PinLayout create_cpu_pin_layout<fam65xx::WDC65C02>();

template<>
PinLayout create_cpu_pin_layout<fam65xx::WDC65C816>();

template<>
PinLayout create_cpu_pin_layout<fam65xx::RICOH_2A03>();

template<>
PinLayout create_cpu_pin_layout<fam65xx::ROCKWELL_R65C02>();

// Helper function to get CPU-specific pin state from bus and CPU state
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, bus_state_t bus_state);

// Fallback version for when bus state is not available
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu);

#endif // CPU_PIN_LAYOUTS_H