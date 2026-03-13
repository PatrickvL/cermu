#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <SDL_keycode.h>
#include "core/hardware_traits.hpp"
#include "core/chip.hpp"     // ChipBase, ChipInfo
#include "core/port.hpp"
#include "core/peripherals/input_peripheral_device.hpp"
#include "core/device_registry.hpp"

// Forward-declare format descriptor so SystemDescriptor can reference it
struct format_descriptor_t;

#include "core/board_base.hpp"

/**
 * Result of probing a file for system-specific compatibility.
 *
 * Returned by a system's probe_file callback.  Combines the
 * file-to-system confidence score with the optimal configuration
 * for running the file on that system, eliminating the need for
 * separate can_load_file and detect_optimal_configuration passes.
 */
struct SystemProbeResult {
    float               confidence = 0.0f; /**< 0.0-1.0: how well this file matches this system */
    SystemConfiguration configuration;     /**< Optimal config (memory, region, custom settings) */
};

/**
 * System descriptor - provides metadata about an emulated system
 * Each system implementation provides this to describe itself
 */
struct SystemDescriptor {
    const char* name;                    // E.g., "Commodore 64"
    const char* short_name;              // E.g., "C64"
    const char* description;             // Brief description

    /**
     * Data folder name under the top-level data/ directory.
     * Systems that share a hardware family (e.g. C16, C116, Plus/4)
     * all point to the same folder (e.g. "c16").  Used by the probe
     * verification tool to map folder → acceptable systems.
     * May be nullptr for systems with no data folder (e.g. CHIP-8).
     */
    const char* data_folder;

    /**
     * System name aliases for identification and selection.
     *
     * Serves multiple purposes:
     * - Command-line --system selection (case-insensitive matching)
     * - Filepath heuristic matching (directory names, archive names)
     * - UI system picker / search
     *
     * The first entry is the canonical/default name for this system
     * variant and should match short_name.  All comparisons are
     * case-insensitive, so casing-only variants (e.g. "C64" vs "c64")
     * must NOT be included — store each alias in its natural casing.
     *
     * Example: VIC-20 → {"VIC20", "VIC-20", "VIC 20", "VIC_20"}
     * Example: Plus/4 → {"Plus4", "Plus/4", "Plus-4"}
     */
    std::vector<const char*> aliases;

    /**
     * NULL-terminated array of pointers to format descriptors that
     * this system can load.  Used both for file-dialog filters and
     * as a generic gatekeeper during system identification: the
     * registry runs each format's identify() callback first, and
     * only calls probe_file() when at least one format matches.
     *
     * May be nullptr for systems whose file types have no dedicated
     * format descriptors (e.g., CHIP-8 raw binaries).  In that case
     * the probe runs unconditionally.
     *
     * Example:  { &PRG_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR, nullptr }
     */
    const format_descriptor_t* const* supported_formats;

    // Hardware traits (fixed characteristics)
    HardwareTraits hardware_traits;

    /**
     * System-specific file probe.
     *
     * Called during system identification after the generic format
     * gatekeeper has matched a format from supported_formats (passed
     * as matched_format).  Responsible for returning:
     *   - confidence: how well the file's content matches this system
     *   - configuration: the optimal SystemConfiguration for running it
     *
     * When supported_formats is nullptr the probe is called with
     * matched_format == nullptr; the callback must handle identification
     * entirely (extension checks, content heuristics, etc.).
     *
     * @param matched_format  Best-matching format descriptor, or nullptr
     * @param filepath        File path (for context hints, e.g. "ntsc" in name)
     * @param data            Full file content buffer
     * @param size            File size in bytes
     * @return                Probe result: confidence + configuration
     */
    std::function<SystemProbeResult(
        const format_descriptor_t* matched_format,
        const char* filepath,
        const uint8_t* data, size_t size
    )> probe_file;
};

// ============================================================================
// SYSTEM CHIP — a chip's role within a specific emulated system
// ============================================================================

// (ChipBase already included at top of this file via chip.h)

