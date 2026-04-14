#pragma once
/**
 * lcd_types.hpp — LCD display parameter types
 *
 * Enums and structs describing LCD-specific physical display characteristics:
 * panel technology, backlight type, subpixel layout, pixel grid visibility,
 * response time ghosting, backlight uniformity, and optical properties.
 *
 * These types are used by DisplayCharacteristics (display_device.hpp) and
 * configured by concrete LCD devices (generic_lcd.hpp).
 */

#include <cstdint>

// ============================================================================
// LCD PANEL TECHNOLOGY
// ============================================================================

/// LCD panel technology — determines base optical characteristics.
enum class LCDPanelType : uint8_t {
    STN,    ///< Super Twisted Nematic (Game Boy DMG/Pocket, Game Gear, Lynx)
    DSTN,   ///< Dual-scan STN (improved STN, some early color handhelds)
    TN,     ///< Twisted Nematic (GBC, GBA, DS, most 90s–2000s handhelds)
    IPS,    ///< In-Plane Switching (PSP, Switch, modern panels)
};

/// LCD backlight / illumination type.
enum class LCDBacklightType : uint8_t {
    None,               ///< No backlight — reflective LCD (DMG, GBP, GBC, GBA)
    Frontlight,         ///< Front-lit panel (GBA SP AGS-001)
    Electroluminescent, ///< EL panel backlight (Game Boy Light MGL-001, Japan only)
    LED,                ///< LED backlight (GBA SP AGS-101, DS, 3DS, Switch)
    CCFL,               ///< Cold cathode fluorescent (PSP 1000/2000, early laptops)
};

/// LCD subpixel arrangement — affects visible subpixel structure at scale.
enum class LCDSubpixelLayout : uint8_t {
    RGBStripe,      ///< Standard RGB vertical stripe (most TFT panels)
    BGRStripe,      ///< BGR vertical stripe (some Samsung panels)
    Monochrome,     ///< Single-element pixels (DMG, GBP, Game Boy Light)
};

// ============================================================================
// LCD DISPLAY PARAMETERS
// ============================================================================

/// LCD-specific display parameters.
/// These augment the universal DisplayCharacteristics for LCD panels.
/// All values are physically motivated by panel datasheets and measurements.
struct LCDParams {
    // --- Panel type ---
    LCDPanelType       panel_type       = LCDPanelType::TN;
    LCDBacklightType   backlight_type   = LCDBacklightType::LED;
    LCDSubpixelLayout  subpixel_layout  = LCDSubpixelLayout::RGBStripe;

    // --- Pixel grid ---
    float pixel_grid_opacity  = 0.0f;    ///< Visibility of grid lines between pixels (0–1)
    float pixel_grid_width    = 0.10f;   ///< Grid line width as fraction of pixel pitch
    float subpixel_opacity    = 0.0f;    ///< Visibility of RGB subpixel structure (0–1)

    // --- Response time / ghosting ---
    float response_time_ms    = 5.0f;    ///< Pixel response time (ms, grey-to-grey)
    float ghosting_strength   = 0.0f;    ///< Residual image from slow pixel response (0–1)

    // --- Backlight ---
    float backlight_brightness = 1.0f;   ///< Backlight intensity (0=off, 1=max)
    float backlight_bleed      = 0.0f;   ///< Light leaking from edges (0–1)
    float backlight_uniformity = 1.0f;   ///< Backlight evenness (1=perfect, lower=uneven)

    // --- Color reproduction ---
    float color_tint[3]       = {1.0f, 1.0f, 1.0f}; ///< Panel color bias (DMG: green tint)
    float color_saturation    = 1.0f;    ///< Saturation multiplier (STN panels < 1.0)

    // --- Optical properties ---
    float viewing_angle_falloff = 0.0f;  ///< Color/contrast shift at screen edges (TN=high)
    float black_level           = 0.0f;  ///< Minimum displayable brightness (0=ideal)
    float reflection_strength   = 0.0f;  ///< Ambient light reflection on panel surface
    float pixel_pitch_mm        = 0.0f;  ///< Physical pixel pitch in mm (0=auto)
};
