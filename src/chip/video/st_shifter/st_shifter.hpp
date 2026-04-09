#pragma once
/*
 * st_shifter.hpp — Atari ST Shifter (video display chip)
 *
 * The Shifter is the Atari ST's video DAC and shift register, generating
 * the pixel stream from a shared DRAM framebuffer.  It has:
 *
 *   - 16-entry palette (STd: 3-3-3 RGB = 512 colors, STe: 4-4-4 = 4096)
 *   - 3 resolution modes: low (320×200×16), medium (640×200×4), high (640×400×2)
 *   - DMA-driven framebuffer read from shared RAM
 *   - Video base address (read by DMA controller, 256-byte aligned)
 *
 * Register map (memory-mapped):
 *   $FF8201: Video base high byte
 *   $FF8203: Video base mid byte
 *   $FF8205: Video counter high (read-only)
 *   $FF8207: Video counter mid (read-only)
 *   $FF8209: Video counter low (read-only)
 *   $FF820A: Sync mode (50/60 Hz, internal/external)
 *   $FF820D: Video base low byte (STE only)
 *   $FF8240–$FF825F: Palette (16 entries × 16 bits)
 *   $FF8260: Resolution (bits 1:0)
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define ST_SHIFTER_DECL(REG, FLD, CMP) \
    REG(0x00, VBASE_H,   "Video Base Address High")                              \
    REG(0x01, VBASE_M,   "Video Base Address Mid")                               \
    REG(0x02, VCTR_H,    "Video Counter High (RO)")                              \
    REG(0x03, VCTR_M,    "Video Counter Mid (RO)")                               \
    REG(0x04, VCTR_L,    "Video Counter Low (RO)")                               \
    REG(0x05, SYNC,      "Sync Mode")                                            \
      FLD(SYNC, FREQ,      1:1, "Frequency (0=60Hz, 1=50Hz)", Flag,  0, 0)      \
      FLD(SYNC, EXT_SYNC,  0:0, "External sync",             Flag,  0, 0)       \
    REG(0x06, VBASE_L,   "Video Base Address Low (STE)")                         \
    REG(0x07, RES,       "Resolution")                                           \
      FLD(RES, MODE,       1:0, "Mode (0=low, 1=med, 2=high)", Value, 0, 0)

namespace st_shifter {
    namespace reg {
        ST_SHIFTER_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 8;
    }
    using namespace reg;

    inline constexpr int PALETTE_SIZE = 16;

    // Display modes
    inline constexpr uint8_t MODE_LOW  = 0;  // 320×200, 16 colors
    inline constexpr uint8_t MODE_MED  = 1;  // 640×200, 4 colors
    inline constexpr uint8_t MODE_HIGH = 2;  // 640×400, 2 colors (mono)

    // Screen sizes
    inline constexpr int LOW_WIDTH = 320, LOW_HEIGHT = 200;
    inline constexpr int MED_WIDTH = 640, MED_HEIGHT = 200;
    inline constexpr int HI_WIDTH  = 640, HI_HEIGHT  = 400;
}

DECL_EXTRACT(ST_SHIFTER, ST_SHIFTER_DECL)

// ============================================================================
// Atari ST Shifter — Full Implementation
// ============================================================================

struct st_shifter_t : public VideoChipBase {

    st_shifter_t()
        : VideoChipBase(ChipInfo{"ST_Shifter", "Atari", "ST Shifter Video Chip"})
    {
        init_regs(st_shifter::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(ST_SHIFTER_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    // Read hardware register (address relative to $FF8200)
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t offset = BUS_GET_ADDR(bus) & 0x7F;

        // Palette ($FF8240–$FF825F → offset $40–$5F)
        if (offset >= 0x40 && offset < 0x60) {
            uint8_t idx = (offset - 0x40) >> 1;
            if (offset & 1)
                BUS_SET_DATA(bus, palette_[idx] & 0xFF);
            else
                BUS_SET_DATA(bus, (palette_[idx] >> 8) & 0x0F);
            return bus;
        }

        switch (offset) {
            case 0x01: BUS_SET_DATA(bus, video_base_h_); break;
            case 0x03: BUS_SET_DATA(bus, video_base_m_); break;
            case 0x05: BUS_SET_DATA(bus, (video_counter_ >> 16) & 0x3F); break;
            case 0x07: BUS_SET_DATA(bus, (video_counter_ >> 8) & 0xFF); break;
            case 0x09: BUS_SET_DATA(bus, video_counter_ & 0xFF); break;
            case 0x0A: BUS_SET_DATA(bus, regs_.data[st_shifter::SYNC]); break;
            case 0x0D: BUS_SET_DATA(bus, video_base_l_); break;
            case 0x60: BUS_SET_DATA(bus, regs_.data[st_shifter::RES]); break;
            default:   BUS_SET_DATA(bus, 0xFF); break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t offset = BUS_GET_ADDR(bus) & 0x7F;
        uint8_t data = BUS_GET_DATA(bus);

        // Palette
        if (offset >= 0x40 && offset < 0x60) {
            uint8_t idx = (offset - 0x40) >> 1;
            if (offset & 1)
                palette_[idx] = (palette_[idx] & 0x0F00) | data;
            else
                palette_[idx] = (palette_[idx] & 0x00FF) | ((data & 0x0F) << 8);
            update_rgba_palette(idx);
            return bus;
        }

        switch (offset) {
            case 0x01: video_base_h_ = data & 0x3F; break;
            case 0x03: video_base_m_ = data; break;
            case 0x0A: regs_.data[st_shifter::SYNC] = data & 0x03; break;
            case 0x0D: video_base_l_ = data;  break;  // STE only
            case 0x60: regs_.data[st_shifter::RES] = data & 0x03; break;
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // Advance video counter (DMA pointer for current scanline position)
        // One word (2 bytes) fetched per 4 pixels in low-res
        video_counter_ += 2;
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, st_shifter::reg::REG_COUNT);
        std::memset(palette_, 0, sizeof(palette_));
        std::memset(rgba_palette_, 0, sizeof(rgba_palette_));
        video_base_h_ = 0;
        video_base_m_ = 0;
        video_base_l_ = 0;
        video_counter_ = 0;
    }

    // ── Accessors ────────────────────────────────────────────────
    uint32_t video_base_address() const noexcept {
        return (static_cast<uint32_t>(video_base_h_) << 16) |
               (static_cast<uint32_t>(video_base_m_) << 8) |
               video_base_l_;
    }

    uint8_t resolution_mode() const noexcept {
        return regs_.data[st_shifter::RES] & 0x03;
    }

    bool is_50hz() const noexcept {
        return (regs_.data[st_shifter::SYNC] & 0x02) != 0;
    }

    void start_of_frame() noexcept {
        video_counter_ = video_base_address();
    }

    // ── Palette ──────────────────────────────────────────────────
    uint16_t palette_[st_shifter::PALETTE_SIZE] = {};
    uint32_t rgba_palette_[st_shifter::PALETTE_SIZE] = {};  // Precomputed RGBA

    uint8_t  video_base_h_ = 0;
    uint8_t  video_base_m_ = 0;
    uint8_t  video_base_l_ = 0;   // STE only
    uint32_t video_counter_ = 0;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    void update_rgba_palette(uint8_t idx) noexcept {
        uint16_t val = palette_[idx];
        // ST palette: bits 8-10 = R, bits 4-6 = G, bits 0-2 = B (3-3-3)
        uint8_t r = ((val >> 8) & 0x07) * 36;
        uint8_t g = ((val >> 4) & 0x07) * 36;
        uint8_t b = ((val >> 0) & 0x07) * 36;
        rgba_palette_[idx] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("Shifter — Video")
            .value("Resolution", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->resolution_mode(); })
            .value("Video Base", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->video_base_address(); })
            .value("Video Ctr", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->video_counter_; })
            .category("Shifter — Sync")
            .value("Sync Mode", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->regs_.data[st_shifter::SYNC]; })
            .category("Shifter — Palette")
            .value("Color 0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->palette_[0]; })
            .value("Color 1", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const st_shifter_t*>(c)->palette_[1]; });
    }
#endif
};
