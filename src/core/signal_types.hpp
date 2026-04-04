#pragma once

// ============================================================================
// Signal Type Enums — video & audio signal encoding standards
// ============================================================================
//
// Unified signal type classification used across the entire codebase:
//   - Port framework (port.hpp) — describes connector output encoding
//   - Video stream pipeline (sync_types.hpp) — GPU shader selection
//   - VideoPort traits (video_port.hpp) — sample type → signal type mapping
//   - System base class (system.hpp) — advertises active signal type
//   - GUI renderer (session_gui.cpp) — dispatches to correct shader
//
// These enums describe the *encoding* of a signal, not the physical
// connector (which is PortType) or the broadcast standard (which is
// VideoStandard / PAL / NTSC).
// ============================================================================

#include <cstdint>

/// Video signal encoding standard.
enum class VideoSignalType : uint8_t {
    Composite,          ///< Composite / CVBS (palette-indexed luma+chroma)
    SVideo,             ///< S-Video (separate Y/C) — reserved, uses CompositeVideoSample
    RGB,                ///< Analog RGB (3 independent color channels)
    RGBI,               ///< Digital RGBI (4-bit TTL: R, G, B, Intensity)
    YPbPr,              ///< Analog component (Y/Pb/Pr) — reserved, uses RGBVideoSample
    Digital,            ///< Generic digital (HDMI, DVI, etc.)
    Vector,             ///< Vector display (x/y beam deflection + intensity)
    RF,                 ///< RF modulated composite (antenna / channel 3-4)
};

/// Audio signal encoding.
enum class AudioSignalType : uint8_t {
    Mono,               ///< Single channel
    Stereo,             ///< Two channels (left/right)
    Quadraphonic,       ///< Four channels — reserved
};

/// Human-readable name for a VideoSignalType (for diagnostics/printf).
inline const char* signal_type_name(VideoSignalType t) {
    switch (t) {
        case VideoSignalType::Composite:         return "Composite";
        case VideoSignalType::SVideo:            return "S-Video";
        case VideoSignalType::RGB:               return "RGB";
        case VideoSignalType::RGBI:              return "RGBI";
        case VideoSignalType::YPbPr:             return "YPbPr";
        case VideoSignalType::Digital:           return "Digital";
        case VideoSignalType::Vector:            return "Vector";
        case VideoSignalType::RF:                return "RF";
    }
    return "Unknown";
}

/// Human-readable name for an AudioSignalType.
inline const char* signal_type_name(AudioSignalType t) {
    switch (t) {
        case AudioSignalType::Mono:          return "Mono";
        case AudioSignalType::Stereo:        return "Stereo";
        case AudioSignalType::Quadraphonic:  return "Quadraphonic";
    }
    return "Unknown";
}