/**
 * Binds a ChipBase instance to its role in a specific system.
 *
 * Combines:
 *   - the chip itself (via unique_ptr<ChipBase>)
 *   - system-specific metadata (display name, category, address)
 *   - GUI toggle state (debug/settings window visibility)
 *
 * Stored in System::registered_chips_, populated during initialize().
 * Uses uint8_t for toggle state because std::vector<bool> is bit-packed
 * and does not support taking the address of an element.
 */
struct SystemChip {
    ChipBase* chip = nullptr;    // Non-owning: system or owned_chip_adapters_ manages lifetime

    // System-specific metadata (how this chip is named/categorized here)
    const char* display_name;    // "CIA 1 (MOS 6526)" — for UI
    const char* short_name;      // "CIA 1" — for compact display
    const char* category;        // "CPU", "Video", "Audio", "I/O", "Memory", "Bus"
    uint16_t base_address;       // Memory-mapped base address (0 if N/A)

    // GUI toggle state (managed by the GUI layer)
    uint8_t show_detached = 0;   // Detached combined window (layout+debug+settings)

    // Submenu popup width lock — once the popup has been rendered, its
    // auto-sized width is captured and reused so the popup can't jitter
    // horizontally as register values change.  Height is left free so
    // tall layouts always fit.  0 = not yet measured.
    float submenu_locked_w = 0.0f;
};

/**
 * Abstract base class for all emulated systems
 * Provides common infrastructure while requiring system-specific implementations
 *
 * This class combines interface definition with shared implementation to reduce
 * boilerplate code across system implementations (~150 lines saved per system).
 */
class System {
protected:
    // Configuration (all systems need these)
    HardwareTraits hardware_traits_;
    SystemConfiguration config_;
    std::vector<PaletteColor> current_palette_;
    
    // Framebuffer (provided by GUI)
    uint32_t* rgba_framebuffer_;
    int rgba_width_;
    int rgba_height_;
    
    // Emulation state
    uint64_t total_cycles_;
    float speed_multiplier_;
    uint32_t cached_target_fps_ = 60;  // Cached to avoid per-frame virtual dispatch + vector lookup
    bool quit_requested_;

    // Loaded program title — set by load_file() implementations.
    // For SID/NSF: song name + author.  For PRG/NES: bare filename.
    std::string program_title_;

    // Display screen rect — where the emulated display is drawn in SDL window coords.
    // Updated each frame by the GUI after rendering the display image.
    // Used by peripheral devices (e.g. lightpen) for mouse → display coordinate mapping.
    struct ScreenRect { float x = 0, y = 0, w = 0, h = 0; };
    ScreenRect display_screen_rect_;

    // Primary board — owns connector ports as a value member (no pointer
    // indirection).  Created automatically; derived systems just call
    // add_port() during initialize().
    BoardBase primary_board_;

    // =========================================================================
    // REGISTERED CHIPS (generic for all systems)
    // =========================================================================
    /// Chips registered by each system during initialization.
    /// The GUI builds the Hardware menu from this list and routes debug/settings
    /// window rendering through each chip's ChipBase virtual methods.
    std::vector<SystemChip> registered_chips_;

    /// Owned chip objects — keeps ChipPlaceholders and other owned ChipBase
    /// subclasses alive while registered_chips_ holds non-owning pointers.
    std::vector<std::unique_ptr<ChipBase>> owned_chip_adapters_;

    /// Register a self-describing chip with transferred ownership.
    /// The chip's own display_name(), short_name(), category(), and
    /// base_address() supply the SystemChip metadata.
    void register_chip(std::unique_ptr<ChipBase> chip);

    /// Register a chip with transferred ownership — explicit placement.
    /// Kept for backward compatibility; prefer the self-describing overload.
    void register_chip(std::unique_ptr<ChipBase> chip,
                       const char* display_name, const char* short_name,
                       const char* category, uint16_t base_address = 0);

    /// Register a system-owned chip (borrowed pointer, no ownership transfer).
    /// The system class must ensure the chip outlives the System.
    void register_chip(ChipBase* chip,
                       const char* display_name, const char* short_name,
                       const char* category, uint16_t base_address = 0);

