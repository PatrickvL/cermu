#pragma once

#include "core/system.hpp"
#include "core/formats/format_handler.hpp"
#include "systems/commodore/commodore_load_helpers.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "core/input/keyboard_mapper.hpp"
#include "core/input/emu_keys.hpp"
#include "devices/storage/drive_subsystem.hpp"
#include "chip/cpu/fam65xx/fam65xx_types.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declaration — drive device for serial trap finding
class Drive1541Device;

// ── Drive emulation modes ───────────────────────────────────────────────────
//
// Controls how the IEC serial bus and attached drives are emulated.
// Applies to all Commodore systems with an IEC port.

enum class DriveMode : uint8_t {
    WARP,       ///< Cycle-accurate drive CPU, auto-warp when motor on (default)
    ACCURATE,   ///< Cycle-accurate drive CPU, no warp (real-time)
    HOOKED,     ///< KERNAL serial traps — instant I/O, no drive CPU
};

/**
 * CommodoreSystem — shared base class for all Commodore 8-bit systems
 *
 * Sits between System and the three Commodore system families:
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
class CommodoreSystem : public System {
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
    // CYCLE-ACCURATE DRIVE SUBSYSTEM
    //
    // When drive_mode_ is WARP or ACCURATE, the DriveSubsystem owns one or
    // more C1541System instances that are ticked every CPU cycle.  The host
    // system wires its CIA/VIA/TED IEC output pins to the shared IEC bus
    // and reads back the combined bus state.
    //
    // When drive_mode_ is HOOKED, the DriveSubsystem is unused and the
    // existing KERNAL serial traps provide instant I/O.
    //
    // Systems call drive_tick() once per CPU cycle from their system_tick().
    // =========================================================================

    DriveMode      drive_mode_ = DriveMode::WARP;
    DriveSubsystem drive_subsystem_;

    /// True when drive_mode_ is WARP or ACCURATE (cycle-accurate drives active).
    bool is_drive_cycle_accurate() const {
        return drive_mode_ != DriveMode::HOOKED;
    }

    /// True when the drive subsystem is actively warping (skip video + sleep).
    bool is_drive_warping() const {
        return drive_mode_ == DriveMode::WARP && drive_subsystem_.warp().is_warping();
    }

    /// Initialize the drive subsystem — call from derived initialize().
    /// Reads drive_mode from config_.custom_settings["drive_mode"].
    void init_drive_subsystem();

    /// Reset the drive subsystem — call from derived reset().
    void reset_drive_subsystem();

    /// Apply drive_mode from SystemConfiguration custom_settings.
    void apply_drive_mode_config();

    /// Add the DriveMode custom option to a HardwareTraits struct.
    /// Call from derived create_*_hardware_traits() functions.
    /// Public because it's called from free functions that build HardwareTraits.
public:
    static void add_drive_mode_option(HardwareTraits& traits);
protected:

    // =========================================================================
    // KERNAL SERIAL TRAPS — shared IEC bus trap infrastructure
    //
    // Intercept KERNAL ROM serial bus routines to provide instant drive I/O.
    // This is the standard approach (same as VICE's serial-trap.c) — when the
    // CPU reaches specific KERNAL addresses, the C++ trap handler executes
    // the operation directly on the Drive1541Device channel buffers.
    //
    // Trap addresses differ per KERNAL ROM (C64, C128 mode, C128 C64-mode).
    // The dispatch (check_serial_traps) stays in derived classes.  The handler
    // bodies live here because the logic is identical.
    // =========================================================================

    /// IEC command byte constants (shared across all Commodore KERNALs).
    struct IEC {
        static constexpr uint8_t LISTEN_MASK  = 0x20;
        static constexpr uint8_t TALK_MASK    = 0x40;
        static constexpr uint8_t SECOND_MASK  = 0x60;
        static constexpr uint8_t CLOSE_MASK   = 0xE0;
        static constexpr uint8_t OPEN_MASK    = 0xF0;
        static constexpr uint8_t UNLISTEN     = 0x3F;
        static constexpr uint8_t UNTALK       = 0x5F;
        static constexpr uint8_t DEVNR_MASK   = 0x0F;

        // KERNAL zero-page locations (identical across C64/C128/VIC-20)
        static constexpr uint16_t ZP_BSOUR    = 0x95;
        static constexpr uint16_t ZP_TMP_IN   = 0xA4;
        static constexpr uint16_t ZP_STATUS   = 0x90;
    };

    /// Per-device serial trap tracking state.
    struct SerialTrapState {
        uint8_t trap_device    = 0;     ///< LISTEN/TALK command byte
        uint8_t trap_secondary = 0;     ///< Secondary address command byte
        int     active_device  = -1;    ///< Device number currently addressed (-1 = none)
    };

    bool           serial_traps_enabled_ = false;
    SerialTrapState serial_trap_;

    /// Find a 1541 drive for a given device number on the IEC bus.
    /// Uses get_iec_port_index() to locate the IEC serial port.
    Drive1541Device* find_iec_drive(int device_number);

    /// Update serial_traps_enabled_ flag based on attached devices.
    /// Call from on_port_device_changed() when port_index == get_iec_port_index().
    void update_serial_traps_enabled();

    // ── Serial trap handler bodies ──────────────────────────────────────
    //
    // Shared handler logic.  Templated on the CPU type (any fam65xx_t<Traits>
    // instantiation — MOS6510, CSG8502, etc.) so we avoid virtual dispatch
    // and the register accessor types resolve naturally.
    //
    // Parameters:
    //   cpu        — reference to the active fam65xx CPU
    //   ram        — flat RAM buffer (for ZP access)
    //   resume_pc  — KERNAL return address after trap execution
    //
    // Returns true if the trap was handled.

    template <typename CPU>
    bool serial_trap_attention(CPU& cpu, uint8_t* ram, uint16_t resume_pc);

    template <typename CPU>
    bool serial_trap_send(CPU& cpu, uint8_t* ram, uint16_t resume_pc);

    template <typename CPU>
    bool serial_trap_receive(CPU& cpu, uint8_t* ram, uint16_t resume_pc);

    template <typename CPU>
    bool serial_trap_ready(CPU& cpu, uint16_t resume_pc);

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

    // =========================================================================
    // UNMAPPED INPUT — on-screen buttons for guest keys with no host mapping
    //
    // Some guest keys (C128 HELP, LINE FEED, 40/80 DISPLAY, etc.) have no
    // common host keyboard equivalent.  This mechanism exposes them as
    // clickable menu items so the user can still trigger them.
    //
    // Derived systems register entries in their initialize() via
    // register_unmapped_input().  The shared render_unmapped_inputs_menu()
    // draws a "Virtual Keys" sub-menu if any entries exist.
    // =========================================================================

    /// Descriptor for a guest key that has no host keyboard mapping.
    struct UnmappedInput {
        const char* label;      // Menu label (e.g., "HELP", "LINE FEED")
        emu_key_t key;          // EmuKey to inject
        bool toggle;            // true = toggle (press/release on alternate clicks)
        bool pressed;           // Current state for toggles

        UnmappedInput(const char* l, emu_key_t k, bool t = false, bool initial = false)
            : label(l), key(k), toggle(t), pressed(initial) {}
    };

    /// Register a guest key as unmapped (call from derived initialize()).
    void register_unmapped_input(const char* label, emu_key_t key, bool toggle = false, bool initial_state = false);

    /// Render the "Virtual Keys" sub-menu.  Call from render_system_menu_items().
    void render_unmapped_inputs_menu();

    /// Release any pending momentary key presses.  Call once per frame
    /// from the derived system's run_frame() or tick loop.
    void tick_unmapped_inputs();

    /// Called when a toggle-type virtual key changes state.
    /// Override in derived systems to wire keys that bypass the matrix
    /// (e.g., C128 40/80 DISPLAY → MMU sense line).
    virtual void on_unmapped_toggle_changed(emu_key_t /*key*/, bool /*pressed*/) {}

    std::vector<UnmappedInput> unmapped_inputs_;
    int unmapped_release_countdown_ = 0;    // Frames until pending release
    int unmapped_release_index_ = -1;       // Index of key awaiting release

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
    bool is_warping() const override { return is_drive_warping(); }
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

// Template definitions for serial_trap_attention/send/receive/ready live in
// commodore_serial_traps.inl — include that file from .cpp files that call
// check_serial_traps() to avoid pulling Drive1541Device into every TU.
