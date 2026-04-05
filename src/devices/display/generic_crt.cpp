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
 *   Microvitec CUB Series 3 service manual — dot pitch: 0.64 mm (1431),
 *     0.43 mm (1451), 0.31 mm (1441/1442)
 *   Amstrad CPC6128/CTM644/GT65 service manual (Amstrad, 1985)
 *   Apple Monitor II user manual / Apple Monochrome Monitor brochure
 *     — bandwidth: less than 3 dB down at 12 MHz
 *   Apple IIc Color Monitor spec sheet — 0.52 mm slot pitch, 14" viewable
 *   IBM 5151 technical reference (IBM, 1981) — P3 phosphor, 12" mono
 *   Philips CM8833-II spec sheet — 14", 0.42 mm shadowmask, 15.625 kHz
 *   Sony KX-14CP1 / PVM-1390 — Black Trinitron, 14", composite + RGB
 *   JEDEC phosphor data sheets: P22, P31, P39, P3, P4
 *   ITU-R BT.470 (PAL gamma 2.8; PAL-B luma bandwidth 5 MHz)
 *   SMPTE 170M (NTSC composite 4.2 MHz luma, standard receiver gamma 2.2)
 *   KC85 documentation — Junost portable Soviet B&W TV, canonical display
 *   Commodore PET FAQ / Dave's Old Computers — 9" P4 white (2001),
 *     12" P31 green (4000/8000), 6845 CRTC on later models
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
    //  CONSUMER TV (NTSC)
    //  Used by: NES, Sega SG-1000 (NTSC), Sega Master System (NTSC),
    //           ColecoVision (NTSC), VIC-20 (TV), C64 (TV), C128 (TV)
    //
    //  Representative: mid-range NTSC 13" set, 1980–1990 (Zenith/RCA/JVC).
    //  NES: PPU outputs raw ~3.58 MHz composite with no chroma pre-filter,
    //  producing characteristic dot-crawl and color bleed.
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
    //  CONSUMER TV (RF)
    //  Used by: Atari 2600, Acorn Atom (domestic), LC-80
    //
    //  Atari 2600 outputs RF only. Signal passes through channel 3/4 modulator
    //  and TV demodulator, adding ~0.7 dB extra bandwidth loss vs composite.
    //  Effective luma bandwidth ~3.5 MHz, noise floor significantly higher.
    //  Older 1977-era sets also had worse convergence and higher curvature.
    // =========================================================================
    case CRTPreset::ConsumerTV_RF:
        name_              = "Color TV (RF)";
        id_                = "crt_tv_rf";
        primary_port_type_ = PortType::VIDEO_RF;
        accepted_signals_  = VideoSignalMask::RF;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 13.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.50f,   // mm; cheaper early-era tubes
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6200.0f, // K; older sets ran warmer
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.94f, 0.84f}, // warmer white, older P22
                .persistence     = 2.0f,
                .bloom_radius    = 2.00f,
                .bloom_threshold = 0.58f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.95f,
                .softness          = 0.60f,
                .pincushion        = 0.07f,
                .h_linearity       = 0.95f,
                .v_linearity       = 0.96f,
                .convergence_error = {0.80f, 0.55f}, // worse convergence
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.42f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.18f,
                .strength  = 0.60f,
                .phase     = 0.00f,
                .interlace = true,
            },
            .optics = {
                .curvature           = 0.42f,  // early 1970s sets slightly more curved
                .vignette_strength   = 0.40f,
                .reflection_strength = 0.16f,
                .edge_glow           = 0.10f,
                .glass_tint          = {0.90f, 0.96f, 0.88f},
            },
            .signal = {
                .bandwidth          = 3.50f,  // MHz; RF demodulator effective limit
                .noise_level        = 0.065f, // SNR ~32 dB through RF path
                .hum_bar_strength   = 0.075f,
                .ghosting_strength  = 0.25f,  // severe on antenna RF
                .chroma_phase_error = 5.00f,  // degrees; worse via RF demodulator
                .sync_stability     = 0.60f,  // horizontal rolling common
            },
        };
        break;

    // =========================================================================
    //  CONSUMER TV (PAL)
    //  Used by: ZX Spectrum 48K/128K (RF/ULA composite), Amstrad CPC (RF via MP1/MP2),
    //           Oric-1/Atmos (RF), VTech VZ200/VZ300 (RF primary),
    //           Sega SG-1000 (PAL territories), Sega Master System (PAL),
    //           BBC Micro (domestic RF), MSX (European composite),
    //           Memotech MTX, Tatung Einstein (PAL), Spectravideo SVI-318/328,
    //           ColecoVision (PAL), C16/Plus4 (PAL Europe, TV path)
    //
    //  14" was typical in Europe vs 13" US. PAL: 625 lines, 50 Hz field rate.
    //  PAL gamma: 2.8 per ITU-R BT.470. PAL-B luma bandwidth: 5 MHz.
    //  ZX Spectrum: ULA composite output has strong dot-crawl on color
    //  attribute boundaries; color clash is a function of 8×8 cell encoding.
    // =========================================================================
    case CRTPreset::ConsumerTV_PAL:
        name_              = "Color TV (PAL)";
        id_                = "crt_tv_pal";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 14.0f,   // 14" typical in European market
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.42f,
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.80f,   // PAL standard gamma, ITU-R BT.470
            .color_temp  = 6500.0f, // K; PAL D65
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.96f, 0.90f},
                .persistence     = 2.0f,
                .bloom_radius    = 1.70f,
                .bloom_threshold = 0.63f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.88f,
                .softness          = 0.54f,
                .pincushion        = 0.048f,
                .h_linearity       = 0.97f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.60f, 0.42f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.36f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.16f,
                .strength  = 0.52f,
                .phase     = 0.00f,
                .interlace = true,  // PAL 625-line interlaced
            },
            .optics = {
                .curvature           = 0.38f,
                .vignette_strength   = 0.34f,
                .reflection_strength = 0.13f,
                .edge_glow           = 0.09f,
                .glass_tint          = {0.92f, 0.97f, 0.90f},
            },
            .signal = {
                .bandwidth          = 5.00f,  // MHz; PAL-B luma specification
                .noise_level        = 0.040f,
                .hum_bar_strength   = 0.050f,
                .ghosting_strength  = 0.16f,
                .chroma_phase_error = 2.80f,  // PAL decoder generally better than NTSC
                .sync_stability     = 0.74f,
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
    //  COMMODORE PET MONITOR — WHITE (P4)
    //  Used by: Commodore PET 2001 (original, 1977)
    //
    //  PET 2001: 9" integrated monitor, P4 white/blue-white phosphor.
    //  Wikipedia / Dave's Old Computers: "light blue in the original 2001."
    //  Direct TTL video from character ROM logic — no CRTC, no composite decode.
    //  Bandwidth ~8 MHz adequate for 40-column 9" display at 1 MHz 6502 clock.
    // =========================================================================
    case CRTPreset::CommodorePET_White:
        name_              = "PET 2001 Monitor (White)";
        id_                = "crt_pet_white";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 9.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.45f,   // mm; 9" tube, ~40-column resolution
            .brightness  = 1.00f,
            .contrast    = 1.15f,
            .gamma       = 2.20f,
            .color_temp  = 9000.0f, // K; P4 near-white emission
            .phosphor = {
                .type            = PhosphorType::P4,
                .glow_color      = {0.92f, 0.94f, 1.00f}, // P4 blue-white tint
                .persistence     = 1.5f,   // ms; P4 shorter decay than P31
                .bloom_radius    = 1.80f,
                .bloom_threshold = 0.58f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.85f,
                .softness          = 0.48f,
                .pincushion        = 0.05f,
                .h_linearity       = 0.97f,
                .v_linearity       = 0.97f,
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
                .gap       = 0.20f,
                .strength  = 0.65f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.38f,  // small round tube, high curvature
                .vignette_strength   = 0.32f,
                .reflection_strength = 0.10f,
                .edge_glow           = 0.08f,
                .glass_tint          = {0.94f, 0.95f, 1.00f}, // slightly blue glass
            },
            .signal = {
                .bandwidth          = 8.00f,  // MHz; adequate for 40-col 9"
                .noise_level        = 0.018f,
                .hum_bar_strength   = 0.030f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f, // mono: no chroma
                .sync_stability     = 0.94f,
            },
        };
        break;

    // =========================================================================
    //  COMMODORE PET MONITOR — GREEN (P31, 12")
    //  Used by: Commodore PET 4000/8000 series (1980–1982)
    //
    //  4000/8000 series: 12" screen, 6845-based CRTC, 80-column capable.
    //  PET FAQ: "12-inch screen and 6845-based CRTC hardware" replacing
    //  TTL logic in 2001/3000. Same P31 phosphor class as standalone monitors
    //  but integrated and driven by 6845 pixel clock allowing higher bandwidth.
    // =========================================================================
    case CRTPreset::CommodorePET_Green:
        name_              = "PET 4000/8000 Monitor (Green)";
        id_                = "crt_pet_green";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 12.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.38f,   // mm; 12" integrated mono tube
            .brightness  = 1.00f,
            .contrast    = 1.20f,
            .gamma       = 2.50f,
            .color_temp  = 5400.0f,
            .phosphor = {
                .type            = PhosphorType::P31,
                .glow_color      = {0.18f, 1.00f, 0.28f},
                .persistence     = 3.0f,
                .bloom_radius    = 2.10f,
                .bloom_threshold = 0.54f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.78f,
                .softness          = 0.44f,
                .pincushion        = 0.04f,
                .h_linearity       = 0.98f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.00f, 0.00f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.00f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.20f,
                .strength  = 0.65f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.28f,
                .vignette_strength   = 0.26f,
                .reflection_strength = 0.08f,
                .edge_glow           = 0.06f,
                .glass_tint          = {0.84f, 1.00f, 0.84f},
            },
            .signal = {
                .bandwidth          = 14.00f, // MHz; 6845 CRTC 80-col pixel clock
                .noise_level        = 0.012f,
                .hum_bar_strength   = 0.022f,
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
    //  PHILIPS CM8833
    //  Used by: Amstrad CPC (color via RGB), ZX Spectrum (SCART),
    //           C64 (composite), MSX (European composite/RGB)
    //
    //  CM8833-II spec sheet: 14", 0.42 mm dot pitch shadowmask,
    //  15.625 kHz H-scan, 47–62.5 Hz V-scan, analogue RGB + composite,
    //  stereo speakers, 9-pin D + RCA inputs.
    // =========================================================================
    case CRTPreset::PhilipsCM8833:
        name_              = "Philips CM8833";
        id_                = "crt_cm8833";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::Composite | VideoSignalMask::RGB;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 14.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.42f,   // mm; spec sheet
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f, // K; PAL D65 target
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.98f, 0.94f},
                .persistence     = 2.0f,
                .bloom_radius    = 1.20f,
                .bloom_threshold = 0.72f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.65f,
                .softness          = 0.35f,
                .pincushion        = 0.018f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.25f, 0.18f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.25f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.12f,
                .strength  = 0.38f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.20f,
                .vignette_strength   = 0.14f,
                .reflection_strength = 0.07f,
                .edge_glow           = 0.03f,
                .glass_tint          = {0.96f, 0.97f, 0.98f}, // near-neutral glass
            },
            .signal = {
                .bandwidth          = 14.00f, // MHz; RGB analogue path, PAL-spec
                .noise_level        = 0.012f,
                .hum_bar_strength   = 0.008f,
                .ghosting_strength  = 0.02f,
                .chroma_phase_error = 1.00f,
                .sync_stability     = 0.96f,
            },
        };
        break;

    // =========================================================================
    //  MICROVITEC CUB 1431
    //  Used by: BBC Micro B/B+/Master (schools), Acorn Atom (via RGB),
    //           ZX Spectrum (1431MZ variant with ULA interface board)
    //
    //  Microvitec Series 3 service manual: 335.4 mm diagonal (13.2"),
    //  0.64 mm dot pitch black-matrix screen (model 1431), P22 phosphors,
    //  in-line gun, TTL + analogue RGB, 15.625 kHz. The most common BBC
    //  Micro school monitor. (Model 1451 had 0.43 mm; 1441 had 0.31 mm.)
    // =========================================================================
    case CRTPreset::MicrovitecCub1431:
        name_              = "Microvitec Cub 1431";
        id_                = "crt_cub1431";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::RGB;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 13.2f,   // 335.4 mm per service manual
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.64f,   // mm; model 1431, service manual
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 9300.0f, // K; pigmented P22, near D93
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {0.97f, 0.98f, 1.00f}, // pigmented phosphors, slightly cool
                .persistence     = 1.5f,
                .bloom_radius    = 1.00f,
                .bloom_threshold = 0.75f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.75f,
                .softness          = 0.42f,
                .pincushion        = 0.022f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.40f, 0.30f}, // wider convergence at 0.64 mm pitch
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.30f,  // black matrix, pigmented phosphors
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.20f,  // coarser pitch = wider visible gaps
                .strength  = 0.42f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.18f,
                .vignette_strength   = 0.16f,
                .reflection_strength = 0.06f,
                .edge_glow           = 0.03f,
                .glass_tint          = {0.95f, 0.96f, 0.98f},
            },
            .signal = {
                .bandwidth          = 16.00f, // MHz; direct TTL/analogue RGB
                .noise_level        = 0.008f,
                .hum_bar_strength   = 0.000f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.98f,
            },
        };
        break;

    // =========================================================================
    //  AMSTRAD CTM644
    //  Used by: Amstrad CPC 464/664/6128 (colour option)
    //
    //  CTM644 service manual (Amstrad 1985): 14" colour monitor,
    //  6-pin DIN RGB + sync input, 15.625 kHz. Tube sourced from various
    //  suppliers (Philips, Samsung). Dot pitch ~0.42 mm (same class as CM8833).
    //  PAL-timed direct RGB. Color temp factory-set ~6500K. Has speakers.
    // =========================================================================
    case CRTPreset::AmstradCTM644:
        name_              = "Amstrad CTM644";
        id_                = "crt_ctm644";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::RGB;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 14.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.42f,   // mm; tube class from service manual era
            .brightness  = 1.00f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 6500.0f,
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {1.00f, 0.97f, 0.93f},
                .persistence     = 2.0f,
                .bloom_radius    = 1.30f,
                .bloom_threshold = 0.70f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.70f,
                .softness          = 0.38f,
                .pincushion        = 0.020f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.30f, 0.22f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.26f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.15f,
                .strength  = 0.40f,
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.22f,
                .vignette_strength   = 0.16f,
                .reflection_strength = 0.07f,
                .edge_glow           = 0.04f,
                .glass_tint          = {0.95f, 0.96f, 0.97f},
            },
            .signal = {
                .bandwidth          = 14.00f, // MHz; direct 6-pin DIN RGB
                .noise_level        = 0.010f,
                .hum_bar_strength   = 0.006f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.97f,
            },
        };
        break;

    // =========================================================================
    //  AMSTRAD GT65
    //  Used by: Amstrad CPC 464/664/6128 (green monochrome option)
    //
    //  GT65 service manual (Amstrad 1985): 12" green phosphor monitor,
    //  luminance + sync via 6-pin DIN. P31 phosphor, high-contrast mono tube.
    //  Dot pitch ~0.38 mm estimated from tube class and 12" diagonal.
    // =========================================================================
    case CRTPreset::AmstradGT65:
        name_              = "Amstrad GT65";
        id_                = "crt_gt65";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 12.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.38f,   // mm; 12" mono tube estimate
            .brightness  = 1.00f,
            .contrast    = 1.20f,
            .gamma       = 2.50f,
            .color_temp  = 5400.0f, // K; correlated CCT of P31 525 nm
            .phosphor = {
                .type            = PhosphorType::P31,
                .glow_color      = {0.18f, 1.00f, 0.28f}, // P31 525 nm green emission
                .persistence     = 3.0f,   // ms; P31 JEDEC
                .bloom_radius    = 2.20f,
                .bloom_threshold = 0.52f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.80f,
                .softness          = 0.44f,
                .pincushion        = 0.04f,
                .h_linearity       = 0.98f,
                .v_linearity       = 0.98f,
                .convergence_error = {0.00f, 0.00f}, // mono: single gun, no convergence error
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
                .curvature           = 0.28f,
                .vignette_strength   = 0.28f,
                .reflection_strength = 0.08f,
                .edge_glow           = 0.07f,
                .glass_tint          = {0.82f, 1.00f, 0.82f}, // green-tinted glass
            },
            .signal = {
                .bandwidth          = 14.00f, // MHz; direct luminance input
                .noise_level        = 0.012f,
                .hum_bar_strength   = 0.020f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.97f,
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
    //  SOVIET / DDR MONOCHROME TV (Junost class)
    //  Used by: KC85/2/3/4 (canonical display), Z1013, Z9001/KC87 (early),
    //           LC-80 (composite output)
    //
    //  KC85 documentation explicitly states "Junost ('Youth') portable Soviet
    //  B&W TV" as the typical/canonical display. Junost 402/403: ~9–12"
    //  diagonal (23–31 cm), P4 white phosphor, 625-line PAL-compatible,
    //  bandwidth ~5 MHz (limited by Soviet consumer TV circuitry).
    //  Z1013: "existing home electronics such as television sets, BAS signal."
    // =========================================================================
    case CRTPreset::SovietMonoTV:
        name_              = "Junost (Soviet B&W TV)";
        id_                = "crt_junost";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = true;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Monochrome,
            .diagonal    = 12.0f,   // Junost 403: ~31 cm = 12.2"
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.45f,   // mm; approximate, consumer Soviet tube
            .brightness  = 1.00f,
            .contrast    = 1.10f,
            .gamma       = 2.50f,   // Soviet CRTs ran harder than PAL spec
            .color_temp  = 8500.0f, // K; P4 blue-white emission
            .phosphor = {
                .type            = PhosphorType::P4,
                .glow_color      = {0.88f, 0.91f, 1.00f}, // P4 cold white
                .persistence     = 2.0f,   // ms; P4 white, moderate
                .bloom_radius    = 1.90f,
                .bloom_threshold = 0.55f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.90f,
                .softness          = 0.52f,
                .pincushion        = 0.055f,
                .h_linearity       = 0.96f,
                .v_linearity       = 0.96f,
                .convergence_error = {0.00f, 0.00f}, // mono
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.00f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.18f,
                .strength  = 0.60f,
                .phase     = 0.00f,
                .interlace = true,  // 625-line interlaced
            },
            .optics = {
                .curvature           = 0.36f,
                .vignette_strength   = 0.38f,
                .reflection_strength = 0.12f,
                .edge_glow           = 0.08f,
                .glass_tint          = {0.90f, 0.92f, 1.00f}, // slight blue tint, Soviet glass
            },
            .signal = {
                .bandwidth          = 5.00f,  // MHz; Soviet consumer TV, limited
                .noise_level        = 0.055f,
                .hum_bar_strength   = 0.065f,
                .ghosting_strength  = 0.12f,
                .chroma_phase_error = 0.00f,  // mono: no chroma
                .sync_stability     = 0.65f,  // sync instability common on cheap sets
            },
        };
        break;

    // =========================================================================
    //  VECTOR MONITOR
    //  Used by: Atari vector arcade systems (atari_vector_system):
    //           Asteroids, Battlezone, Tempest, Gravitar, Star Wars, etc.
    //
    //  Atari vector cabinets used Wells-Gardner 19V2000 and similar XY
    //  deflection CRTs. P31 phosphor: 525 nm green, ~3 ms persistence —
    //  critical for the characteristic afterglow on fast vectors.
    //  No raster: beam traces vectors directly; no scanlines, no shadow mask
    //  visible at normal drawing resolution. Very high bloom on bright paths.
    //  Color vector games (Tempest, Space Duel) used different P22 color tubes;
    //  this preset covers the dominant monochrome P31 variant.
    // =========================================================================
    case CRTPreset::VectorMonitor:
        name_              = "Atari Vector Monitor";
        id_                = "crt_vector";
        primary_port_type_ = PortType::VIDEO_COMPOSITE;
        accepted_signals_  = VideoSignalMask::Composite;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Vector,
            .diagonal    = 19.0f,   // Wells-Gardner 19V2000: 19"
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.00f,   // N/A for XY vector
            .brightness  = 1.00f,
            .contrast    = 1.20f,
            .gamma       = 2.20f,
            .color_temp  = 5400.0f,
            .phosphor = {
                .type            = PhosphorType::P31,
                .glow_color      = {0.18f, 1.00f, 0.28f}, // P31 525 nm
                .persistence     = 3.0f,   // ms; P31 — the glow IS the effect
                .bloom_radius    = 3.50f,  // heavy: beam traces are point-sources
                .bloom_threshold = 0.30f,  // fires early; vectors run hot
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.60f,  // focused XY beam
                .softness          = 0.35f,
                .pincushion        = 0.02f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.00f, 0.00f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.00f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.00f,
                .strength  = 0.00f,  // no raster, no scanlines
                .phase     = 0.00f,
                .interlace = false,
            },
            .optics = {
                .curvature           = 0.12f,  // relatively flat face
                .vignette_strength   = 0.20f,
                .reflection_strength = 0.09f,
                .edge_glow           = 0.12f,
                .glass_tint          = {0.84f, 1.00f, 0.84f}, // green-tinted cabinet glass
            },
            .signal = {
                .bandwidth          = 0.00f,  // N/A; XY deflection, not raster
                .noise_level        = 0.020f,
                .hum_bar_strength   = 0.000f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.99f,
            },
        };
        break;

    // =========================================================================
    //  ARCADE RASTER MONITOR
    //  Used by: Bomb Jack, Namco arcade systems (bombjack_system,
    //           namco_arcade_system)
    //
    //  Japanese arcade cabinets: Sanyo 20-EZV / Nanao MS-8 / MS-9 /
    //  Wells-Gardner K7000 class. 19–20", shadowmask ~0.63 mm dot pitch
    //  on 19" tubes, P22, 15.625 kHz (or 15.73 for NTSC board timings),
    //  direct RGB with separate sync, no interlace (240p progressive).
    //  Color temp: D93 (9300K), Japanese factory standard.
    //  Namco System 1/2 boards: ~6–8 MHz pixel clocks, RGB analog direct.
    // =========================================================================
    case CRTPreset::ArcadeMonitor:
        name_              = "Arcade Monitor";
        id_                = "crt_arcade";
        primary_port_type_ = PortType::VIDEO_RGB;
        accepted_signals_  = VideoSignalMask::RGB;
        has_speakers_      = false;
        characteristics_   = {
            .technology  = DisplayTechnology::CRT_Shadow,
            .diagonal    = 19.0f,
            .aspect      = 4.0f / 3.0f,
            .dot_pitch   = 0.63f,   // mm; typical 19" arcade shadowmask
            .brightness  = 1.05f,
            .contrast    = 1.00f,
            .gamma       = 2.20f,
            .color_temp  = 9300.0f, // K; D93 Japanese factory standard
            .phosphor = {
                .type            = PhosphorType::P22,
                .glow_color      = {0.96f, 0.97f, 1.00f},
                .persistence     = 1.8f,
                .bloom_radius    = 1.10f,
                .bloom_threshold = 0.72f,
                .decay_curve     = PhosphorDecay::Exponential,
            },
            .beam = {
                .width             = 0.70f,
                .softness          = 0.38f,
                .pincushion        = 0.015f,
                .h_linearity       = 0.99f,
                .v_linearity       = 0.99f,
                .convergence_error = {0.20f, 0.15f},
                .corner_pin        = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            .mask = {
                .pattern         = MaskPattern::Shadow,
                .opacity         = 0.30f,
                .triad_size      = 1.00f,
                .slot_mask_width = 0.00f,
            },
            .scanlines = {
                .gap       = 0.18f,  // 240p on 19" gives very visible gaps
                .strength  = 0.50f,
                .phase     = 0.00f,
                .interlace = false,  // 240p progressive
            },
            .optics = {
                .curvature           = 0.14f,
                .vignette_strength   = 0.18f,
                .reflection_strength = 0.06f,
                .edge_glow           = 0.04f,
                .glass_tint          = {0.96f, 0.97f, 1.00f}, // neutral glass, arcade cabinet
            },
            .signal = {
                .bandwidth          = 14.00f, // MHz; direct RGB analog board
                .noise_level        = 0.010f,
                .hum_bar_strength   = 0.000f,
                .ghosting_strength  = 0.000f,
                .chroma_phase_error = 0.000f,
                .sync_stability     = 0.99f,
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

// --- Consumer TV (NTSC) ---
static const DeviceDescriptor crt_tv_desc{
    "crt_tv", "Color TV",
    "Generic 13\" NTSC CRT television — NES, Sega SG-1000/SMS (NTSC), ColecoVision",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_tv_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::ConsumerTV); }
)

// --- Consumer TV (RF) ---
static const DeviceDescriptor crt_tv_rf_desc{
    "crt_tv_rf", "Color TV (RF)",
    "Generic CRT TV via RF modulator — Atari 2600, LC-80, Acorn Atom (domestic)",
    PortType::VIDEO_RF, false
};
REGISTER_DEVICE(crt_tv_rf_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::ConsumerTV_RF); }
)

