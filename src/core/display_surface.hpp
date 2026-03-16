#pragma once

#include "chip/video/video_pixel_unit.hpp"
#include "core/palette_table.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// DisplaySurface — owns an indexed framebuffer + palette + flush pipeline
// ============================================================================
//
// Replaces the {framebuffer_, pixel_, frame_indices_} triplet duplicated
// across 15+ system classes. Systems that do frame-based rendering
// (character grids, tile maps, bitmap modes) create a DisplaySurface
// and use it to:
//
//   1. Write palette indices into the index buffer via indices()
//   2. Call flush() to route them through VideoPixelUnit
//   3. Expose palette/pixel_unit for GPU indexed registration
//
// Scanline-based video chips (VIC-II, TED, TIA, PPU) that write
// pixel-by-pixel during their tick loop continue to own their own
// VideoPixelUnit. This type is for batch/frame renderers.
//
// Memory layout:
//   - index_buf_    : uint8_t[width * height]  — palette indices (owned)
//   - framebuffer_  : uint32_t[width * height] — RGBA output (owned)
//   - pixel_        : VideoPixelUnit            — flush engine
//   - palette_      : PaletteTable              — RGBA palette (owned copy)
//
// The RGBA framebuffer is allocated internally so systems don't need
// separate arrays. SessionGUI's external set_framebuffer override
// replaces the pointer in pixel_ to its own allocation; the internal
// buffer serves as fallback for headless/test operation.
//
// Usage:
//   // In system header:
//   DisplaySurface display_;
//
//   // In initialize():
//   display_.init(320, 240);
//   display_.set_palette(kc85_constants::PALETTE, 16);
//   register_gpu_palette(&display_.pixel(), display_.palette_data(), display_.palette_size());
//
//   // In render_frame():
//   display_.clear();
//   uint8_t* idx = display_.indices();
//   // ... write palette indices ...
//   display_.flush();
//
//   // In set_framebuffer() override:
//   display_.pixel().set_framebuffer(buffer, width, height);
// ============================================================================

class DisplaySurface {
public:
    DisplaySurface() = default;
    ~DisplaySurface() {
        delete[] index_buf_;
        delete[] framebuffer_;
    }

    // Non-copyable, movable
    DisplaySurface(const DisplaySurface&) = delete;
    DisplaySurface& operator=(const DisplaySurface&) = delete;
    DisplaySurface(DisplaySurface&& o) noexcept { *this = std::move(o); }
    DisplaySurface& operator=(DisplaySurface&& o) noexcept {
        if (this != &o) {
            delete[] index_buf_;
            delete[] framebuffer_;
            width_       = o.width_;
            height_      = o.height_;
            index_buf_   = o.index_buf_;
            framebuffer_ = o.framebuffer_;
            palette_     = o.palette_;
            pixel_       = o.pixel_;
            o.index_buf_   = nullptr;
            o.framebuffer_ = nullptr;
            o.width_ = o.height_ = 0;
        }
        return *this;
    }

    /// Allocate buffers for the given dimensions.
    /// Safe to call multiple times (re-allocates if size changes).
    void init(int width, int height) {
        if (width == width_ && height == height_ && index_buf_) return;

        delete[] index_buf_;
        delete[] framebuffer_;

        width_  = width;
        height_ = height;
        int total = width * height;

        index_buf_   = new uint8_t[total]();
        framebuffer_ = new uint32_t[total]();

        pixel_.set_framebuffer(framebuffer_, width, height);
    }

    /// Set the palette from a source array (copies data).
    void set_palette(const uint32_t* palette, int count) {
        palette_.set(palette, count);
    }

    /// Set the palette from a PaletteTable.
    void set_palette(const PaletteTable& pal) {
        palette_ = pal;
    }

    /// Direct access to the palette for in-place updates.
    PaletteTable&       palette()       { return palette_; }
    const PaletteTable& palette() const { return palette_; }

    /// Palette data pointer (for register_gpu_palette).
    const uint32_t* palette_data() const { return palette_.data(); }
    int             palette_size() const { return palette_.size(); }

    /// Clear the index buffer to zero (background).
    void clear() {
        if (index_buf_) std::memset(index_buf_, 0, width_ * height_);
    }

    /// Clear the index buffer to a specific index value.
    void clear(uint8_t value) {
        if (index_buf_) std::memset(index_buf_, value, width_ * height_);
    }

    /// Flush indices through the pixel unit (GPU or CPU path).
    void flush() {
        pixel_.flush_indexed_frame(index_buf_, palette_.data());
    }

    /// Flush with an external palette (for systems with runtime palettes).
    void flush(const uint32_t* palette) {
        pixel_.flush_indexed_frame(index_buf_, palette);
    }

    // --- Accessors ---

    uint8_t*        indices()       { return index_buf_; }
    const uint8_t*  indices() const { return index_buf_; }

    uint32_t*       framebuffer()       { return framebuffer_; }
    const uint32_t* framebuffer() const { return framebuffer_; }

    VideoPixelUnit& pixel()       { return pixel_; }
    const VideoPixelUnit& pixel() const { return pixel_; }

    int width()  const { return width_; }
    int height() const { return height_; }

private:
    int             width_       = 0;
    int             height_      = 0;
    uint8_t*        index_buf_   = nullptr;
    uint32_t*       framebuffer_ = nullptr;
    PaletteTable    palette_;
    VideoPixelUnit  pixel_;
};
