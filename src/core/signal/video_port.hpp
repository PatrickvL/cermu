#pragma once

// ============================================================================
// VideoPort — board-facing owner of a video output
// ============================================================================
//
// Templated on the output type.  Owns the output, the frame buffer, and
// the sync event list.  Included only by boards — never by chips.
//
// Current implementation includes a bridge to IndexedFrameBuffer for
// backward compatibility with the existing display pipeline during
// incremental chip migration.
// ============================================================================

#include "core/signal/sync_types.hpp"
#include "core/signal/sync_flag.hpp"
#include "core/signal/video_out.hpp"
#include "core/signal/video_sample_types.hpp"
#include "core/cermu.hpp"

#include <cstdint>
#include <cstring>

// Forward declaration for bridge — chips never see this
class IndexedFrameBuffer;

// SignalTraits — maps a sample type to its VideoSignalType enum value
template<typename SampleT> struct SignalTraits;

template<> struct SignalTraits<CompositeVideoSample> {
    static constexpr VideoSignalType type = VideoSignalType::Composite;
};
template<> struct SignalTraits<RGBVideoSample> {
    static constexpr VideoSignalType type = VideoSignalType::RGB;
};
template<> struct SignalTraits<RGBIVideoSample> {
    static constexpr VideoSignalType type = VideoSignalType::RGBI;
};
template<> struct SignalTraits<VectorVideoSample> {
    static constexpr VideoSignalType type = VideoSignalType::Vector;
};

// ============================================================================
// SyncHandler — free-function dispatch for sync event processing
// ============================================================================
// Uses overloaded free functions dispatched on a sample type tag to avoid
// member function specialization ordering issues.

namespace detail {

struct CompositeTag {};
struct RGBTag {};
struct RGBITag {};
struct VectorTag {};

template<typename SampleT> struct SyncTag;
template<> struct SyncTag<CompositeVideoSample> { using type = CompositeTag; };
template<> struct SyncTag<RGBVideoSample>       { using type = RGBTag; };
template<> struct SyncTag<RGBIVideoSample>      { using type = RGBITag; };
template<> struct SyncTag<VectorVideoSample>    { using type = VectorTag; };

// Composite: classify sync pulse by run length
inline void handle_sync_impl(CompositeTag,
                              SyncEvent* events, uint32_t& count,
                              int& sync_run,
                              SyncFlag prev, SyncFlag flags,
                              uint32_t pos) noexcept
{
    bool was_sync = has_flag(prev, SyncFlag::HSync);
    bool now_sync = has_flag(flags, SyncFlag::HSync);

    if (was_sync && !now_sync) {
        // Falling edge — classify by pulse width
        SyncType type;
        if (sync_run > 150)
            type = SyncType::VSync;
        else
            type = SyncType::HSync;

        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, type, prev };
        sync_run = 0;
    } else if (now_sync) {
        ++sync_run;
    }
}

// RGB: separate H and V sync lines
inline void handle_sync_impl(RGBTag,
                              SyncEvent* events, uint32_t& count,
                              int& /*sync_run*/,
                              SyncFlag prev, SyncFlag flags,
                              uint32_t pos) noexcept
{
    if (has_flag(prev, SyncFlag::HSync) && !has_flag(flags, SyncFlag::HSync)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::HSync, prev };
    }
    if (has_flag(prev, SyncFlag::VSync) && !has_flag(flags, SyncFlag::VSync)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::VSync, prev };
    }
}

// RGBI: same as RGB
inline void handle_sync_impl(RGBITag,
                              SyncEvent* events, uint32_t& count,
                              int& sync_run,
                              SyncFlag prev, SyncFlag flags,
                              uint32_t pos) noexcept
{
    handle_sync_impl(RGBTag{}, events, count, sync_run, prev, flags, pos);
}

