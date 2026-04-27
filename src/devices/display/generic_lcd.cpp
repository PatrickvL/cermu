/**
 * generic_lcd.cpp â€” LCD panel presets and device registration
 *
 * Hardware-accurate LCD display characteristics for each preset.
 * Values are based on panel datasheets, teardown measurements, and
 * community documentation of each handheld's display properties.
 */

#include "devices/display/generic_lcd.hpp"
#include "core/device_registry.hpp"

// ============================================================================
// CONSTRUCTOR â€” preset switch
// ============================================================================

GenericLCD::GenericLCD(LCDPreset preset) : preset_(preset) {
    switch (preset) {

    // ========================================================================
    // GAME BOY DMG â€” Original Game Boy (DOT MATRIX GAME)
    // Sharp 160Ã—144 STN reflective LCD, no backlight, 4 shades of green
    // ========================================================================
    case LCDPreset::GameBoyDMG:
        name_             = "Game Boy DMG LCD";
        id_               = "lcd_panel";  // default ID for Game Boy manifest
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.6f,
            .aspect      = 10.0f / 9.0f,
            .dot_pitch   = 0.36f,
            .brightness  = 0.60f,  // reflective, no backlight â€” dimmer
            .contrast    = 0.80f,
            .gamma       = 2.20f,
            .color_temp  = 5500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::STN,
                .backlight_type   = LCDBacklightType::None,
                .subpixel_layout  = LCDSubpixelLayout::Monochrome,

                .pixel_grid_opacity  = 0.45f,  // very visible grid on DMG
                .pixel_grid_width    = 0.18f,
                .subpixel_opacity    = 0.0f,   // monochrome â€” no subpixels

                .response_time_ms    = 80.0f,  // STN is very slow
                .ghosting_strength   = 0.35f,  // significant ghosting

                .backlight_brightness = 0.0f,  // no backlight
                .backlight_bleed      = 0.0f,
                .backlight_uniformity = 1.0f,

                .color_tint       = {0.60f, 0.74f, 0.06f},  // iconic DMG green
                .color_saturation = 0.0f,   // monochrome

                .viewing_angle_falloff = 0.3f,  // STN has limited viewing angle
                .black_level           = 0.05f,
                .reflection_strength   = 0.15f, // reflective panel catches ambient light
                .pixel_pitch_mm        = 0.36f,
            },
        };
        break;

    // ========================================================================
    // GAME BOY POCKET â€” MGB-001
    // Sharp 160Ã—144 STN reflective, improved contrast, grey-scale
    // ========================================================================
    case LCDPreset::GameBoyPocket:
        name_             = "Game Boy Pocket LCD";
        id_               = "lcd_gbp";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.6f,
            .aspect      = 10.0f / 9.0f,
            .dot_pitch   = 0.36f,
            .brightness  = 0.65f,
            .contrast    = 0.90f,
            .gamma       = 2.20f,
            .color_temp  = 6000.0f,
            .lcd = {
                .panel_type       = LCDPanelType::STN,
                .backlight_type   = LCDBacklightType::None,
                .subpixel_layout  = LCDSubpixelLayout::Monochrome,

                .pixel_grid_opacity  = 0.40f,
                .pixel_grid_width    = 0.16f,
                .subpixel_opacity    = 0.0f,

                .response_time_ms    = 60.0f,  // improved over DMG
                .ghosting_strength   = 0.25f,

                .backlight_brightness = 0.0f,
                .backlight_bleed      = 0.0f,
                .backlight_uniformity = 1.0f,

                .color_tint       = {0.82f, 0.85f, 0.78f},  // neutral grey-green
                .color_saturation = 0.0f,

                .viewing_angle_falloff = 0.25f,
                .black_level           = 0.04f,
                .reflection_strength   = 0.12f,
                .pixel_pitch_mm        = 0.36f,
            },
        };
        break;

    // ========================================================================
    // GAME BOY LIGHT â€” MGL-001 (Japan only)
    // STN with electroluminescent backlight, monochrome green
    // ========================================================================
    case LCDPreset::GameBoyLight:
        name_             = "Game Boy Light LCD";
        id_               = "lcd_gbl";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.6f,
            .aspect      = 10.0f / 9.0f,
            .dot_pitch   = 0.36f,
            .brightness  = 0.75f,
            .contrast    = 0.85f,
            .gamma       = 2.20f,
            .color_temp  = 5200.0f,
            .lcd = {
                .panel_type       = LCDPanelType::STN,
                .backlight_type   = LCDBacklightType::Electroluminescent,
                .subpixel_layout  = LCDSubpixelLayout::Monochrome,

                .pixel_grid_opacity  = 0.40f,
                .pixel_grid_width    = 0.16f,
                .subpixel_opacity    = 0.0f,

                .response_time_ms    = 60.0f,
                .ghosting_strength   = 0.25f,

                .backlight_brightness = 0.70f,  // EL panel â€” softer than LED
                .backlight_bleed      = 0.10f,
                .backlight_uniformity = 0.85f,  // EL not perfectly even

                .color_tint       = {0.55f, 0.78f, 0.55f},  // EL green tint
                .color_saturation = 0.0f,

                .viewing_angle_falloff = 0.25f,
                .black_level           = 0.03f,
                .reflection_strength   = 0.08f,
                .pixel_pitch_mm        = 0.36f,
            },
        };
        break;

    // ========================================================================
    // GAME BOY COLOR â€” CGB-001
    // Sharp 160Ã—144 TN reflective, color (32768 colors, ~56 on screen)
    // ========================================================================
    case LCDPreset::GameBoyColor:
        name_             = "Game Boy Color LCD";
        id_               = "lcd_gbc";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.6f,
            .aspect      = 10.0f / 9.0f,
            .dot_pitch   = 0.30f,
            .brightness  = 0.55f,  // reflective color â€” dimmer than mono
            .contrast    = 0.70f,
            .gamma       = 2.20f,
            .color_temp  = 5800.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::None,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.35f,
                .pixel_grid_width    = 0.14f,
                .subpixel_opacity    = 0.25f,  // visible subpixels at scale

                .response_time_ms    = 40.0f,
                .ghosting_strength   = 0.15f,

                .backlight_brightness = 0.0f,
                .backlight_bleed      = 0.0f,
                .backlight_uniformity = 1.0f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.75f,  // limited color gamut

                .viewing_angle_falloff = 0.35f,  // TN â€” significant
                .black_level           = 0.06f,
                .reflection_strength   = 0.15f,
                .pixel_pitch_mm        = 0.30f,
            },
        };
        break;

    // ========================================================================
    // GAME BOY ADVANCE â€” AGB-001
    // Sharp 240Ã—160 TN reflective, 32768 colors
    // ========================================================================
    case LCDPreset::GameBoyAdvance:
        name_             = "Game Boy Advance LCD";
        id_               = "lcd_gba";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.9f,
            .aspect      = 3.0f / 2.0f,
            .dot_pitch   = 0.24f,
            .brightness  = 0.50f,
            .contrast    = 0.75f,
            .gamma       = 2.20f,
            .color_temp  = 5800.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::None,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.30f,
                .pixel_grid_width    = 0.12f,
                .subpixel_opacity    = 0.20f,

                .response_time_ms    = 30.0f,
                .ghosting_strength   = 0.10f,

                .backlight_brightness = 0.0f,
                .backlight_bleed      = 0.0f,
                .backlight_uniformity = 1.0f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.80f,

                .viewing_angle_falloff = 0.30f,
                .black_level           = 0.05f,
                .reflection_strength   = 0.15f,
                .pixel_pitch_mm        = 0.24f,
            },
        };
        break;

    // ========================================================================
    // GBA SP AGS-001 â€” frontlit
    // ========================================================================
    case LCDPreset::GBASP_AGS001:
        name_             = "GBA SP AGS-001 LCD";
        id_               = "lcd_gbasp001";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.9f,
            .aspect      = 3.0f / 2.0f,
            .dot_pitch   = 0.24f,
            .brightness  = 0.70f,
            .contrast    = 0.80f,
            .gamma       = 2.20f,
            .color_temp  = 5500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::Frontlight,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.25f,
                .pixel_grid_width    = 0.12f,
                .subpixel_opacity    = 0.15f,

                .response_time_ms    = 25.0f,
                .ghosting_strength   = 0.08f,

                .backlight_brightness = 0.65f,
                .backlight_bleed      = 0.15f,  // frontlight has visible bleed
                .backlight_uniformity = 0.75f,  // frontlight fairly uneven

                .color_tint       = {0.95f, 0.98f, 1.0f},
                .color_saturation = 0.80f,

                .viewing_angle_falloff = 0.30f,
                .black_level           = 0.04f,
                .reflection_strength   = 0.10f,
                .pixel_pitch_mm        = 0.24f,
            },
        };
        break;

    // ========================================================================
    // GBA SP AGS-101 â€” LED backlit, best GBA screen
    // ========================================================================
    case LCDPreset::GBASP_AGS101:
        name_             = "GBA SP AGS-101 LCD";
        id_               = "lcd_gbasp101";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 2.9f,
            .aspect      = 3.0f / 2.0f,
            .dot_pitch   = 0.24f,
            .brightness  = 1.0f,
            .contrast    = 1.0f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::LED,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.20f,
                .pixel_grid_width    = 0.10f,
                .subpixel_opacity    = 0.12f,

                .response_time_ms    = 20.0f,
                .ghosting_strength   = 0.05f,

                .backlight_brightness = 1.0f,
                .backlight_bleed      = 0.05f,
                .backlight_uniformity = 0.90f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.90f,

                .viewing_angle_falloff = 0.25f,
                .black_level           = 0.03f,
                .reflection_strength   = 0.05f,
                .pixel_pitch_mm        = 0.24f,
            },
        };
        break;

    // ========================================================================
    // NINTENDO DS â€” top screen
    // ========================================================================
    case LCDPreset::NintendoDS:
        name_             = "Nintendo DS LCD";
        id_               = "lcd_ds";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 3.0f,
            .aspect      = 256.0f / 192.0f,
            .dot_pitch   = 0.21f,
            .brightness  = 0.90f,
            .contrast    = 0.95f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::LED,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.15f,
                .pixel_grid_width    = 0.08f,
                .subpixel_opacity    = 0.10f,

                .response_time_ms    = 15.0f,
                .ghosting_strength   = 0.03f,

                .backlight_brightness = 0.90f,
                .backlight_bleed      = 0.05f,
                .backlight_uniformity = 0.90f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.90f,

                .viewing_angle_falloff = 0.20f,
                .black_level           = 0.02f,
                .reflection_strength   = 0.04f,
                .pixel_pitch_mm        = 0.21f,
            },
        };
        break;

    // ========================================================================
    // SEGA GAME GEAR
    // Asahi 160Ã—144 STN with CCFL backlight, 4096 colors
    // ========================================================================
    case LCDPreset::GameGear:
        name_             = "Game Gear LCD";
        id_               = "lcd_gamegear";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 3.2f,
            .aspect      = 10.0f / 9.0f,
            .dot_pitch   = 0.34f,
            .brightness  = 0.80f,
            .contrast    = 0.75f,
            .gamma       = 2.20f,
            .color_temp  = 6000.0f,
            .lcd = {
                .panel_type       = LCDPanelType::STN,
                .backlight_type   = LCDBacklightType::CCFL,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.35f,
                .pixel_grid_width    = 0.15f,
                .subpixel_opacity    = 0.20f,

                .response_time_ms    = 50.0f,
                .ghosting_strength   = 0.20f,

                .backlight_brightness = 0.85f,
                .backlight_bleed      = 0.10f,
                .backlight_uniformity = 0.80f,  // CCFL not great

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.70f,

                .viewing_angle_falloff = 0.35f,
                .black_level           = 0.05f,
                .reflection_strength   = 0.08f,
                .pixel_pitch_mm        = 0.34f,
            },
        };
        break;

    // ========================================================================
    // ATARI LYNX
    // STN with CCFL backlight, 4096 colors, 160Ã—102
    // ========================================================================
    case LCDPreset::AtariLynx:
        name_             = "Atari Lynx LCD";
        id_               = "lcd_lynx";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 3.5f,
            .aspect      = 160.0f / 102.0f,
            .dot_pitch   = 0.38f,
            .brightness  = 0.75f,
            .contrast    = 0.70f,
            .gamma       = 2.20f,
            .color_temp  = 5800.0f,
            .lcd = {
                .panel_type       = LCDPanelType::STN,
                .backlight_type   = LCDBacklightType::CCFL,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.35f,
                .pixel_grid_width    = 0.16f,
                .subpixel_opacity    = 0.18f,

                .response_time_ms    = 55.0f,
                .ghosting_strength   = 0.22f,

                .backlight_brightness = 0.80f,
                .backlight_bleed      = 0.12f,
                .backlight_uniformity = 0.78f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.65f,

                .viewing_angle_falloff = 0.35f,
                .black_level           = 0.05f,
                .reflection_strength   = 0.08f,
                .pixel_pitch_mm        = 0.38f,
            },
        };
        break;

    // ========================================================================
    // PSP 1000
    // Sharp 480Ã—272 TN with CCFL backlight, 16.77M colors
    // ========================================================================
    case LCDPreset::PSP1000:
        name_             = "PSP 1000 LCD";
        id_               = "lcd_psp1000";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 4.3f,
            .aspect      = 480.0f / 272.0f,
            .dot_pitch   = 0.165f,
            .brightness  = 0.95f,
            .contrast    = 0.95f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::TN,
                .backlight_type   = LCDBacklightType::CCFL,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.10f,
                .pixel_grid_width    = 0.06f,
                .subpixel_opacity    = 0.08f,

                .response_time_ms    = 12.0f,
                .ghosting_strength   = 0.02f,

                .backlight_brightness = 0.95f,
                .backlight_bleed      = 0.08f,
                .backlight_uniformity = 0.85f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.92f,

                .viewing_angle_falloff = 0.20f,
                .black_level           = 0.02f,
                .reflection_strength   = 0.04f,
                .pixel_pitch_mm        = 0.165f,
            },
        };
        break;

    // ========================================================================
    // NINTENDO SWITCH
    // 6.2" IPS with LED backlight, 1280Ã—720
    // ========================================================================
    case LCDPreset::NintendoSwitch:
        name_             = "Nintendo Switch LCD";
        id_               = "lcd_switch";
        accepted_signals_ = VideoSignalMask::Digital;
        has_speakers_     = true;
        characteristics_  = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 6.2f,
            .aspect      = 16.0f / 9.0f,
            .dot_pitch   = 0.10f,
            .brightness  = 1.0f,
            .contrast    = 1.0f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .lcd = {
                .panel_type       = LCDPanelType::IPS,
                .backlight_type   = LCDBacklightType::LED,
                .subpixel_layout  = LCDSubpixelLayout::RGBStripe,

                .pixel_grid_opacity  = 0.05f,
                .pixel_grid_width    = 0.04f,
                .subpixel_opacity    = 0.04f,

                .response_time_ms    = 8.0f,
                .ghosting_strength   = 0.01f,

                .backlight_brightness = 1.0f,
                .backlight_bleed      = 0.03f,
                .backlight_uniformity = 0.95f,

                .color_tint       = {1.0f, 1.0f, 1.0f},
                .color_saturation = 0.95f,

                .viewing_angle_falloff = 0.05f,  // IPS â€” excellent viewing angles
                .black_level           = 0.015f,
                .reflection_strength   = 0.03f,
                .pixel_pitch_mm        = 0.10f,
            },
        };
        break;
    }
}