// --- Consumer TV (PAL) ---
static const DeviceDescriptor crt_tv_pal_desc{
    "crt_tv_pal", "Color TV (PAL)",
    "Generic 14\" PAL CRT — ZX Spectrum, CPC, BBC, Oric, SMS PAL, MSX Europe",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_tv_pal_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::ConsumerTV_PAL); }
)

// --- Commodore 1702 ---
static const DeviceDescriptor crt_1702_desc{
    "crt_1702", "Commodore 1702",
    "Commodore 1702 — 13\", 0.31 mm Hitachi, composite + S-Video, speaker",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_1702_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::Commodore1702); }
)

// --- Commodore 1902A ---
static const DeviceDescriptor crt_1902_desc{
    "crt_1902", "Commodore 1902A",
    "Commodore 1902A — 14\", 0.28 mm, composite + S-Video + RGBI (C128 80-col)",
    PortType::VIDEO_RGBI, false
};
REGISTER_DEVICE(crt_1902_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::Commodore1902); }
)

// --- PET 2001 Monitor (White) ---
static const DeviceDescriptor crt_pet_white_desc{
    "crt_pet_white", "PET 2001 Monitor (White)",
    "Commodore PET 2001 integrated 9\" white P4 phosphor monitor (1977)",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_pet_white_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::CommodorePET_White); }
)

