/**
 * port.cpp - Generic Port Framework Implementation
 */

#include "core/port.hpp"
#include "core/port_registry.hpp"
#include "core/cermu.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

// ============================================================================
// SELF-REGISTER STANDARD CONNECTOR DEFINITIONS
// ============================================================================
// All predefined signal tables are registered in the PortRegistry so
// boards can look up definitions by PortType at runtime.

using namespace PortSignals;

REGISTER_PORT(PortDefinition{
    PortType::CONTROL_PORT_DB9, "Control Port (DB-9)",
    CONTROL_PORT_SIGNALS, CONTROL_PORT_SIGNAL_COUNT, false, false
})

REGISTER_PORT(PortDefinition{
    PortType::IEC_SERIAL, "IEC Serial Bus",
    IEC_SERIAL_SIGNALS, IEC_SERIAL_SIGNAL_COUNT, false, true
})

REGISTER_PORT(PortDefinition{
    PortType::CASSETTE_PORT, "Cassette Port",
    CASSETTE_PORT_SIGNALS, CASSETTE_PORT_SIGNAL_COUNT, false, false
})

REGISTER_PORT(PortDefinition{
    PortType::USER_PORT, "User Port",
    USER_PORT_SIGNALS, USER_PORT_SIGNAL_COUNT, false, false
})

// Video/audio output connector types — no signal lines, used for output descriptors
REGISTER_PORT(PortDefinition{
    PortType::VIDEO_COMPOSITE, "Composite Video",
    nullptr, 0, false, false
})

REGISTER_PORT(PortDefinition{
    PortType::AUDIO_MONO, "Audio (Mono)",
    nullptr, 0, false, false
})

REGISTER_PORT(PortDefinition{
    PortType::AUDIO_STEREO, "Audio (Stereo)",
    nullptr, 0, false, false
})

// ============================================================================
// CONNECTOR TYPE NAMES
// ============================================================================

const char* port_type_name(PortType type) {
    switch (type) {
        case PortType::CONTROL_PORT_DB9: return "Control Port (DB-9)";
        case PortType::IEC_SERIAL:       return "IEC Serial Bus";
        case PortType::CASSETTE_PORT:    return "Cassette Port";
        case PortType::USER_PORT:        return "User Port";
        case PortType::EXPANSION_PORT:   return "Expansion Port";
        case PortType::CONTROLLER_NES:   return "NES Controller";
        case PortType::CONTROLLER_SNES:  return "SNES Controller";
        case PortType::CONTROLLER_ATARI: return "Atari Controller";
        case PortType::VIDEO_COMPOSITE:  return "Composite Video";
        case PortType::VIDEO_SVIDEO:     return "S-Video";
        case PortType::VIDEO_RGB:        return "RGB Video";
        case PortType::VIDEO_RGBI:       return "RGBI Video";
        case PortType::VIDEO_COMPONENT:  return "Component Video";
        case PortType::VIDEO_HDMI:       return "HDMI Video";
        case PortType::AUDIO_MONO:       return "Audio (Mono)";
        case PortType::AUDIO_STEREO:     return "Audio (Stereo)";
        case PortType::AUDIO_SPDIF:      return "S/PDIF Audio";
        case PortType::AUDIO_HDMI:       return "HDMI Audio";
        case PortType::CUSTOM:           return "Custom";
        default:                              return "Unknown";
    }
}

// ============================================================================
// PREDEFINED SIGNAL LINE TABLES
// ============================================================================

