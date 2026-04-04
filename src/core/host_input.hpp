#pragma once
/**
 * host_input.h - Host Input Provider Framework
 *
 * Defines how emulated peripheral devices receive input from the host machine.
 * Each input-accepting device (joystick, mouse, lightpen, paddles) can be bound
 * to a host input source:
 *
 *   HostInputType::KEYBOARD     — Host keyboard keys (configurable mapping)
 *   HostInputType::SDL_GAMEPAD  — Physical game controller via SDL GameController
 *   HostInputType::HOST_MOUSE   — Host mouse position + buttons
 *
 * The GUI layer routes SDL events to devices via InputPeripheralDevice::process_sdl_event().
 * Each device interprets events according to its current binding.
 *
 * Joystick keyboard mappings default to cursor keys + Right Ctrl (fire),
 * matching the common C64 configuration.  These can be customised per-device
 * through render_device_ui().
 */

#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <cstdint>
#include <string>

// ============================================================================
// HOST INPUT TYPE
// ============================================================================

/// What kind of host hardware drives an emulated input device.
enum class HostInputType {
    NONE,           ///< No host input — device state is set programmatically
    KEYBOARD,       ///< Host keyboard keys (configurable key map)
    SDL_GAMEPAD,    ///< SDL GameController (USB/BT gamepad, joystick)
    HOST_MOUSE,     ///< Host mouse (position + buttons)
};

/// Human-readable label for a HostInputType.
inline const char* host_input_type_name(HostInputType type) {
    switch (type) {
        case HostInputType::NONE:        return "None";
        case HostInputType::KEYBOARD:    return "Keyboard";
        case HostInputType::SDL_GAMEPAD: return "Game Controller";
        case HostInputType::HOST_MOUSE:  return "Host Mouse";
        default:                         return "Unknown";
    }
}

// ============================================================================
// HOST INPUT BINDING
// ============================================================================

/**
 * Describes which host input source is currently driving an emulated device.
 */
struct HostInputBinding {
    HostInputType type = HostInputType::NONE;

    /// SDL GameController/joystick instance ID (from SDL_CONTROLLERDEVICEADDED).
    /// Only meaningful when type == SDL_GAMEPAD.  -1 = any/auto.
    int gamepad_instance_id = -1;

    /// Human-readable label for current binding (e.g. "Keyboard (Arrows+RCtrl)").
    std::string label = "None";
};

// ============================================================================
// JOYSTICK KEYBOARD MAP
// ============================================================================

/**
 * Maps host keyboard scancodes to joystick directions + fire.
 * Uses scancodes (not keycodes) so the mapping is layout-independent.
 */
struct JoystickKeyMap {
    SDL_Scancode up    = SDL_SCANCODE_UP;
    SDL_Scancode down  = SDL_SCANCODE_DOWN;
    SDL_Scancode left  = SDL_SCANCODE_LEFT;
    SDL_Scancode right = SDL_SCANCODE_RIGHT;
    SDL_Scancode fire  = SDL_SCANCODE_RCTRL;

    /// Optional second fire button.
    SDL_Scancode fire2 = SDL_SCANCODE_RSHIFT;

    /// Human-readable description of this mapping.
    const char* description() const {
        // Static — only one mapping description shown at a time
        static char buf[128];
        snprintf(buf, sizeof(buf), "Arrows + RCtrl/RShift");
        return buf;
    }
};



// ============================================================================
// SCANCODE BITSET — O(1) membership test for SDL scancode sets
// ============================================================================

/// Compact bitset covering all SDL scancodes (SDL_NUM_SCANCODES = 512).
/// Uses 8 × uint64_t = 64 bytes.  All operations are branch-free bit ops.
struct ScancodeBitset {
    static constexpr int NUM_WORDS = SDL_NUM_SCANCODES / 64;  // 8
    uint64_t words[NUM_WORDS] = {};

    void set(SDL_Scancode sc) {
        auto idx = static_cast<unsigned>(sc);
        if (idx < SDL_NUM_SCANCODES) words[idx >> 6] |= (uint64_t{1} << (idx & 63));
    }

    bool test(SDL_Scancode sc) const {
        auto idx = static_cast<unsigned>(sc);
        return idx < SDL_NUM_SCANCODES && (words[idx >> 6] & (uint64_t{1} << (idx & 63))) != 0;
    }

    /// Bulk-add from a raw scancode array.
    void add_from_array(const SDL_Scancode* keys, int count) {
        for (int i = 0; i < count; i++) set(keys[i]);
    }

    /// Reset all bits to zero.
    void clear() {
        for (auto& w : words) w = 0;
    }
};

// ============================================================================
// CONTROLLER KEYBOARD MAP PRESETS (generic, cross-device)
// ============================================================================

/// Maximum scancodes in a single controller preset (12 = SNES gamepad buttons).
static constexpr int MAX_CONTROLLER_PRESET_KEYS = 12;

/**
 * A named keyboard-to-controller-button mapping preset.
 *
 * Each device type (joystick, NES gamepad, …) defines its own static array
 * of presets.  The generic auto_assign_controller_keymaps() scores each
 * preset against the guest keyboard scancodes and inter-device usage to
 * pick the one with the fewest collisions.
 */
struct ControllerKeyMapPreset {
    const char*  name;                                   ///< Short display name
    SDL_Scancode keys[MAX_CONTROLLER_PRESET_KEYS];       ///< Scancodes used
    int          key_count;                              ///< Valid entries in keys[]
    bool         requires_numpad;                        ///< True if preset uses numpad keys

    /// Add all keys of this preset into a bitset.
    void add_to_bitset(ScancodeBitset& bs) const {
        for (int i = 0; i < key_count; i++) bs.set(keys[i]);
    }
};

/**
 * Count collisions between a preset's keys and a scancode bitset.
 * O(key_count) — each test is a single bit-check.
 */
inline int count_keymap_collisions(const ControllerKeyMapPreset& preset,
                                   const ScancodeBitset& claimed) {
    int collisions = 0;
    for (int i = 0; i < preset.key_count; i++) {
        if (claimed.test(preset.keys[i])) ++collisions;
    }
    return collisions;
}

