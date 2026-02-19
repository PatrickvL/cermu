#pragma once
/**
 * connector.h - Generic Connector Framework
 *
 * Provides a hardware-accurate abstraction for physical connectors (ports/jacks)
 * on emulated systems.  Every connector has a set of named signal lines that
 * bridge between the system's internal chips and externally attached peripheral
 * devices.  Signal lines are active-low (matching real hardware pull-ups), so
 * an idle/unconnected state is all-ones.
 *
 * Architecture:
 *   System Chips  <-->  ConnectorPort  <-->  PeripheralDevice
 *
 * - The system writes its output signals into the ConnectorPort.
 * - The PeripheralDevice reads them and updates its own output signals.
 * - When either side reads the combined state, both contributions are AND-ed
 *   (open-collector/wired-AND, matching real hardware bus behaviour).
 *
 * ConnectorType enumerates all known physical connector standards so that the
 * DeviceRegistry can match peripherals to compatible ports.
 */

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// SIGNAL DIRECTION
// ============================================================================

/// Direction of a signal line from the *system's* perspective.
enum class SignalDirection {
    INPUT,          ///< System reads, device drives (e.g. joystick fire button)
    OUTPUT,         ///< System drives, device reads (e.g. IEC ATN from host)
    BIDIRECTIONAL   ///< Either side may drive (e.g. IEC DATA/CLK)
};

// ============================================================================
// CONNECTOR TYPE
// ============================================================================

/// Physical connector standard.
enum class ConnectorType {
    // --- Commodore family ---
    CONTROL_PORT_DB9,   ///< DB-9 joystick/paddle/lightpen/mouse (C64, VIC-20, Amiga, Atari)
    IEC_SERIAL,         ///< Commodore IEC serial bus (DIN-6) — disk drives, printers
    CASSETTE_PORT,      ///< Commodore datasette port (12-pin edge)
    USER_PORT,          ///< Commodore user port (24-pin edge) — RS-232, modems
    EXPANSION_PORT,     ///< Cartridge/expansion slot (44-pin edge on C64)

    // --- Nintendo ---
    CONTROLLER_NES,     ///< NES 7-pin controller port
    CONTROLLER_SNES,    ///< SNES 7-pin controller port

    // --- Atari ---
    CONTROLLER_ATARI,   ///< Atari 2600 controller port (DB-9, similar to Commodore)

    // --- Generic ---
    AUDIO_VIDEO,        ///< A/V output (active peripherals rarely connect here)
    POWER,              ///< Power supply connector
    CUSTOM,             ///< System-specific / non-standard

    COUNT               ///< Sentinel — total number of types
};

/// Human-readable name for a ConnectorType.
const char* connector_type_name(ConnectorType type);

// ============================================================================
// SIGNAL LINE DEFINITION
// ============================================================================

/// Describes one signal line on a connector.
struct SignalLine {
    const char*     name;       ///< Human-readable label (e.g. "UP", "ATN", "DATA")
    SignalDirection direction;  ///< Direction from system's perspective
    uint8_t         bit_index;  ///< Bit position in the port's 32-bit signal word
};

// ============================================================================
// CONNECTOR DEFINITION
// ============================================================================

/// Static descriptor for a connector type — shared by all ports of the same kind.
struct ConnectorDefinition {
    ConnectorType       type;
    const char*         name;           ///< E.g. "Control Port 1", "IEC Serial Bus"
    const SignalLine*   signals;        ///< Array of signal line descriptors
    uint8_t             signal_count;   ///< Number of entries in `signals`
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class PeripheralDevice;

// ============================================================================
// CONNECTOR PORT
// ============================================================================

/**
 * Represents a single physical connector jack on an emulated system.
 *
 * The system side writes its output via write_system_signals().
 * External devices are attached with attach_device() / detach_device().
 * read_signals() returns the wired-AND of system and device outputs.
 *
 * An optional callback (on_device_output_changed) lets the system react
 * immediately when the device drives new signal values.
 */
class ConnectorPort {
public:
    /// Callback: invoked when the attached device's output signals change.
    using SignalChangeCallback = std::function<void(ConnectorPort* port, uint32_t combined_state)>;

    explicit ConnectorPort(const ConnectorDefinition& def, int port_index = 0);
    ~ConnectorPort();

    // --- Identification ------------------------------------------------

    const ConnectorDefinition& get_definition() const { return definition_; }
    ConnectorType get_type() const { return definition_.type; }
    const char* get_name() const { return definition_.name; }
    int get_port_index() const { return port_index_; }

    // --- Signal state --------------------------------------------------

    /**
     * Read the combined signal state (wired-AND of system + device outputs).
     * Bits corresponding to signals not actively driven LOW remain HIGH (1).
     */
    uint32_t read_signals() const;

    /**
     * Set system-side output signals.
     * @param mask  Bits to modify
     * @param value New values for those bits (1 = release/high, 0 = assert/low)
     */
    void write_system_signals(uint32_t mask, uint32_t value);

    /// Get the raw system-side output state.
    uint32_t get_system_signals() const { return system_signals_; }

    // --- Device management ---------------------------------------------

    /// Returns the currently attached device, or nullptr.
    PeripheralDevice* get_attached_device() const { return attached_device_; }

    /**
     * Attach a peripheral device to this port.
     * @return true on success, false if connector types are incompatible.
     */
    bool attach_device(PeripheralDevice* device);

    /// Detach the current device (if any).  The device is NOT destroyed.
    void detach_device();

    /// Install a callback for device-output changes.
    void set_signal_change_callback(SignalChangeCallback cb) { on_device_output_changed_ = std::move(cb); }

    // --- Device notification (called by PeripheralDevice) ---------------

