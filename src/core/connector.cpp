/**
 * connector.cpp - Generic Connector Framework Implementation
 */

#include "connector.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// ============================================================================
// CONNECTOR TYPE NAMES
// ============================================================================

const char* connector_type_name(ConnectorType type) {
    switch (type) {
        case ConnectorType::CONTROL_PORT_DB9: return "Control Port (DB-9)";
        case ConnectorType::IEC_SERIAL:       return "IEC Serial Bus";
        case ConnectorType::CASSETTE_PORT:    return "Cassette Port";
        case ConnectorType::USER_PORT:        return "User Port";
        case ConnectorType::EXPANSION_PORT:   return "Expansion Port";
        case ConnectorType::CONTROLLER_NES:   return "NES Controller";
        case ConnectorType::CONTROLLER_SNES:  return "SNES Controller";
        case ConnectorType::CONTROLLER_ATARI: return "Atari Controller";
        case ConnectorType::AUDIO_VIDEO:      return "Audio/Video";
        case ConnectorType::POWER:            return "Power";
        case ConnectorType::CUSTOM:           return "Custom";
        default:                              return "Unknown";
    }
}

// ============================================================================
// PREDEFINED SIGNAL LINE TABLES
// ============================================================================

namespace ConnectorSignals {

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

} // namespace ConnectorSignals

// ============================================================================
// CONNECTOR PORT IMPLEMENTATION
// ============================================================================

ConnectorPort::ConnectorPort(const ConnectorDefinition& def, int port_index)
    : definition_(def)
    , port_index_(port_index)
    , system_signals_(0xFFFFFFFF)   // All lines idle (high)
    , combined_device_signals_(0xFFFFFFFF)   // No devices: all high
{
}

ConnectorPort::~ConnectorPort() {
    // Don't call detach_device() here — it accesses device objects
    // (get_name, on_detach) that may already be destroyed when the
    // owning EmulatedSystem's destructor runs (owned_devices_ is
    // destroyed before connector_ports_ due to member declaration order).
    attached_devices_.clear();
}

uint32_t ConnectorPort::read_signals() const {
    // Wired-AND: system output AND combined device outputs.
    // A line is LOW (asserted) if ANY participant pulls it low.
    return system_signals_ & combined_device_signals_;
}

void ConnectorPort::write_system_signals(uint32_t mask, uint32_t value) {
    system_signals_ = (system_signals_ & ~mask) | (value & mask);

    // Notify all attached devices of the new combined state
    uint32_t combined = read_signals();
    for (auto* device : attached_devices_) {
        device->on_signal_change(combined);
    }
}

bool ConnectorPort::attach_device(PeripheralDevice* device) {
    if (!device) return false;

    // Type compatibility check
    if (device->get_connector_type() != definition_.type) {
        printf("Connector: Cannot attach '%s' — incompatible connector type "
               "(device needs %s, port is %s)\n",
               device->get_name(),
               connector_type_name(device->get_connector_type()),
               connector_type_name(definition_.type));
        return false;
    }

    // For point-to-point ports: detach existing device first
    if (!definition_.is_bus && !attached_devices_.empty()) {
        detach_device(nullptr);
    }

    // Check for duplicate attachment
    for (auto* d : attached_devices_) {
        if (d == device) {
            printf("Connector: '%s' already attached to %s (port %d)\n",
                   device->get_name(), definition_.name, port_index_);
            return false;
        }
    }

    attached_devices_.push_back(device);
    recompute_device_signals();
    device->on_attach(this);

    printf("Connector: '%s' attached to %s (port %d)%s\n",
           device->get_name(), definition_.name, port_index_,
           definition_.is_bus ? " [bus]" : "");
    return true;
}

void ConnectorPort::detach_device(PeripheralDevice* device) {
    if (attached_devices_.empty()) return;

    if (device == nullptr) {
        // Detach ALL devices
        for (auto* d : attached_devices_) {
            printf("Connector: '%s' detached from %s (port %d)\n",
                   d->get_name(), definition_.name, port_index_);
            d->on_detach();
        }
        attached_devices_.clear();
    } else {
        // Detach a specific device
        auto it = std::find(attached_devices_.begin(), attached_devices_.end(), device);
        if (it == attached_devices_.end()) return;

        printf("Connector: '%s' detached from %s (port %d)\n",
               device->get_name(), definition_.name, port_index_);
        device->on_detach();
        attached_devices_.erase(it);
    }

    recompute_device_signals();

    // Notify system that signals changed (device(s) removed)
    if (on_device_output_changed_) {
        on_device_output_changed_(this, read_signals());
    }
}

void ConnectorPort::notify_device_output_changed(uint32_t /*device_signals*/) {
    // A device changed its output — recompute the AND of all device outputs
    recompute_device_signals();

    if (on_device_output_changed_) {
        on_device_output_changed_(this, read_signals());
    }
}

void ConnectorPort::recompute_device_signals() {
    uint32_t combined = 0xFFFFFFFF;
    for (auto* d : attached_devices_) {
        combined &= d->get_output_signals();
    }
    combined_device_signals_ = combined;
}