    /// Register all factory-created chips from a Board instance.
    /// Each chip is registered as a borrowed pointer (Board owns them).
    /// Uses the self-describing register_chip(ChipBase*) path — chips carry
    /// their own display_name, short_name, category, and base_address.
    template<typename BusMem>
    void register_bus_chips(BusMem& mem) {
        for (const auto& chip : mem.owned_chips()) {
            SystemChip sc;
            sc.chip = chip.get();
            sc.display_name  = chip->display_name();
            sc.short_name    = chip->short_name();
            sc.category      = chip->category();
            sc.base_address  = chip->base_address();
            sc.show_detached = 0;
            registered_chips_.push_back(std::move(sc));
        }
    }

    // =========================================================================
    // BOARD & PORT OWNERSHIP
    // =========================================================================
    //
    // Connector ports (physical jacks) live on primary_board_, a BoardBase
    // value member.  All port creation goes through add_port() which always
    // delegates to primary_board_.  No conditional, no indirection.
    //
    // Systems that have a Board<Spec> (bus_mem_) for memory dispatch register
    // it via register_board() so that generic code can iterate all boards.
    // Device ownership stays on System (peripherals are user-attached, not
    // board-soldered components).
    //

    /// All boards on this system.  primary_board_ is always boards_[0];
    /// additional boards (e.g. bus_mem_) are appended via register_board().
    std::vector<BoardBase*> boards_;

    /// Register an additional board (e.g. bus_mem_) for iteration.
    /// The primary_board_ is always registered automatically.
    void register_board(BoardBase* board) { boards_.push_back(board); }

    /// The primary board (owns connector ports, always valid).
    [[nodiscard]] BoardBase& primary_board() { return primary_board_; }
    [[nodiscard]] const BoardBase& primary_board() const { return primary_board_; }

    /// Peripheral device instances owned by the system (attached to ports).
    std::vector<std::unique_ptr<PeripheralDevice>> owned_devices_;

    /// Helper: add a connector port (called by derived systems in initialize).
    int add_port(const PortDefinition& def, int port_number = 0);

    /// Tick all attached peripheral devices (call once per frame).
    void tick_peripherals();

    /// Called after a device is attached to or detached from a port.
    /// Derived systems can override to update cached device pointers.
    virtual void on_port_device_changed(int /*port_index*/) {}

    // =========================================================================
    // DEFAULT PERIPHERAL ATTACHMENT (declarative, data-driven)
    // =========================================================================

    /// Describes a peripheral device to be attached to a connector port
    /// on system start-up.  Systems declare their defaults by overriding
    /// get_default_peripherals().
    struct DefaultPeripheral {
        int         port_index;   ///< Index into primary_board_ ports
        const char* device_id;    ///< DeviceRegistry ID (e.g. "joystick")
    };

    /// Return the system's preferred start-up peripherals.
    /// Called by attach_default_peripherals() after connector ports are
    /// created. Default: empty (no auto-attached devices).
    virtual std::vector<DefaultPeripheral> get_default_peripherals() const {
        return {};
    }

    // =========================================================================
    // GUEST KEYBOARD COLLISION CONTEXT
    // =========================================================================

    /// Return host scancodes consumed by the guest keyboard matrix.
    /// Used by auto_assign_controller_keymaps() to pick presets with the
    /// fewest collisions.  Default: empty (system has no guest keyboard).
    virtual int get_guest_keyboard_scancodes(const SDL_Scancode** out_scancodes) const {
        if (out_scancodes) *out_scancodes = nullptr;
        return 0;
    }

    /// Auto-assign keyboard presets to all controller devices, minimising
    /// collisions with the guest keyboard and avoiding inter-device
    /// key conflicts.  Called at the end of attach_default_peripherals().
    void auto_assign_controller_keymaps();
    
public:
    System();
    virtual ~System() = default;

    /// Attach every peripheral listed by get_default_peripherals() and
    /// then run auto_assign_controller_keymaps().  Called automatically by
    /// the framework after initialize(); systems no longer need to call
    /// this explicitly.
    void attach_default_peripherals();
    
    // Non-virtual implementations (identical for all systems - cannot override)
    const SystemConfiguration& get_configuration() const;
    const HardwareTraits& get_hardware_traits() const;
    const SystemTiming& get_current_timing() const;
    const DisplayTraits& get_display_traits() const;
    const AudioTraits& get_audio_traits() const;
    uint64_t get_total_cycles() const;
    float get_speed_multiplier() const;
    bool is_quit_requested() const { return quit_requested_; }
    void request_quit() { quit_requested_ = true; }

