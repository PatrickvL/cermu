#pragma once

// ============================================================================
// SyncEvent / SyncType — shared by all VideoPort specializations
// ============================================================================

#include <cstdint>
#include "core/signal/sync_flag.hpp"
#include "core/signal_types.hpp"  // VideoSignalType

enum class SyncType : uint8_t {
    HSync,          // Horizontal sync (raster)
    VSync,          // Vertical sync (raster)
    Equalizing,     // Equalizing pulse (interlace)
    FrameEnd,       // Vector: drawing list complete
    BeamOn,         // Vector: beam started drawing
    BeamOff,        // Vector: beam stopped drawing
};

struct SyncEvent {
    uint32_t   signal_pos;    // sample offset in signal at edge
    SyncType   type;
    SyncFlag flags;         // original SyncFlag at this event (for VBlank detection)
};

// FrameData — handed off to the display layer once per frame.
// Carries all metadata that a display device (monitor) needs to
// reconstruct and present the image.  The palette and dimensional
// fields originate from the video chip and flow through the video
// port, mirroring how a real video signal is self-describing.
struct FrameData {
    void*           signal_output;
    uint32_t        signal_output_len;
    SyncEvent*      sync_events;
    uint32_t        sync_count;
    VideoSignalType signal_type;
    int             back_porch;     // samples after HSync before visible pixels
    int             display_width;  // visible pixels per scanline (0 = use fb width)
    const uint32_t* palette;        // RGBA palette (from video chip, nullptr for RGB/vector)
    uint16_t        palette_size;   // number of palette entries
};

// Maximum signal buffer: enough for one full frame of the largest system.
// VDC (MOS 8563) double-pixel mode: 128 chars × 16 ppc × ~313 lines ≈ 641K.
// FrameEnd fires partway through the frame; post-FrameEnd samples go into
// the callback-provided buffer.  This only needs to cover up to FrameEnd.
inline constexpr uint32_t MAX_SIGNAL_SAMPLES = 1024 * 640;  // 655360
inline constexpr uint32_t MAX_SYNC_EVENTS    = 400;
