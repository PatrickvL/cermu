/**
 * generic_crt.cpp — Generic CRT monitor implementation and registration
 */

#include "devices/display/generic_crt.hpp"
#include "core/device_registry.hpp"

// ============================================================================
// CONSTRUCTION
// ============================================================================

GenericCRT::GenericCRT(CRTPreset preset)
    : preset_(preset)
{
    switch (preset) {
        case CRTPreset::ConsumerTV:
            name_              = "Color TV";
            id_                = "crt_tv";
            primary_port_type_ = PortType::VIDEO_COMPOSITE;
            accepted_signals_  = DisplaySignals::COMPOSITE;
            has_speakers_      = true;
            characteristics_   = {
                DisplayTechnology::CRT_Shadow, PhosphorType::P22,
                /*diagonal*/ 13.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.4f,
                /*persistence*/ 2.0f, /*color_temp*/ 6500.0f,
                /*dot_pitch*/ 0.42f, /*scanline_gap*/ 0.15f,
                /*brightness*/ 1.0f, /*contrast*/ 1.0f, /*gamma*/ 2.2f
            };
            break;

        case CRTPreset::Commodore1702:
            name_              = "Commodore 1702";
            id_                = "crt_1702";
            primary_port_type_ = PortType::VIDEO_COMPOSITE;
            accepted_signals_  = DisplaySignals::COMPOSITE_SVIDEO;
            has_speakers_      = true;
            characteristics_   = {
                DisplayTechnology::CRT_Shadow, PhosphorType::P22,
                /*diagonal*/ 13.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.35f,
                /*persistence*/ 2.0f, /*color_temp*/ 6500.0f,
                /*dot_pitch*/ 0.31f, /*scanline_gap*/ 0.12f,
                /*brightness*/ 1.0f, /*contrast*/ 1.0f, /*gamma*/ 2.2f
            };
            break;

        case CRTPreset::Commodore1902:
            name_              = "Commodore 1902";
            id_                = "crt_1902";
            primary_port_type_ = PortType::VIDEO_RGBI;
            accepted_signals_  = DisplaySignals::COMPOSITE | DisplaySignals::SVIDEO
                               | DisplaySignals::RGBI;
            has_speakers_      = true;
            characteristics_   = {
                DisplayTechnology::CRT_Shadow, PhosphorType::P22,
                /*diagonal*/ 13.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.32f,
                /*persistence*/ 2.0f, /*color_temp*/ 6500.0f,
                /*dot_pitch*/ 0.31f, /*scanline_gap*/ 0.10f,
                /*brightness*/ 1.0f, /*contrast*/ 1.0f, /*gamma*/ 2.2f
            };
            break;

        case CRTPreset::RGBMonitor:
            name_              = "RGB Monitor";
            id_                = "crt_rgb";
            primary_port_type_ = PortType::VIDEO_RGB;
            accepted_signals_  = DisplaySignals::COMPOSITE | DisplaySignals::SVIDEO
                               | DisplaySignals::RGB;
            has_speakers_      = false;
            characteristics_   = {
                DisplayTechnology::CRT_Aperture, PhosphorType::P22,
                /*diagonal*/ 14.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.15f,
                /*persistence*/ 1.5f, /*color_temp*/ 9300.0f,
                /*dot_pitch*/ 0.25f, /*scanline_gap*/ 0.08f,
                /*brightness*/ 1.1f, /*contrast*/ 1.0f, /*gamma*/ 2.2f
            };
            break;

        case CRTPreset::MonochromeGreen:
            name_              = "Green Monitor";
            id_                = "crt_green";
            primary_port_type_ = PortType::VIDEO_COMPOSITE;
            accepted_signals_  = DisplaySignals::COMPOSITE;
            has_speakers_      = false;
            characteristics_   = {
                DisplayTechnology::CRT_Monochrome, PhosphorType::P31,
                /*diagonal*/ 12.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.3f,
                /*persistence*/ 3.0f, /*color_temp*/ 5500.0f,
                /*dot_pitch*/ 0.31f, /*scanline_gap*/ 0.2f,
                /*brightness*/ 1.0f, /*contrast*/ 1.2f, /*gamma*/ 2.5f
            };
            break;

        case CRTPreset::MonochromeAmber:
            name_              = "Amber Monitor";
            id_                = "crt_amber";
            primary_port_type_ = PortType::VIDEO_COMPOSITE;
            accepted_signals_  = DisplaySignals::COMPOSITE;
            has_speakers_      = false;
            characteristics_   = {
                DisplayTechnology::CRT_Monochrome, PhosphorType::Custom,
                /*diagonal*/ 12.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.3f,
                /*persistence*/ 3.0f, /*color_temp*/ 4000.0f,
                /*dot_pitch*/ 0.31f, /*scanline_gap*/ 0.2f,
                /*brightness*/ 1.0f, /*contrast*/ 1.2f, /*gamma*/ 2.5f
            };
            break;

        case CRTPreset::DirectOutput:
            name_              = "Direct Output";
            id_                = "direct_output";
            primary_port_type_ = PortType::VIDEO_COMPOSITE;
            accepted_signals_  = DisplaySignals::ALL_RASTER;
            has_speakers_      = false;
            characteristics_   = {
                DisplayTechnology::LCD, PhosphorType::P22,
                /*diagonal*/ 24.0f, /*aspect*/ 4.0f / 3.0f, /*curvature*/ 0.0f,
                /*persistence*/ 0.0f, /*color_temp*/ 6500.0f,
                /*dot_pitch*/ 0.0f, /*scanline_gap*/ 0.0f,
                /*brightness*/ 1.0f, /*contrast*/ 1.0f, /*gamma*/ 2.2f
            };
            break;
    }
}

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor crt_tv_descriptor = {
    "crt_tv",
    "Color TV",
    "Generic 13\" color CRT television (composite, built-in speaker)",
    PortType::VIDEO_COMPOSITE,
    false
};