namespace PortSignals {

// --- Control Port (DB-9) ---
const SignalLine CONTROL_PORT_SIGNALS[] = {
    { "UP",       SignalDirection::INPUT,         JOY_UP    },
    { "DOWN",     SignalDirection::INPUT,         JOY_DOWN  },
    { "LEFT",     SignalDirection::INPUT,         JOY_LEFT  },
    { "RIGHT",    SignalDirection::INPUT,         JOY_RIGHT },
    { "POTX",     SignalDirection::BIDIRECTIONAL, JOY_POTX  },
    { "POTY",     SignalDirection::BIDIRECTIONAL, JOY_POTY  },
    { "FIRE",     SignalDirection::INPUT,         JOY_FIRE  },
    { "LIGHTPEN", SignalDirection::INPUT,         LIGHT_PEN },
};
const uint8_t CONTROL_PORT_SIGNAL_COUNT = sizeof(CONTROL_PORT_SIGNALS) / sizeof(CONTROL_PORT_SIGNALS[0]);

// --- IEC Serial Bus ---
const SignalLine IEC_SERIAL_SIGNALS[] = {
    { "ATN",   SignalDirection::OUTPUT,        IEC_ATN   },
    { "CLK",   SignalDirection::BIDIRECTIONAL, IEC_CLK   },
    { "DATA",  SignalDirection::BIDIRECTIONAL, IEC_DATA  },
    { "SRQ",   SignalDirection::INPUT,         IEC_SRQ   },
    { "RESET", SignalDirection::OUTPUT,        IEC_RESET },
};
const uint8_t IEC_SERIAL_SIGNAL_COUNT = sizeof(IEC_SERIAL_SIGNALS) / sizeof(IEC_SERIAL_SIGNALS[0]);

// --- Cassette Port ---
const SignalLine CASSETTE_PORT_SIGNALS[] = {
    { "MOTOR",  SignalDirection::OUTPUT,        CASS_MOTOR },
    { "READ",   SignalDirection::INPUT,         CASS_READ  },
    { "WRITE",  SignalDirection::OUTPUT,        CASS_WRITE },
    { "SENSE",  SignalDirection::INPUT,         CASS_SENSE },
};
const uint8_t CASSETTE_PORT_SIGNAL_COUNT = sizeof(CASSETTE_PORT_SIGNALS) / sizeof(CASSETTE_PORT_SIGNALS[0]);

// --- User Port ---
const SignalLine USER_PORT_SIGNALS[] = {
    { "PB0",   SignalDirection::BIDIRECTIONAL, USER_PB0  },
    { "PB1",   SignalDirection::BIDIRECTIONAL, USER_PB1  },
    { "PB2",   SignalDirection::BIDIRECTIONAL, USER_PB2  },
    { "PB3",   SignalDirection::BIDIRECTIONAL, USER_PB3  },
    { "PB4",   SignalDirection::BIDIRECTIONAL, USER_PB4  },
    { "PB5",   SignalDirection::BIDIRECTIONAL, USER_PB5  },
    { "PB6",   SignalDirection::BIDIRECTIONAL, USER_PB6  },
    { "PB7",   SignalDirection::BIDIRECTIONAL, USER_PB7  },
    { "SP",    SignalDirection::BIDIRECTIONAL, USER_SP   },
    { "CNT",   SignalDirection::BIDIRECTIONAL, USER_CNT  },
    { "PA2",   SignalDirection::BIDIRECTIONAL, USER_PA2  },
    { "FLAG",  SignalDirection::INPUT,         USER_FLAG },
    { "RESET", SignalDirection::OUTPUT,        USER_RESET },
};
const uint8_t USER_PORT_SIGNAL_COUNT = sizeof(USER_PORT_SIGNALS) / sizeof(USER_PORT_SIGNALS[0]);

// --- NES Controller Port (7-pin) ---
const SignalLine NES_CONTROLLER_SIGNALS[] = {
    { "CLK",   SignalDirection::OUTPUT, NES_CLK   },
    { "LATCH", SignalDirection::OUTPUT, NES_LATCH },
    { "D0",    SignalDirection::INPUT,  NES_D0    },
    { "D3",    SignalDirection::INPUT,  NES_D3    },
    { "D4",    SignalDirection::INPUT,  NES_D4    },
};
const uint8_t NES_CONTROLLER_SIGNAL_COUNT = sizeof(NES_CONTROLLER_SIGNALS) / sizeof(NES_CONTROLLER_SIGNALS[0]);

// --- NES Expansion Port (48-pin) ---
const SignalLine NES_EXPANSION_SIGNALS[] = {
    { "D0",    SignalDirection::INPUT,         NEXP_D0    },
    { "D1",    SignalDirection::INPUT,         NEXP_D1    },
    { "D2",    SignalDirection::INPUT,         NEXP_D2    },
    { "D3",    SignalDirection::INPUT,         NEXP_D3    },
    { "D4",    SignalDirection::INPUT,         NEXP_D4    },
    { "OUT0",  SignalDirection::OUTPUT,        NEXP_OUT0  },
    { "OUT1",  SignalDirection::OUTPUT,        NEXP_OUT1  },
    { "OUT2",  SignalDirection::OUTPUT,        NEXP_OUT2  },
    { "CLK",   SignalDirection::OUTPUT,        NEXP_CLK   },
    { "LATCH", SignalDirection::OUTPUT,        NEXP_LATCH },
    { "/IRQ",  SignalDirection::INPUT,         NEXP_IRQ   },
};
const uint8_t NES_EXPANSION_SIGNAL_COUNT = sizeof(NES_EXPANSION_SIGNALS) / sizeof(NES_EXPANSION_SIGNALS[0]);

// --- Apple 1 Expansion Connector (44-pin edge) ---
const SignalLine APPLE1_EXPANSION_SIGNALS[] = {
    { "/RESET", SignalDirection::OUTPUT,        A1_RESET },
    { "/IRQ",   SignalDirection::INPUT,         A1_IRQ   },
    { "RDY",    SignalDirection::INPUT,         A1_RDY   },
    { "PHI2",   SignalDirection::OUTPUT,        A1_PHI2  },
    { "R/W",    SignalDirection::BIDIRECTIONAL, A1_RW    },
};
const uint8_t APPLE1_EXPANSION_SIGNAL_COUNT = sizeof(APPLE1_EXPANSION_SIGNALS) / sizeof(APPLE1_EXPANSION_SIGNALS[0]);

// --- Apple 1 Cassette Interface (ACI) ---
const SignalLine APPLE1_CASSETTE_SIGNALS[] = {
    { "CASS_IN",  SignalDirection::INPUT,  A1_CASS_IN  },
    { "CASS_OUT", SignalDirection::OUTPUT, A1_CASS_OUT },
};
const uint8_t APPLE1_CASSETTE_SIGNAL_COUNT = sizeof(APPLE1_CASSETTE_SIGNALS) / sizeof(APPLE1_CASSETTE_SIGNALS[0]);

} // namespace PortSignals

