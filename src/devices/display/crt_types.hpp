#pragma once
/**
 * crt_types.hpp — CRT display parameter types
 *
 * Enums and structs describing CRT-specific physical display characteristics:
 * phosphor types, electron beam geometry, shadow mask patterns, scanline
 * simulation, glass optics, and analogue signal impairments.
 *
 * These types are used by DisplayCharacteristics (display_device.hpp) and
 * configured by concrete CRT devices (generic_crt.hpp).
 */

#include <cstdint>

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
// CRT DISPLAY CHARACTERISTICS — grouped sub-struct design
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
