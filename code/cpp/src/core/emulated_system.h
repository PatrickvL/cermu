#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>

// ============================================================================
// SYSTEM HARDWARE TRAITS
// ============================================================================

/**
 * Video region standard
 */
enum class VideoRegion {
    NTSC,           // NTSC (North America, Japan)
    PAL,            // PAL (Europe, Australia)
    PAL_M,          // PAL-M (Brazil)
    SECAM,          // SECAM (France, Eastern Europe)
    CUSTOM          // Custom/configurable timing
};

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
    VideoRegion region;             // Video region standard
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
 * Region configuration option
 */
struct RegionOption {
    const char* name;               // E.g., "NTSC", "PAL", "PAL-N"
    VideoRegion region;             // Video region enum
    SystemTiming timing;            // Timing characteristics for this region
    bool is_default;                // Whether this is the default configuration
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
    std::vector<RegionOption> region_options;
    std::vector<PeripheralOption> peripheral_options;
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
    const char** supported_extensions;   // NULL-terminated array of file extensions (e.g., {".prg", ".d64", NULL})
    
    // Hardware traits (fixed characteristics)
    HardwareTraits hardware_traits;
    
    // File detection callback - returns confidence 0.0-1.0 that this system can load the file
    std::function<float(const char* filepath, const uint8_t* data, size_t size)> can_load_file;
};

/**
 * Base interface for all emulated systems
 * Systems implement this interface and register themselves with the SystemRegistry
 */
class IEmulatedSystem {
public:
    virtual ~IEmulatedSystem() = default;
    
    // System identification
    virtual const SystemDescriptor& get_descriptor() const = 0;
    
    // Configuration management
    virtual const SystemConfiguration& get_configuration() const = 0;
    virtual bool set_configuration(const SystemConfiguration& config) = 0;
    virtual bool apply_configuration() = 0;  // Apply current configuration (may require reset)
    
    // Hardware trait queries
    virtual const HardwareTraits& get_hardware_traits() const = 0;
    virtual const SystemTiming& get_current_timing() const = 0;  // Get timing for current region
    virtual const DisplayTraits& get_display_traits() const = 0;
    virtual const AudioTraits& get_audio_traits() const = 0;
    
    // System lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void reset() = 0;
    
    // Execution
    virtual void tick() = 0;       // Execute one system cycle
    virtual void run_frame() = 0;  // Execute one complete frame
    
    // File loading
    virtual bool load_file(const char* filepath) = 0;
    
    // Display
    virtual uint32_t* get_framebuffer() = 0;
    virtual void get_display_dimensions(int* width, int* height) const = 0;
    virtual void set_framebuffer(uint32_t* buffer, int width, int height) = 0;
    
    // Input
    virtual void handle_keyboard_event(int key, bool pressed) = 0;
    virtual void handle_controller_event(int controller, int button, bool pressed) = 0;
    
    // GUI integration
    virtual void render_system_menu_items() = 0;        // Render system-specific menu items in "System" menu
    virtual void render_debug_windows(void* gui_state) = 0;  // Render system-specific debug windows (gui_state_t*)
    virtual void render_configuration_ui() = 0;         // Render configuration UI in settings panel
    
    // State
    virtual uint64_t get_total_cycles() const = 0;
    virtual uint32_t get_target_fps() const = 0;
    
    // Emulation control
    virtual void set_speed_multiplier(float multiplier) = 0;
    virtual float get_speed_multiplier() const = 0;
};

/**
 * System factory function type
 * Each system provides a factory function to create instances
 */
using SystemFactory = std::function<std::unique_ptr<IEmulatedSystem>()>;

/**
 * System registry - maintains list of available emulated systems
 * Systems self-register during static initialization
 */
class SystemRegistry {
public:
    static SystemRegistry& instance();
    
    // Register a new system (called during static initialization by system implementations)
    void register_system(const SystemDescriptor& descriptor, SystemFactory factory);
    
    // Get all registered systems
    const std::vector<std::pair<SystemDescriptor, SystemFactory>>& get_systems() const {
        return systems_;
    }
    
    // Find best system for a file (returns nullptr if no suitable system found)
    std::unique_ptr<IEmulatedSystem> create_system_for_file(const char* filepath);
    
    // Create a specific system by short name (e.g., "C64", "CHIP8")
    std::unique_ptr<IEmulatedSystem> create_system_by_name(const char* short_name);
    
private:
    SystemRegistry() = default;
    std::vector<std::pair<SystemDescriptor, SystemFactory>> systems_;
};

/**
 * Helper macro for system registration
 * Place this in each system's .cpp file to auto-register the system
 */
#define REGISTER_SYSTEM(descriptor, factory) \
    namespace { \
        struct SystemRegistrar { \
            SystemRegistrar() { \
                SystemRegistry::instance().register_system(descriptor, factory); \
            } \
        }; \
        static SystemRegistrar registrar; \
    }