// Vector: BeamOn transitions + FrameEnd
inline void handle_sync_impl(VectorTag,
                              SyncEvent* events, uint32_t& count,
                              int& /*sync_run*/,
                              SyncFlag prev, SyncFlag flags,
                              uint32_t pos) noexcept
{
    if (has_flag(flags, SyncFlag::FrameEnd) && !has_flag(prev, SyncFlag::FrameEnd)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::FrameEnd, flags };
    }

    bool beam_changed = has_flag(flags, SyncFlag::BeamOn) !=
                        has_flag(prev, SyncFlag::BeamOn);
    if (beam_changed) {
        SyncType t = has_flag(flags, SyncFlag::BeamOn)
                   ? SyncType::BeamOn : SyncType::BeamOff;
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, t, flags };
    }
}

} // namespace detail

// ============================================================================
// VideoPort class template
// ============================================================================

template<typename StreamT>
class VideoPort {
public:
    using Output = StreamT;
    using Sample = typename StreamT::value_type;

    VideoPort() {
        active_buf_            = buf_;
        output_.ptr            = active_buf_;
        output_.base           = active_buf_;
        output_.prev_flags     = SyncFlag::None;
        output_.ctx            = this;
        output_.on_sync_change = &VideoPort::cold_path;
    }

    Output& output() noexcept { return output_; }

    // ====================================================================
    // External buffer management — allows DisplayPipeline (or similar)
    // to provide the sample buffer and control frame-end buffer swaps.
    //
    // set_active_buffer(): redirect the output to an external buffer.
    // set_frame_end_callback(): install a callback that returns the next
    //   buffer pointer on FrameEnd.  If null, cold_path returns active_buf_
    //   (single-buffer mode, current default behavior).
    // ====================================================================

    void set_active_buffer(Sample* buf) noexcept {
        active_buf_  = buf;
        output_.base = buf;
        output_.ptr  = buf;
        // Clear stale frame-completion state — any previous completed_base
        // pointed into the old buffer and is now dangling.  Also reset
        // prev_flags so the first sample doesn't trigger a spurious sync
        // edge from whatever state the port was in before the switch.
        output_.prev_flags     = SyncFlag::None;
        output_.frame_len      = 0;
        output_.completed_base = nullptr;
    }

    void set_active_sync_buffer(SyncEvent* events, uint32_t* count) noexcept {
        active_sync_        = events;
        active_sync_count_  = count;
        // Clear stale completed-sync pointers — they referenced the old
        // sync array which may be freed or belong to a different pipeline.
        completed_sync_       = nullptr;
        completed_sync_count_ = 0;
        sync_run_             = 0;
    }

    void set_frame_end_callback(Sample* (*cb)(void*) noexcept, void* ctx) noexcept {
        on_frame_end_    = cb;
        frame_end_ctx_   = ctx;
    }

    // ====================================================================
    // Reset to internal buffer — reverts any external buffer binding,
    // restoring the default single-buffer mode using the built-in buf_[].
    // Only safe when emulation is paused (no concurrent drive() calls).
    // ====================================================================

    void reset_to_internal_buffer() noexcept {
        active_buf_        = buf_;
        output_.base       = buf_;
        output_.ptr        = buf_;
        output_.prev_flags = SyncFlag::None;
        output_.frame_len      = 0;
        output_.completed_base = nullptr;
        active_sync_       = sync_events_;
        active_sync_count_ = &sync_count_;
        *active_sync_count_ = 0;
        completed_sync_        = nullptr;
        completed_sync_count_  = 0;
        sync_run_          = 0;
        on_frame_end_      = nullptr;
        frame_end_ctx_     = nullptr;
    }

    // ====================================================================
    // Display binding — register framebuffer + palette for automatic
    // signal→framebuffer reconstruction on swap_frame().
    // Call once during initialize() for per-dot-clock systems.
    // Per-frame systems that render to the framebuffer directly should
    // NOT bind a display — swap_frame() just resets the output for them.
    // ====================================================================

