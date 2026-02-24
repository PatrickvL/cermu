#pragma once

#include "../../core/emulated_system.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/keyboard_mapper.h"
#include <memory>

/**
 * CommodoreSystem — shared base class for all Commodore 8-bit systems
 *
 * Sits between EmulatedSystem and the three Commodore system families:
 *   - VIC-20     (VIC20System)
 *   - 264 Series (Commodore264System<V>: C16, C116, Plus/4)
 *   - C64        (C64System)
 *
 * Provides the members and methods that are identical across all three
 * families:
 *
 *   Members:
 *     keyboard_        — Commodore keyboard matrix (may be nullptr for C64’s
 *                        indirect keyboard access path)
 *     keyboard_mapper_ — layered keyboard mapping engine
 *     cycles_per_frame_ — CPU cycles per video frame (region-dependent)
 *
 *   Methods (fully implemented, no override needed):
 *     set_configuration()    — stores config_, updates cached_target_fps_
 *     set_speed_multiplier() — stores speed_multiplier_ (base class member)
 *     handle_text_input()    — delegates to keyboard_mapper_
 *     release_all_keys()     — delegates to keyboard_mapper_
 *     handle_keyboard_event_ex() — optional pre-intercept hook, then keyboard_mapper_
 *
 * C64System overrides handle_keyboard_event_ex() to add SID player
 * and disc-flip hotkey intercepts before the mapper dispatch.
 */
class CommodoreSystem : public EmulatedSystem {
protected:
    // Commodore keyboard matrix — owned by the derived system's chip
    // infrastructure.  May be nullptr for C64System (which accesses
    // the keyboard through c64_t indirection).
    commodore_keyboard_t* keyboard_ = nullptr;

    // Layered keyboard mapping engine (host-layout → Commodore matrix)
    std::unique_ptr<KeyboardMapper> keyboard_mapper_;

    // CPU cycles per video frame — set by apply_configuration() from
    // the selected region option's timing.cycles_per_frame.
    uint32_t cycles_per_frame_ = 0;

public:
    CommodoreSystem() = default;
    ~CommodoreSystem() override = default;

    // ---- Identical across all Commodore systems ----

    bool set_configuration(const SystemConfiguration& config) override;
    void set_speed_multiplier(float multiplier) override;
    void handle_text_input(const char* text) override;
    void release_all_keys() override;

    // Default implementation dispatches to keyboard_mapper_ if available,
    // else falls back to handle_keyboard_event(key, pressed).
    // C64System overrides to add SID player / disc-flip intercepts.
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode,
                                  uint16_t mod, bool pressed, bool repeat) override;
};
