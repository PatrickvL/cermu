#pragma once
/**
 * neos_mouse.h - NEOS Mouse (Third-party, bit-bang protocol)
 *
 * The NEOS mouse is a third-party mouse for the Commodore 64 that uses a
 * bit-bang handshake protocol over the CIA port lines to transmit mouse
 * movement data.  It was produced by Nihon Electronics in the mid-1980s.
 *
 * Protocol overview (active-low signals):
 *   The host software toggles the RIGHT line (active-low strobe) to clock
 *   through four nibbles of position delta.  The mouse drives data on lines
 *   UP/DOWN/LEFT/FIRE:
 *
 *     Strobe 0 → X delta high nibble (bits 7-4)
 *     Strobe 1 → X delta low nibble  (bits 3-0)
 *     Strobe 2 → Y delta high nibble (bits 7-4)
 *     Strobe 3 → Y delta low nibble  (bits 3-0)
 *
 *   The four data bits are mapped to:
 *     bit 3 → JOY_FIRE (pin 6)
 *     bit 2 → JOY_LEFT (pin 3)
 *     bit 1 → JOY_DOWN (pin 2)
 *     bit 0 → JOY_UP   (pin 1)
 *
 *   Left button maps to remaining port lines (directly active-low on JOY_FIRE
 *   when not in data phase).  In practice the button state is read between
 *   data transfers.
 *
 * Emulation approach:
 *   We accumulate host mouse motion and expose deltas in the 4-nibble protocol.
 *   The system wrapper reads CIA port B outputs and detects the strobe.
 *   We expose a read_nibble(strobe_phase) API for the wrapper to call.
 */

#include "../../core/peripherals/control_port_device.h"

class NEOSMouse : public ControlPortInputDevice {
public:
    NEOSMouse();
    ~NEOSMouse() override = default;

    const char* get_name() const override { return "NEOS Mouse"; }
    const char* get_id() const override   { return "mouse_neos"; }

    // --- Host input support -------------------------------------------
    int  get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        (void)index; return HostInputType::HOST_MOUSE;
    }
    bool process_sdl_event(const SDL_Event& event) override;
    void on_input_source_will_change() override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- NEOS-specific API ---------------------------------------------

    /// Move the mouse by relative delta.  Accumulated until latched.
    void move(int dx, int dy);

    /// Left button (directly read as FIRE when not in data phase).
    void set_left_button(bool pressed) { left_button_ = pressed; update_output(); }

    /// Right button (active-low, directly readable).
    void set_right_button(bool pressed) { right_button_ = pressed; update_output(); }

    /// Called by the system wrapper when the strobe (RIGHT line) toggles.
    /// Advances the protocol phase and outputs the next data nibble.
    void strobe();

    /// Latch current accumulated deltas into the transmit registers and
    /// reset the protocol phase to 0.  Called at the start of a read cycle.
    void latch_deltas();

    /// Get current protocol phase (0-3).
    int get_phase() const { return phase_; }

private:

    // Accumulated motion (raw host mouse delta)
    int accum_dx_ = 0;
    int accum_dy_ = 0;

    // Latched deltas for the current protocol cycle
    int8_t latched_dx_ = 0;
    int8_t latched_dy_ = 0;

    // Button state
    bool left_button_  = false;
    bool right_button_ = false;

    // Protocol phase (0-3): which nibble to output next
    int phase_ = 0;

    /// Recalculate output signals based on current phase and button state.
    void update_output();

    /// Output a 4-bit nibble on the direction/fire lines.
    void output_nibble(uint8_t nibble);
};