    /// Whether the system is ready to execute (e.g. has ROM loaded).
    /// Systems that require media (cartridge, disk) return false until loaded.
    virtual bool is_system_ready() const { return true; }

    /// Update the screen rect where the emulated display is rendered (SDL window coords).
    void set_display_screen_rect(float x, float y, float w, float h) {
        display_screen_rect_ = {x, y, w, h};
    }
    const ScreenRect& get_display_screen_rect() const { return display_screen_rect_; }

    // --- Port Access (always via primary_board_) --------------------------

    /// Get all connector ports on this system.
    const std::vector<std::unique_ptr<Port>>& get_ports() const {
        return primary_board_.get_ports();
    }

    /// Get a connector port by index (nullptr if out of range).
    Port* get_port(int index) {
        return primary_board_.get_port(index);
    }

    /// Get all boards (primary_board_ first, then registered additional boards).
    const std::vector<BoardBase*>& get_boards() const { return boards_; }

    /// Get all owned peripheral device instances.
    const std::vector<std::unique_ptr<PeripheralDevice>>& get_owned_devices() const {
        return owned_devices_;
    }

    /// Attach a device to a connector port (creates from DeviceRegistry).
    /// On bus ports, multiple devices can be attached simultaneously.
    bool attach_device_to_port(int port_index, const char* device_id);

    /// Detach whatever device is on a connector port (point-to-point ports).
    /// For bus ports, detaches ALL devices.  Use the overload with device ptr
    /// to detach a specific device from a bus.
    void detach_device_from_port(int port_index);

    /// Detach a specific device from a bus port.
    void detach_device_from_port(int port_index, PeripheralDevice* device);

    /// Route an SDL event to all owned devices that accept host input.
    /// Returns true if any device consumed the event.
    bool process_sdl_event_for_devices(const SDL_Event& event);

    /// Render the generic peripheral connector UI (called from GUI layer).
    void render_peripheral_port_ui();

    /// Render right-aligned connector icons in the ImGui main menu bar.
    /// Each icon opens a popup menu for managing the attached peripheral
    /// (detach, switch device, device settings).  Must be called between
    /// BeginMainMenuBar() and EndMainMenuBar().
    /// Returns the total width consumed so the caller can position the
    /// status text accordingly.
    float render_port_menu_bar_icons();

    /// Render host input binding selector for a device (called from connector UI).
    void render_host_input_binding_ui(InputPeripheralDevice* device);

    /// Automatically bind available host input devices to attached peripherals.
    /// Assigns SDL gamepads to gamepad-compatible devices (round-robin), falls
    /// back to keyboard for remaining joystick devices, and binds host mouse to
    /// mouse-accepting devices.  Call after initialize() to make a newly created
    /// system immediately playable with whatever controllers are connected.
    void auto_bind_host_inputs();
    
    // Default implementations (can be overridden if needed)
    virtual void set_framebuffer(uint32_t* buffer, int width, int height);
    virtual bool initialize();
    virtual void shutdown();
    virtual void handle_controller_event(int controller, int button, bool pressed);
    virtual void render_debug_windows(void* gui_state, std::mutex& emu_mutex);

    // --- Screenshot -------------------------------------------------------

    /// Save the current framebuffer to a PNG file.
    /// Returns true on success.
    bool save_screenshot(const char* filename) const;

    /// Save a cropped region of the framebuffer to a PNG file.
    /// Returns true on success.
    bool save_screenshot_cropped(const char* filename,
                                 int crop_x, int crop_y,
                                 int crop_w, int crop_h) const;

    // --- Registered Chips (generic for all systems) -----------------------

    /// Get all registered chips in this system.
    const std::vector<SystemChip>& get_registered_chips() const { return registered_chips_; }
    std::vector<SystemChip>& get_registered_chips() { return registered_chips_; }
    
