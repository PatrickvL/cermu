#pragma once

// ============================================================================
// VideoPort — board-facing owner of a video output stream
// ============================================================================
//
// Templated on the stream type.  Owns the stream, the frame buffer, and
// the sync event list.  Included only by boards — never by chips.
//
// Current implementation includes a bridge to IndexedFrameBuffer for
// backward compatibility with the existing display pipeline during
// incremental chip migration.
// ============================================================================

#include "core/signal/sync_types.hpp"
#include "core/signal/video_flags.hpp"
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
                              VideoFlags prev, VideoFlags flags,
                              uint32_t pos) noexcept
{
    bool was_sync = has_flag(prev, VideoFlags::HSync);
    bool now_sync = has_flag(flags, VideoFlags::HSync);

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
                              VideoFlags prev, VideoFlags flags,
                              uint32_t pos) noexcept
{
    if (has_flag(prev, VideoFlags::HSync) && !has_flag(flags, VideoFlags::HSync)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::HSync, prev };
    }
    if (has_flag(prev, VideoFlags::VSync) && !has_flag(flags, VideoFlags::VSync)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::VSync, prev };
    }
}

// RGBI: same as RGB
inline void handle_sync_impl(RGBITag,
                              SyncEvent* events, uint32_t& count,
                              int& sync_run,
                              VideoFlags prev, VideoFlags flags,
                              uint32_t pos) noexcept
{
    handle_sync_impl(RGBTag{}, events, count, sync_run, prev, flags, pos);
}