// --- PET 4000/8000 Monitor (Green) ---
static const DeviceDescriptor crt_pet_green_desc{
    "crt_pet_green", "PET 4000/8000 Monitor (Green)",
    "Commodore PET 4000/8000 integrated 12\" P31 green, 6845 CRTC (1980)",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_pet_green_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::CommodorePET_Green); }
)

// --- RGB Monitor (aperture grille) ---
static const DeviceDescriptor crt_rgb_desc{
    "crt_rgb", "RGB Monitor (Aperture Grille)",
    "Professional aperture-grille RGB — Trinitron/KX-14CP1 class, 0.25 mm",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_rgb_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::RGBMonitor); }
)

// --- Philips CM8833 ---
static const DeviceDescriptor crt_cm8833_desc{
    "crt_cm8833", "Philips CM8833",
    "Philips CM8833-II — 14\", 0.42 mm shadowmask, RGB + composite, stereo",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_cm8833_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::PhilipsCM8833); }
)

// --- Microvitec Cub 1431 ---
static const DeviceDescriptor crt_cub1431_desc{
    "crt_cub1431", "Microvitec Cub 1431",
    "Microvitec Cub 1431 — 13.2\", 0.64 mm, TTL/analogue RGB (BBC Micro, schools)",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_cub1431_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::MicrovitecCub1431); }
)

