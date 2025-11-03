/*
 * cpu_pin_layouts.cpp - CPU trait-specific pin layout implementations
 */

#include "cpu_pin_layouts.h"
#include "fam65xx.hpp"
#include "../../core/system_lines.h"

using namespace fam65xx;

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS 
// ============================================================================
// Explicit template instantiations to ensure template functions are compiled
template PinLayout create_cpu_pin_layout<fam65xx::MOS6502>();
template PinLayout create_cpu_pin_layout<fam65xx::MOS6510>();  
template PinLayout create_cpu_pin_layout<fam65xx::WDC_W65C02S>();
template PinLayout create_cpu_pin_layout<fam65xx::WDC_65C816>();
template PinLayout create_cpu_pin_layout<fam65xx::RICOH_2A03>();
template PinLayout create_cpu_pin_layout<fam65xx::ROCKWELL_R65C02>();

template std::vector<PinState> get_cpu_pin_states<fam65xx::MOS6502>(fam65xx::fam65xx_t<fam65xx::MOS6502>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::MOS6510>(fam65xx::fam65xx_t<fam65xx::MOS6510>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::WDC_W65C02S>(fam65xx::fam65xx_t<fam65xx::WDC_W65C02S>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::WDC_65C816>(fam65xx::fam65xx_t<fam65xx::WDC_65C816>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::RICOH_2A03>(fam65xx::fam65xx_t<fam65xx::RICOH_2A03>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::ROCKWELL_R65C02>(fam65xx::fam65xx_t<fam65xx::ROCKWELL_R65C02>* cpu, bus_state_t bus_state);