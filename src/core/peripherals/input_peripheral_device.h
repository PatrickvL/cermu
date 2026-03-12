#pragma once
/**
 * input_peripheral_device.h - Base class for host-input-driven peripherals
 *
 * Sits between PeripheralDevice and concrete input device implementations
 * (joysticks, mice, gamepads, light guns, paddles, …).  Factors out the
 * host-input plumbing that only makes sense for interactive peripherals:
 *
 *   - Host input binding storage and lifecycle (binding_, on_input_source_will_change)
 *   - SDL event routing and pre-processing helpers
 *   - Controller keyboard-preset infrastructure (auto-assign, collision scoring)
 *   - Guest keyboard scancode context for collision display
 *   - Common GUI helpers (input source badge, keymap preset combo)
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

#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#ifdef CERMU_HAS_GUI
#include "imgui.h"
#endif

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

    /// Get the current host input binding.
    const HostInputBinding& get_host_input_binding() const { return binding_; }

    /// Set a new host input binding.  Calls on_input_source_will_change() before
    /// updating, giving subclasses a chance to release signals and reset state.
    virtual void set_host_input_binding(const HostInputBinding& binding) {
        on_input_source_will_change();
        binding_ = binding;
    }

    /// Process an SDL event according to the current binding.
    /// Called by the GUI layer for every SDL event while emulation is running.
    /// Returns true if the event was consumed by this device.
    virtual bool process_sdl_event(const SDL_Event& /*event*/) { return false; }

    // --- Controller keyboard preset selection --------------------------

    /// Number of available keyboard-to-controller presets (0 = not configurable).
    /// Default implementation returns the table size from get_keymap_presets_table().
    virtual int get_keymap_preset_count() const { return get_keymap_presets_table_size(); }

    /// Get a specific preset descriptor by index.
    /// Default implementation does bounds-checked lookup in get_keymap_presets_table().
    virtual const ControllerKeyMapPreset& get_keymap_preset(int index) const {
        const int count = get_keymap_presets_table_size();
        if (index >= 0 && index < count) return get_keymap_presets_table()[index];
        static const ControllerKeyMapPreset empty{"None", {}, 0, false};
        return empty;
    }

    /// Apply a preset by index (device converts it into its own keymap struct).
    virtual void apply_keymap_preset(int /*index*/) {}

    /// Which preset index is currently active? Returns -1 if custom/unknown.
    virtual int get_active_keymap_preset() const { return -1; }

    /// Provide guest keyboard scancode context for collision UI display.
    /// Called by System::auto_assign_controller_keymaps().
    void set_guest_keyboard_context(const SDL_Scancode* keys, int count) {
        guest_keyboard_scancodes_.clear();
        guest_keyboard_scancodes_.add_from_array(keys, count);
    }

    // --- Input-source-dependent settings UI -----------------------------

    /// Render settings that depend on the current input source (e.g. keyboard
    /// key-map preset selector).  Called by System after the Input
    /// Source combo so that source-dependent controls appear below it.
    /// Default implementation renders the keymap preset combo when keyboard
    /// is the active input source and presets are available.