    void bind_display(IndexedFrameBuffer* fb, const uint32_t* palette,
                      int display_width = 0, int back_porch_pixels = 0) noexcept {
        bound_fb_         = fb;
        bound_palette_    = palette;
        bound_line_width_ = display_width;
        bound_back_porch_ = back_porch_pixels;
    }

    void set_palette(const uint32_t* palette, uint16_t size = 0) noexcept {
        bound_palette_ = palette;
        if (size) bound_palette_size_ = size;
    }

    // ====================================================================
    // Frame output binding — automatically store the last FrameData in
    // an external location (e.g. System::last_frame_data_) so the GUI
    // thread can snapshot signal data between run_frame() calls.
    // ====================================================================

    void bind_frame_output(FrameData* out) noexcept { frame_output_ = out; }

    const FrameData& last_frame_data() const noexcept { return last_frame_; }

    // ====================================================================
    // Bridge suppression — skip CPU-side reconstruction when the GPU
    // signal shader is handling display directly.  The display binding
    // stays intact so the bridge can be re-enabled if needed.
    // ====================================================================

    void set_bridge_suppressed(bool suppress) noexcept { bridge_suppressed_ = suppress; }
    bool bridge_suppressed() const noexcept { return bridge_suppressed_; }

    FrameData swap_frame() noexcept {
        // Use the snapshot taken at FrameEnd (self-bounding reset);
        // fall back to current ptr position for non-FrameEnd callers.
        // completed_base points to the buffer that holds the finished frame;
        // if no FrameEnd was detected, active_buf_ is the current (only) buffer.
        Sample* frame_buf = output_.completed_base
            ? output_.completed_base : active_buf_;
        const uint32_t len = output_.frame_len
            ? output_.frame_len
            : static_cast<uint32_t>(output_.ptr - active_buf_);

        // Use completed frame's sync data if available (double-buffer mode);
        // fall back to active sync arrays (single-buffer legacy path).
        SyncEvent* frame_sync = completed_sync_
            ? completed_sync_ : active_sync_;
        uint32_t frame_sync_count = completed_sync_
            ? completed_sync_count_ : *active_sync_count_;

        FrameData fd {
            .signal_output = frame_buf,
            .signal_output_len    = len,
            .sync_events   = frame_sync,
            .sync_count    = frame_sync_count,
            .signal_type   = SignalTraits<Sample>::type,
            .back_porch    = bound_back_porch_,
            .display_width = bound_line_width_,
            .palette       = bound_palette_,
            .palette_size  = bound_palette_size_,
        };

        // Store for later access (e.g. GUI thread snapshot)
        last_frame_ = fd;
        if (frame_output_) *frame_output_ = fd;

        // Auto-reconstruct into bound framebuffer before resetting.
        // Suppressed when the GPU signal shader handles display directly.
        if (bound_fb_ && !bridge_suppressed_) {
            reconstruct_to_framebuffer(fd, bound_fb_, bound_palette_,
                                       bound_line_width_, bound_back_porch_);
        }

        // In triple-buffer mode (DisplayPipeline connected via on_frame_end_),
        // the callback already rotated buffers at FrameEnd and the chip may
        // be actively writing to the new slot.  Don't touch base, ptr,
        // prev_flags, or sync_count — the new frame is already in progress.
        //
        // In single-buffer mode (no DisplayPipeline), reset to the start of
        // the internal buffer so the next frame overwrites the old one.
        if (!on_frame_end_) {
            output_.base        = active_buf_;
            output_.ptr         = active_buf_;
            output_.prev_flags  = SyncFlag::None;
            *active_sync_count_ = 0;
            sync_run_           = 0;
        }

        // Always clear frame-completion bookkeeping so frame_ended()
        // returns false and the next FrameEnd can be detected.
        output_.frame_len      = 0;
        output_.completed_base = nullptr;
        completed_sync_        = nullptr;
        completed_sync_count_  = 0;
        return fd;
    }

