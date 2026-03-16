#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

// ============================================================================
// PaletteTable — fixed-capacity RGBA palette with optional runtime updates
// ============================================================================
//
// A lightweight value type that wraps a palette of up to 256 RGBA colors.
// Used by:
//   - Video chips that own a fixed palette (VIC-II 16 colors, TED 128, …)
//   - Systems that decode palettes from PROM/RAM (Namco, BombJack)
//   - DisplaySurface for GPU indexed rendering registration
//
// All entries are 0xAARRGGBB (ABGR little-endian) — the format used by
// OpenGL GL_RGBA8 textures and ImGui.
//
// Two usage patterns:
//
//   Static palette (compile-time data):
//     PaletteTable pal(spectrum_ula::PALETTE, 16);
//     // pal.data() returns a pointer to its internal copy
//
//   Dynamic palette (decoded at runtime):
//     PaletteTable pal(128);
//     pal.decode_from(ram_ptr, 128, my_decode_fn);
//
// Maximum 256 entries — sufficient for all current systems (TIA=128,
// TED=128, BombJack=128, NES emphasis cache handled separately).
// ============================================================================

class PaletteTable {
public:
    static constexpr int MAX_ENTRIES = 256;

    PaletteTable() = default;

    /// Construct with a fixed palette (copies data).
    PaletteTable(const uint32_t* source, int count) {
        set(source, count);
    }

    /// Construct with a given capacity (entries zeroed).
    explicit PaletteTable(int count) : count_(std::min(count, MAX_ENTRIES)) {
        std::memset(entries_, 0, sizeof(entries_));
    }

    /// Replace the entire palette with new data.
    void set(const uint32_t* source, int count) {
        count_ = std::min(count, MAX_ENTRIES);
        std::memcpy(entries_, source, count_ * sizeof(uint32_t));
    }

    /// Set a single entry.
    void set_entry(int index, uint32_t rgba) {
        if (index >= 0 && index < count_) entries_[index] = rgba;
    }

    /// Decode from raw hardware bytes using a caller-provided function.
    /// The function signature: uint32_t decode(uint8_t raw_byte)
    template<typename DecodeFn>
    void decode_from(const uint8_t* raw, int count, DecodeFn fn) {
        count_ = std::min(count, MAX_ENTRIES);
        for (int i = 0; i < count_; i++) {
            entries_[i] = fn(raw[i]);
        }
    }

    /// Read-only access to palette entries.
    const uint32_t* data()  const { return entries_; }
    int             size()  const { return count_; }
    bool            empty() const { return count_ == 0; }

    uint32_t operator[](int index) const {
        return (index >= 0 && index < count_) ? entries_[index] : 0;
    }

private:
    uint32_t entries_[MAX_ENTRIES] = {};
    int      count_ = 0;
};
