#pragma once
/*
 * nes_ports.hpp — NES/Famicom Port Definitions (shared, header-only)
 *
 * Static PortDefinition constants for all NES-family ports.
 * These are used by NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Lives in src/ports/ because multiple system variants reference them.
 */

#include "core/port.hpp"

namespace NesPorts {

// ============================================================================
// NES Controller Ports (7-pin, removable)
// ============================================================================

inline const PortDefinition NES_CONTROLLER_1 = {
    PortType::CONTROLLER_NES,
    "Controller Port 1",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

inline const PortDefinition NES_CONTROLLER_2 = {
    PortType::CONTROLLER_NES,
    "Controller Port 2",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// NES Expansion Port (bottom, 48-pin)
// ============================================================================

inline const PortDefinition NES_EXPANSION = {
    PortType::EXPANSION_PORT,
    "Expansion Port",
    PortSignals::NES_EXPANSION_SIGNALS,
    PortSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// Famicom Controller Ports (hardwired, not removable)
// ============================================================================

inline const PortDefinition FC_CONTROLLER_1 = {
    PortType::CONTROLLER_NES,
    "Controller I (hardwired)",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

inline const PortDefinition FC_CONTROLLER_2 = {
    PortType::CONTROLLER_NES,
    "Controller II (hardwired, microphone)",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// Famicom Expansion Port (15-pin)
// ============================================================================

inline const PortDefinition FC_EXPANSION = {
    PortType::EXPANSION_PORT,
    "Expansion Port (15-pin)",
    PortSignals::NES_EXPANSION_SIGNALS,
    PortSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

} // namespace NesPorts
