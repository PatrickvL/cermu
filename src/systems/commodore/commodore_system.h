#pragma once

#include "../../core/emulated_system.h"
#include "../../core/formats/format_handler.h"
#include "commodore_load_helpers.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../core/input/keyboard_mapper.h"
#include <memory>
#include <string>

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

    // =========================================================================
    // DEFERRED LOADING — shared infrastructure for all Commodore systems
    //
    // File loading is deferred until KERNAL/BASIC boot completes.  This
    // avoids the problem where BASIC's cold-start NEW routine zeroes the
    // program start area, corrupting data loaded before boot.  The system
    // stores the parsed format result and applies it only after BASIC
    // reaches its READY state.
    //
    // Systems provide hardware-specific behaviour through virtual hooks:
    //   is_basic_ready()        — query BASIC state (warm-start vector, VARTAB)
    //   build_load_context()    — provide memory callbacks + BASIC parameters
    //   inject_keys()           — write into the keyboard buffer
    //   is_system_initialized() — check whether initialize() has completed
    //
    // Optional hooks for system-specific extensions:
    //   on_file_parsed()          — post-parse processing (e.g. SID region)
    //   pre_apply_pending_load()  — intercept before shared DISK/TAPE/STD paths
    //   get_iec_port_index()      — IEC serial bus port for D64 disk loading
    //   get_cassette_port_index() — cassette port for TAP tape loading
    // =========================================================================

    /** How the deferred load should be applied after BASIC READY. */
    enum class LoadMode {
        DIRECT,         ///< Standard: write program data to RAM, inject RUN
        DISK_FAST,      ///< D64: disk inserted in 1541 + fast PRG extraction to RAM
        TAPE_INSERTED   ///< TAP: tape loaded in datasette, inject LOAD + press play
    };

    struct PendingLoad {
        format_load_result_t result;  // Parsed file data (owns heap allocations)
        std::string filepath;         // Original filepath for SYS-from-filename
        bool active = false;          // Whether a deferred load is pending
        LoadMode mode = LoadMode::DIRECT;  // How to apply the load
    };
    PendingLoad pending_load_;
    bool boot_completed_ = false;  // Set after first deferred load; skips VARTAB check

    // ---- Pure virtual hooks (must implement) ----

    /// Check if BASIC has reached READY state (safe to inject program).
    virtual bool is_basic_ready() const = 0;

    /// Build the system-specific commodore_load_context_t.
    virtual commodore_load_context_t build_load_context() = 0;

    /// Inject a NUL-terminated string into the system's keyboard buffer.
    virtual void inject_keys(const char* str) = 0;

    /// Check whether the system has completed initialization.
    virtual bool is_system_initialized() const = 0;

    // ---- Optional virtual hooks (have defaults) ----

    /// Called after format_load_file() succeeds, before storing PendingLoad.
    /// Return false to cancel the load.
    virtual bool on_file_parsed(format_load_result_t& result,
                                const char* filepath) {
        (void)result; (void)filepath; return true;
    }

    /// Called at the start of apply_pending_load().  Return true if the
    /// load was fully handled (skips shared DISK_FAST/TAPE/STANDARD paths).
    virtual bool pre_apply_pending_load() { return false; }

    /// IEC serial bus connector port index for D64 disk loading.  -1 = none.
    virtual int get_iec_port_index() const { return -1; }

    /// Cassette connector port index for TAP tape loading.  -1 = none.
    virtual int get_cassette_port_index() const { return -1; }

    // ---- Shared loading methods ----

    /// Check pending deferred load and apply if BASIC is ready.
    void check_deferred_load();

    /// Apply the pending load result to system memory.
    void apply_pending_load();

    /// Release any active pending load and reset state.
    void clear_pending_load();

    /// Reset all deferred-loading state.  Called from derived reset().
    void reset_load_state();

public:
    CommodoreSystem() = default;
    ~CommodoreSystem() override = default;

    // ---- File loading (shared implementation) ----
    bool load_file(const char* filepath) override;

    /// Attach a container/streamable media to the appropriate storage device.
    /// D64 → IEC serial bus → 1541 drive; TAP → cassette port → datasette.
    /// Auto-attaches the device if not yet connected.
    bool attach_media(const char* filepath) override;

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

    // Returns all host scancodes that close a Commodore keyboard matrix
    // contact.  Used by auto_assign_controller_keymaps() for collision
    // detection — the returned array is the superset across C64, VIC-20,
    // and C16/Plus4 keyboards.
    int get_guest_keyboard_scancodes(const SDL_Scancode** out) const override;
};
