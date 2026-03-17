#pragma once

#include "core/palette_table.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <algorithm>

// ============================================================================
// IndexedFrameBuffer — unified display output for all emulated systems
// ============================================================================
//
// Owns a palette-indexed pixel buffer and routes it to the host display
// through one of two paths:
//
//   CPU path:  palette[index] → RGBA framebuffer   (32-bit texture upload)
//   GPU path:  raw indices → R8 index buffer        (8-bit texture + shader)
//
// Replaces the previous DisplaySurface split.  Every
// emulated system creates one IndexedFrameBuffer in its System subclass
// and registers it with the base class via register_display().
//
// Two rendering models coexist:
//
//   Frame-based (character grids, tile maps, full-screen bitmap renderers):
//     1. System writes palette indices into indices()
//     2. System calls flush() to route them to the host
//
//   Scanline-based (VIC-II, TED, TIA, NES PPU, VIC):
//     1. Chip fills its own per-scanline color_line buffer during ticks
//     2. Chip calls flush_line(row, color_line, palette, width) at scanline end
//     3. IndexedFrameBuffer routes the line to RGBA or GPU index buffer
//
// Buffer ownership:
//   indices_     : owned (allocated by init())
//   rgba_        : owned fallback; GUI provides external via set_framebuffer()
//   ext_indices_ : GUI-provided R8 index buffer for GPU path (non-owning)
//
// Memory layout after init(320, 240):
//   indices_  → uint8_t[320 * 240]   — palette indices
//   rgba_     → uint32_t[320 * 240]  — RGBA output (internal fallback)
//
// The host (SessionGUI) calls:
//   set_framebuffer(buf, w, h)   → provides external RGBA destination
//   set_index_buffer(buf)        → enables GPU indexed path
//
// Future extension points:
//   - Interlace / field tracking (even/odd field flag)
//   - Frame doubling / mixing (double-buffer indices_)
//   - User-selectable alternate palette (swap palette_ pointer)
//   - Gamma / color model transforms (apply to palette before flush)
//   - Post-processing shader parameters (passed alongside palette to host)
// ============================================================================

class IndexedFrameBuffer {
public:
    IndexedFrameBuffer() = default;
    ~IndexedFrameBuffer() {
        delete[] indices_;
        delete[] rgba_;
    }

    // Non-copyable, movable
    IndexedFrameBuffer(const IndexedFrameBuffer&) = delete;
    IndexedFrameBuffer& operator=(const IndexedFrameBuffer&) = delete;
    IndexedFrameBuffer(IndexedFrameBuffer&& o) noexcept { swap(o); }
    IndexedFrameBuffer& operator=(IndexedFrameBuffer&& o) noexcept {
        if (this != &o) { IndexedFrameBuffer tmp(std::move(o)); swap(tmp); }
        return *this;
    }

    // ====================================================================
    // Initialization
    // ====================================================================

    /// Allocate buffers for the given dimensions.
    /// Safe to call multiple times (re-allocates if size changes).
    void init(int width, int height) {
        if (width == width_ && height == height_ && indices_) return;

        delete[] indices_;
        delete[] rgba_;

        width_  = width;
        height_ = height;
        int total = width * height;

        indices_ = new uint8_t[total]();
        rgba_    = new uint32_t[total]();
    }

    // ====================================================================
    // Palette
    // ====================================================================

    /// Set the palette from a raw ABGR array (copies data).
    void set_palette(const uint32_t* palette, int count) {
        palette_.set(palette, count);
    }

    /// Set the palette from an existing PaletteTable.
    void set_palette(const PaletteTable& pal) {
        palette_ = pal;
    }

    /// Direct access to the palette for in-place updates.
    PaletteTable&       palette()       { return palette_; }
    const PaletteTable& palette() const { return palette_; }

    /// Palette data pointer (for GPU texture upload).
    const uint32_t* palette_data() const { return palette_.data(); }
    int             palette_size() const { return palette_.size(); }

    // ====================================================================
    // External buffer management (called by GUI / host)
    // ====================================================================

    /// Provide an external RGBA framebuffer from the host.
    /// Pass nullptr to revert to the internal fallback buffer.
    void set_framebuffer(uint32_t* buf, int width, int height) {
        assert(!buf || (width >= width_ && height >= height_));
        ext_rgba_  = buf;
        ext_width_ = width;
        ext_height_ = height;
    }

    /// Provide an external index buffer for GPU indexed rendering.
    /// Pass nullptr to disable GPU path (reverts to CPU palette lookup).
    void set_index_buffer(uint8_t* buf) {
        ext_indices_ = buf;
    }

    // ====================================================================
    // Clear
    // ====================================================================