// ============================================================================
// DEVICE REGISTRATION
// ============================================================================

// --- Game Boy DMG LCD (default "lcd_panel" ID for gameboy manifest) ---
static const DeviceDescriptor lcd_dmg_desc{
    "lcd_panel", "Game Boy DMG LCD",
    "Sharp STN reflective LCD â€” 160×144, 4 shades of green, 80 ms response",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_dmg_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameBoyDMG); }
)

// --- Game Boy Pocket ---
static const DeviceDescriptor lcd_gbp_desc{
    "lcd_gbp", "Game Boy Pocket LCD",
    "Sharp STN reflective LCD â€” 160×144, grey-scale, improved contrast",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gbp_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameBoyPocket); }
)

// --- Game Boy Light ---
static const DeviceDescriptor lcd_gbl_desc{
    "lcd_gbl", "Game Boy Light LCD",
    "STN LCD with EL backlight â€” 160×144, monochrome green (Japan only)",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gbl_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameBoyLight); }
)

// --- Game Boy Color ---
static const DeviceDescriptor lcd_gbc_desc{
    "lcd_gbc", "Game Boy Color LCD",
    "Sharp TN reflective LCD â€” 160×144, 32768 colors",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gbc_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameBoyColor); }
)

// --- Game Boy Advance ---
static const DeviceDescriptor lcd_gba_desc{
    "lcd_gba", "Game Boy Advance LCD",
    "Sharp TN reflective LCD â€” 240×160, 32768 colors",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gba_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameBoyAdvance); }
)

