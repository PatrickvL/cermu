#pragma once
/*
 * gb_ppu.hpp — Game Boy PPU (Pixel Processing Unit) — Sharp SM83/LR35902
 *
 * The Game Boy PPU renders 160×144 pixels at ~59.73 Hz (DMG) / ~61.1 Hz (GBC).
 * It has 4 operating modes per scanline: OAM search, pixel transfer, H-Blank,
 * and V-Blank (10 lines after visible area).
 *
 * Features:
 *   - 160×144 visible display (256×256 virtual background map)
 *   - Two 32×32 tile maps (background + window), 8×8 tiles
 *   - 8KB VRAM (16KB on GBC with bank switching)
 *   - 40 sprites (OAM), 10 per scanline, 8×8 or 8×16
 *   - 4 shades of green (DMG) or 32768 colors (GBC)
 *   - Background scroll (SCX, SCY), window position (WX, WY)
 *   - DMA for OAM transfer
 *   - STAT interrupts (mode transitions, LYC compare)
 *
 * Register map ($FF40–$FF4B):
 *   $FF40: LCDC  — LCD control (enable, window, BG, sprite size, maps, data)
 *   $FF41: STAT  — LCD status (mode, coincidence flag, interrupt enables)
 *   $FF42: SCY   — Background scroll Y
 *   $FF43: SCX   — Background scroll X
 *   $FF44: LY    — Current scanline (read only)
 *   $FF45: LYC   — LY compare (generates STAT interrupt on match)
 *   $FF46: DMA   — OAM DMA source address (high byte)
 *   $FF47: BGP   — Background palette (DMG)
 *   $FF48: OBP0  — Object palette 0 (DMG)
 *   $FF49: OBP1  — Object palette 1 (DMG)
 *   $FF4A: WY    — Window Y position
 *   $FF4B: WX    — Window X position + 7
 */

#include "chip/video/video_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define GB_PPU_DECL(REG, FLD, CMP) \
    REG(0x00, LCDC,  "LCD Control")                                                \
      FLD(LCDC, LCD_EN,        7:7, "LCD enable",             Flag, 0, 0)          \
      FLD(LCDC, WIN_MAP,       6:6, "Window tile map select", Flag, 0, 0)          \
      FLD(LCDC, WIN_EN,        5:5, "Window enable",          Flag, 0, 0)          \
      FLD(LCDC, TILE_DATA,     4:4, "BG/Win tile data area",  Flag, 0, 0)          \
      FLD(LCDC, BG_MAP,        3:3, "BG tile map select",     Flag, 0, 0)          \
      FLD(LCDC, OBJ_SIZE,      2:2, "Sprite size (8×16)",     Flag, 0, 0)          \
      FLD(LCDC, OBJ_EN,        1:1, "Sprite enable",          Flag, 0, 0)          \
      FLD(LCDC, BG_WIN_EN,     0:0, "BG/Window enable",       Flag, 0, 0)          \
    REG(0x01, STAT,  "LCD Status")                                                 \
      FLD(STAT, LYC_INT,       6:6, "LYC=LY interrupt",       Flag, 0, 0)          \
      FLD(STAT, OAM_INT,       5:5, "OAM interrupt",          Flag, 0, 0)          \
      FLD(STAT, VBLANK_INT,    4:4, "V-Blank interrupt",       Flag, 0, 0)          \
      FLD(STAT, HBLANK_INT,    3:3, "H-Blank interrupt",       Flag, 0, 0)          \
      FLD(STAT, LYC_FLAG,      2:2, "LYC coincidence flag",    Flag, 0, 0)          \
      FLD(STAT, MODE,          1:0, "PPU mode",               Value, 0, 0)         \
    REG(0x02, SCY,   "Scroll Y")                                                   \
    REG(0x03, SCX,   "Scroll X")                                                   \
    REG(0x04, LY,    "Current scanline (read-only)")                               \
    REG(0x05, LYC,   "LY compare")                                                \
    REG(0x06, DMA,   "OAM DMA source address")                                    \
    REG(0x07, BGP,   "BG Palette (DMG)")                                           \
    REG(0x08, OBP0,  "OBJ Palette 0 (DMG)")                                       \
    REG(0x09, OBP1,  "OBJ Palette 1 (DMG)")                                       \
    REG(0x0A, WY,    "Window Y position")                                          \
    REG(0x0B, WX,    "Window X position")

namespace gb_ppu {
    namespace reg {
        GB_PPU_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 12;
    }
    using namespace reg;

    inline constexpr int SCREEN_WIDTH  = 160;
    inline constexpr int SCREEN_HEIGHT = 144;
    inline constexpr int VBLANK_LINES  = 10;
    inline constexpr int TOTAL_LINES   = SCREEN_HEIGHT + VBLANK_LINES;
    inline constexpr int DOTS_PER_LINE = 456;
    inline constexpr int VRAM_SIZE     = 8192;
    inline constexpr int OAM_SIZE      = 160;   // 40 sprites × 4 bytes
}

DECL_EXTRACT(GB_PPU, GB_PPU_DECL)

// ============================================================================
// Game Boy PPU — Stub Implementation
// ============================================================================

struct gb_ppu_t : public VideoChipBase {

    gb_ppu_t()
        : VideoChipBase(ChipInfo{"SM83_PPU", "Sharp", "Game Boy Pixel Processing Unit"})
    {
        init_regs(gb_ppu::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GB_PPU_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        if (addr < gb_ppu::reg::REG_COUNT) {
            if (addr == gb_ppu::LY)
                BUS_SET_DATA(bus, ly_);
            else
                BUS_SET_DATA(bus, regs_.data[addr]);
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        uint8_t data = BUS_GET_DATA(bus);
        if (addr < gb_ppu::reg::REG_COUNT && addr != gb_ppu::LY) {
            regs_.data[addr] = data;
            if (addr == gb_ppu::DMA) {
                dma_pending_ = true;
                dma_source_ = static_cast<uint16_t>(data) << 8;
            }
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Mode state machine (OAM search → pixel transfer → H-Blank → V-Blank)
        // TODO: Scanline rendering, sprite evaluation, STAT interrupts
        dot_counter_++;
        if (dot_counter_ >= gb_ppu::DOTS_PER_LINE) {
            dot_counter_ = 0;
            ly_++;
            if (ly_ >= gb_ppu::TOTAL_LINES)
                ly_ = 0;
        }
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, gb_ppu::reg::REG_COUNT);
        regs_.data[gb_ppu::LCDC] = 0x91;  // LCD on, BG on
        regs_.data[gb_ppu::BGP]  = 0xFC;  // Default palette
        ly_ = 0;
        dot_counter_ = 0;
        dma_pending_ = false;
        dma_source_ = 0;
        std::memset(vram_, 0, sizeof(vram_));
        std::memset(oam_, 0, sizeof(oam_));
    }

    // ── VRAM / OAM ──────────────────────────────────────────────────
    uint8_t vram_[gb_ppu::VRAM_SIZE] = {};
    uint8_t oam_[gb_ppu::OAM_SIZE] = {};

    uint8_t ly_ = 0;
    uint16_t dot_counter_ = 0;
    bool    dma_pending_ = false;
    uint16_t dma_source_ = 0;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("GB PPU — LCD Control")
            .value("LCDC", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::LCDC]; })
            .value("STAT", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::STAT]; })
            .category("GB PPU — Position")
            .value("LY", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->ly_; })
            .value("SCX", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::SCX]; })
            .value("SCY", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::SCY]; })
            .category("GB PPU — Palette")
            .value("BGP", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::BGP]; });
    }
#endif
};
