/**
 * generic_crt.cpp — Generic CRT monitor implementation and registration
 *
 * All DisplayCharacteristics values are grounded in hardware measurements,
 * service manual specifications, and phosphor data sheets.
 *
 * Primary sources:
 *   Commodore 1702 service manual (Commodore, 1983) — Hitachi tube,
 *     0.31 mm dot pitch, S-Video luma bandwidth ~3.0 MHz
 *   Commodore 1902A service manual (Commodore, 1985) — 14", 0.28 mm,
 *     composite + S-Video + RGBI, C128 80-column compatible
 *   Sony KX-14CP1 "Black Trinitron" — 14", 0.25 mm aperture grille,
 *     9300K, RGB + composite; canonical MSX/Amiga RGB monitor
 *   JEDEC phosphor data sheets: P22 (color TV), P31 (green 525 nm),
 *     P3 (amber 590 nm), P4 (white)
 *   SMPTE 170M — NTSC composite luma limit 4.2 MHz, receiver gamma 2.2
 *   IBM 5151 technical reference (IBM, 1981) — P3 phosphor, 12" mono
 *   Apple Monitor II specification — bandwidth < 3 dB at 12 MHz
 *   CRT restoration community measurements (retrorgb.com, shmups.system11.org)
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

    // =========================================================================
    //  CONSUMER TV
    //  Used by: NES, Sega SG-1000/SMS, ColecoVision, VIC-20 (TV), C64 (TV),
    //           C128 (TV), Atari 2600, ZX Spectrum (RF), Amstrad CPC (RF),
    //           BBC Micro (domestic), Oric, VTech VZ, MSX (composite), and
    //           most other systems whose primary domestic output was a TV set.
    //
    //  Representative: generic 13" NTSC/PAL consumer set, 1980–1990.
    //  NES PPU outputs raw composite with no chroma pre-filter, producing
    //  characteristic dot-crawl and color bleed through this path.
    // =========================================================================
    case CRTPreset::ConsumerTV:
        name_              = "Color TV";
        id_                = "crt_tv";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 13.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.42f,   // mm; typical 13" consumer shadowmask
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,   // NTSC standard receiver gamma (SMPTE 170M)
            .color_temp  = 6500.0f, // K; D65 target
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.96f, 0.88f},
                .persistence     = 2.0f,    // ms; P22 JEDEC exponential decay
                .bloom_radius    = 1.80f,
                .bloom_threshold = 0.62f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.90f,
                .softness          = 0.55f,
                .pincushion        = 0.05f,
                .h_linearity       = 0.97f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.65f, 0.45f}, // typical 3-gun edge error
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.38f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.16f,
                .strength  = 0.55f,
                .phase     = 0.00f,
                .interlace = true,  // 525/625-line interlaced broadcast standard
            },
            .optics = {
                .curvature           = 0.40f,
                .vignette_strength   = 0.35f,
                .reflection_strength = 0.14f,
                .edge_glow           = 0.09f,
                .glass_tint          = {0.91f, 0.97f, 0.89f}, // Fe₂O₃ green-tinted leaded glass
            },
            .signal = {
                .bandwidth          = 4.20f,  // MHz; NTSC FCC luma limit (SMPTE 170M)
                .noise_level        = 0.042f,
                .hum_bar_strength   = 0.055f,
                .ghosting_strength  = 0.18f,  // multi-path RF/composite echo
                .chroma_phase_error = 3.50f,  // degrees; composite decoder drift
                .sync_stability     = 0.72f,
            },
        };
        break;

    // =========================================================================
    //  COMMODORE 1702
    //  Used by: C64 (primary), VIC-20, C16/Plus4/C116, C128
    //
    //  Service manual (Commodore, 1983): Hitachi 13" shadowmask tube,
    //  0.31 mm dot pitch. Composite + S-Video inputs. Factory white point 6500K.
    //  S-Video luma path: ~3.0 MHz (binding limit per spec sheet, not the
    //  composite spec). Chroma: ~0.6 MHz. Factory convergence tolerance:
    //  ≤0.5 mm at screen edges. C64/VIC-II drives 240p progressive.
    // =========================================================================
    case CRTPreset::Commodore1702:
        name_              = "Commodore 1702";
        id_                = "crt_1702";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite | VideoSignalMask::SVideo;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 13.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.31f,   // mm; Hitachi tube, service manual p.6
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.97f, 0.92f},
                .persistence     = 2.0f,
                .bloom_radius    = 1.40f,
                .bloom_threshold = 0.68f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.72f,
                .softness          = 0.40f,
                .pincushion        = 0.025f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.30f, 0.20f}, // ≤0.5 mm at edge; service manual
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.28f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.13f,
                .strength  = 0.45f,
                .phase     = 0.00f,
                .interlace = false, // C64/VIC-II: 240p progressive
            },
            .optics = {
                .curvature           = 0.35f,
                .vignette_strength   = 0.22f,
                .reflection_strength = 0.09f,
                .edge_glow           = 0.05f,
                .glass_tint          = {0.93f, 0.97f, 0.92f}, // mild green glass tint
            },
            .signal = {
                .bandwidth          = 3.00f,  // MHz; S-Video luma path per spec sheet
                .noise_level        = 0.022f,
                .hum_bar_strength   = 0.018f, // low: S-Video bypasses composite decode
                .ghosting_strength  = 0.04f,
                .chroma_phase_error = 1.20f,
                .sync_stability     = 0.93f,
            },
        };
        break;

    // =========================================================================
    //  COMMODORE 1902A
    //  Used by: C128 (80-column RGBI mode, primary), C64 (composite/S-Video),
    //           VIC-20, C16/Plus4 (composite path)
    //
    //  Service manual (Commodore, 1985): 14" shadowmask, 0.28 mm dot pitch.
    //  Inputs: composite, S-Video, and RGBI (TTL 4-bit, for C128 VDC chip).
    //  RGBI path bypasses composite decode entirely — signal parameters below
    //  reflect the RGBI mode. C128 VDC at 80 columns requires ~16 MHz dot
    //  clock; the 1902A RGBI analogue path handles this comfortably.
    //  Color temp factory-set ~6500K. Has built-in speaker.
    // =========================================================================
    case CRTPreset::Commodore1902:
        name_              = "Commodore 1902A";
        id_                = "crt_1902";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::Composite | VideoSignalMask::SVideo
                           | VideoSignalMask::RGBI;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 14.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.28f,   // mm; service manual; finer than 1702
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.97f, 0.93f},
                .persistence     = 2.0f,
                .bloom_radius    = 1.20f,
                .bloom_threshold = 0.70f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.65f,
                .softness          = 0.35f,
                .pincushion        = 0.020f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.22f, 0.16f}, // tighter than 1702; 14" in-line gun
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.24f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.11f,
                .strength  = 0.38f,
                .phase     = 0.00f,
                .interlace = false, // C128 VDC 80-col: 480-line progressive
            },
            .optics = {
                .curvature           = 0.25f,
                .vignette_strength   = 0.18f,
                .reflection_strength = 0.08f,
                .edge_glow           = 0.04f,
                .glass_tint          = {0.94f, 0.97f, 0.93f},
            },
            .signal = {
                .bandwidth          = 16.00f, // MHz; RGBI path, C128 VDC 80-col dot clock
                .noise_level        = 0.010f,
                .hum_bar_strength   = 0.000f, // RGBI: no AC-coupled composite stage
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.97f,
            },
        };
        break;

    // =========================================================================
    //  RGB MONITOR (aperture grille)
    //  Used by: C64/Amiga (1084), Apple IIGS, MSX (Sony KX-14CP1),
    //           ZX Spectrum 128K+, BBC Master (SCART RGB), Amstrad CPC (RGB)
    //
    //  Sony KX-14CP1 "Black Trinitron": 14", single-gun aperture grille,
    //  0.25 mm stripe pitch, RGB + composite, 9300K (D93 Japanese factory
    //  standard), marketed alongside Sony MSX Hit-Bit range.
    //  Single-gun Trinitron: convergence is physically self-correcting —
    //  one electron gun, one set of deflection plates. Aperture open ratio ~72%.
    // =========================================================================
    case CRTPreset::RGBMonitor:
        name_              = "RGB Monitor";
        id_                = "crt_rgb";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::Composite | VideoSignalMask::SVideo
                           | VideoSignalMask::RGB;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Aperture,
            .diagonal    = 14.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.25f,   // mm; Trinitron stripe pitch, 14" KX-14CP1
            .brightness  = 1.10f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 9300.0f, // K; D93 Japanese factory standard
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {0.96f, 0.97f, 1.00f}, // cool D93 white point
                .persistence     = 1.5f,
                .bloom_radius    = 0.75f,
                .bloom_threshold = 0.80f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.50f,
                .softness          = 0.22f,
                .pincushion        = 0.008f,
                .h_linearity       = 1.00f,
                .v_linearity       = 1.00f,
                .convergence_error = {0.12f, 0.10f}, // single-gun: physically near-perfect
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Aperture,
                .opacity         = 0.16f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.72f, // Trinitron aperture open ratio
            },
            .scanlines = {
                .gap       = 0.08f,
                .strength  = 0.28f,
                .phase     = 0.50f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.10f, // Trinitron flat-face faceplate
                .vignette_strength   = 0.10f,
                .reflection_strength = 0.05f,
                .edge_glow           = 0.02f,
                .glass_tint          = {0.97f, 0.97f, 1.00f}, // faint blue anti-glare coating
            },
            .signal = {
                .bandwidth          = 20.00f, // MHz; limited only by phosphor/yoke response
                .noise_level        = 0.006f,
                .hum_bar_strength   = 0.000f, // direct RGB: no composite decode stage
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 1.000f,
            },
        };
        break;

    // =========================================================================
    //  MONOCHROME GREEN (P31)
    //  Used by: Apple II/IIe/IIc (Apple Monitor II), Commodore PET (all series),
    //           BBC Micro (mono option), Memotech MTX (business), Z9001 (late)
    //
    //  Apple Monitor II specification: composite input, bandwidth < 3 dB
    //  at 12 MHz. P31 emission peak 525 nm, JEDEC persistence ~3 ms.
    //  Single electron gun: convergence error is zero by definition.
    // =========================================================================
    case CRTPreset::MonochromeGreen:
        name_              = "Green Monitor";
        id_                = "crt_green";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 12.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.31f,
            .brightness  = 1.00f,
            .contrast    = 1.20f,
            .gamma       = 2.50f,
            .color_temp  = 5400.0f, // K; correlated CCT of P31 525 nm emission
            .phosphor = {
                .type            = PhosphorType::P31,
                .glow_color      = {0.18f, 1.00f, 0.28f}, // P31 525 nm
                .persistence     = 3.0f,   // ms; P31 JEDEC spec
                .bloom_radius    = 2.30f,
                .bloom_threshold = 0.52f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.80f,
                .softness          = 0.45f,
                .pincushion        = 0.04f,
                .h_linearity       = 0.98f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.00f, 0.00f}, // mono: single gun, no convergence error
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.00f,  // mono: no RGB triad structure
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.22f,
                .strength  = 0.68f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.30f,
                .vignette_strength   = 0.28f,
                .reflection_strength = 0.08f,
                .edge_glow           = 0.07f,
                .glass_tint          = {0.82f, 1.00f, 0.82f}, // green-tinted glass
            },
            .signal = {
                .bandwidth          = 12.00f, // MHz; Apple Monitor II: <3 dB at 12 MHz
                .noise_level        = 0.014f,
                .hum_bar_strength   = 0.025f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f, // mono: no chroma path
                .sync_stability     = 0.96f,
            },
        };
        break;

    // =========================================================================
    //  MONOCHROME AMBER (P3)
    //  Used by: Apple 1 (composite terminal), LC-80, Z9001 (early mono output),
    //           various business terminals and word-processor monitors
    //
    //  IBM 5151 technical reference: P3 phosphor (~590 nm amber), 12" diagonal,
    //  TTL mono input. MDA 720-px horizontal at 16.257 MHz dot clock requires
    //  ~16 MHz analogue bandwidth. P3 and P31 are the same phosphor family
    //  (ZnS with different activators); persistence is identical at ~3 ms.
    // =========================================================================
    case CRTPreset::MonochromeAmber:
        name_              = "Amber Monitor";
        id_                = "crt_amber";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 12.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.31f,
            .brightness  = 1.00f,
            .contrast    = 1.20f,
            .gamma       = 2.50f,
            .color_temp  = 2900.0f, // K; correlated CCT of 590 nm P3 emission
            .phosphor = {
                .type            = PhosphorType::Custom,
                .glow_color      = {1.00f, 0.60f, 0.05f}, // P3 ~590 nm amber
                .persistence     = 3.0f,   // ms; same ZnS family as P31
                .bloom_radius    = 2.40f,
                .bloom_threshold = 0.50f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.80f,
                .softness          = 0.45f,
                .pincushion        = 0.04f,
                .h_linearity       = 0.98f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.00f, 0.00f}, // mono: single gun
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.00f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.22f,
                .strength  = 0.68f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.30f,
                .vignette_strength   = 0.28f,
                .reflection_strength = 0.08f,
                .edge_glow           = 0.08f,
                .glass_tint          = {1.00f, 0.94f, 0.82f}, // warm amber-tinted glass
            },
            .signal = {
                .bandwidth          = 16.00f, // MHz; IBM MDA 720-px dot clock requirement
                .noise_level        = 0.012f,
                .hum_bar_strength   = 0.020f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.97f,
            },
        };
        break;

    // =========================================================================
    //  DIRECT OUTPUT
    // =========================================================================
    case CRTPreset::DirectOutput:
        name_              = "Direct Output";
        id_                = "direct_output";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::All;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::LCD,
            .diagonal    = 24.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.00f,
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 1.00f, 1.00f},
                .persistence     = 0.0f,
                .bloom_radius    = 0.0f,
                .bloom_threshold = 1.0f,
                .decay_curve     = PhosphorDecay::Linear,
            },
            .beam = {
                .width             = 0.0f,
                .softness          = 0.0f,
                .pincushion        = 0.0f,
                .h_linearity       = 1.0f,
                .v_linearity       = 1.0f,
                .convergence_error = {0.0f, 0.0f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.0f,
                .triad_size      = 1.0f,
                .slot_mask_width = 0.0f,
            },
            .scanlines = {
                .gap       = 0.0f,
                .strength  = 0.0f,
                .phase     = 0.0f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.0f,
                .vignette_strength   = 0.0f,
                .reflection_strength = 0.0f,
                .edge_glow           = 0.0f,
                .glass_tint          = {1.0f, 1.0f, 1.0f},
            },
            .signal = {
                .bandwidth          = 1000.0f,
                .noise_level        = 0.0f,
                .hum_bar_strength   = 0.0f,
                .ghosting_strength  = 0.0f,
                .chroma_phase_error = 0.0f,
                .sync_stability     = 1.0f,
            },
        };
        break;
    }
}

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor crt_tv_desc{
    "crt_tv", "Color TV",
    "Generic 13\" CRT television — composite, interlaced, built-in speaker",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_tv_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::ConsumerTV); }
)

static const DeviceDescriptor crt_1702_desc{
    "crt_1702", "Commodore 1702",
    "Commodore 1702 — 13\", 0.31 mm Hitachi, composite + S-Video, speaker",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_1702_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::Commodore1702); }
)

static const DeviceDescriptor crt_1902_desc{
    "crt_1902", "Commodore 1902A",
    "Commodore 1902A — 14\", 0.28 mm, composite + S-Video + RGBI (C128 80-col)",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_1902_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::Commodore1902); }
)

static const DeviceDescriptor crt_rgb_desc{
    "crt_rgb", "RGB Monitor",
    "Aperture-grille RGB — Sony KX-14CP1 class, 0.25 mm, 9300K (D93)",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_rgb_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::RGBMonitor); }
)

static const DeviceDescriptor crt_green_desc{
    "crt_green", "Green Monitor",
    "Monochrome P31 green CRT — Apple Monitor II / PET class, 12\"",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_green_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::MonochromeGreen); }
)

static const DeviceDescriptor crt_amber_desc{
    "crt_amber", "Amber Monitor",
    "Monochrome P3 amber CRT — IBM 5151 class, 590 nm, 12\"",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_amber_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::MonochromeAmber); }
)

static const DeviceDescriptor direct_output_desc{
    "direct_output", "Direct Output",
    "Flat/direct output — no CRT simulation",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(direct_output_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::DirectOutput); }
)

// ============================================================================
// SYSTEM → MONITOR MAPPING  (reference, not compiled)
//
//  c64_system              → Commodore1702 (S-Video preferred), ConsumerTV (alt)
//  vic20_system            → Commodore1702, ConsumerTV (alt)
//  c16_system (C16/Plus4)  → Commodore1702, ConsumerTV (PAL TV path)
//  c128_system             → Commodore1902 (RGBI 80-col primary)
//                            Commodore1702 (40-col composite/S-Video)
//                            RGBMonitor (Trinitron/1084 RGB path)
//
//  chip8_system            → ConsumerTV
//
//  nes_system              → ConsumerTV (NTSC composite)
//
//  pet_system              → MonochromeGreen (all PET variants)
//
//  atari2600_system        → ConsumerTV
//
//  bbc_micro_system        → ConsumerTV (domestic RF/composite)
//                            MonochromeGreen (monochrome option)
//  bbc_master_system       → RGBMonitor (SCART RGB)
//
//  spectrum_system         → ConsumerTV (primary; ULA RF/composite)
//                            RGBMonitor (SCART RGB, 128K+)
//
//  amstrad_cpc_system      → RGBMonitor (CTM644/CM8833 6-pin DIN RGB)
//                            MonochromeGreen (GT65 luminance path)
//                            ConsumerTV (MP1/MP2 RF modulator)
//
//  acorn_atom_system       → ConsumerTV (RF), MonochromeGreen (alt)
//
//  lc80_system             → MonochromeAmber
//  z1013_system            → ConsumerTV (BAS composite signal to home TV)
//  z9001_system            → MonochromeGreen (late), MonochromeAmber (early)
//  kc85_system             → ConsumerTV (Junost Soviet TV, composite)
//
//  bombjack_system         → RGBMonitor (arcade RGB direct)
//  namco_arcade_system     → RGBMonitor
//  atari_vector_system     → MonochromeGreen (P31 vector phosphor glow)
//
//  apple_ii_system         → MonochromeGreen (Apple Monitor II, primary)
//                            MonochromeAmber (alt)
//                            ConsumerTV (color composite, home users)
//
//  apple1_system           → MonochromeAmber (terminal monitor)
//
//  msx_system              → RGBMonitor (Sony KX-14CP1 Trinitron, primary)
//                            ConsumerTV (composite, budget)
//
//  sega_sg1000_system      → ConsumerTV
//  sega_sms_system         → ConsumerTV
//  colecovision_system     → ConsumerTV
//
//  memotech_mtx_system     → ConsumerTV (RF/composite)
//                            MonochromeGreen (MTX512 business use)
//  tatung_einstein_system  → ConsumerTV, RGBMonitor
//  spectravideo_system     → ConsumerTV, RGBMonitor
//
//  vtech_vz_system         → ConsumerTV (RF only)
//  oric_system             → ConsumerTV (RF primary)
//                            RGBMonitor (Atmos SCART RGB, later)
//
// ============================================================================