#pragma once

#include "core/chip.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// NAMED PALETTE — shared descriptor for user-selectable palette tables
// ============================================================================

struct NamedPalette {
    const char*     id;     // Machine-readable (for config persistence)
    const char*     name;   // Human-readable (for UI display)
    const uint32_t* data;   // Palette color entries
    int             count;  // Number of entries
};

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

    // --- Active palette (current rendering palette) ---
    const uint32_t* system_palette() const { return system_palette_; }
    uint16_t        palette_size()   const { return palette_size_; }

    // --- Named palette registry (user-selectable alternatives) ---
    const NamedPalette* named_palettes()     const { return named_palettes_; }
    int                 named_palette_count() const { return named_palette_count_; }

    // Select palette by id string.  Returns the matched entry, or nullptr.
    // Updates system_palette_ and calls on_palette_selected() for chip-specific
    // post-processing (e.g. NES PPU emphasis cache rebuild).
    const NamedPalette* select_palette(const char* id) {
        for (int i = 0; i < named_palette_count_; ++i) {
            if (std::strcmp(named_palettes_[i].id, id) == 0) {
                system_palette_ = named_palettes_[i].data;
                palette_size_   = static_cast<uint16_t>(named_palettes_[i].count);
                on_palette_selected(named_palettes_[i]);
                return &named_palettes_[i];
            }
        }
        return nullptr;
    }

    /// Override for chip-specific post-processing after palette selection.
    /// Called automatically by select_palette().
    virtual void on_palette_selected(const NamedPalette& /*np*/) {}

protected:
    const uint32_t* system_palette_ = nullptr;
    uint16_t        palette_size_   = 0;

    // Subclasses call this to register their palette table
    void set_named_palettes(const NamedPalette* palettes, int count) {
        named_palettes_ = palettes;
        named_palette_count_ = count;
        // Default to first palette if none set
        if (count > 0 && !system_palette_) {
            system_palette_ = palettes[0].data;
            palette_size_   = static_cast<uint16_t>(palettes[0].count);
        }
    }

private:
    const NamedPalette* named_palettes_ = nullptr;
    int                 named_palette_count_ = 0;
};