// --- Amstrad CTM644 ---
static const DeviceDescriptor crt_ctm644_desc{
    "crt_ctm644", "Amstrad CTM644",
    "Amstrad CTM644 — 14\", 0.42 mm, 6-pin DIN RGB with speakers (CPC series)",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_ctm644_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::AmstradCTM644); }
)

// --- Amstrad GT65 ---
static const DeviceDescriptor crt_gt65_desc{
    "crt_gt65", "Amstrad GT65",
    "Amstrad GT65 — 12\", P31 green, luminance input (CPC series green option)",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_gt65_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::AmstradGT65); }
)

// --- Monochrome Green ---
static const DeviceDescriptor crt_green_desc{
    "crt_green", "Green Monitor",
    "Monochrome P31 green CRT — Apple Monitor II / Amdek / PET 2001-N class",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_green_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::MonochromeGreen); }
)

// --- Monochrome Amber ---
static const DeviceDescriptor crt_amber_desc{
    "crt_amber", "Amber Monitor",
    "Monochrome P3 amber CRT — IBM 5151 class, 590 nm, 12\"",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_amber_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::MonochromeAmber); }
)

// --- Soviet / DDR Mono TV ---
static const DeviceDescriptor crt_junost_desc{
    "crt_junost", "Junost (Soviet B&W TV)",
    "Soviet Junost portable mono TV — canonical display for KC85, Z1013, Z9001",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_junost_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::SovietMonoTV); }
)