    // Pure virtual (must implement in derived classes)
    virtual const SystemDescriptor& get_descriptor() const = 0;
    virtual bool set_configuration(const SystemConfiguration& config) = 0;
    virtual bool apply_configuration() = 0;
    virtual void reset() = 0;
    virtual void tick() = 0;
    virtual void run_frame() = 0;
    virtual bool load_file(const char* filepath) = 0;

    /// Try to attach a container/streamable media file to the appropriate
    /// storage device (e.g. D64 → 1541 drive, TAP → datasette).  If the
    /// required device is not yet connected, auto-attaches it.
    /// Returns true if the file was accepted by a storage device.
    /// Default: returns false (system has no storage devices).
    virtual bool attach_media(const char* filepath) { return false; }

    /// Title of the currently loaded program (set by load_file()).
    /// Empty string if nothing is loaded.
    const std::string& get_program_title() const { return program_title_; }

    /// Optional mode label shown in the window title (e.g. "SID Player").
    /// Returns nullptr when no special mode is active.
    virtual const char* get_mode_label() const { return nullptr; }

    /// Optional subtitle info (e.g. "[3/25]" for subtune 3 of 25).
    /// Returns empty string when not applicable.
    virtual std::string get_subtitle_info() const { return {}; }

    virtual uint32_t* get_framebuffer() = 0;
    virtual void get_display_dimensions(int* width, int* height) const = 0;
    virtual void handle_keyboard_event(SDL_Keycode key, bool pressed) = 0;
    virtual void render_system_menu_items() = 0;
    virtual void render_configuration_ui() = 0;
    uint32_t get_target_fps() const { return cached_target_fps_; }

    /// Precise frame time in seconds, derived from hardware timing.
    /// Avoids integer-FPS rounding error (e.g. PAL=19656/985248≈19.95ms,
    /// not 1/50=20.00ms).  Falls back to 1/target_fps if cycles_per_frame is 0.
    double get_target_frame_time() const {
        const auto& t = hardware_traits_.timing;
        if (t.cycles_per_frame > 0 && t.cpu_frequency_hz > 0)
            return static_cast<double>(t.cycles_per_frame) / static_cast<double>(t.cpu_frequency_hz);
        return 1.0 / (cached_target_fps_ > 0 ? cached_target_fps_ : 60);
    }

    virtual void set_speed_multiplier(float multiplier) = 0;

    // Extended keyboard event handler with full SDL event information.
    // Receives keycode, scancode, modifier state, and repeat flag.
    // Systems that implement the KeyboardMapper should override this.
    // Default implementation falls back to handle_keyboard_event(key, pressed).
    virtual void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat);

    // Text input handler — receives characters from SDL_TEXTINPUT.
    // For character-based keyboard mapping: the character produced by
    // the host keyboard layout, independent of which physical key was pressed.
    // Systems using KeyboardMapper should override this.
    // Default implementation does nothing.
    virtual void handle_text_input(const char* text);

    // Release all keyboard input (e.g., on window focus loss).
    // Systems using KeyboardMapper should override this.
    virtual void release_all_keys();

    // Convenience: read a file, probe the system's descriptor for the
    // optimal configuration, merge with the current config (never
    // downgrading memory), and apply.
    // Safe to call both before and after initialize().
    void apply_file_configuration(const char* filepath);

    // Return a default SystemConfiguration based on hardware_traits_.
    // Selects the default memory, region, and peripheral options.
    // Used internally by apply_file_configuration when no probe is set.
    SystemConfiguration default_configuration() const;

    // ---- Audio output --------------------------------------------------
    // Fill \p buffer with up to \p max_samples mono float samples in the
    // range -1.0 .. +1.0 and return the number actually written.
    // Called from the SDL audio callback at the rate advertised by
    // get_audio_traits().sample_rate_hz.
    // Default implementation returns 0 (silence).
    virtual uint32_t get_audio_samples(float* buffer, uint32_t max_samples);

    // Notify the system of the *actual* audio device sample rate.
    // SDL may negotiate a rate different from the one advertised by
    // get_audio_traits() (e.g. 48000 Hz instead of 44100 Hz on Linux).
    // The system should adjust its audio generation to match.
    // Default implementation does nothing.
    virtual void set_audio_sample_rate(int sample_rate_hz);
};

// Include SystemRegistry (moved to separate file)
#include "core/system_registry.hpp"
