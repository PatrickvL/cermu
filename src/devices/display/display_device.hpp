#pragma once
/**
 * display_device.hpp — Abstract base for display peripheral devices
 *
 * Intermediate base between PeripheralDevice and concrete display devices
 * (CRT monitors, LCD panels, vector monitors, etc.).  Models the physical
 * display hardware that connects to a system's video (and optionally audio)
 * output port(s).
 *
 * Display devices:
 *   - Accept video signals from video output ports
 *   - Declare which signal types they support (composite, S-Video, RGB, …)
 *   - Describe physical display characteristics (phosphor, mask, geometry)
 *   - Optionally process audio through built-in speakers
 *
 * Real hardware came in many connector configurations:
 *   - Combined A/V in a single DIN or SCART connector
 *   - Separate video and audio cables/jacks
 *   - Adapter cables converting one connector type to another
 * The display device declares what *signal types* it accepts; the port/cable
 * wiring that delivers those signals is handled by the Port framework.
 */

#include "core/port.hpp"
#include "core/signal_types.hpp"
#include <cstdint>

// ============================================================================
// DISPLAY TECHNOLOGY
// ============================================================================

/// Physical display technology.
enum class DisplayTechnology : uint8_t {
    CRT_Shadow,       ///< CRT with shadow mask (consumer TVs, many monitors)
    CRT_Aperture,     ///< CRT with aperture grille (Trinitron, Diamondtron)
    CRT_SlotMask,     ///< CRT with slot mask (some professional monitors)
    CRT_Monochrome,   ///< CRT with single-phosphor coating (green/amber/white)
    CRT_Vector,       ///< CRT vector display (random-scan, no raster)
    LCD,              ///< LCD flat panel
    LED,              ///< LED display
};

// ============================================================================
// PHOSPHOR TYPE
// ============================================================================

/// Standard CRT phosphor designations.
/// Each type implies a specific color, persistence, and spectral response.
enum class PhosphorType : uint8_t {
    P1,     ///< Green, medium persistence (oscilloscope)
    P4,     ///< White, medium-short persistence (B&W television)
    P7,     ///< Blue/yellow, long persistence (radar, early terminals)
    P22,    ///< Tricolor (RGB), medium-short (color CRT — most common)
    P31,    ///< Green, medium persistence (oscilloscope, vector arcade)
    P39,    ///< Green, long persistence (radar, storage displays)
    P43,    ///< Green, medium persistence (military displays)
    Custom, ///< User-defined phosphor characteristics
};

// ============================================================================
// DISPLAY CHARACTERISTICS
// ============================================================================

/// Physical properties of a display device that influence rendering.
/// Concrete display devices provide these; the rendering pipeline reads them
/// to configure shaders, geometry correction, and post-processing.
struct DisplayCharacteristics {
    // --- Technology ---
    DisplayTechnology technology   = DisplayTechnology::CRT_Shadow;
    PhosphorType      phosphor     = PhosphorType::P22;

    // --- Geometry ---
    float screen_diagonal_inches   = 13.0f;  ///< Nominal screen diagonal
    float aspect_ratio             = 4.0f / 3.0f;  ///< Width / height
    float curvature                = 0.0f;   ///< 0 = flat, 1 = typical CRT curve

    // --- Phosphor response ---
    float persistence_ms           = 2.0f;   ///< Phosphor decay to 10% (milliseconds)
    float color_temperature_k      = 6500.0f; ///< Color temperature (Kelvin)

    // --- Mask / grille ---
    float dot_pitch_mm             = 0.28f;  ///< Mask dot pitch or grille pitch
    float scanline_gap             = 0.0f;   ///< 0 = no visible gap, 1 = full gap

    // --- Tone response ---
    float brightness               = 1.0f;   ///< Relative brightness (1.0 = nominal)
    float contrast                 = 1.0f;   ///< Relative contrast
    float gamma                    = 2.2f;   ///< Display gamma
};

// ============================================================================
// VIDEO SIGNAL TYPE MASK
// ============================================================================

/// Bitmask for video signal type acceptance.
using VideoSignalMask = uint16_t;

/// Convert a VideoSignalType to its corresponding bitmask bit.
inline constexpr VideoSignalMask video_signal_bit(VideoSignalType t) {
    return static_cast<VideoSignalMask>(1u << static_cast<uint8_t>(t));
}

