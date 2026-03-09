#pragma once

#include "../../core/chip.h"
#include <cstdint>

// ============================================================================
// VIDEO CHIP BASE — intermediate base for all video/display chips
// ============================================================================
//
// Shared foundation for video chips (VIC-II, TED, VIC 6560/6561, NES PPU,
// TIA, MC6845, Ferranti ULA, MC6847, etc.).  Holds state and accessors
// that are common across video generators:
//
//   - System palette (RGBA color table + size)
//
// Future candidates: output resolution, pixel clock divider, visible area,
// blanking intervals, display standard traits (PAL/NTSC-specific constants
// that recur across chips).

class VideoChipBase : public ChipBase {
public:
    VideoChipBase() { category_ = "Video"; }
    explicit VideoChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Video"; }

    // --- Palette ---
    const uint32_t* system_palette() const { return system_palette_; }
    uint16_t        palette_size()   const { return palette_size_; }

protected:
    const uint32_t* system_palette_ = nullptr;
    uint16_t        palette_size_   = 0;
};
