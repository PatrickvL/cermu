#pragma once
/**
 * datasette_1530.h - Commodore 1530 Datasette Tape Drive Peripheral
 *
 * Emulates the Commodore 1530 Datasette (C2N) unit that connects through
 * the cassette port.  Handles TAP file playback — each pulse edge is timed
 * according to the TAP format's cycle counts and presented on the READ line.
 */

#include "devices/storage/storage_device.hpp"
#include <cstdint>
#include <vector>
#include <string>

class Datasette1530Device : public StorageDevice {
public:
    Datasette1530Device();
    ~Datasette1530Device() override = default;

    const char* get_name() const override { return "Datasette (1530)"; }
    const char* get_id() const override   { return "datasette"; }
    PortType get_port_type() const override { return PortType::CASSETTE_PORT; }
    void reset() override;
    void tick() override;                      ///< Drive the TAP playback clock
    void on_signal_change(uint32_t signals) override;
    uint32_t get_output_signals() const override;

    bool has_activity() const override { return playing_ && motor_on_; }

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- Datasette-specific API ----------------------------------------

    /// Load a TAP file into the datasette.
    bool load_tap(const char* filepath);

    /// Insert a blank tape (for recording — placeholder).
    void insert_blank();

    /// Eject the tape (delegates to StorageDevice::eject_media).
    void eject();

    /// Transport controls.
    void press_play();
    void press_stop();
    void press_rewind();
    void press_fast_forward();

    /// Tape position info.
    bool is_tape_loaded() const    { return media_loaded_; }
    bool is_playing() const        { return playing_; }
    bool is_motor_on() const       { return motor_on_; }
    uint32_t get_tape_position() const { return tape_position_; }
    uint32_t get_tape_length() const   { return static_cast<uint32_t>(tap_data_.size()); }

protected:
    // --- StorageDevice override ----------------------------------------
    bool swap_media(const char* filepath) override { return load_tap(filepath); }

private:
    uint32_t    state_;             ///< Output signal state

    // TAP data
    std::vector<uint8_t> tap_data_;
    uint8_t     tap_version_;       ///< TAP format version (0 or 1)
    uint32_t    tape_position_;     ///< Current byte position in tap_data_

    // Transport state
    bool        playing_;
    bool        motor_on_;
    bool        button_pressed_;    ///< PLAY button (directly drives SENSE line)

    // Pulse timing
    uint32_t    pulse_countdown_;   ///< Cycles remaining until next edge
    bool        read_level_;        ///< Current READ line level

    /// Read the next pulse length from TAP data (in system cycles).
    uint32_t read_next_pulse();
};
