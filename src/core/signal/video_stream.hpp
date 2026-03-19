#pragma once

// ============================================================================
// VideoStream — chip-facing write interface for video signal output
// ============================================================================
//
// This is the ONLY type a chip needs to know about.  It holds a write pointer,
// a base pointer for position calculation, the previous flags byte for sync
// edge detection, and a cold-path function pointer for the rare sync event.
//
// The chip includes only this header — it never sees VideoPort, traits,
// reconstructors, or GPU upload code.
//
// Usage:
//   stream_->drive({ .color_index = color_latch_, .flags = flags_prepack_ });
//
// The flags comparison is a single byte compare.  The cold path executes
// ~313 times per 8M-cycle frame and is marked noinline at its definition
// site in VideoPort.
// ============================================================================

#include "core/signal/video_flags.hpp"
#include "core/cermu.hpp"       // FORCE_INLINE

#include <cstdint>

template<typename SampleT>
struct VideoStream {
    using value_type = SampleT;

    SampleT*   ptr;
    SampleT*   base;
    VideoFlags prev_flags = VideoFlags::None;

    void (*on_sync_change)(void* ctx, VideoFlags flags, uint32_t pos) noexcept;
    void* ctx;

    FORCE_INLINE
    void drive(SampleT s) noexcept {
        SampleT* pos = ptr;
        *ptr++ = s;

        if (unlikely(s.flags != prev_flags))
            on_sync_change(ctx, s.flags, static_cast<uint32_t>(pos - base));

        prev_flags = s.flags;
    }
};

// ============================================================================
// NullVideoStream — discards output for inactive chips
// ============================================================================
//
// For chips that must keep ticking but whose output should be discarded
// (e.g. C128 mode switching).  The scratch cell stays permanently in L1
// because it is always the same address.
// ============================================================================

template<typename SampleT>
struct NullVideoStream {
    using value_type = SampleT;

    SampleT    scratch;
    VideoFlags prev_flags = VideoFlags::None;

    FORCE_INLINE
    void drive(SampleT s) noexcept {
        scratch    = s;
        prev_flags = s.flags;
    }
};
