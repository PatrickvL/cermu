#pragma once
/*
 * nes_standard_controller.h — NES Standard Controller Peripheral
 *
 * Models the standard NES gamepad (NES-004) as a PeripheralDevice.
 * Internally composes a CD4021 shift register for the serial
 * LATCH/CLK/D0 protocol used on the NES controller port.
 *
 * Lives in src/devices/input/ because NES controller ports appear on
 * NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Button bitmask layout (MSB → LSB, matching hardware shift order):
 *   A  B  SELECT  START  UP  DOWN  LEFT  RIGHT
 *   7  6    5       4     3    2     1      0
 */

#include "../../core/connector.h"
#include "../../chip/input/cd4021.h"

#include <cstdint>

class NesStandardController : public PeripheralDevice {
public:
    // Button bitmask — matches CD4021 parallel load bit order
    enum Button : uint8_t {
        RIGHT  = 0x01,
        LEFT   = 0x02,
        DOWN   = 0x04,
        UP     = 0x08,
        START  = 0x10,
        SELECT = 0x20,
        B      = 0x40,
        A      = 0x80,
    };

    NesStandardController();
    ~NesStandardController() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "NES Standard Controller"; }
    const char* get_id()   const override { return "nes_gamepad"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROLLER_NES; }
    void reset() override;

    // --- Signal protocol -----------------------------------------------
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override { return output_signals_; }

    // --- Host Input (keyboard / gamepad) -------------------------------
    bool accepts_host_input() const override { return true; }
    int get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override {
        if (index == 0) return HostInputType::KEYBOARD;
        if (index == 1) return HostInputType::SDL_GAMEPAD;
        return HostInputType::NONE;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override { binding_ = binding; }
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef IMGUI_VERSION
    void render_device_ui() override;
#endif

    // --- Controller-specific API ---------------------------------------
    void set_button_state(Button button, bool pressed);
    uint8_t get_button_state() const { return button_state_; }

private:
    bool process_keyboard_event(const SDL_Event& event);
    bool process_gamepad_event(const SDL_Event& event);

    CD4021 cd4021_;                         // Internal shift register
    uint8_t button_state_ = 0;             // Current button bitmask (active-high)
    bool latch_was_high_ = false;          // Edge detection for LATCH signal
    uint32_t output_signals_ = 0xFFFFFFFF; // Active-low output (all released)
    HostInputBinding binding_{};
};