/// Predefined masks for common monitor signal configurations.
namespace DisplaySignals {
    inline constexpr VideoSignalMask COMPOSITE   = video_signal_bit(VideoSignalType::Composite);
    inline constexpr VideoSignalMask SVIDEO      = video_signal_bit(VideoSignalType::SVideo);
    inline constexpr VideoSignalMask RGB         = video_signal_bit(VideoSignalType::RGB);
    inline constexpr VideoSignalMask RGBI        = video_signal_bit(VideoSignalType::RGBI);
    inline constexpr VideoSignalMask YPBPR       = video_signal_bit(VideoSignalType::YPbPr);
    inline constexpr VideoSignalMask DIGITAL     = video_signal_bit(VideoSignalType::Digital);
    inline constexpr VideoSignalMask VECTOR      = video_signal_bit(VideoSignalType::Vector);

    // --- Common combinations ---
    inline constexpr VideoSignalMask COMPOSITE_SVIDEO  = COMPOSITE | SVIDEO;
    inline constexpr VideoSignalMask COMPOSITE_RGB     = COMPOSITE | RGB;
    inline constexpr VideoSignalMask ALL_ANALOG        = COMPOSITE | SVIDEO | RGB | YPBPR;
    inline constexpr VideoSignalMask ALL_RASTER        = COMPOSITE | SVIDEO | RGB | RGBI | YPBPR | DIGITAL;
}

// ============================================================================
// DISPLAY DEVICE (base class)
// ============================================================================

/**
 * Abstract base for display peripheral devices.
 *
 * Concrete implementations model specific monitor hardware:
 *   - Commodore 1702 (composite + S-Video, P22 shadow mask CRT)
 *   - Commodore 1084 (composite + S-Video + RGB, P22 aperture grille)
 *   - Atari/Wells-Gardner vector monitor (vector, P31/P22)
 *   - Generic 13" color TV (composite only, P22 shadow mask)
 *   - Generic monochrome CRT (composite, P4 white / P31 green)
 *
 * Display devices are output-only peripherals — they observe the video
 * signal but do not drive any signals back to the system.  They are
 * passive receivers in the wired-AND signal protocol.
 */
class DisplayDevice : public PeripheralDevice {
public:
    ~DisplayDevice() override = default;

    // --- Signal acceptance ----------------------------------------------

    /// Bitmask of VideoSignalType values this display can accept.
    virtual VideoSignalMask get_accepted_video_signals() const = 0;

    /// Check if a specific signal type is accepted.
    bool accepts_signal(VideoSignalType type) const {
        return (get_accepted_video_signals() & video_signal_bit(type)) != 0;
    }

    // --- Display characteristics ----------------------------------------

    /// Physical properties of this display (phosphor, mask, geometry, etc.).
    /// The rendering pipeline queries these to configure shaders and
    /// post-processing effects.
    virtual const DisplayCharacteristics& get_display_characteristics() const = 0;

    // --- Built-in audio -------------------------------------------------

    /// Whether this display has built-in speaker(s).
    virtual bool has_builtin_speakers() const { return false; }

    // --- Multi-port compatibility ---------------------------------------

    /// Displays can attach to both video and audio ports.
    /// Maps accepted video signal types to video PortTypes, and — if the
    /// display has built-in speakers — also accepts audio PortTypes.
    bool is_compatible_with(PortType type) const override {
        // Check video port types against accepted signal types
        if (accepts_signal(video_signal_for_port(type)))
            return true;
        // Check audio port types for displays with built-in speakers
        if (has_builtin_speakers()) {
            if (type == PortType::AUDIO_MONO || type == PortType::AUDIO_STEREO)
                return true;
        }
        return false;
    }

    // --- PeripheralDevice defaults for output-only device ----------------

    /// Displays have no state to reset.
    void reset() override {}

    /// Passive receiver — all signal lines released (idle).
    uint32_t get_output_signals() const override { return 0xFFFFFFFF; }

    /// Displays do not accept host input.
    InputPeripheralDevice* as_input_device() override { return nullptr; }

    // --- Helpers --------------------------------------------------------

    /// Map a video PortType to its corresponding VideoSignalType.
    /// Returns a sentinel (CompositeArtifact + 1) for non-video port types.
    static constexpr VideoSignalType video_signal_for_port(PortType type) {
        switch (type) {
            case PortType::VIDEO_COMPOSITE: return VideoSignalType::Composite;
            case PortType::VIDEO_SVIDEO:    return VideoSignalType::SVideo;
            case PortType::VIDEO_RGB:       return VideoSignalType::RGB;
            case PortType::VIDEO_RGBI:      return VideoSignalType::RGBI;
            case PortType::VIDEO_COMPONENT: return VideoSignalType::YPbPr;
            case PortType::VIDEO_HDMI:      return VideoSignalType::Digital;
            default: return static_cast<VideoSignalType>(255);
        }
    }
};
