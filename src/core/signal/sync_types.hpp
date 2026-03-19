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

// FrameData — handed off to the display layer once per frame
struct FrameData {
    void*      stream;
    uint32_t   stream_len;
    SyncEvent* sync_events;
    uint32_t   sync_count;
    SignalType signal_type;
    int        back_porch;     // samples after HSync before visible pixels
    int        display_width;  // visible pixels per scanline (0 = use fb width)
};

// Maximum stream buffer: enough for one full frame of the largest system.
// C64 PAL: 504×312 ≈ 157K samples.  Allow generous headroom.
inline constexpr uint32_t MAX_STREAM_SAMPLES = 512 * 320;  // ~163K
inline constexpr uint32_t MAX_SYNC_EVENTS    = 400;
