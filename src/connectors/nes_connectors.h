#pragma once
/*
 * nes_connectors.h — NES/Famicom Connector Definitions (shared, header-only)
 *
 * Static ConnectorDefinition constants for all NES-family connector ports.
 * These are used by NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Lives in src/connectors/ because multiple system variants reference them.
 */

#include "core/connector.h"

namespace NesConnectors {

// ============================================================================
// NES Controller Ports (7-pin, removable)
// ============================================================================

inline const ConnectorDefinition NES_CONTROLLER_1 = {
    ConnectorType::CONTROLLER_NES,
    "Controller Port 1",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

inline const ConnectorDefinition NES_CONTROLLER_2 = {
    ConnectorType::CONTROLLER_NES,
    "Controller Port 2",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// NES Expansion Port (bottom, 48-pin)
// ============================================================================

inline const ConnectorDefinition NES_EXPANSION = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    ConnectorSignals::NES_EXPANSION_SIGNALS,
    ConnectorSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// Famicom Controller Ports (hardwired, not removable)
// ============================================================================

inline const ConnectorDefinition FC_CONTROLLER_1 = {
    ConnectorType::CONTROLLER_NES,
    "Controller I (hardwired)",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

inline const ConnectorDefinition FC_CONTROLLER_2 = {
    ConnectorType::CONTROLLER_NES,
    "Controller II (hardwired, microphone)",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

// ============================================================================
// Famicom Expansion Port (15-pin)
// ============================================================================

inline const ConnectorDefinition FC_EXPANSION = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port (15-pin)",
    ConnectorSignals::NES_EXPANSION_SIGNALS,
    ConnectorSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

} // namespace NesConnectors
