#pragma once
/**
 * port_manifest.hpp — Declarative port manifest system
 *
 * Provides a compact one-liner declaration macro for system connector ports,
 * analogous to the chip manifest FOR_EACH pattern.  Systems define a
 * FOR_EACH_PORT macro; visitor macros expand rows into PortSlot arrays,
 * port index enums, and default-peripheral lists.
 *
 * Every visitor macro receives (ctx, tag, type, name, port_num, is_internal,
 * is_bus, default_dev) where ctx is a pass-through context argument (board
 * pointer, system reference, or `unused`) — same convention as the chip
 * visitor macros.
 *
 * Usage:
 *   // In system header / manifest header:
 *   #define C64_FOR_EACH_PORT(V, ctx)                                            \
 *       V(ctx, CONTROL1,  CONTROL_PORT_DB9, "Control Port 1", 1, false, false, "mouse_1351") \
 *       V(ctx, CONTROL2,  CONTROL_PORT_DB9, "Control Port 2", 2, false, false, "joystick")   \
 *       V(ctx, IEC,       IEC_SERIAL,       "IEC Serial Bus", 0, false, true,  "1541")       \
 *       ...
 *
 *   CERMU_PORT_MANIFEST(C64, C64_FOR_EACH_PORT)
 *
 * This generates:
 *   - enum C64Port { PORT_CONTROL1, PORT_CONTROL2, ..., C64_PORT_COUNT };
 *   - inline constexpr PortSlot kC64Ports[] = { ... };
 */

#include "core/port.hpp"

// ============================================================================
// PORT SLOT — compact port descriptor for manifest-driven creation
// ============================================================================

struct PortSlot {
    PortType    type;           ///< Connector type (determines default signals)
    const char* name;           ///< Display name ("Control Port 1", "IEC Serial Bus")
    int         port_number;    ///< 0 = no numbering, 1+ = player/slot number
    bool        is_internal;    ///< Internal connectors (keyboard) hidden from icon bar
    bool        is_bus;         ///< Shared bus (IEC): multiple devices may attach
    const char* default_device; ///< DeviceRegistry ID for auto-attach, or nullptr
};

/// Build a full PortDefinition from a PortSlot, auto-resolving standard
/// signal tables based on PortType.  Call once per port during setup.
PortDefinition make_port_definition(const PortSlot& slot);

// ============================================================================
// PORT VISITOR MACROS — consume FOR_EACH_PORT rows
// ============================================================================

/// Expand a FOR_EACH_PORT row into a PortSlot aggregate initializer.
#define PORT_VISITOR_SLOT(ctx, tag, type, name, port_num, is_internal, is_bus, default_dev) \
    PortSlot{PortType::type, name, port_num, is_internal, is_bus, default_dev},

/// Expand a FOR_EACH_PORT row into an enum entry for port indexing.
#define PORT_VISITOR_ENUM_ENTRY(ctx, tag, type, name, port_num, is_internal, is_bus, default_dev) \
    PORT_##tag,

/// Count one port (expands to +1).
#define PORT_VISITOR_COUNT_ONE(ctx, tag, type, name, port_num, is_internal, is_bus, default_dev) +1

// ============================================================================
// CONVENIENCE — generate enum + slot array from a FOR_EACH macro
// ============================================================================

/// Place at namespace or file scope (not inside a class).  Generates:
///   enum <PREFIX>Port { PORT_TAG1, PORT_TAG2, ..., <PREFIX>_PORT_COUNT };
///   inline constexpr PortSlot k<PREFIX>Ports[] = { ... };
#define CERMU_PORT_MANIFEST(PREFIX, FOR_EACH)                                    \
    enum PREFIX##Port {                                                          \
        FOR_EACH(PORT_VISITOR_ENUM_ENTRY, unused)                                \
        PREFIX##_PORT_COUNT                                                      \
    };                                                                           \
    inline constexpr PortSlot k##PREFIX##Ports[] = {                             \
        FOR_EACH(PORT_VISITOR_SLOT, unused)                                      \
    };
