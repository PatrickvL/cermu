#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <SDL_keycode.h>
#include "chip.h"     // VideoStandard, ChipBase, ChipIdentity
#include "connector.h"
#include "device_registry.h"

// Forward-declare format descriptor so SystemDescriptor can reference it
struct format_descriptor_s;
typedef struct format_descriptor_s format_descriptor_t;

// ============================================================================
// SYSTEM HARDWARE TRAITS
// ============================================================================

// VideoStandard enum and ChipBase are defined in chip.h (included at top)

/**
 * Framebuffer color format
 */
enum class FramebufferFormat {
    RGBA8888,       // 32-bit RGBA (8 bits per channel)
    RGB888,         // 24-bit RGB (8 bits per channel)
    RGB565,         // 16-bit RGB (5-6-5 bits)
    PALETTE_INDEXED_8,  // 8-bit palette index
    PALETTE_INDEXED_4,  // 4-bit palette index
    PALETTE_INDEXED_2,  // 2-bit palette index
    MONOCHROME_1    // 1-bit monochrome
};

/**
 * Audio output format
 */
enum class AudioFormat {
    NONE,           // No audio
    MONO_8BIT,      // Mono 8-bit samples
    MONO_16BIT,     // Mono 16-bit samples
    STEREO_8BIT,    // Stereo 8-bit samples
    STEREO_16BIT,   // Stereo 16-bit samples
    CUSTOM          // Custom format
};

/**
 * Hardware timing characteristics
 */
struct SystemTiming {
    uint32_t cpu_frequency_hz;      // CPU clock frequency in Hz
    uint32_t video_frequency_hz;    // Video chip frequency in Hz (0 if not applicable)
    uint32_t audio_sample_rate_hz;  // Audio sample rate in Hz (0 if no audio)
    uint32_t target_fps;            // Target frames per second
    uint32_t cycles_per_frame;      // CPU cycles per frame
    VideoStandard standard;           // Video standard (PAL, NTSC, etc.)
};

/**
 * Color palette entry (RGBA)
 */
struct PaletteColor {
    uint8_t r, g, b, a;
    
    PaletteColor() : r(0), g(0), b(0), a(255) {}
    PaletteColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    
    uint32_t to_rgba32() const {
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
};

/**
 * Display characteristics (fixed hardware traits)
 */
struct DisplayTraits {
    int native_width;               // Native display width in pixels
    int native_height;              // Native display height in pixels
    int visible_width;              // Visible area width (may differ from native)
    int visible_height;             // Visible area height (may differ from native)
    FramebufferFormat format;       // Framebuffer color format
    int palette_size;               // Number of colors in palette (0 if RGB)
    float pixel_aspect_ratio;       // Pixel aspect ratio (1.0 = square pixels)
    bool has_overscan;              // Whether system has overscan/border area
    
    // Palette data (for palette-indexed modes)
    std::vector<PaletteColor> default_palette;  // Default color palette
};

/**
 * Audio characteristics (fixed hardware traits)
 */
struct AudioTraits {
    AudioFormat format;             // Audio output format
    int sample_rate_hz;             // Audio sample rate in Hz
    int channels;                   // Number of audio channels (1=mono, 2=stereo)
    const char* chip_name;          // Name of audio chip (e.g., "SID 6581", "PSG AY-3-8910")
};

/**
 * Memory configuration option
 */
struct MemoryOption {
    const char* name;               // E.g., "16KB", "64KB", "128KB + REU"
    uint32_t ram_size;              // RAM size in bytes
    uint32_t rom_size;              // ROM size in bytes
    bool is_default;                // Whether this is the default configuration
};

/**
 * Describes a video standard configuration available for a system.
 */
struct VideoStandardConfig {
    const char*   name;         // Display name, e.g. "PAL-B", "NTSC-M"
    VideoStandard standard;     // Color encoding and line standard
    SystemTiming  timing;       // Clock, scanline and frame timing
    bool          is_default = false;
};

/**
 * Peripheral/expansion option
 */
struct PeripheralOption {
    const char* id;                 // Unique identifier (e.g., "disk_drive", "printer")
    const char* name;               // Display name (e.g., "1541 Disk Drive", "MPS-803 Printer")
    const char* description;        // Brief description
    bool enabled_by_default;        // Whether enabled by default
};

/**
 * Custom configuration option (system-specific dropdowns)
 */
struct CustomOption {
    const char* id;                 // Key used in SystemConfiguration::custom_settings
    const char* name;               // Display label (e.g., "SID Revision")
    const char* description;        // Tooltip text (nullable)
    std::vector<const char*> choices;  // Option values (display & storage)
    int default_index;              // Index into choices for the default
};

/**
 * Complete hardware trait descriptor
 * Describes the fixed hardware characteristics of the system
 */
struct HardwareTraits {
    // Display
    DisplayTraits display;
    
    // Audio
    AudioTraits audio;
    
    // Timing (for default/primary region)
    SystemTiming timing;
    