#ifdef CERMU_HAS_GUI
    virtual void render_input_source_settings_ui() {
        if (binding_.type != HostInputType::KEYBOARD) return;
        if (get_keymap_preset_count() == 0) return;

        int active = get_active_keymap_preset();
        const char* preview = (active >= 0) ? get_keymap_preset(active).name : "Custom";

        if (ImGui::BeginCombo("Key Map", preview)) {
            for (int i = 0; i < get_keymap_preset_count(); i++) {
                const auto& preset = get_keymap_preset(i);
                int collisions = count_keymap_collisions(preset, guest_keyboard_scancodes_);

                char label[128];
                if (collisions > 0) {
                    snprintf(label, sizeof(label), "%s  (%d collision%s)",
                             preset.name, collisions, collisions > 1 ? "s" : "");
                } else {
                    snprintf(label, sizeof(label), "%s", preset.name);
                }

                bool selected = (i == active);
                if (ImGui::Selectable(label, selected)) {
                    apply_keymap_preset(i);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
#else
    virtual void render_input_source_settings_ui() {}
#endif

protected:
    HostInputBinding binding_;  ///< Current host input binding (shared by all devices)

    /// Called before binding_ is updated.  Override to release signals, clear
    /// accumulators, or reset protocol state.  ControlPortInputDevice overrides
    /// this to release_all_signals() + notify_port().
    virtual void on_input_source_will_change() {}

    /// Guest keyboard scancode bitset — set by the system for collision
    /// display in the keymap preset UI.  O(1) per-key lookup.
    ScancodeBitset guest_keyboard_scancodes_;

    // --- Keymap preset table hook --------------------------------------
    // Subclasses with a static preset table override these two.  The default
    // get_keymap_preset_count() / get_keymap_preset() call through to them.

    /// Pointer to the static preset array.  nullptr if no presets.
    virtual const ControllerKeyMapPreset* get_keymap_presets_table() const { return nullptr; }
    /// Number of entries in the preset table.
    virtual int get_keymap_presets_table_size() const { return 0; }

    // --- SDL event pre-processing helpers (static, no vtable cost) -----

    /// Extract keyboard press/release info.  Returns false if the event is
    /// not a key-down/key-up or is an auto-repeat.
    static bool extract_key_event(const SDL_Event& event, SDL_Scancode& scancode, bool& pressed) {
        if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;
        if (event.type == SDL_KEYDOWN && event.key.repeat) return false;
        pressed  = (event.type == SDL_KEYDOWN);
        scancode = event.key.keysym.scancode;
        return true;
    }

    /// Extract gamepad instance ID from an SDL controller event.
    /// Returns -1 if the event is not a controller button or axis event.
    static SDL_JoystickID get_gamepad_instance_id(const SDL_Event& event) {
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP)
            return event.cbutton.which;
        if (event.type == SDL_CONTROLLERAXISMOTION)
            return event.caxis.which;
        return -1;
    }

    /// Returns false if the event should be filtered out because it belongs
    /// to a different gamepad than the one we're bound to.
    bool should_accept_gamepad_event(const SDL_Event& event) const {
        SDL_JoystickID eid = get_gamepad_instance_id(event);
        if (eid < 0) return false;
        if (binding_.gamepad_instance_id >= 0 && eid != binding_.gamepad_instance_id)
            return false;
        return true;
    }

    /// Result of axis-to-digital conversion.
    struct DigitalAxes {
        bool left  = false;
        bool right = false;
        bool up    = false;
        bool down  = false;
    };

    /// Convert a left-stick axis event to digital directions (50% threshold).
    /// Returns true if the event was an axis event on the left stick.
    static bool axis_to_digital(const SDL_Event& event, DigitalAxes& out) {
        if (event.type != SDL_CONTROLLERAXISMOTION) return false;
        constexpr int16_t THRESHOLD = 16384;
        int16_t value = event.caxis.value;
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            out.left  = (value < -THRESHOLD);
            out.right = (value >  THRESHOLD);
            return true;
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            out.up   = (value < -THRESHOLD);
            out.down = (value >  THRESHOLD);
            return true;
        }
        return false;
    }

    /// Extract mouse button press/release.  Returns true if the event was a
    /// mouse button down/up for the specified SDL button (e.g. SDL_BUTTON_LEFT).
    static bool extract_mouse_button(const SDL_Event& event, uint8_t sdl_button, bool& pressed) {
        if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP) return false;
        if (event.button.button != sdl_button) return false;
        pressed = (event.type == SDL_MOUSEBUTTONDOWN);
        return true;
    }

    // --- GUI helpers ---------------------------------------------------

#ifdef CERMU_HAS_GUI
    /// Render a compact input-source badge ([Keys], [Pad], [Mouse]) inline.
    void render_input_source_badge() const {
        switch (binding_.type) {
            case HostInputType::KEYBOARD:
                ImGui::SameLine(); ImGui::TextDisabled("[Keys]");  break;
            case HostInputType::SDL_GAMEPAD:
                ImGui::SameLine(); ImGui::TextDisabled("[Pad]");   break;
            case HostInputType::HOST_MOUSE:
                ImGui::SameLine(); ImGui::TextDisabled("[Mouse]"); break;
            default: break;
        }
    }
#endif
};
