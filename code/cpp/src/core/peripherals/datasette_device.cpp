/**
 * datasette_device.cpp - Commodore Datasette Implementation
 *
 * Plays back TAP files through the cassette port's READ signal line.
 * TAP v0: each byte = pulse length * 8 cycles (0 = 256*8 = 2048 cycles).
 * TAP v1: byte 0x00 signals a 3-byte little-endian long pulse.
 */

#include "datasette_device.h"
#include "../device_registry.h"
#include <cstdio>
#include <cstring>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// TAP file header
struct TAPHeader {
    char     signature[12];  // "C64-TAPE-RAW"
    uint8_t  version;        // 0 or 1
    uint8_t  pad[3];
    uint32_t data_length;    // Length of pulse data
};

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

DatasetteDevice::DatasetteDevice()
    : state_(0xFFFFFFFF)
    , tap_version_(0)
    , tape_position_(0)
    , tape_loaded_(false)
    , playing_(false)
    , motor_on_(false)
    , button_pressed_(false)
    , pulse_countdown_(0)
    , read_level_(true)
{
}

void DatasetteDevice::reset() {
    playing_ = false;
    motor_on_ = false;
    tape_position_ = 0;
    pulse_countdown_ = 0;
    read_level_ = true;
    state_ = 0xFFFFFFFF;
    if (port_) port_->notify_device_output_changed(state_);
}

// ============================================================================
// SIGNAL I/O
// ============================================================================

uint32_t DatasetteDevice::get_output_signals() const {
    return state_;
}

void DatasetteDevice::on_signal_change(uint32_t signals) {
    // The MOTOR signal is active-low: 0 = motor running
    bool motor = !(signals & (1u << ConnectorSignals::CASS_MOTOR));
    if (motor != motor_on_) {
        motor_on_ = motor;
    }
}

// ============================================================================
// TAPE TRANSPORT
// ============================================================================

bool DatasetteDevice::load_tap(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("Datasette: Cannot open '%s'\n", filepath);
        return false;
    }

    TAPHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        printf("Datasette: Failed to read TAP header from '%s'\n", filepath);
        return false;
    }

    // Verify signature
    if (memcmp(header.signature, "C64-TAPE-RAW", 12) != 0) {
        fclose(f);
        printf("Datasette: Invalid TAP signature in '%s'\n", filepath);
        return false;
    }

    tap_version_ = header.version;

    // Read pulse data
    tap_data_.resize(header.data_length);
    size_t read = fread(tap_data_.data(), 1, header.data_length, f);
    fclose(f);

    if (read != header.data_length) {
        printf("Datasette: Warning — read %zu of %u bytes from '%s'\n",
               read, header.data_length, filepath);
        tap_data_.resize(read);
    }

    tape_loaded_ = true;
    tape_position_ = 0;
    playing_ = false;
    pulse_countdown_ = 0;
    read_level_ = true;

    printf("Datasette: Loaded '%s' (TAP v%d, %u bytes of pulse data)\n",
           filepath, tap_version_, header.data_length);
    return true;
}

void DatasetteDevice::insert_blank() {
    tap_data_.clear();
    tap_version_ = 0;
    tape_loaded_ = true;
    tape_position_ = 0;
    playing_ = false;
}

void DatasetteDevice::eject() {
    tap_data_.clear();
    tape_loaded_ = false;
    tape_position_ = 0;
    playing_ = false;
    pulse_countdown_ = 0;
    read_level_ = true;
    state_ = 0xFFFFFFFF;
    if (port_) port_->notify_device_output_changed(state_);
}

void DatasetteDevice::press_play() {
    if (!tape_loaded_) return;
    button_pressed_ = true;
    playing_ = true;

    // SENSE line: active-low, 0 = button pressed
    state_ &= ~(1u << ConnectorSignals::CASS_SENSE);
    if (port_) port_->notify_device_output_changed(state_);
}

void DatasetteDevice::press_stop() {
    button_pressed_ = false;
    playing_ = false;

    // SENSE line: released (button not pressed)
    state_ |= (1u << ConnectorSignals::CASS_SENSE);

    // READ line: released
    state_ |= (1u << ConnectorSignals::CASS_READ);
    read_level_ = true;

    if (port_) port_->notify_device_output_changed(state_);
}

void DatasetteDevice::press_rewind() {
    tape_position_ = 0;
    pulse_countdown_ = 0;
}

void DatasetteDevice::press_fast_forward() {
    if (tape_loaded_) {
        tape_position_ = static_cast<uint32_t>(tap_data_.size());
    }
}

// ============================================================================
// TICK — Pulse playback
// ============================================================================

uint32_t DatasetteDevice::read_next_pulse() {
    if (tape_position_ >= tap_data_.size()) return 0;

    uint8_t byte = tap_data_[tape_position_++];

    if (byte != 0) {
        // Standard pulse: length = byte * 8 cycles
        return static_cast<uint32_t>(byte) * 8;
    }

    // Byte is 0x00
    if (tap_version_ == 0) {
        // TAP v0: 0 means 256 * 8 = 2048 cycles
        return 256 * 8;
    }

    // TAP v1: next 3 bytes are a 24-bit little-endian cycle count
    if (tape_position_ + 3 > tap_data_.size()) return 0;
    uint32_t lo  = tap_data_[tape_position_++];
    uint32_t mid = tap_data_[tape_position_++];
    uint32_t hi  = tap_data_[tape_position_++];
    return lo | (mid << 8) | (hi << 16);
}

void DatasetteDevice::tick() {
    // Only play back if motor on AND play button pressed AND tape loaded
    if (!playing_ || !motor_on_ || !tape_loaded_) return;

    if (pulse_countdown_ == 0) {
        // Load next pulse
        uint32_t next = read_next_pulse();
        if (next == 0) {
            // End of tape
            press_stop();
            return;
        }
        pulse_countdown_ = next;
        // Toggle READ level at pulse start
        read_level_ = !read_level_;

        // Update READ signal line
        if (read_level_) {
            state_ |= (1u << ConnectorSignals::CASS_READ);
        } else {
            state_ &= ~(1u << ConnectorSignals::CASS_READ);
        }
        if (port_) port_->notify_device_output_changed(state_);
    }

    pulse_countdown_--;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void DatasetteDevice::render_device_ui() {
    if (!tape_loaded_) {
        ImGui::TextDisabled("No tape loaded");
        return;
    }
    float progress = tap_data_.empty() ? 0.0f
                    : static_cast<float>(tape_position_) / static_cast<float>(tap_data_.size());
    ImGui::ProgressBar(progress, ImVec2(-1, 0), playing_ ? "Playing..." : "Stopped");
    ImGui::Text("Motor: %s | Position: %u / %zu",
                motor_on_ ? "ON" : "OFF", tape_position_, tap_data_.size());

    if (ImGui::Button(playing_ ? "Stop" : "Play")) {
        if (playing_) press_stop(); else press_play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rewind")) press_rewind();
    ImGui::SameLine();
    if (ImGui::Button("Eject")) eject();
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor datasette_descriptor = {
    "datasette",
    "Datasette (1530)",
    "Commodore 1530 (C2N) cassette tape drive — plays back TAP files",
    ConnectorType::CASSETTE_PORT,
    false
};

REGISTER_DEVICE(datasette_descriptor, []() {
    return std::make_unique<DatasetteDevice>();
})
