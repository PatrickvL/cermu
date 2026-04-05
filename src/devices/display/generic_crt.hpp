#pragma once
/**
 * generic_crt.hpp — Generic CRT monitor peripheral device
 *
 * A configurable CRT display device that models common consumer and
 * professional monitors of the 8-bit era.  A CRTPreset selects a bundle
 * of display characteristics and accepted signal types.
 *
 * Presets cover the most common real-world monitor categories:
 *   - Consumer TV (NTSC)    (13" composite, shadow mask, speaker)
 *   - Consumer TV (RF)      (13" RF modulator path — Atari 2600 class)
 *   - Consumer TV (PAL)     (14" PAL composite, shadow mask, speaker)
 *   - Commodore 1702        (composite + S-Video, shadow mask, speaker)
 *   - Commodore 1902A       (composite + S-Video + RGBI, shadow mask, speaker)
 *   - RGB monitor           (composite + S-Video + RGB, aperture grille)
 *   - Philips CM8833        (14" RGB + composite, shadowmask, stereo)
 *   - Microvitec Cub 1431   (13.2" TTL/analogue RGB, 0.64 mm)
 *   - Amstrad CTM644        (14" RGB, 6-pin DIN, speakers)
 *   - Amstrad GT65          (12" P31 green monochrome)
 *   - Monochrome green      (composite, P31 phosphor, 525 nm)
 *   - Monochrome amber      (composite, P3 phosphor, 590 nm)
 *   - PET 2001 white        (9" integrated P4 white)
 *   - PET 4000/8000 green   (12" integrated P31 green, 6845 CRTC)
 *   - Soviet mono TV        (Junost portable B&W — KC85 canonical)
 *   - Vector monitor        (19" XY P31 green — Atari arcade)
 *   - Arcade monitor        (19" RGB shadowmask — Sanyo/Nanao class)
 *   - Direct output         (flat display, no CRT effects)
 */

#include "devices/display/display_device.hpp"

// ============================================================================
// CRT PRESETS
// ============================================================================

/// Predefined CRT monitor configurations.
enum class CRTPreset : uint8_t {
    // --- Consumer TVs ---
    ConsumerTV,         ///< 13" NTSC composite TV with built-in speaker
    ConsumerTV_RF,      ///< 13" CRT TV via RF modulator (Atari 2600 class)
    ConsumerTV_PAL,     ///< 14" PAL composite TV with built-in speaker

    // --- Commodore family ---
    Commodore1702,      ///< Commodore 1702 — composite + S-Video, speaker
    Commodore1902,      ///< Commodore 1902A — composite + S-Video + RGBI, speaker
    CommodorePET_White, ///< PET 2001 integrated 9" white P4 monitor
    CommodorePET_Green, ///< PET 4000/8000 integrated 12" green P31 monitor

    // --- RGB / professional monitors ---
    RGBMonitor,         ///< Professional RGB monitor (composite + S-Video + RGB)
    PhilipsCM8833,      ///< Philips CM8833-II — 14", RGB + composite, stereo
    MicrovitecCub1431,  ///< Microvitec Cub 1431 — 13.2", TTL/analogue RGB (BBC)
    AmstradCTM644,      ///< Amstrad CTM644 — 14", 6-pin DIN RGB, speakers
    AmstradGT65,        ///< Amstrad GT65 — 12", P31 green monochrome

    // --- Monochrome ---
    MonochromeGreen,    ///< Green phosphor monochrome (P31, 525 nm)
    MonochromeAmber,    ///< Amber phosphor monochrome (P3, 590 nm)

    // --- Specialist ---
    SovietMonoTV,       ///< Junost portable Soviet B&W TV (KC85 canonical)
    VectorMonitor,      ///< Atari vector arcade XY monitor (P31 green)
    ArcadeMonitor,      ///< 19" arcade RGB shadowmask (Sanyo/Nanao class)

    // --- Passthrough ---
    DirectOutput,       ///< Flat/direct output — no CRT effects
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