// --- Vector Monitor ---
static const DeviceDescriptor crt_vector_desc{
    "crt_vector", "Atari Vector Monitor",
    "Wells-Gardner 19V2000 XY vector CRT — P31 green, 3 ms persistence",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(crt_vector_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::VectorMonitor); }
)

// --- Arcade Monitor ---
static const DeviceDescriptor crt_arcade_desc{
    "crt_arcade", "Arcade Monitor",
    "19\" arcade RGB shadowmask — Sanyo/Nanao MS-8 class, 0.63 mm, D93, 240p",
    PortType::VIDEO_RGB, false
};
REGISTER_DEVICE(crt_arcade_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::ArcadeMonitor); }
)

// --- Direct Output ---
static const DeviceDescriptor direct_output_desc{
    "direct_output", "Direct Output",
    "Flat display — clean output with no CRT effects",
    PortType::VIDEO_COMPOSITE, false
};
REGISTER_DEVICE(direct_output_desc,
    []() { return std::make_unique<GenericCRT>(CRTPreset::DirectOutput); }
)

// ============================================================================
// SYSTEM → MONITOR MAPPING  (reference, not compiled)
//
//  c64_system              → Commodore1702 (S-Video), ConsumerTV (alt)
//  vic20_system            → Commodore1702, ConsumerTV (alt)
//  c16_system (C16/Plus4)  → Commodore1702, PhilipsCM8833, ConsumerTV_PAL
//  c128_system             → Commodore1702 (primary), PhilipsCM8833 (RGB)
//                            Commodore1902 (RGBI 80-col)
//
//  chip8_system            → ConsumerTV (CHIP-8 ran on TV sets)
//
//  nes_system              → ConsumerTV (NTSC), ConsumerTV_PAL (PAL)
//
//  pet_system              → CommodorePET_White (2001 original, 9")
//                            CommodorePET_Green (4000/8000 series, 12")
//
//  atari2600_system        → ConsumerTV_RF (RF-only output)
//
//  bbc_micro_system        → MicrovitecCub1431 (schools, primary)
//                            ConsumerTV_PAL (domestic RF)
//                            MonochromeGreen (monochrome option)
//  bbc_master_system       → MicrovitecCub1431, PhilipsCM8833
//
//  spectrum_system         → ConsumerTV_PAL (primary; ULA RF/composite)
//                            PhilipsCM8833 (SCART RGB, 128K+)
//
//  amstrad_cpc_system      → AmstradCTM644 (colour, bundled)
//                            AmstradGT65 (green, bundled alt)
//                            ConsumerTV_PAL (via MP1/MP2 modulator)
//
//  acorn_atom_system       → ConsumerTV_PAL (RF), MonochromeGreen (alt)
//
//  lc80_system             → SovietMonoTV (B&W TV composite/RF)
//  z1013_system            → SovietMonoTV (BAS signal to home TV)
//  z9001_system            → SovietMonoTV (early mono), ConsumerTV_PAL (later)
//  kc85_system             → SovietMonoTV (Junost — documented canonical)
//
//  bombjack_system         → ArcadeMonitor (Sanyo/Nanao 19" RGB)
//  namco_arcade_system     → ArcadeMonitor
//  atari_vector_system     → VectorMonitor (Wells-Gardner XY P31)
//
//  apple_ii_system         → MonochromeGreen (Apple Monitor II, primary)
//                            MonochromeAmber (alt)
//                            ConsumerTV (color composite, home users)
//
//  apple1_system           → MonochromeAmber (terminal monitor)
//
//  msx_system              → RGBMonitor (Sony KX-14CP1 Black Trinitron)
//                            PhilipsCM8833 (European)
//                            ConsumerTV_PAL (composite, budget)
//
//  sega_sg1000_system      → ConsumerTV (NTSC Japan), ConsumerTV_PAL (PAL)
//  sega_sms_system         → ConsumerTV (NTSC), ConsumerTV_PAL (PAL)
//
//  colecovision_system     → ConsumerTV (NTSC), ConsumerTV_PAL (PAL)
//
//  memotech_mtx_system     → ConsumerTV_PAL (RF/composite primary)
//                            MonochromeGreen (MTX512 business use)
//  tatung_einstein_system  → ConsumerTV_PAL, PhilipsCM8833
//  spectravideo_system     → ConsumerTV_PAL, PhilipsCM8833, RGBMonitor
//
//  vtech_vz_system         → ConsumerTV_PAL (RF only — VZ200/VZ300)
//  oric_system             → ConsumerTV_PAL (RF primary — Oric-1 RF out)
//                            PhilipsCM8833 (Atmos RGB SCART, later)
//
// ============================================================================