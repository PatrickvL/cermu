/**
 * connector.cpp - Generic Connector Framework Implementation
 */

#include "connector.h"
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

} // namespace ConnectorSignals

// ============================================================================
// CONNECTOR PORT IMPLEMENTATION
// ============================================================================

ConnectorPort::ConnectorPort(const ConnectorDefinition& def, int port_index)
    : definition_(def)
    , port_index_(port_index)
    , system_signals_(0xFFFFFFFF)   // All lines idle (high)
    , device_signals_(0xFFFFFFFF)   // No device: all high
    , attached_device_(nullptr)
{
}

ConnectorPort::~ConnectorPort() {
    detach_device();
}

uint32_t ConnectorPort::read_signals() const {
    // Wired-AND: both sides contribute.
    // A line is LOW (asserted) if EITHER side pulls it low.
    return system_signals_ & device_signals_;
}

void ConnectorPort::write_system_signals(uint32_t mask, uint32_t value) {
    system_signals_ = (system_signals_ & ~mask) | (value & mask);

    // Notify the attached device of the new combined state
    if (attached_device_) {
        attached_device_->on_signal_change(read_signals());
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

    // Detach existing device first
    if (attached_device_) {
        detach_device();
    }

    attached_device_ = device;
    device_signals_  = device->get_output_signals();
    device->on_attach(this);

    printf("Connector: '%s' attached to %s (port %d)\n",
           device->get_name(), definition_.name, port_index_);
    return true;
}

void ConnectorPort::detach_device() {
    if (attached_device_) {
        printf("Connector: '%s' detached from %s (port %d)\n",
               attached_device_->get_name(), definition_.name, port_index_);
        attached_device_->on_detach();
        attached_device_ = nullptr;
        device_signals_  = 0xFFFFFFFF;  // All lines released

        // Notify system that signals changed (device removed)
        if (on_device_output_changed_) {
            on_device_output_changed_(this, read_signals());
        }
    }
}

void ConnectorPort::notify_device_output_changed(uint32_t device_signals) {
    device_signals_ = device_signals;

    if (on_device_output_changed_) {
        on_device_output_changed_(this, read_signals());
    }
}
