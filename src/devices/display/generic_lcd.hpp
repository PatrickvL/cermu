#pragma once
/**
 * generic_lcd.hpp — Generic LCD panel peripheral device
 *
 * A configurable LCD display device that models common handheld and
 * portable LCD panels.  An LCDPreset selects a bundle of display
 * characteristics tuned to match specific real hardware.
 *
 * Presets cover well-known LCD panels:
 *   - Game Boy DMG        (STN, reflective, monochrome green-tint)
 *   - Game Boy Pocket     (STN, reflective, monochrome grey)
 *   - Game Boy Light      (STN, EL backlight, monochrome green)
 *   - Game Boy Color      (TN, reflective, color)
 *   - Game Boy Advance    (TN, reflective, color)
 *   - GBA SP (AGS-001)    (TN, frontlight, color)
 *   - GBA SP (AGS-101)    (TN, LED backlight, color)
 *   - Nintendo DS (top)   (TN, LED backlight, color)
 *   - Game Gear           (STN, CCFL backlight, color)
 *   - Atari Lynx          (STN, CCFL backlight, color)
 *   - PSP 1000            (TN, CCFL backlight, wide)
 *   - Nintendo Switch     (IPS, LED backlight, wide)
 */

#include "devices/display/display_device.hpp"

// ============================================================================
// LCD PRESETS
// ============================================================================

/// Predefined LCD panel configurations.
enum class LCDPreset : uint8_t {
    // --- Nintendo Game Boy family ---
    GameBoyDMG,         ///< Original Game Boy — STN reflective, monochrome green
    GameBoyPocket,      ///< Game Boy Pocket — STN reflective, monochrome grey
    GameBoyLight,       ///< Game Boy Light — STN EL backlight, monochrome green
    GameBoyColor,       ///< Game Boy Color — TN reflective, color
    GameBoyAdvance,     ///< Game Boy Advance — TN reflective, color
    GBASP_AGS001,       ///< GBA SP AGS-001 — TN frontlight, color
    GBASP_AGS101,       ///< GBA SP AGS-101 — TN LED backlight, color
    NintendoDS,         ///< Nintendo DS — TN LED backlight, color

    // --- Sega / Atari handhelds ---
    GameGear,           ///< Sega Game Gear — STN CCFL backlight, color
    AtariLynx,          ///< Atari Lynx — STN CCFL backlight, color

    // --- Modern handhelds ---
    PSP1000,            ///< PSP 1000 — TN CCFL backlight, wide
    NintendoSwitch,     ///< Nintendo Switch — IPS LED backlight, wide
};

// ============================================================================
// GENERIC LCD DEVICE
// ============================================================================

class GenericLCD : public DisplayDevice {
public:
    explicit GenericLCD(LCDPreset preset = LCDPreset::GameBoyDMG);
    ~GenericLCD() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return name_; }
    const char* get_id() const override   { return id_; }
    PortType get_port_type() const override { return PortType::VIDEO_COMPOSITE; }

    // --- DisplayDevice interface ---------------------------------------
    video_signal_mask_t get_accepted_video_signals() const override { return accepted_signals_; }
    const DisplayCharacteristics& get_display_characteristics() const override { return characteristics_; }
    bool has_builtin_speakers() const override { return has_speakers_; }

    // --- Preset query --------------------------------------------------
    LCDPreset get_preset() const { return preset_; }

    /// Mutable access to display characteristics (for GUI tuning).
    DisplayCharacteristics& mutable_characteristics() { return characteristics_; }

private:
    LCDPreset              preset_;
    const char*            name_;
    const char*            id_;
    video_signal_mask_t    accepted_signals_;
    DisplayCharacteristics characteristics_;
    bool                   has_speakers_;
};
