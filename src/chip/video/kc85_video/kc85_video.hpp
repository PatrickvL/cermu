#pragma once

// ============================================================================
// KC85 Video Generator — memory-mapped bitmap display
// ============================================================================
//
// Models the video generation circuitry of the KC85 computer family:
//
//   KC85/2, /3: Discrete TTL logic driving a 320×256 bitmap from IRM
//               (Image RAM).  ZX Spectrum-like interleaved addressing with
//               separate pixel and color areas.
//
//   KC85/4:     U82720 custom ASIC driving a 320×256 bitmap from dual-
//               plane IRM with per-byte color resolution and switchable
//               active plane.
//
// The generator takes a pointer to the IRM and produces indexed pixel
// output + a composite video stream.  It does not own the IRM — the
// system / memory subsystem manages the RAM chip.
//
// Blink support: when enabled (PIO-B bit 7), characters with color byte
// bit 7 set alternate between fg and bg based on a blink timer.
// ============================================================================

#include "core/signal/composite_video_out.hpp"
#include "core/signal/sync_flag.hpp"

#include <cstdint>

namespace kc85_video_constants {
    inline constexpr int WIDTH           = 320;
    inline constexpr int HEIGHT          = 256;
    inline constexpr int KC4_PIXEL_COLS  = 40;     // KC85/4: 40 byte-columns
    inline constexpr int BG_COLOR_OFFSET = 16;     // bg colors start at palette index 16
}

// KC85/2,3 vs KC85/4 video mode selection
enum class KC85VideoMode : uint8_t {
    Standard,   // KC85/2, /3: ZX Spectrum-like interleaved addressing
    Extended,   // KC85/4: dual-plane with per-byte color, column-major layout
};

struct KC85VideoGenerator {
    // --- Configuration (set once at init) ---
    void set_stream(CompositeVideoOut* s) { video_stream_ = s; }
    void set_mode(KC85VideoMode m) { mode_ = m; }

    // --- Per-frame dynamic state (set by system before render_frame) ---
    void set_irm(const uint8_t* irm) { irm_ = irm; }
    void set_blink_bg(bool b) { blink_bg_ = b; }
    void set_active_plane(int p) { active_plane_ = p; }  // KC85/4 only

    // --- Render one frame ---
    // Decodes IRM into display (if set) and drives video stream (if set).
    void render_frame();

private:
    void render_standard();
    void render_extended();
    void drive_stream();

    CompositeVideoOut* video_stream_ = nullptr;
    const uint8_t*       irm_          = nullptr;
    KC85VideoMode        mode_         = KC85VideoMode::Standard;
    bool                 blink_bg_     = false;
    int                  active_plane_ = 0;
    uint8_t              pixel_buf_[kc85_video_constants::WIDTH * kc85_video_constants::HEIGHT] = {};
};
