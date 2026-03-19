#pragma once

// ============================================================================
// VideoFlags — shared concrete flag type for all video signal standards
// ============================================================================
//
// All flag signals across every video system are subsets of the same small
// set of sync and control signals.  All fit in uint8_t with no bit conflicts.
// One concrete enum covers every system — no template parameter needed.
//
// Raster systems use: HSync, VSync (RGB only), Blank, Burst
// Vector systems use: BeamOn, FrameEnd
// ============================================================================

#include <cstdint>

enum class VideoFlags : uint8_t {
    None     = 0x00,
    HSync    = 0x01,   // composite sync on composite systems; H-sync on RGB
    VSync    = 0x02,   // unused on composite (classified from pulse length); explicit on RGB
    Blank    = 0x04,   // blanking active — no visible output
    Burst    = 0x08,   // composite color burst gate; unused on RGB.
                       // Downstream use (analog chroma phase calibration) not yet implemented.
    BeamOn   = 0x10,   // vector: beam actively drawing
    FrameEnd = 0x20,   // vector: drawing list complete; semantic equivalent of VSync
};

constexpr VideoFlags operator|(VideoFlags a, VideoFlags b) noexcept {
    return VideoFlags(uint8_t(a) | uint8_t(b));
}

constexpr VideoFlags operator&(VideoFlags a, VideoFlags b) noexcept {
    return VideoFlags(uint8_t(a) & uint8_t(b));
}

constexpr VideoFlags operator~(VideoFlags a) noexcept {
    return VideoFlags(~uint8_t(a));
}

constexpr bool has_flag(VideoFlags flags, VideoFlags test) noexcept {
    return uint8_t(flags & test) != 0;
}
