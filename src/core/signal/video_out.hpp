#pragma once

// ============================================================================
// VideoOut — chip-facing write interface for video signal output
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
struct VideoOut {
    using value_type = SampleT;

    SampleT*   ptr;
    SampleT*   base;
    VideoFlags prev_flags = VideoFlags::None;

    // Length of the most recently completed frame (samples).
    // Set when on_sync_change signals a frame boundary;
    // consumed and cleared by swap_frame().
    uint32_t   frame_len = 0;

    // Points to the buffer that holds the completed frame's data.
    // Set by drive() just before swapping base/ptr to the new buffer.
    // Consumed by swap_frame() to build FrameData, then cleared.
    SampleT*   completed_base = nullptr;

    // Cold-path callback for sync edge processing.  Returns a non-null
    // pointer when the current sample marks a frame boundary (FrameEnd
    // flag) — the stream then snapshots frame_len and resets ptr/base to
    // the returned address.  Returning nullptr means "not a frame boundary".
    //
    // This design makes double-buffer swapping transparent to the stream:
    // the callback simply returns &back_buf[0] instead of &front_buf[0].
    // A disconnected stub callback returns &stub[0] on every frame end,
    // making the stream silently overwrite a small scratch buffer.
    SampleT* (*on_sync_change)(void* ctx, VideoFlags flags, uint32_t pos) noexcept;
    void* ctx;

    // Frame-end detection: true once a FrameEnd flag has been driven.
    // Sticky until swap_frame() clears frame_len.  Zero overhead — reads
    // a field already in cache (no extra bool needed).
    FORCE_INLINE bool frame_ended() const noexcept {
        return frame_len != 0;
    }

    FORCE_INLINE
    void drive(SampleT s) noexcept {
        *ptr++ = s;

        if (unlikely(prev_flags != s.flags)) {
            const uint32_t len = static_cast<uint32_t>(ptr - base);
            SampleT* new_base = on_sync_change(ctx, s.flags, len - 1);
            if (new_base) {
                frame_len = len;
                completed_base = base;
                base = new_base;
                ptr  = new_base;
            }
            prev_flags = s.flags;
        }
    }
};

// ============================================================================
// NullVideoOut — discards output for inactive chips
// ============================================================================
//
// For chips that must keep ticking but whose output should be discarded
// (e.g. C128 mode switching).  The scratch cell stays permanently in L1
// because it is always the same address.
// ============================================================================

template<typename SampleT>
struct NullVideoOut {
    using value_type = SampleT;

    SampleT    scratch;
    VideoFlags prev_flags = VideoFlags::None;

    FORCE_INLINE
    void drive(SampleT s) noexcept {
        scratch    = s;
        prev_flags = s.flags;
    }
};
