/*
 * nes_ports.cpp — NES/Famicom port self-registration
 *
 * Registers NES-family port types in the PortRegistry so boards
 * and tooling can discover them by PortType at runtime.
 */

#include "ports/nes_ports.hpp"
#include "core/port_registry.hpp"

using namespace PortSignals;

// NES controller port (7-pin, shared by NES + Famicom controllers)
REGISTER_PORT(PortDefinition{
    PortType::CONTROLLER_NES, "NES Controller Port",
    NES_CONTROLLER_SIGNALS, NES_CONTROLLER_SIGNAL_COUNT, false, false
})

// NES/Famicom expansion port (active-low accent signals)
REGISTER_PORT(PortDefinition{
    PortType::EXPANSION_PORT, "NES Expansion Port",
    NES_EXPANSION_SIGNALS, NES_EXPANSION_SIGNAL_COUNT, false, false
})