    // Available configurations
    std::vector<MemoryOption> memory_options;
    std::vector<VideoStandardConfig> video_standard_configs;
    std::vector<PeripheralOption> peripheral_options;
    std::vector<CustomOption> custom_options;
};

/**
 * System configuration (user-selectable options)
 */
struct SystemConfiguration {
    // Memory configuration
    int memory_option_index;        // Selected memory configuration index
    
    // Region configuration
    int region_option_index;        // Selected region configuration index
    
    // Enabled peripherals (map of peripheral ID -> enabled state)
    std::map<std::string, bool> enabled_peripherals;
    
    // Custom settings (system-specific key-value pairs)
    std::map<std::string, std::string> custom_settings;
    
    SystemConfiguration()
        : memory_option_index(0)
        , region_option_index(0) {}
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
     * NULL-terminated array of pointers to format descriptors that
     * this system can load.  The GUI uses these to build file-dialog
     * filters and tooltips without any system-specific knowledge.
     *
     * Example:  { &PRG_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR, nullptr }
     */
    const format_descriptor_t* const* supported_formats;

    // Hardware traits (fixed characteristics)
    HardwareTraits hardware_traits;
    
    // File detection callback - returns confidence 0.0-1.0 that this system can load the file
    std::function<float(const char* filepath, const uint8_t* data, size_t size)> can_load_file;
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
 * Stored in EmulatedSystem::registered_chips_, populated during initialize().
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

    // Submenu popup size lock — once the popup has been rendered, its size is
    // captured and reused as both min and max constraint so the popup cannot
    // grow (or shrink) on subsequent frames.  0 = not yet measured.
    float submenu_locked_w = 0.0f;
    float submenu_locked_h = 0.0f;
};

/**
 * Abstract base class for all emulated systems
 * Provides common infrastructure while requiring system-specific implementations
 *
 * This class combines interface definition with shared implementation to reduce
 * boilerplate code across system implementations (~150 lines saved per system).
 */
class EmulatedSystem {
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

    /// Register a chip with transferred ownership (e.g. ChipPlaceholder).
    /// The chip is moved into owned_chip_adapters_ and a raw pointer stored
    /// in the SystemChip entry.
    void register_chip(std::unique_ptr<ChipBase> chip,
                       const char* display_name, const char* short_name,
                       const char* category, uint16_t base_address = 0);

    /// Register a system-owned chip (borrowed pointer, no ownership transfer).
    /// The system class must ensure the chip outlives the EmulatedSystem.
    void register_chip(ChipBase* chip,
                       const char* display_name, const char* short_name,
                       const char* category, uint16_t base_address = 0);

    // =========================================================================
    // CONNECTOR PORTS & PERIPHERAL DEVICES (generic for all systems)
    // =========================================================================
    /// Connector ports registered by each system during initialization.
    std::vector<std::unique_ptr<ConnectorPort>> connector_ports_;

    /// Peripheral device instances owned by the system (attached to ports).
    std::vector<std::unique_ptr<PeripheralDevice>> owned_devices_;

    /// Helper: add a connector port (called by derived systems in initialize).
    int add_connector_port(const ConnectorDefinition& def, int port_number = 0);

    /// Tick all attached peripheral devices (call once per frame).
    void tick_peripherals();

    /// Called after a device is attached to or detached from a port.
    /// Derived systems can override to update cached device pointers.
    virtual void on_port_device_changed(int /*port_index*/) {}
    
public:
    EmulatedSystem();
    virtual ~EmulatedSystem() = default;
    
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

    // --- Connector Port Access (generic, available for all systems) ---------

    /// Get all connector ports on this system.
    const std::vector<std::unique_ptr<ConnectorPort>>& get_connector_ports() const {
        return connector_ports_;
    }

    /// Get a connector port by index (nullptr if out of range).
    ConnectorPort* get_connector_port(int index) {
        if (index >= 0 && index < static_cast<int>(connector_ports_.size()))
            return connector_ports_[index].get();
        return nullptr;
    }

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
    void render_peripheral_connector_ui();

    /// Render right-aligned connector icons in the ImGui main menu bar.
    /// Each icon opens a popup menu for managing the attached peripheral
    /// (detach, switch device, device settings).  Must be called between
    /// BeginMainMenuBar() and EndMainMenuBar().
    /// Returns the total width consumed so the caller can position the
    /// status text accordingly.
    float render_connector_menu_bar_icons();

    /// Render host input binding selector for a device (called from connector UI).
    void render_host_input_binding_ui(PeripheralDevice* device);

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
    virtual void render_debug_windows(void* gui_state);

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

    /// Title of the currently loaded program (set by load_file()).
    /// Empty string if nothing is loaded.
    const std::string& get_program_title() const { return program_title_; }

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

    // Analyze a file and return the optimal SystemConfiguration for it.
    // Called by create_system_for_file() after the system is created but
    // before initialize().  The returned configuration is applied via
    // set_configuration() + apply_configuration().
    // Default implementation returns a configuration using each trait's
    // default option.
    virtual SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size);

    // Convenience: read a file, detect optimal configuration, merge with
    // the current config (never downgrading memory), and apply.
    // Safe to call both before and after initialize().
    void apply_file_configuration(const char* filepath);

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
#include "system_registry.h"
