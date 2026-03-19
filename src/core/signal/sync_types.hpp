#pragma once

// ============================================================================
// SyncEvent / SyncType — shared by all VideoPort specializations
// ============================================================================

#include <cstdint>
#include "core/signal/video_flags.hpp"

enum class SyncType : uint8_t {
    HSync,          // Horizontal sync (raster)
    VSync,          // Vertical sync (raster)
    Equalizing,     // Equalizing pulse (interlace)
    FrameEnd,       // Vector: drawing list complete
    BeamOn,         // Vector: beam started drawing
    BeamOff,        // Vector: beam stopped drawing
};

struct SyncEvent {
    uint32_t   stream_pos;    // sample offset in stream at edge
    SyncType   type;
    VideoFlags flags;         // original VideoFlags at this event (for VBlank detection)
};

// Signal type identifier for GPU shader selection
enum class SignalType : uint8_t {
    Composite,
    SVideo,
    RGB,
    RGBI,
    Vector,
    CompositeArtifact,
};