// --- GBA SP AGS-001 ---
static const DeviceDescriptor lcd_gbasp001_desc{
    "lcd_gbasp001", "GBA SP AGS-001 LCD",
    "TN LCD with frontlight â€” 240×160, first clamshell GBA",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gbasp001_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GBASP_AGS001); }
)

// --- GBA SP AGS-101 ---
static const DeviceDescriptor lcd_gbasp101_desc{
    "lcd_gbasp101", "GBA SP AGS-101 LCD",
    "TN LCD with LED backlight â€” 240×160, best GBA screen",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gbasp101_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GBASP_AGS101); }
)

// --- Nintendo DS ---
static const DeviceDescriptor lcd_ds_desc{
    "lcd_ds", "Nintendo DS LCD",
    "TN LCD with LED backlight â€” 256×192",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_ds_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::NintendoDS); }
)

// --- Game Gear ---
static const DeviceDescriptor lcd_gamegear_desc{
    "lcd_gamegear", "Game Gear LCD",
    "Asahi STN LCD with CCFL backlight â€” 160×144, 4096 colors",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_gamegear_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::GameGear); }
)

// --- Atari Lynx ---
static const DeviceDescriptor lcd_lynx_desc{
    "lcd_lynx", "Atari Lynx LCD",
    "STN LCD with CCFL backlight â€” 160×102, 4096 colors",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_lynx_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::AtariLynx); }
)

// --- PSP 1000 ---
static const DeviceDescriptor lcd_psp1000_desc{
    "lcd_psp1000", "PSP 1000 LCD",
    "Sharp TN LCD with CCFL backlight â€” 480×272, 16.77M colors",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_psp1000_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::PSP1000); }
)

// --- Nintendo Switch ---
static const DeviceDescriptor lcd_switch_desc{
    "lcd_switch", "Nintendo Switch LCD",
    "IPS LCD with LED backlight â€” 1280×720, wide color gamut",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(lcd_switch_desc,
    []() { return std::make_unique<GenericLCD>(LCDPreset::NintendoSwitch); }
)
