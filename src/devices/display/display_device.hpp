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
// PHOSPHOR DECAY & MASK PATTERN ENUMS
// ============================================================================

/// Phosphor luminance decay model.
enum class PhosphorDecay : uint8_t {
    Exponential,  ///< Standard phosphor exponential decay (most CRTs)
    Linear,       ///< Linear fade (synthetic / direct output)
};

/// Shadow mask / aperture grille pattern type.
enum class MaskPattern : uint8_t {
    Shadow,     ///< Delta-gun shadow mask (most color CRTs)
    Aperture,   ///< Aperture grille / Trinitron (vertical stripes)
    SlotMask,   ///< Slot mask (some professional monitors)
};

// ============================================================================
// DISPLAY CHARACTERISTICS — grouped sub-struct design
// ============================================================================

/// Phosphor parameters: material type, glow, persistence, bloom.
struct PhosphorParams {
    PhosphorType  type            = PhosphorType::P22;
    float         glow_color[3]   = {1.0f, 1.0f, 1.0f};
    float         persistence     = 2.0f;    ///< ms; decay to 10%
    float         bloom_radius    = 1.0f;
    float         bloom_threshold = 0.7f;
    PhosphorDecay decay_curve     = PhosphorDecay::Exponential;
};

/// Electron beam geometry and deflection accuracy.
struct BeamParams {
    float width             = 0.70f;
    float softness          = 0.40f;
    float pincushion        = 0.03f;
    float h_linearity       = 0.99f;
    float v_linearity       = 0.99f;
    float convergence_error[2] = {0.30f, 0.20f}; ///< {edge, center} in mm
    float corner_pin[4]     = {0.0f, 0.0f, 0.0f, 0.0f};
};

/// Shadow mask / aperture grille parameters.
struct MaskParams {
    MaskPattern pattern         = MaskPattern::Shadow;
    float       opacity         = 0.25f;
    float       triad_size      = 1.00f;
    float       slot_mask_width = 0.00f;   ///< Aperture open ratio for grille types
};

/// Raster scanline simulation.
struct ScanlineParams {
    float gap       = 0.12f;   ///< Inter-line gap darkness
    float strength  = 0.45f;   ///< Scanline darkening intensity
    float phase     = 0.00f;   ///< Sub-pixel phase shift per line
    bool  interlace = false;   ///< Interlaced scanning
};

/// Glass optics, curvature, and surface reflections.
struct OpticsParams {
    float curvature           = 0.30f;   ///< 0 = flat, 1 = deep curve
    float vignette_strength   = 0.20f;
    float reflection_strength = 0.08f;
    float edge_glow           = 0.05f;
    float glass_tint[3]       = {0.94f, 0.97f, 0.92f};
};

/// Analogue signal path impairments.
struct SignalParams {
    float bandwidth          = 4.20f;   ///< MHz; luma bandwidth limit
    float noise_level        = 0.020f;
    float hum_bar_strength   = 0.020f;
    float ghosting_strength  = 0.040f;
    float chroma_phase_error = 1.00f;   ///< degrees
    float sync_stability     = 0.90f;
};

/// Physical properties of a display device that influence rendering.
/// Concrete display devices provide these; the rendering pipeline reads them
/// to configure shaders, geometry correction, and post-processing.
struct DisplayCharacteristics {
    // --- Top-level scalars ---
    DisplayTechnology technology = DisplayTechnology::CRT_Shadow;
    float diagonal   = 13.0f;         ///< Nominal screen diagonal (inches)
    float aspect     = 4.0f / 3.0f;   ///< Width / height
    float dot_pitch  = 0.28f;         ///< Mask dot pitch or grille pitch (mm)
    float brightness = 1.0f;          ///< Relative brightness (1.0 = nominal)
    float contrast   = 1.0f;          ///< Relative contrast
    float gamma      = 2.2f;          ///< Display gamma
    float color_temp = 6500.0f;       ///< Color temperature (Kelvin)

    // --- Grouped sub-structs ---
    PhosphorParams  phosphor;
    BeamParams      beam;
    MaskParams      mask;
    ScanlineParams  scanlines;
    OpticsParams    optics;
    SignalParams    signal;
};

/// Map a PhosphorType to an RGB tint color for monochrome rendering.
/// Color CRTs (P22) return white (1,1,1) — no tinting.
inline constexpr void phosphor_tint_rgb(PhosphorType p, float& r, float& g, float& b) {
    switch (p) {
        case PhosphorType::P1:  r = 0.2f; g = 1.0f; b = 0.2f; break;  // Green
        case PhosphorType::P4:  r = 1.0f; g = 1.0f; b = 1.0f; break;  // White (B&W TV)
        case PhosphorType::P7:  r = 0.4f; g = 0.6f; b = 1.0f; break;  // Blue-white
        case PhosphorType::P22: r = 1.0f; g = 1.0f; b = 1.0f; break;  // Tricolor — no tint
        case PhosphorType::P31: r = 0.2f; g = 1.0f; b = 0.2f; break;  // Green (classic terminal)
        case PhosphorType::P39: r = 0.2f; g = 1.0f; b = 0.3f; break;  // Green, long persist
        case PhosphorType::P43: r = 0.3f; g = 1.0f; b = 0.3f; break;  // Green, military
        case PhosphorType::Custom: r = 1.0f; g = 0.7f; b = 0.2f; break;  // Amber
        default: r = 1.0f; g = 1.0f; b = 1.0f; break;
    }
}

// ============================================================================
// VIDEO SIGNAL TYPE MASK
// ============================================================================

/// Bitmask for video signal type acceptance.
using video_signal_mask_t = uint16_t;

/// Convert a VideoSignalType to its corresponding bitmask bit.
inline constexpr video_signal_mask_t video_signal_bit(VideoSignalType t) {
    return static_cast<video_signal_mask_t>(1u << static_cast<uint8_t>(t));
}

/// Predefined masks for common monitor signal configurations.
namespace VideoSignalMask {
    inline constexpr video_signal_mask_t Composite      = video_signal_bit(VideoSignalType::Composite);
    inline constexpr video_signal_mask_t SVideo         = video_signal_bit(VideoSignalType::SVideo);
    inline constexpr video_signal_mask_t RGB            = video_signal_bit(VideoSignalType::RGB);
    inline constexpr video_signal_mask_t RGBI           = video_signal_bit(VideoSignalType::RGBI);
    inline constexpr video_signal_mask_t YPbPr          = video_signal_bit(VideoSignalType::YPbPr);
    inline constexpr video_signal_mask_t Digital        = video_signal_bit(VideoSignalType::Digital);
    inline constexpr video_signal_mask_t Vector         = video_signal_bit(VideoSignalType::Vector);

    // --- Common combinations ---
    inline constexpr video_signal_mask_t CompositeSVideo = Composite | SVideo;
    inline constexpr video_signal_mask_t CompositeRGB    = Composite | RGB;
    inline constexpr video_signal_mask_t AllAnalog        = Composite | SVideo | RGB | YPbPr;
    inline constexpr video_signal_mask_t All              = Composite | SVideo | RGB | RGBI | YPbPr | Digital;
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
    virtual video_signal_mask_t get_accepted_video_signals() const = 0;

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
    /// Returns a sentinel for non-video port types.
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