static const DeviceDescriptor crt_1702_descriptor = {
    "crt_1702",
    "Commodore 1702",
    "Commodore 1702 monitor (composite + S-Video, built-in speaker)",
    PortType::VIDEO_COMPOSITE,
    false
};

static const DeviceDescriptor crt_rgb_descriptor = {
    "crt_rgb",
    "RGB Monitor",
    "Professional RGB monitor (composite + S-Video + RGB, aperture grille)",
    PortType::VIDEO_RGB,
    false
};

static const DeviceDescriptor crt_green_descriptor = {
    "crt_green",
    "Green Monitor",
    "Monochrome green phosphor CRT (P31)",
    PortType::VIDEO_COMPOSITE,
    false
};

static const DeviceDescriptor crt_amber_descriptor = {
    "crt_amber",
    "Amber Monitor",
    "Monochrome amber phosphor CRT",
    PortType::VIDEO_COMPOSITE,
    false
};

REGISTER_DEVICE(crt_tv_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::ConsumerTV);
})

REGISTER_DEVICE(crt_1702_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::Commodore1702);
})

static const DeviceDescriptor crt_1902_descriptor = {
    "crt_1902",
    "Commodore 1902",
    "Commodore 1902 monitor (composite + S-Video + RGBI, built-in speakers)",
    PortType::VIDEO_RGBI,
    false
};

REGISTER_DEVICE(crt_1902_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::Commodore1902);
})

REGISTER_DEVICE(crt_rgb_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::RGBMonitor);
})

REGISTER_DEVICE(crt_green_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::MonochromeGreen);
})

REGISTER_DEVICE(crt_amber_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::MonochromeAmber);
})

static const DeviceDescriptor direct_output_descriptor = {
    "direct_output",
    "Direct Output",
    "Flat display — clean output with no CRT effects",
    PortType::VIDEO_COMPOSITE,
    false
};

REGISTER_DEVICE(direct_output_descriptor, []() {
    return std::make_unique<GenericCRT>(CRTPreset::DirectOutput);
})
