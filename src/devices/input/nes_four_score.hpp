#pragma once
/*
 * nes_four_score.h — NES Four Score (NES-034) Multitap Adapter
 *
 * Models the NES Four Score 4-player adapter as a PeripheralDevice.
 * Each Four Score instance handles one NES controller port, providing
 * two controller slots (primary and secondary player).
 *
 *   Port 1 Four Score → Player 1 (primary) + Player 3 (secondary)
 *   Port 2 Four Score → Player 2 (primary) + Player 4 (secondary)
 *
 * Internal architecture:
 *   - Two NesStandardController sub-devices for host input capture
 *   - 24-bit shift register for the serial D0 protocol:
 *     [8 bits primary] [8 bits secondary] [8-bit signature]
 *   - Signature: $10 for port 1, $08 for port 2
 *
 * Lives in src/devices/input/ because the Four Score is a cross-system
 * NES controller port accessory.
 */

#include "core/peripherals/input_peripheral_device.hpp"
#include "devices/input/nes_standard_controller.hpp"

#include <cstdint>
#include <memory>

class NesFourScore : public InputPeripheralDevice {
public:
    /// Signature bytes for Four Score detection by games.
    static constexpr uint8_t SIGNATURE_PORT1 = 0x10;  // $4016 bits 17-24
    static constexpr uint8_t SIGNATURE_PORT2 = 0x08;  // $4017 bits 17-24

    NesFourScore();
    ~NesFourScore() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "NES Four Score"; }
    const char* get_id()   const override { return "nes_four_score"; }
    PortType get_port_type() const override { return PortType::CONTROLLER_NES; }
    void reset() override;

    // --- Lifecycle ------------------------------------------------------
    void on_attach(Port* port) override;

    // --- Signal protocol -----------------------------------------------
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override { return output_signals_; }

    // --- Host Input (forwarded to sub-controllers) ---------------------
    int get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override {
        if (index == 0) return HostInputType::KEYBOARD;
        if (index == 1) return HostInputType::SDL_GAMEPAD;
        return HostInputType::NONE;
    }
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

    // --- Keymap preset support (delegates to sub-controllers) ----------
    int get_keymap_preset_count() const override;
    const ControllerKeyMapPreset& get_keymap_preset(int index) const override;
    void apply_keymap_preset(int index) override;
    int get_active_keymap_preset() const override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
    void render_input_source_settings_ui() override;
#endif

    // --- Sub-device access ---------------------------------------------
    NesStandardController* get_primary_controller()   { return primary_.get(); }
    NesStandardController* get_secondary_controller() { return secondary_.get(); }

private:
    void on_input_source_will_change() override;
    const ControllerKeyMapPreset* get_keymap_presets_table() const override;
    int get_keymap_presets_table_size() const override;

    std::unique_ptr<NesStandardController> primary_;    // Player N
    std::unique_ptr<NesStandardController> secondary_;  // Player N+2

    uint32_t shift_register_ = 0;          // 24-bit shift register
    uint8_t  shift_count_ = 0;             // Bits shifted out so far
    uint8_t  signature_ = SIGNATURE_PORT1; // Auto-detected from port index
    bool     latch_was_high_ = false;      // Edge detection for LATCH
    bool     clk_was_high_ = false;        // Edge detection for CLK
    uint32_t output_signals_ = 0xFFFFFFFF; // Active-low output (all released)
};