    /// Clear the index buffer to zero (background color).
    void clear() {
        if (indices_) std::memset(indices_, 0, width_ * height_);
    }

    /// Clear the index buffer to a specific palette index.
    void clear(uint8_t value) {
        if (indices_) std::memset(indices_, value, width_ * height_);
    }

    // ====================================================================
    // Frame flush — for frame-based renderers
    // ====================================================================

    /// Flush the internal index buffer through the palette to the output.
    void flush() {
        flush_frame(indices_, palette_.data());
    }

    /// Flush with an explicit palette (for chips with runtime palettes).
    void flush(const uint32_t* palette) {
        flush_frame(indices_, palette);
    }

    /// Flush an external index array (same dimensions) through a palette.
    /// Used by chips that own their own frame_indices_ buffer.
    void flush_frame(const uint8_t* frame_indices, const uint32_t* palette) {
        const int total = width_ * height_;
        if (total <= 0 || !frame_indices) return;

        if (ext_indices_) {
            // GPU path — copy raw indices; palette applied by shader
            std::memcpy(ext_indices_, frame_indices, total);
        } else {
            // CPU path — resolve palette now
            uint32_t* dst = active_framebuffer();
            if (dst && palette) {
                for (int i = 0; i < total; ++i) {
                    dst[i] = palette[frame_indices[i]];
                }
            }
        }
    }

    // ====================================================================
    // Line flush — for scanline-based renderers
    // ====================================================================

    /// Flush a full scanline from an external color_line source buffer.
    /// The chip calls this at the end of each visible scanline.
    void flush_line(int row, const uint8_t* color_line,
                    const uint32_t* palette, int line_width) {
        flush_line_range(row, color_line, palette, 0, line_width);
    }

    /// Flush a sub-range [x_start, x_end) of a scanline.
    void flush_line_range(int row, const uint8_t* color_line,
                          const uint32_t* palette,
                          int x_start, int x_end) {
        if (!color_line) return;
        if (row < 0 || row >= height_) return;

        x_start = std::max(x_start, 0);
        x_end   = std::min(x_end, width_);
        if (x_start >= x_end) return;

        const int span = x_end - x_start;

        if (ext_indices_) {
            // GPU path — copy raw indices
            std::memcpy(ext_indices_ + row * width_ + x_start,
                        color_line + x_start, span);
        } else {
            // CPU path — palette lookup
            uint32_t* dst = active_framebuffer();
            if (dst && palette) {
                uint32_t* const row_ptr = dst + row * width_;
                for (int x = x_start; x < x_end; ++x) {
                    row_ptr[x] = palette[color_line[x]];
                }
            }
        }

        // Also copy into the internal index buffer for screenshot/debug use
        if (indices_ && color_line != indices_ + row * width_) {
            std::memcpy(indices_ + row * width_ + x_start,
                        color_line + x_start, span);
        }
    }

    // ====================================================================
    // Accessors
    // ====================================================================

    uint8_t*        indices()       { return indices_; }
    const uint8_t*  indices() const { return indices_; }

    /// Active RGBA framebuffer (external if provided, otherwise internal).
    uint32_t*       framebuffer()       { return active_framebuffer(); }
    const uint32_t* framebuffer() const {
        return ext_rgba_ ? ext_rgba_ : rgba_;
    }

    int width()  const { return width_; }
    int height() const { return height_; }

    /// Whether GPU indexed rendering is active.
    bool gpu_indexed() const { return ext_indices_ != nullptr; }

    /// The GPU index buffer (for host to read/snapshot).
    const uint8_t* index_buffer() const { return ext_indices_; }

private:
    int       width_        = 0;
    int       height_       = 0;
    uint8_t*  indices_      = nullptr;   // Palette indices (owned)
    uint32_t* rgba_         = nullptr;   // RGBA fallback (owned)
    uint32_t* ext_rgba_     = nullptr;   // External RGBA buffer (GUI-owned)
    int       ext_width_    = 0;
    int       ext_height_   = 0;
    uint8_t*  ext_indices_  = nullptr;   // External GPU index buffer (GUI-owned)
    PaletteTable palette_;

    uint32_t* active_framebuffer() {
        return ext_rgba_ ? ext_rgba_ : rgba_;
    }

    void swap(IndexedFrameBuffer& o) noexcept {
        std::swap(width_, o.width_);
        std::swap(height_, o.height_);
        std::swap(indices_, o.indices_);
        std::swap(rgba_, o.rgba_);
        std::swap(ext_rgba_, o.ext_rgba_);
        std::swap(ext_width_, o.ext_width_);
        std::swap(ext_height_, o.ext_height_);
        std::swap(ext_indices_, o.ext_indices_);
        std::swap(palette_, o.palette_);
    }
};
