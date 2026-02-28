#pragma once
/**
 * input_peripheral_device.h - Base class for host-input-driven peripherals
 *
 * Sits between PeripheralDevice and concrete input device implementations
 * (joysticks, mice, gamepads, light guns, paddles, …).  Factors out the
 * host-input plumbing that only makes sense for interactive peripherals:
 *
 *   - Host input binding management (keyboard / gamepad / mouse selection)
 *   - SDL event routing
 *   - Controller keyboard-preset infrastructure (auto-assign, collision scoring)
 *   - Guest keyboard scancode context for collision display
 *
 * Non-interactive devices (disk drives, tape drives, printers, modems, REU, …)
 * inherit directly from PeripheralDevice and carry none of this overhead.
 *
 * The parent class exposes as_input_device() so that system-level code can
 * query input capability without dynamic_cast:
 *
 *     if (auto* input = device->as_input_device()) {
 *         input->process_sdl_event(event);
 *     }
 */

#include "../connector.h"
#include "../host_input.h"

// Forward-declare SDL_Event so device headers don't need full SDL includes.
union SDL_Event;

/**
 * Abstract base for all peripheral devices that accept real-time host input.
 *
 * Every concrete input device (joystick, mouse, gamepad, light gun, paddle, …)
 * should derive from this class (or from a further specialisation such as
 * ControlPortInputDevice / PotInputDevice).
 */
class InputPeripheralDevice : public PeripheralDevice {
public:
    ~InputPeripheralDevice() override = default;

    // --- PeripheralDevice override: type query -------------------------

    InputPeripheralDevice* as_input_device() override { return this; }
    const InputPeripheralDevice* as_input_device() const override { return this; }

    // --- Host Input ----------------------------------------------------

    /// Which host input types can drive this device?
    virtual int get_supported_input_type_count() const { return 0; }
    virtual HostInputType get_supported_input_type(int /*index*/) const { return HostInputType::NONE; }

    /// Get/set the current host input binding.
    virtual const HostInputBinding& get_host_input_binding() const {
        static const HostInputBinding none{};
        return none;
    }
    virtual void set_host_input_binding(const HostInputBinding& /*binding*/) {}

    /// Process an SDL event according to the current binding.
    /// Called by the GUI layer for every SDL event while emulation is running.
    /// Returns true if the event was consumed by this device.
    virtual bool process_sdl_event(const SDL_Event& /*event*/) { return false; }

    // --- Controller keyboard preset selection --------------------------

    /// Number of available keyboard-to-controller presets (0 = not configurable).
    virtual int get_keymap_preset_count() const { return 0; }

    /// Get a specific preset descriptor by index.
    virtual const ControllerKeyMapPreset& get_keymap_preset(int /*index*/) const {
        static const ControllerKeyMapPreset empty{"None", {}, 0, false};
        return empty;
    }

    /// Apply a preset by index (device converts it into its own keymap struct).
    virtual void apply_keymap_preset(int /*index*/) {}

    /// Which preset index is currently active? Returns -1 if custom/unknown.
    virtual int get_active_keymap_preset() const { return -1; }

    /// Provide guest keyboard scancode context for collision UI display.
    /// Called by EmulatedSystem::auto_assign_controller_keymaps().
    void set_guest_keyboard_context(const SDL_Scancode* keys, int count) {
        guest_keyboard_scancodes_.clear();
        guest_keyboard_scancodes_.add_from_array(keys, count);
    }

    // --- Input-source-dependent settings UI -----------------------------

    /// Render settings that depend on the current input source (e.g. keyboard
    /// key-map preset selector).  Called by EmulatedSystem after the Input
    /// Source combo so that source-dependent controls appear below it.
    virtual void render_input_source_settings_ui() {}

protected:
    /// Guest keyboard scancode bitset — set by the system for collision
    /// display in the keymap preset UI.  O(1) per-key lookup.
    ScancodeBitset guest_keyboard_scancodes_;
};