    // ====================================================================
    // Manual bridge: reconstruct signal into IndexedFrameBuffer.
    // Prefer bind_display() + swap_frame() for automatic reconstruction.
    // Only meaningful for raster (composite/RGB/RGBI) signal types.
    // ====================================================================

    void reconstruct_to_framebuffer(const FrameData& fd,
                                    IndexedFrameBuffer* fb,
                                    const uint32_t* palette,
                                    int display_width,
                                    int back_porch_pixels) const;

private:
    Output    output_;
    Sample    buf_[MAX_SIGNAL_SAMPLES];  // Default internal buffer — fallback before DisplayPipeline connect and for headless builds
    Sample*   active_buf_ = nullptr;     // Points to current frame's sample buffer

    // Sync event storage — internal fallback arrays.
    // In double-buffer mode, active_sync_ is redirected to DisplayPipeline's arrays.
    SyncEvent sync_events_[MAX_SYNC_EVENTS];  // internal fallback
    SyncEvent* active_sync_ = sync_events_;   // where cold_path writes
    uint32_t  sync_count_ = 0;                // internal fallback count
    uint32_t* active_sync_count_ = &sync_count_; // pointer to active count
    int       sync_run_   = 0;                // transient sync pulse accumulator

    // Completed frame sync snapshot — set at FrameEnd before buffer swap
    SyncEvent* completed_sync_ = nullptr;
    uint32_t   completed_sync_count_ = 0;

    // Frame-end buffer swap callback — returns next frame's buffer pointer.
    // Null = single-buffer mode (return active_buf_).
    Sample* (*on_frame_end_)(void* ctx) noexcept = nullptr;
    void*   frame_end_ctx_ = nullptr;

    // Last frame data — stored by swap_frame() before reset
    FrameData  last_frame_{};
    FrameData* frame_output_ = nullptr;  // external binding (e.g. System::last_frame_data_)

    // Display binding for automatic reconstruction
    IndexedFrameBuffer* bound_fb_           = nullptr;
    const uint32_t*     bound_palette_      = nullptr;
    uint16_t            bound_palette_size_  = 0;
    int                 bound_line_width_    = 0;
    int                 bound_back_porch_    = 0;
    bool                bridge_suppressed_   = false;

    FORCE_NOINLINE
    static Sample* cold_path(void* ctx, SyncFlag flags, uint32_t pos) noexcept {
        auto* self = static_cast<VideoPort*>(ctx);
        using Tag = typename detail::SyncTag<Sample>::type;
        detail::handle_sync_impl(Tag{},
                                 self->active_sync_, *self->active_sync_count_,
                                 self->sync_run_,
                                 self->output_.prev_flags, flags, pos);
        if (!has_flag(flags, SyncFlag::FrameEnd))
            return static_cast<Sample*>(nullptr);

        // Snapshot the completed frame's sync data to locals BEFORE the
        // callback redirects active_sync_ to a new buffer.  The callback
        // (DisplayPipeline::on_frame_end → set_active_sync_buffer) clears
        // completed_sync_, so we must write it AFTER the callback returns.
        SyncEvent* done_sync  = self->active_sync_;
        uint32_t   done_count = *self->active_sync_count_;

        Sample* new_buf = self->on_frame_end_
            ? self->on_frame_end_(self->frame_end_ctx_)
            : self->active_buf_;

        self->completed_sync_       = done_sync;
        self->completed_sync_count_ = done_count;
        return new_buf;
    }
};

// ============================================================================
// Concrete port type aliases — only these names appear in board headers
// ============================================================================

using CompositeVideoPort = VideoPort<VideoOut<CompositeVideoSample>>;
using RGBVideoPort       = VideoPort<VideoOut<RGBVideoSample>>;
using RGBIVideoPort      = VideoPort<VideoOut<RGBIVideoSample>>;
using VectorVideoPort    = VideoPort<VideoOut<VectorVideoSample>>;