// Vector: BeamOn transitions + FrameEnd
inline void handle_sync_impl(VectorTag,
                              SyncEvent* events, uint32_t& count,
                              int& /*sync_run*/,
                              VideoFlags prev, VideoFlags flags,
                              uint32_t pos) noexcept
{
    if (has_flag(flags, VideoFlags::FrameEnd) && !has_flag(prev, VideoFlags::FrameEnd)) {
        if (count < MAX_SYNC_EVENTS)
            events[count++] = { pos, SyncType::FrameEnd, flags };
    }

    bool beam_changed = has_flag(flags, VideoFlags::BeamOn) !=
                        has_flag(prev, VideoFlags::BeamOn);
    if (beam_changed) {
        SyncType t = has_flag(flags, VideoFlags::BeamOn)
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
    using Stream = StreamT;
    using Sample = typename StreamT::value_type;

    VideoPort() {
        active_buf_            = buf_;
        stream_.ptr            = active_buf_;
        stream_.base           = active_buf_;
        stream_.prev_flags     = VideoFlags::None;
        stream_.ctx            = this;
        stream_.on_sync_change = &VideoPort::cold_path;
    }

    Stream& stream() noexcept { return stream_; }

    // ====================================================================
    // External buffer management — allows DisplayPipeline (or similar)
    // to provide the sample buffer and control frame-end buffer swaps.
    //
    // set_active_buffer(): redirect the stream to an external buffer.
    // set_frame_end_callback(): install a callback that returns the next
    //   buffer pointer on FrameEnd.  If null, cold_path returns active_buf_
    //   (single-buffer mode, current default behavior).
    // ====================================================================

    void set_active_buffer(Sample* buf) noexcept {
        active_buf_  = buf;
        stream_.base = buf;
        stream_.ptr  = buf;
    }

    void set_active_sync_buffer(SyncEvent* events, uint32_t* count) noexcept {
        active_sync_        = events;
        active_sync_count_  = count;
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
        stream_.base       = buf_;
        stream_.ptr        = buf_;
        active_sync_       = sync_events_;
        active_sync_count_ = &sync_count_;
        on_frame_end_      = nullptr;
        frame_end_ctx_     = nullptr;
    }

    // ====================================================================
    // Display binding — register framebuffer + palette for automatic
    // stream→framebuffer reconstruction on swap_frame().
    // Call once during initialize() for per-dot-clock systems.
    // Per-frame systems that render to the framebuffer directly should
    // NOT bind a display — swap_frame() just resets the stream for them.
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
    // thread can snapshot stream data between run_frame() calls.
    // ====================================================================

    void bind_frame_output(FrameData* out) noexcept { frame_output_ = out; }

    const FrameData& last_frame_data() const noexcept { return last_frame_; }

    // ====================================================================
    // Bridge suppression — skip CPU-side reconstruction when the GPU
    // stream shader is handling display directly.  The display binding
    // stays intact so the bridge can be re-enabled if needed.
    // ====================================================================

    void set_bridge_suppressed(bool suppress) noexcept { bridge_suppressed_ = suppress; }
    bool bridge_suppressed() const noexcept { return bridge_suppressed_; }

    FrameData swap_frame() noexcept {
        // Use the snapshot taken at FrameEnd (self-bounding reset);
        // fall back to current ptr position for non-FrameEnd callers.
        // completed_base points to the buffer that holds the finished frame;
        // if no FrameEnd was detected, active_buf_ is the current (only) buffer.
        Sample* frame_buf = stream_.completed_base
            ? stream_.completed_base : active_buf_;
        const uint32_t len = stream_.frame_len
            ? stream_.frame_len
            : static_cast<uint32_t>(stream_.ptr - active_buf_);

        // Use completed frame's sync data if available (double-buffer mode);
        // fall back to active sync arrays (single-buffer legacy path).
        SyncEvent* frame_sync = completed_sync_
            ? completed_sync_ : active_sync_;
        uint32_t frame_sync_count = completed_sync_
            ? completed_sync_count_ : *active_sync_count_;

        FrameData fd {
            .stream        = frame_buf,
            .stream_len    = len,
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
        // Suppressed when the GPU stream shader handles display directly.
        if (bound_fb_ && !bridge_suppressed_) {
            reconstruct_to_framebuffer(fd, bound_fb_, bound_palette_,
                                       bound_line_width_, bound_back_porch_);
        }

        stream_.base           = active_buf_;
        stream_.ptr            = active_buf_;
        stream_.prev_flags     = VideoFlags::None;
        stream_.frame_len      = 0;
        stream_.completed_base = nullptr;
        completed_sync_        = nullptr;
        completed_sync_count_  = 0;
        *active_sync_count_    = 0;
        sync_run_              = 0;
        return fd;
    }

    // ====================================================================
    // Manual bridge: reconstruct stream into IndexedFrameBuffer.
    // Prefer bind_display() + swap_frame() for automatic reconstruction.
    // Only meaningful for raster (composite/RGB/RGBI) signal types.
    // ====================================================================

    void reconstruct_to_framebuffer(const FrameData& fd,
                                    IndexedFrameBuffer* fb,
                                    const uint32_t* palette,
                                    int display_width,
                                    int back_porch_pixels) const;

private:
    Stream    stream_;
    Sample    buf_[MAX_STREAM_SAMPLES];  // Default internal buffer — fallback before DisplayPipeline connect and for headless builds
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
    static Sample* cold_path(void* ctx, VideoFlags flags, uint32_t pos) noexcept {
        auto* self = static_cast<VideoPort*>(ctx);
        using Tag = typename detail::SyncTag<Sample>::type;
        detail::handle_sync_impl(Tag{},
                                 self->active_sync_, *self->active_sync_count_,
                                 self->sync_run_,
                                 self->stream_.prev_flags, flags, pos);
        if (!has_flag(flags, VideoFlags::FrameEnd))
            return static_cast<Sample*>(nullptr);
        // Snapshot completed frame's sync data before the callback swaps buffers
        self->completed_sync_       = self->active_sync_;
        self->completed_sync_count_ = *self->active_sync_count_;
        return self->on_frame_end_
            ? self->on_frame_end_(self->frame_end_ctx_)
            : self->active_buf_;
    }
};

// ============================================================================
// Concrete port type aliases — only these names appear in board headers
// ============================================================================

using CompositeVideoPort = VideoPort<VideoOut<CompositeVideoSample>>;
using RGBVideoPort       = VideoPort<VideoOut<RGBVideoSample>>;
using RGBIVideoPort      = VideoPort<VideoOut<RGBIVideoSample>>;
using VectorVideoPort    = VideoPort<VideoOut<VectorVideoSample>>;