    /// Called by the attached device when it changes its output signals.
    void notify_device_output_changed(uint32_t device_signals);

private:
    ConnectorDefinition     definition_;
    int                     port_index_;        ///< E.g. port 1 vs port 2
    uint32_t                system_signals_;    ///< System-side output (all 1s = idle)
    uint32_t                device_signals_;    ///< Device-side output (all 1s = nothing attached)
    PeripheralDevice*       attached_device_;
    SignalChangeCallback    on_device_output_changed_;
};

// ============================================================================
// PERIPHERAL DEVICE (base class)
// ============================================================================

/**
 * Abstract base class for all peripheral devices that can be attached to a
 * ConnectorPort.
 *
 * Concrete devices (joystick, 1541 drive, datasette, lightpen, …) derive from
 * this and implement the required virtual methods.
 */
class PeripheralDevice {
public:
    virtual ~PeripheralDevice() = default;

    // --- Identification ------------------------------------------------

    /// Device display name (e.g. "Joystick", "1541 Disk Drive").
    virtual const char* get_name() const = 0;

    /// Short identifier (e.g. "joystick", "1541").
    virtual const char* get_id() const = 0;

    /// Which connector type this device requires.
    virtual ConnectorType get_connector_type() const = 0;

    // --- Lifecycle ------------------------------------------------------

    /// Called once after being attached to a port.
    virtual void on_attach(ConnectorPort* port) { port_ = port; }

    /// Called just before being detached.
    virtual void on_detach() { port_ = nullptr; }

    /// Power-on / hardware reset.
    virtual void reset() = 0;

    /// Per-system-cycle tick (optional — some devices have their own clock).
    virtual void tick() {}

    // --- Signal I/O ----------------------------------------------------

    /**
     * Called when the system-side signals change.
     * The device should read the relevant bits and update its internal state.
     */
    virtual void on_signal_change(uint32_t signal_state) { (void)signal_state; }

    /**
     * Return the device's current output signal state.
     * Active-low convention: 1 = released/high, 0 = asserted/low.
     * Default: all lines released (no device or idle device).
     */
    virtual uint32_t get_output_signals() const { return 0xFFFFFFFF; }

    // --- GUI -----------------------------------------------------------

#ifdef IMGUI_VERSION
    /// Optional device-specific settings/status UI.
    virtual void render_device_ui() {}
#endif

protected:
    ConnectorPort* port_ = nullptr;   ///< Port this device is attached to (set by on_attach)
};

// ============================================================================
// PREDEFINED SIGNAL LINE TABLES
// ============================================================================
// These define the signal layout for each connector type.
// Systems reference them when constructing ConnectorPort instances.

namespace ConnectorSignals {

// --- Control Port (DB-9) ---
// Matched to Commodore 64 / VIC-20 / Atari joystick wiring.
// Active-low: pressing a direction pulls the line LOW.
enum ControlPortBit : uint8_t {
    JOY_UP    = 0,  // Pin 1
    JOY_DOWN  = 1,  // Pin 2
    JOY_LEFT  = 2,  // Pin 3
    JOY_RIGHT = 3,  // Pin 4
    JOY_POTX  = 4,  // Pin 5 — Paddle / Pot X
    JOY_POTY  = 5,  // Pin 6 — Paddle / Pot Y (active on Port 1 only in C64)
    JOY_FIRE  = 6,  // Pin 6 on C64 maps to bit 4 of CIA port; we use bit 6 here
    LIGHT_PEN = 7,  // Pin 6 directly triggers VIC-II lightpen latch on Port 1
};

extern const SignalLine CONTROL_PORT_SIGNALS[];
extern const uint8_t    CONTROL_PORT_SIGNAL_COUNT;

// --- IEC Serial Bus (DIN-6) ---
// Active-low open-collector bus.  Multiple devices share the same bus.
enum IECBit : uint8_t {
    IEC_ATN   = 0,  // Attention — always driven by host
    IEC_CLK   = 1,  // Clock
    IEC_DATA  = 2,  // Data
    IEC_SRQ   = 3,  // Service Request (directly to CIA1 FLAG; directly active-low)
    IEC_RESET = 4,  // Reset (directly active-low)
};

extern const SignalLine IEC_SERIAL_SIGNALS[];
extern const uint8_t    IEC_SERIAL_SIGNAL_COUNT;

// --- Cassette Port ---
enum CassetteBit : uint8_t {
    CASS_MOTOR  = 0,  // Motor control (active-low: 0 = motor ON)
    CASS_READ   = 1,  // Data from tape
    CASS_WRITE  = 2,  // Data to tape
    CASS_SENSE  = 3,  // Play button sense (0 = pressed)
};

extern const SignalLine CASSETTE_PORT_SIGNALS[];
extern const uint8_t    CASSETTE_PORT_SIGNAL_COUNT;

// --- User Port ---
// Directly connected to CIA2 Port B plus control lines.
enum UserPortBit : uint8_t {
    USER_PB0 = 0,  USER_PB1 = 1, USER_PB2 = 2,  USER_PB3 = 3,
    USER_PB4 = 4,  USER_PB5 = 5, USER_PB6 = 6,  USER_PB7 = 7,
    USER_SP  = 8,  // CIA2 Serial Port
    USER_CNT = 9,  // CIA2 Counter
    USER_PA2 = 10, // CIA2 PA2 (directly controllable)
    USER_FLAG = 11, // CIA2 FLAG (directly active-low, directly triggers NMI)
    USER_RESET = 12,
};

extern const SignalLine USER_PORT_SIGNALS[];
extern const uint8_t    USER_PORT_SIGNAL_COUNT;

} // namespace ConnectorSignals
