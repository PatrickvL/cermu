#pragma once
/*
 * nes_ports.hpp — NES/Famicom Port Manifests (shared, header-only)
 *
 * Declarative port manifests for all NES-family systems.
 * Used by NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Lives in src/ports/ because multiple system variants reference them.
 */

#include "core/port_manifest.hpp"

// ============================================================================
// NES Port Manifest (removable controllers, 48-pin expansion)
// ============================================================================
//                              tag       type             name                       num  int  bus  default_device
#define NES_FOR_EACH_PORT(V, ctx)                                                                                  \
    V(ctx, CTRL1,     CONTROLLER_NES,   "Controller Port 1",         1, false, false, "nes_gamepad")               \
    V(ctx, CTRL2,     CONTROLLER_NES,   "Controller Port 2",         2, false, false, "nes_gamepad")               \
    V(ctx, EXPANSION, EXPANSION_PORT,   "Expansion Port",            0, false, false, nullptr)                     \
    V(ctx, VIDEO,     VIDEO_COMPOSITE,  "Video Out",                 0, false, false, "crt_tv")                    \
    V(ctx, AUDIO,     AUDIO_MONO,       "Audio Out",                 0, false, false, nullptr)

inline constexpr PortSlot kNESPorts[] = {
    NES_FOR_EACH_PORT(PORT_VISITOR_SLOT, unused)
};

// ============================================================================
// Famicom Port Manifest (hardwired controllers, 15-pin expansion)
// ============================================================================
//                             tag       type             name                                num  int  bus  default_device
#define FC_FOR_EACH_PORT(V, ctx)                                                                                              \
    V(ctx, CTRL1,     CONTROLLER_NES,   "Controller I (hardwired)",              1, false, false, "nes_gamepad")               \
    V(ctx, CTRL2,     CONTROLLER_NES,   "Controller II (hardwired, microphone)", 2, false, false, "nes_gamepad")               \
    V(ctx, EXPANSION, EXPANSION_PORT,   "Expansion Port (15-pin)",               0, false, false, nullptr)                     \
    V(ctx, VIDEO,     VIDEO_COMPOSITE,  "Video Out",                             0, false, false, "crt_tv")                    \
    V(ctx, AUDIO,     AUDIO_MONO,       "Audio Out",                             0, false, false, nullptr)

inline constexpr PortSlot kFCPorts[] = {
    FC_FOR_EACH_PORT(PORT_VISITOR_SLOT, unused)
};
