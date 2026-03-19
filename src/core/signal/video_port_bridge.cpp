#include "core/signal/video_port.hpp"
#include "core/signal/video_sample_types.hpp"
#include "core/indexed_frame_buffer.hpp"

// ============================================================================
// reconstruct_to_framebuffer — bridge from 1D video stream to 2D scanlines
// ============================================================================
//
// Walks the sync event list to find HSync events, then copies the visible
// portion of each scanline (starting at back_porch_pixels after sync)
// into the IndexedFrameBuffer using flush_line().
//
// This is a transitional adapter: it lets migrated chips produce a flat
// pixel stream while the existing display pipeline (IndexedFrameBuffer →
// GUI texture upload) continues to work unchanged.  Once all chips are
// migrated and the GPU reconstruction pipeline is in place, this bridge
// becomes unnecessary.
// ============================================================================

// Composite specialization — color_index directly maps to palette index
template<>
void CompositeVideoPort::reconstruct_to_framebuffer(
    const FrameData& fd,
    IndexedFrameBuffer* fb,
    const uint32_t* palette,
    int display_width,
    int back_porch_pixels) const
{
    if (!fb || !fd.stream || fd.stream_len == 0) return;

    const auto* samples = static_cast<const CompositeVideoSample*>(fd.stream);
    const int fb_width  = fb->width();
    const int fb_height = fb->height();
    const int line_width = (display_width > 0) ? display_width : fb_width;

    int scanline = 0;

    for (uint32_t i = 0; i < fd.sync_count && scanline < fb_height; ++i) {
        if (fd.sync_events[i].type != SyncType::HSync)
            continue;

        // Skip VBlank lines — VSync flag distinguishes vertical blanking from
        // horizontal blanking.  Per-dot-clock streams have Blank set on all HSync
        // samples (HSync is within HBlank), so Blank alone cannot identify VBlank.
        if (has_flag(fd.sync_events[i].flags, VideoFlags::VSync))
            continue;

        // Start of visible line: sync position + back porch
        uint32_t line_start = fd.sync_events[i].stream_pos + back_porch_pixels;
        if (line_start >= fd.stream_len) continue;

        // Extract color indices for this scanline
        int pixels_available = static_cast<int>(fd.stream_len - line_start);
        int pixels_to_copy = (pixels_available < line_width) ? pixels_available : line_width;
        pixels_to_copy = (pixels_to_copy < fb_width) ? pixels_to_copy : fb_width;

        // Build a temporary color_line from the stream samples
        // (IndexedFrameBuffer::flush_line expects uint8_t color indices)
        uint8_t color_line[1024];  // generous for any system
        for (int x = 0; x < pixels_to_copy; ++x) {
            const auto& s = samples[line_start + x];
            color_line[x] = has_flag(s.flags, VideoFlags::Blank) ? 0 : s.color_index;
        }

        fb->flush_line(scanline, color_line, palette, pixels_to_copy);
        ++scanline;
    }
}

// RGB specialization
template<>
void RGBVideoPort::reconstruct_to_framebuffer(
    const FrameData& fd,
    IndexedFrameBuffer* fb,
    const uint32_t* /*palette*/,
    int display_width,
    int back_porch_pixels) const
{
    if (!fb || !fd.stream || fd.stream_len == 0) return;

    const auto* samples = static_cast<const RGBVideoSample*>(fd.stream);
    const int fb_width  = fb->width();
    const int fb_height = fb->height();
    const int line_width = (display_width > 0) ? display_width : fb_width;

    // For RGB, we write RGBA directly to the framebuffer since there's
    // no palette indirection.  Use flush_line with a synthetic palette
    // would be wasteful — write direct RGBA instead.
    uint32_t* rgba = fb->framebuffer();
    if (!rgba) return;

    int scanline = 0;

    for (uint32_t i = 0; i < fd.sync_count && scanline < fb_height; ++i) {
        if (fd.sync_events[i].type != SyncType::HSync)
            continue;

        // Skip VBlank lines
        if (has_flag(fd.sync_events[i].flags, VideoFlags::Blank))
            continue;

        uint32_t line_start = fd.sync_events[i].stream_pos + back_porch_pixels;
        if (line_start >= fd.stream_len) continue;

        int pixels_available = static_cast<int>(fd.stream_len - line_start);
        int pixels_to_copy = (pixels_available < line_width) ? pixels_available : line_width;
        pixels_to_copy = (pixels_to_copy < fb_width) ? pixels_to_copy : fb_width;

        uint32_t* row = rgba + scanline * fb_width;
        for (int x = 0; x < pixels_to_copy; ++x) {
            const auto& s = samples[line_start + x];
            if (has_flag(s.flags, VideoFlags::Blank)) {
                row[x] = 0xFF000000;  // black
            } else {
                row[x] = 0xFF000000 | (s.b << 16) | (s.g << 8) | s.r;
            }
        }
        ++scanline;
    }
}

// RGBI specialization
template<>
void RGBIVideoPort::reconstruct_to_framebuffer(
    const FrameData& fd,
    IndexedFrameBuffer* fb,
    const uint32_t* palette,
    int display_width,
    int back_porch_pixels) const
{
    if (!fb || !fd.stream || fd.stream_len == 0) return;

    const auto* samples = static_cast<const RGBIVideoSample*>(fd.stream);
    const int fb_width  = fb->width();
    const int fb_height = fb->height();
    const int line_width = (display_width > 0) ? display_width : fb_width;

    int scanline = 0;

    for (uint32_t i = 0; i < fd.sync_count && scanline < fb_height; ++i) {
        if (fd.sync_events[i].type != SyncType::HSync)
            continue;

        // Skip VBlank lines
        if (has_flag(fd.sync_events[i].flags, VideoFlags::Blank))
            continue;

        uint32_t line_start = fd.sync_events[i].stream_pos + back_porch_pixels;
        if (line_start >= fd.stream_len) continue;

        int pixels_available = static_cast<int>(fd.stream_len - line_start);
        int pixels_to_copy = (pixels_available < line_width) ? pixels_available : line_width;
        pixels_to_copy = (pixels_to_copy < fb_width) ? pixels_to_copy : fb_width;

        uint8_t color_line[1024];
        for (int x = 0; x < pixels_to_copy; ++x) {
            const auto& s = samples[line_start + x];
            color_line[x] = has_flag(s.flags, VideoFlags::Blank) ? 0 : (s.rgbi & 0x0F);
        }

        fb->flush_line(scanline, color_line, palette, pixels_to_copy);
        ++scanline;
    }
}

// Vector specialization — no-op (vector doesn't use scanline-based display)
template<>
void VectorVideoPort::reconstruct_to_framebuffer(
    const FrameData& /*fd*/,
    IndexedFrameBuffer* /*fb*/,
    const uint32_t* /*palette*/,
    int /*display_width*/,
    int /*back_porch_pixels*/) const
{
    // Vector displays require a dedicated GPU rendering pipeline.
    // No IndexedFrameBuffer bridge is applicable.
}
