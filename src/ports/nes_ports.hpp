#pragma once
/*
 * nes_ports.hpp — NES/Famicom Port Manifests (shared, header-only)
 *
 * Declarative port manifests for all NES-family systems.
 * Used by NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Lives in src/ports/ because multiple system variants reference them.
 */

#include "core/port.hpp"                   // PortSlot, PortType

// ============================================================================
// NES Port Manifest (removable controllers, 48-pin expansion)
// ============================================================================

inline constexpr PortSlot kNESPorts[] = {
    {PortType::CONTROLLER_NES,  "Controller Port 1",  1, false, false, "nes_gamepad"},
    {PortType::CONTROLLER_NES,  "Controller Port 2",  2, false, false, "nes_gamepad"},
    {PortType::EXPANSION_PORT,  "Expansion Port",     0, false, false, nullptr},
    {PortType::VIDEO_COMPOSITE, "Video Out",          0, false, false, "crt_tv"},
    {PortType::AUDIO_MONO,      "Audio Out",          0, false, false, nullptr},
};

// ============================================================================
// Famicom Port Manifest (hardwired controllers, 15-pin expansion)
// ============================================================================

inline constexpr PortSlot kFCPorts[] = {
    {PortType::CONTROLLER_NES,  "Controller I (hardwired)",              1, false, false, "nes_gamepad"},
    {PortType::CONTROLLER_NES,  "Controller II (hardwired, microphone)", 2, false, false, "nes_gamepad"},
    {PortType::EXPANSION_PORT,  "Expansion Port (15-pin)",               0, false, false, nullptr},
    {PortType::VIDEO_COMPOSITE, "Video Out",                             0, false, false, "crt_tv"},
    {PortType::AUDIO_MONO,      "Audio Out",                             0, false, false, nullptr},
};
