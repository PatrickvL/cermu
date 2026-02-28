#pragma once
/*
 * nes_zapper.h — NES Zapper Light Gun Peripheral
 *
 * Models the NES Zapper (NES-005) as a PeripheralDevice.
 * The Zapper detects light by reading the PPU framebuffer color at
 * the aim position. It uses NES_D3 for the light sensor (active-low
 * when light is detected) and NES_D4 for the trigger.
 *
 * Lives in src/devices/input/ because the Zapper is compatible
 * across NES, Famicom (via adapter), and VS. System.
 *
 * Signal mapping on CONTROLLER_NES port:
 *   D3 (bit 3): Light sensor — LOW when white/bright pixel detected
 *   D4 (bit 4): Trigger — LOW when trigger is pulled
 */

#include "../../core/connector.h"

#include <cstdint>

class NesZapper : public PeripheralDevice {
public:
    NesZapper();
    ~NesZapper() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "NES Zapper"; }
    const char* get_id()   const override { return "nes_zapper"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROLLER_NES; }
    void reset() override;

    // --- Signal protocol -----------------------------------------------
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override { return output_signals_; }

    // --- Host Input (mouse) --------------------------------------------
    bool accepts_host_input() const override { return true; }
    int get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        return (index == 0) ? HostInputType::HOST_MOUSE : HostInputType::NONE;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override { binding_ = binding; }
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- Zapper-specific API -------------------------------------------

    /// Set whether a bright pixel is under the aim point.
    /// Called by the system after PPU renders, checking the framebuffer.
    void set_light_detected(bool detected) { light_detected_ = detected; update_output(); }

    /// Set trigger state (from mouse click)
    void set_trigger(bool pulled) { trigger_pulled_ = pulled; update_output(); }

    /// Get aim position in screen coordinates (set by mouse movement)
    int get_aim_x() const { return aim_x_; }
    int get_aim_y() const { return aim_y_; }

    /// Set aim position (for direct control / testing)
    void set_aim_position(int x, int y) { aim_x_ = x; aim_y_ = y; }

    bool is_trigger_pulled() const { return trigger_pulled_; }
    bool is_light_detected() const { return light_detected_; }

private:
    void update_output();

    int aim_x_ = 128;          // Screen X coordinate of aim point
    int aim_y_ = 120;          // Screen Y coordinate of aim point
    bool trigger_pulled_ = false;
    bool light_detected_ = false;
    uint32_t output_signals_ = 0xFFFFFFFF;  // All released (active-low)
    HostInputBinding binding_{};
};