// ============================================================================
// MANIFEST SUPPORT — make_port_definition()
// ============================================================================

#include "core/port_manifest.hpp"
#include "core/port_registry.hpp"

PortDefinition make_port_definition(const PortSlot& slot) {
    // Look up standard signal table from the global registry.
    // For types without a registry entry (CUSTOM, EXPANSION_PORT with
    // system-specific signals) this returns an empty definition
    // (signals=nullptr, signal_count=0) which is perfectly valid.
    const auto& base = PortRegistry::instance().lookup(slot.type);

    return PortDefinition{
        slot.type,
        slot.name,
        base.signals,
        base.signal_count,
        slot.is_internal,
        slot.is_bus,
    };
}

// ============================================================================
// CONNECTOR PORT IMPLEMENTATION
// ============================================================================

Port::Port(const PortDefinition& def, int port_index)
    : definition_(def)
    , port_index_(port_index)
    , system_signals_(0xFFFFFFFF)   // All lines idle (high)
    , combined_device_signals_(0xFFFFFFFF)   // No devices: all high
{
}

Port::Port()
    : definition_{}
    , port_index_(0)
    , system_signals_(0xFFFFFFFF)
    , combined_device_signals_(0xFFFFFFFF)
{
}

void Port::init(const PortDefinition& def, int port_index) {
    definition_ = def;
    port_index_ = port_index;
    system_signals_ = 0xFFFFFFFF;
    combined_device_signals_ = 0xFFFFFFFF;
}

Port::~Port() {
    // Don't call detach_device() here — it accesses device objects
    // (get_name, on_detach) that may already be destroyed when the
    // owning System's destructor runs (owned_devices_ is destroyed
    // before the board which owns the ports).
    attached_devices_.clear();
}

uint32_t Port::read_signals() const {
    // Wired-AND: system output AND combined device outputs.
    // A line is LOW (asserted) if ANY participant pulls it low.
    return system_signals_ & combined_device_signals_;
}

void Port::write_system_signals(uint32_t mask, uint32_t value) {
    system_signals_ = (system_signals_ & ~mask) | (value & mask);

    // Notify all attached devices of the new combined state
    uint32_t combined = read_signals();
    for (auto* device : attached_devices_) {
        device->on_signal_change(combined);
    }
}

bool Port::attach_device(PeripheralDevice* device) {
    if (!device) return false;

    // Type compatibility check
    if (!device->is_compatible_with(definition_.type)) {
        log_info("Port: Cannot attach '%s' — incompatible port type "
               "(device needs %s, port is %s)\n",
               device->get_name(),
               port_type_name(device->get_port_type()),
               port_type_name(definition_.type));
        return false;
    }

    // For point-to-point ports: detach existing device first
    if (!definition_.is_bus && !attached_devices_.empty()) {
        detach_device(nullptr);
    }

    // Check for duplicate attachment
    for (auto* d : attached_devices_) {
        if (d == device) {
            log_info("Port: '%s' already attached to %s (port %d)\n",
                   device->get_name(), definition_.name, port_index_);
            return false;
        }
    }

    attached_devices_.push_back(device);
    recompute_device_signals();
    device->on_attach(this);

            log_debug("Port: '%s' attached to %s (port %d)%s\n",
               device->get_name(), definition_.name, port_index_,
               definition_.is_bus ? " [bus]" : "");
    return true;
}

void Port::detach_device(PeripheralDevice* device) {
    if (attached_devices_.empty()) return;

    if (device == nullptr) {
        // Detach ALL devices
        for (auto* d : attached_devices_) {
                            log_debug("Port: '%s' detached from %s (port %d)\n",
                       d->get_name(), definition_.name, port_index_);
            d->on_detach(this);
        }
        attached_devices_.clear();
    } else {
        // Detach a specific device
        auto it = std::find(attached_devices_.begin(), attached_devices_.end(), device);
        if (it == attached_devices_.end()) return;

                    log_debug("Port: '%s' detached from %s (port %d)\n",
                   device->get_name(), definition_.name, port_index_);
        device->on_detach(this);
        attached_devices_.erase(it);
    }

    recompute_device_signals();

    // Notify system that signals changed (device(s) removed)
    if (on_device_output_changed_) {
        on_device_output_changed_(this, read_signals());
    }
}

void Port::notify_device_output_changed(uint32_t /*device_signals*/) {
    // A device changed its output — recompute the AND of all device outputs
    recompute_device_signals();

    if (on_device_output_changed_) {
        on_device_output_changed_(this, read_signals());
    }
}

void Port::recompute_device_signals() {
    uint32_t combined = 0xFFFFFFFF;
    for (auto* d : attached_devices_) {
        combined &= d->get_output_signals();
    }
    combined_device_signals_ = combined;
}
