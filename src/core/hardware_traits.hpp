// =============================================================================
// hardware_traits.h — Hardware trait and configuration types for emulated systems
// =============================================================================
//
// Describes fixed hardware characteristics (display, audio, timing, palette)
// and user-selectable configuration options (memory size, video standard,
// peripherals, custom settings).
//
// Used by System, SystemDescriptor, and concrete system implementations.
//
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/chip.hpp"     // VideoStandard

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
 * Result of probing a file for system-specific compatibility.
 *
 * Returned by a system's probe_file callback.  Combines the
 * file-to-system confidence score with the optimal configuration
 * for running the file on that system, eliminating the need for
 * separate can_load_file and detect_optimal_configuration passes.
 */
