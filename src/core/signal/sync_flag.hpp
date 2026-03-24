#pragma once

// ============================================================================
// SyncFlag — shared concrete flag type for all video signal standards
// ============================================================================
//
// All flag signals across every video system are subsets of the same small
// set of sync and control signals.  All fit in uint8_t with no bit conflicts.
// One concrete enum covers every system — no template parameter needed.
//
// Raster systems use: HSync, VSync (RGB only), Blank, Burst
// Vector systems use: BeamOn, FrameEnd
//
// Visibility asymmetry: HSync and VSync are *invisible* to composite decoders
// — they classify sync pulses by duration rather than discrete flags.  An RGB
// decoder sees both explicitly.  Blank and Burst are always visible.
// ============================================================================

#include <cstdint>

enum class SyncFlag : uint8_t {
    None     = 0x00,
    HSync    = 0x01,   // composite sync on composite systems; H-sync on RGB
    VSync    = 0x02,   // unused on composite (classified from pulse length); explicit on RGB
    Blank    = 0x04,   // blanking active — no visible output
    Burst    = 0x08,   // composite color burst gate; unused on RGB.
                       // Downstream use (analog chroma phase calibration) not yet implemented.
    BeamOn   = 0x10,   // vector: beam actively drawing
    FrameEnd = 0x20,   // vector: drawing list complete; semantic equivalent of VSync
};

constexpr SyncFlag operator|(SyncFlag a, SyncFlag b) noexcept {
    return SyncFlag(uint8_t(a) | uint8_t(b));
}

constexpr SyncFlag operator&(SyncFlag a, SyncFlag b) noexcept {
    return SyncFlag(uint8_t(a) & uint8_t(b));
}

constexpr SyncFlag operator~(SyncFlag a) noexcept {
    return SyncFlag(~uint8_t(a));
}

constexpr SyncFlag& operator|=(SyncFlag& a, SyncFlag b) noexcept {
    return a = a | b;
}

constexpr SyncFlag& operator&=(SyncFlag& a, SyncFlag b) noexcept {
    return a = a & b;
}

constexpr SyncFlag& operator^=(SyncFlag& a, SyncFlag b) noexcept {
    return a = SyncFlag(uint8_t(a) ^ uint8_t(b));
}

constexpr bool has_flag(SyncFlag flags, SyncFlag test) noexcept {
    return uint8_t(flags & test) != 0;
}
