#pragma once
/**
 * generic_crt.hpp — Generic CRT monitor peripheral device
 *
 * A configurable CRT display device that models common consumer and
 * professional monitors of the 8-bit era.  A CRTPreset selects a bundle
 * of display characteristics and accepted signal types.
 *
 * Presets cover the most common real-world monitor categories:
 *   - Consumer TV        (composite, shadow mask, built-in speaker)
 *   - Commodore 1702     (composite + S-Video, shadow mask, speaker)
 *   - Commodore 1902A    (composite + S-Video + RGBI, shadow mask, speaker)
 *   - RGB monitor        (composite + S-Video + RGB, aperture grille)
 *   - Monochrome green   (composite, P31 phosphor, 525 nm)
 *   - Monochrome amber   (composite, P3 phosphor, 590 nm)
 *   - Direct output      (flat display, no CRT effects)
 */

#include "devices/display/display_device.hpp"

// ============================================================================
// CRT PRESETS
// ============================================================================

/// Predefined CRT monitor configurations.
enum class CRTPreset : uint8_t {
    ConsumerTV,       ///< 13" composite TV with built-in speaker
    Commodore1702,    ///< Commodore 1702 — composite + S-Video, speaker
    Commodore1902,    ///< Commodore 1902A — composite + S-Video + RGBI, speaker
    RGBMonitor,       ///< Professional RGB monitor (composite + S-Video + RGB)
    MonochromeGreen,  ///< Green phosphor monochrome (P31, 525 nm)
    MonochromeAmber,  ///< Amber phosphor monochrome (P3, 590 nm)
    DirectOutput,     ///< Flat/direct output — no CRT effects
};

// ============================================================================
// GENERIC CRT DEVICE
// ============================================================================

class GenericCRT : public DisplayDevice {
public:
    explicit GenericCRT(CRTPreset preset = CRTPreset::ConsumerTV);
    ~GenericCRT() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return name_; }
    const char* get_id() const override   { return id_; }
    PortType get_port_type() const override { return primary_port_type_; }

    // --- DisplayDevice interface ---------------------------------------
    video_signal_mask_t get_accepted_video_signals() const override { return accepted_signals_; }
    const DisplayCharacteristics& get_display_characteristics() const override { return characteristics_; }
    bool has_builtin_speakers() const override { return has_speakers_; }

    // --- Preset query --------------------------------------------------
    CRTPreset get_preset() const { return preset_; }

    /// Mutable access to display characteristics (for GUI tuning).
    DisplayCharacteristics& mutable_characteristics() { return characteristics_; }

private:
    CRTPreset              preset_;
    const char*            name_;
    const char*            id_;
    PortType               primary_port_type_;
    video_signal_mask_t    accepted_signals_;
    DisplayCharacteristics characteristics_;
    bool                   has_speakers_;
};