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
 *
 * Mode timing per scanline (456 dots total):
 *   Mode 2 (OAM search):     80 dots   — scan OAM for sprites on this line
 *   Mode 3 (Pixel transfer): ~172 dots  — fetch tiles, mix BG/WIN/OBJ, push pixels
 *   Mode 0 (H-Blank):        ~204 dots  — idle until end of scanline
 *   Mode 1 (V-Blank):        4560 dots  — 10 blank lines (scanlines 144–153)
 */

#include "chip/video/video_chip_base.hpp"
#include "core/signal/composite_video_out.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

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
    inline constexpr int VRAM_SIZE      = 8192;
    inline constexpr int VRAM_BANK_SIZE = 8192;
    inline constexpr int VRAM_TOTAL     = 16384;  // 2 banks (CGB)
    inline constexpr int OAM_SIZE       = 160;    // 40 sprites × 4 bytes

    // Mode durations (in dots)
    inline constexpr int OAM_SEARCH_DOTS    = 80;    // Mode 2
    inline constexpr int PIXEL_TRANSFER_DOTS = 172;  // Mode 3 (minimum, varies with sprites/scroll)
    inline constexpr int HBLANK_DOTS        = 204;   // Mode 0 (maximum, fills remainder)

    // PPU modes
    inline constexpr uint8_t MODE_HBLANK = 0;
    inline constexpr uint8_t MODE_VBLANK = 1;
    inline constexpr uint8_t MODE_OAM    = 2;
    inline constexpr uint8_t MODE_XFER   = 3;

    // DMG palette: 4 shades (lightest to darkest)
    // Classic green-tint palette matching original Game Boy LCD
    inline constexpr uint32_t DMG_PALETTE[4] = {
        0xFFE0F8D0,  // Color 0: lightest (near-white green)
        0xFF88C070,  // Color 1: light green
        0xFF346856,  // Color 2: dark green
        0xFF081820,  // Color 3: darkest (near-black)
    };
}

DECL_EXTRACT(GB_PPU, GB_PPU_DECL)

// ============================================================================
// Game Boy PPU — Full DMG Implementation
// ============================================================================

struct gb_ppu_t : public VideoChipBase {

    gb_ppu_t()
        : VideoChipBase(ChipInfo{"SM83_PPU", "Sharp", "Game Boy Pixel Processing Unit"})
    {
        init_regs(gb_ppu::reg::REG_COUNT);

        // Set up DMG palette for display pipeline
        system_palette_ = gb_ppu::DMG_PALETTE;
        palette_size_   = 4;

        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GB_PPU_REG_INFO);
        register_debug_fields();
#endif
    }

    // ── Video output ─────────────────────────────────────────────────
    CompositeVideoOut* video_out_ = nullptr;
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }

    bool headless_ = false;  // Skip rendering/video output for maximum throughput

    // ── CGB mode ─────────────────────────────────────────────────────
    void set_cgb_mode(bool enabled) {
        cgb_mode_ = enabled;
        if (enabled) {
            system_palette_ = cgb_palette_rgba_;
            palette_size_ = 64;
        }
    }

    static inline uint32_t rgb555_to_rgba(uint16_t c) {
        uint8_t r = (c & 0x1F); r = (r << 3) | (r >> 2);
        uint8_t g = ((c >> 5) & 0x1F); g = (g << 3) | (g >> 2);
        uint8_t b = ((c >> 10) & 0x1F); b = (b << 3) | (b >> 2);
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    // CGB palette register access ($FF68–$FF6B) — called by system I/O dispatch
    void write_bgpi(uint8_t data) { bgpi_ = data; }
    uint8_t read_bgpi() const { return bgpi_; }
    void write_bgpd(uint8_t data) {
        uint8_t idx = bgpi_ & 0x3F;
        cgb_bg_pal_[idx] = data;
        int pair = idx & ~1;
        uint16_t rgb = cgb_bg_pal_[pair] | (cgb_bg_pal_[pair + 1] << 8);
        cgb_palette_rgba_[pair / 2] = rgb555_to_rgba(rgb);
        if (bgpi_ & 0x80) bgpi_ = 0x80 | ((idx + 1) & 0x3F);
    }
    uint8_t read_bgpd() const { return cgb_bg_pal_[bgpi_ & 0x3F]; }
    void write_obpi(uint8_t data) { obpi_ = data; }
    uint8_t read_obpi() const { return obpi_; }
    void write_obpd(uint8_t data) {
        uint8_t idx = obpi_ & 0x3F;
        cgb_obj_pal_[idx] = data;
        int pair = idx & ~1;
        uint16_t rgb = cgb_obj_pal_[pair] | (cgb_obj_pal_[pair + 1] << 8);
        cgb_palette_rgba_[32 + pair / 2] = rgb555_to_rgba(rgb);
        if (obpi_ & 0x80) obpi_ = 0x80 | ((idx + 1) & 0x3F);
    }
    uint8_t read_obpd() const { return cgb_obj_pal_[obpi_ & 0x3F]; }

    bool has_mmio() const override { return true; }

    // ── CS-tick: self-dispatch register access when chip-selected ────
    bus_state_t tick_mmio(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        if (addr < gb_ppu::reg::REG_COUNT) {
            if (addr == gb_ppu::LY) {
                BUS_SET_DATA(bus, ly_);
            } else if (addr == gb_ppu::STAT) {
                // STAT: bits 0-2 are read-only (mode + LYC flag), bits 3-6 are R/W
                uint8_t stat = (regs_.data[gb_ppu::STAT] & 0x78) | (mode_ & 0x03);
                if (ly_ == regs_.data[gb_ppu::LYC])
                    stat |= 0x04;  // LYC coincidence flag
                BUS_SET_DATA(bus, stat | 0x80);  // Bit 7 always 1
            } else {
                BUS_SET_DATA(bus, regs_.data[addr]);
            }
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x0F;
        uint8_t data = BUS_GET_DATA(bus);
        if (addr < gb_ppu::reg::REG_COUNT && addr != gb_ppu::LY) {
            if (addr == gb_ppu::STAT) {
                // Only bits 3-6 are writable
                regs_.data[gb_ppu::STAT] = (regs_.data[gb_ppu::STAT] & 0x07) | (data & 0x78);
            } else {
                regs_.data[addr] = data;
            }
            if (addr == gb_ppu::DMA) {
                dma_pending_ = true;
                dma_source_ = static_cast<uint16_t>(data) << 8;
            }
        }
        return bus;
    }

    // ── Per-dot tick ─────────────────────────────────────────────────

    // Returns a STAT interrupt request flag (bit 1 of IF) when STAT conditions are met.
    // The caller (system tick) is responsible for ORing this into io_regs_[IF].
    // Returns: bitmask for IF register (bit 0 = VBlank, bit 1 = STAT)
    uint8_t tick() noexcept {
        uint8_t irq = 0;
        uint8_t lcdc = regs_.data[gb_ppu::LCDC];

        // LCD disabled — blank output, stay in mode 0, LY=0
        // Must still track dot timing to emit periodic FrameEnd and prevent
        // signal buffer overrun.
        if (!(lcdc & 0x80)) {
            if (unlikely(!headless_) && video_out_) {
                SyncFlag flags = SyncFlag::Blank;
                if (lcd_off_counter_ == 0)
                    flags = flags | SyncFlag::FrameEnd;
                video_out_->drive({0, flags});
            }
            lcd_off_counter_++;
            if (lcd_off_counter_ >= gb_ppu::DOTS_PER_LINE * gb_ppu::TOTAL_LINES)
                lcd_off_counter_ = 0;
            dot_counter_ = 0;
            ly_ = 0;
            mode_ = gb_ppu::MODE_HBLANK;
            stat_irq_line_ = false;
            window_line_counter_ = 0;
            return 0;
        }
        lcd_off_counter_ = 0;

        uint8_t prev_mode = mode_;

        // Determine current mode based on scanline and dot position
        if (ly_ >= gb_ppu::SCREEN_HEIGHT) {
            mode_ = gb_ppu::MODE_VBLANK;
        } else if (dot_counter_ < gb_ppu::OAM_SEARCH_DOTS) {
            mode_ = gb_ppu::MODE_OAM;
        } else if (dot_counter_ < gb_ppu::OAM_SEARCH_DOTS + gb_ppu::PIXEL_TRANSFER_DOTS) {
            mode_ = gb_ppu::MODE_XFER;
        } else {
            mode_ = gb_ppu::MODE_HBLANK;
        }

        // Mode transition events
        if (mode_ != prev_mode) {
            switch (mode_) {
                case gb_ppu::MODE_OAM:
                    // OAM search start: evaluate sprites for this scanline
                    if (unlikely(!headless_)) oam_search();
                    break;
                case gb_ppu::MODE_XFER:
                    // Pixel transfer start: render the scanline
                    if (unlikely(!headless_)) render_scanline();
                    scanline_pixel_ = 0;
                    break;
                case gb_ppu::MODE_HBLANK:
                    break;
                case gb_ppu::MODE_VBLANK:
                    irq |= 0x01;  // VBlank interrupt (IF bit 0) — always fires
                    break;
            }
        }

        // Drive video output (skip in headless mode)
        if (unlikely(!headless_) && video_out_) {
            SyncFlag flags = SyncFlag::None;

            // HSync pulse: active at dot 0, cleared at dot 1 to create
            // the falling edge that the signal shader uses for scanline
            // detection.
            if (dot_counter_ == 0) {
                flags = flags | SyncFlag::HSync;
            }
            if (ly_ >= gb_ppu::SCREEN_HEIGHT) {
                flags = flags | SyncFlag::VSync | SyncFlag::Blank;
            }
            if (ly_ == 0 && dot_counter_ == 0) {
                flags = flags | SyncFlag::FrameEnd;
            }

            // During pixel transfer, output the rendered pixels
            uint8_t color = 0;
            if (mode_ == gb_ppu::MODE_XFER && scanline_pixel_ < gb_ppu::SCREEN_WIDTH) {
                color = scanline_buffer_[scanline_pixel_++];
            } else {
                flags = flags | SyncFlag::Blank;
            }

            video_out_->drive({color, flags});
        }

        // STAT IRQ line — active when ANY enabled condition holds.
        // Real hardware uses a single wired-OR line; the interrupt only fires
        // on the rising edge (0→1).  Without this, games like Pinball Fantasies
        // and Pinball Deluxe misfire because a mode-change that keeps the line
        // high (e.g. OAM→HBlank when both are enabled) would spuriously re-assert IF.
        uint8_t stat = regs_.data[gb_ppu::STAT];
        bool stat_line = false;
        if ((stat & 0x20) && mode_ == gb_ppu::MODE_OAM)    stat_line = true;
        if ((stat & 0x08) && mode_ == gb_ppu::MODE_HBLANK) stat_line = true;
        if ((stat & 0x10) && mode_ == gb_ppu::MODE_VBLANK) stat_line = true;

        // LYC compare check (coincidence) — active the entire scanline
        bool lyc_match = (ly_ == regs_.data[gb_ppu::LYC]);
        if ((stat & 0x40) && lyc_match) stat_line = true;

        // Rising edge → set STAT interrupt in IF
        if (stat_line && !stat_irq_line_) {
            irq |= 0x02;
        }
        stat_irq_line_ = stat_line;

        // Advance dot counter and scanline
        dot_counter_++;
        if (dot_counter_ >= gb_ppu::DOTS_PER_LINE) {
            dot_counter_ = 0;
            ly_++;
            if (ly_ >= gb_ppu::TOTAL_LINES) {
                ly_ = 0;
                window_line_counter_ = 0;
            }
        }

        return irq;
    }

    void reset() override {
        std::memset(regs_.data, 0, gb_ppu::reg::REG_COUNT);
        regs_.data[gb_ppu::LCDC] = 0x91;  // LCD on, BG on
        regs_.data[gb_ppu::BGP]  = 0xFC;  // Default palette
        ly_ = 0;
        dot_counter_ = 0;
        mode_ = gb_ppu::MODE_OAM;
        stat_irq_line_ = false;
        scanline_pixel_ = 0;
        window_line_counter_ = 0;
        dma_pending_ = false;
        dma_source_ = 0;
        sprite_count_ = 0;
        lcd_off_counter_ = 0;
        std::memset(vram_, 0, sizeof(vram_));
        std::memset(oam_, 0, sizeof(oam_));
        std::memset(scanline_buffer_, 0, sizeof(scanline_buffer_));
        std::memset(bg_priority_, 0, sizeof(bg_priority_));
        std::memset(bg_color_index_, 0, sizeof(bg_color_index_));
        // CGB state
        vram_bank_ = 0;
        bgpi_ = 0;
        obpi_ = 0;
        std::memset(cgb_bg_pal_, 0, sizeof(cgb_bg_pal_));
        std::memset(cgb_obj_pal_, 0, sizeof(cgb_obj_pal_));
        std::memset(cgb_palette_rgba_, 0, sizeof(cgb_palette_rgba_));
    }

    // ── VRAM / OAM ──────────────────────────────────────────────────
    uint8_t vram_[gb_ppu::VRAM_TOTAL] = {};  // 16KB: 2 banks (CGB), DMG uses bank 0 only
    uint8_t oam_[gb_ppu::OAM_SIZE] = {};

    // CGB extensions
    bool cgb_mode_ = false;
    uint8_t vram_bank_ = 0;              // CPU VRAM bank select (0 or 1)
    uint8_t cgb_bg_pal_[64] = {};        // BG palette RAM (8 pal × 4 colors × 2 bytes)
    uint8_t cgb_obj_pal_[64] = {};       // OBJ palette RAM
    uint8_t bgpi_ = 0;                   // BG Palette Index ($FF68)
    uint8_t obpi_ = 0;                   // OBJ Palette Index ($FF6A)
    uint32_t cgb_palette_rgba_[64] = {};  // Computed RGBA (0–31=BG, 32–63=OBJ)

    uint8_t  ly_ = 0;
    uint16_t dot_counter_ = 0;
    uint8_t  mode_ = gb_ppu::MODE_OAM;
    bool     stat_irq_line_ = false;  // Previous state of combined STAT IRQ line (for edge detection)
    bool     dma_pending_ = false;
    uint16_t dma_source_ = 0;
    uint32_t lcd_off_counter_ = 0;  // FrameEnd timing when LCD is off

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    // Scanline rendering state
    uint8_t scanline_buffer_[gb_ppu::SCREEN_WIDTH] = {};  // Color indices for current line
    bool    bg_priority_[gb_ppu::SCREEN_WIDTH] = {};      // BG-to-OAM priority per pixel
    uint8_t bg_color_index_[gb_ppu::SCREEN_WIDTH] = {};   // CGB: raw BG color ID for priority
    uint8_t scanline_pixel_ = 0;                          // Current pixel being output
    uint8_t window_line_counter_ = 0;                     // Window internal line counter

    // Sprite evaluation results (max 10 sprites per scanline)
    struct SpriteEntry {
        uint8_t y;
        uint8_t x;
        uint8_t tile;
        uint8_t attr;
    };
    SpriteEntry sprite_buffer_[10] = {};
    uint8_t sprite_count_ = 0;

    // ── OAM search: find up to 10 sprites on this scanline ───────────
    void oam_search() noexcept {
        uint8_t lcdc = regs_.data[gb_ppu::LCDC];
        int sprite_height = (lcdc & 0x04) ? 16 : 8;
        sprite_count_ = 0;

        for (int i = 0; i < 40 && sprite_count_ < 10; i++) {
            uint8_t sy = oam_[i * 4 + 0];  // Y position (screen Y + 16)
            uint8_t sx = oam_[i * 4 + 1];  // X position (screen X + 8)
            uint8_t tile = oam_[i * 4 + 2];
            uint8_t attr = oam_[i * 4 + 3];

            int screen_y = sy - 16;
            if (ly_ >= screen_y && ly_ < screen_y + sprite_height) {
                sprite_buffer_[sprite_count_++] = { sy, sx, tile, attr };
            }
        }
    }

    // ── Render one scanline into scanline_buffer_ ────────────────────
    void render_scanline() noexcept {
        uint8_t lcdc = regs_.data[gb_ppu::LCDC];
        uint8_t bgp  = regs_.data[gb_ppu::BGP];

        std::memset(scanline_buffer_, 0, gb_ppu::SCREEN_WIDTH);
        std::memset(bg_priority_, 0, gb_ppu::SCREEN_WIDTH);
        if (cgb_mode_) std::memset(bg_color_index_, 0, gb_ppu::SCREEN_WIDTH);

        // Background and window
        // CGB: BG/Win always rendered (LCDC.0 only affects sprite priority)
        // DMG: BG/Win rendered only when LCDC.0=1
        if (cgb_mode_ || (lcdc & 0x01)) {
            render_bg_line(lcdc, bgp);
            if (lcdc & 0x20) {
                render_window_line(lcdc, bgp);
            }
        }

        // Sprites
        if (lcdc & 0x02) {
            render_sprites(lcdc);
        }
    }

    // ── Background rendering ─────────────────────────────────────────
    void render_bg_line(uint8_t lcdc, uint8_t bgp) noexcept {
        uint8_t scy = regs_.data[gb_ppu::SCY];
        uint8_t scx = regs_.data[gb_ppu::SCX];

        // Tile map base: $9800 (LCDC bit 3 = 0) or $9C00 (LCDC bit 3 = 1)
        uint16_t map_base = (lcdc & 0x08) ? 0x1C00 : 0x1800;

        // Tile data base: $8800 signed (LCDC bit 4 = 0) or $8000 unsigned (LCDC bit 4 = 1)
        bool unsigned_addr = (lcdc & 0x10) != 0;

        uint8_t y = static_cast<uint8_t>(scy + ly_);
        uint8_t tile_row = y >> 3;    // Which row of tiles (0-31)
        uint8_t fine_y = y & 0x07;    // Which pixel row within the tile (0-7)

        for (int px = 0; px < gb_ppu::SCREEN_WIDTH; px++) {
            uint8_t x = static_cast<uint8_t>(scx + px);
            uint8_t tile_col = x >> 3;
            uint8_t fine_x = x & 0x07;

            // Fetch tile index from tile map (always VRAM bank 0)
            uint16_t map_addr = map_base + tile_row * 32 + tile_col;
            uint8_t tile_idx = vram_[map_addr];

            // Calculate tile data address
            uint16_t tile_addr;
            if (unsigned_addr) {
                tile_addr = tile_idx * 16;  // $8000 base, unsigned
            } else {
                // $9000 base, signed index (-128 to 127)
                tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile_idx) * 16);
            }

            if (cgb_mode_) {
                // CGB: read tile attributes from VRAM bank 1
                uint8_t attr = vram_[gb_ppu::VRAM_BANK_SIZE + map_addr];
                uint8_t pal = attr & 0x07;
                uint16_t bank_off = (attr & 0x08) ? gb_ppu::VRAM_BANK_SIZE : 0;
                uint8_t eff_y = (attr & 0x40) ? (7 - fine_y) : fine_y;

                uint8_t lo = vram_[bank_off + tile_addr + eff_y * 2];
                uint8_t hi = vram_[bank_off + tile_addr + eff_y * 2 + 1];
                uint8_t bit = (attr & 0x20) ? fine_x : (7 - fine_x);
                uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

                scanline_buffer_[px] = pal * 4 + color_id;
                bg_color_index_[px] = color_id;
                bg_priority_[px] = (attr & 0x80) != 0;
            } else {
                // Each tile row is 2 bytes: low byte + high byte
                uint8_t lo = vram_[tile_addr + fine_y * 2];
                uint8_t hi = vram_[tile_addr + fine_y * 2 + 1];

                // Extract 2-bit color (bit 7 = leftmost pixel)
                uint8_t bit = 7 - fine_x;
                uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

                // Apply BGP palette mapping
                uint8_t color = (bgp >> (color_id * 2)) & 0x03;
                scanline_buffer_[px] = color;
                bg_priority_[px] = (color_id != 0);  // Non-zero BG has priority in BG-to-OBJ
            }
        }
    }

    // ── Window rendering ─────────────────────────────────────────────
    void render_window_line(uint8_t lcdc, uint8_t bgp) noexcept {
        uint8_t wy = regs_.data[gb_ppu::WY];
        uint8_t wx = regs_.data[gb_ppu::WX];

        if (ly_ < wy) return;
        if (wx > 166) return;  // WX=166 means window is off-screen

        int win_x_start = wx - 7;  // WX=7 means window starts at screen X=0

        // Window tile map: $9800 (LCDC bit 6 = 0) or $9C00 (LCDC bit 6 = 1)
        uint16_t map_base = (lcdc & 0x40) ? 0x1C00 : 0x1800;
        bool unsigned_addr = (lcdc & 0x10) != 0;

        uint8_t win_y = window_line_counter_;
        uint8_t tile_row = win_y >> 3;
        uint8_t fine_y = win_y & 0x07;

        bool rendered = false;
        for (int px = std::max(0, win_x_start); px < gb_ppu::SCREEN_WIDTH; px++) {
            int win_px = px - win_x_start;
            uint8_t tile_col = static_cast<uint8_t>(win_px) >> 3;
            uint8_t fine_x = static_cast<uint8_t>(win_px) & 0x07;

            uint16_t map_addr = map_base + tile_row * 32 + tile_col;
            uint8_t tile_idx = vram_[map_addr];

            uint16_t tile_addr;
            if (unsigned_addr) {
                tile_addr = tile_idx * 16;
            } else {
                tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile_idx) * 16);
            }

            if (cgb_mode_) {
                uint8_t attr = vram_[gb_ppu::VRAM_BANK_SIZE + map_addr];
                uint8_t pal = attr & 0x07;
                uint16_t bank_off = (attr & 0x08) ? gb_ppu::VRAM_BANK_SIZE : 0;
                uint8_t eff_y = (attr & 0x40) ? (7 - fine_y) : fine_y;

                uint8_t lo = vram_[bank_off + tile_addr + eff_y * 2];
                uint8_t hi = vram_[bank_off + tile_addr + eff_y * 2 + 1];
                uint8_t bit = (attr & 0x20) ? fine_x : (7 - fine_x);
                uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

                scanline_buffer_[px] = pal * 4 + color_id;
                bg_color_index_[px] = color_id;
                bg_priority_[px] = (attr & 0x80) != 0;
            } else {
                uint8_t lo = vram_[tile_addr + fine_y * 2];
                uint8_t hi = vram_[tile_addr + fine_y * 2 + 1];
                uint8_t bit = 7 - fine_x;
                uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
                uint8_t color = (bgp >> (color_id * 2)) & 0x03;

                scanline_buffer_[px] = color;
                bg_priority_[px] = (color_id != 0);
            }
            rendered = true;
        }

        if (rendered) window_line_counter_++;
    }

    // ── Sprite rendering ─────────────────────────────────────────────
    void render_sprites(uint8_t lcdc) noexcept {
        int sprite_height = (lcdc & 0x04) ? 16 : 8;

        // Render sprites in reverse order (lower OAM index = higher priority = drawn last)
        for (int i = sprite_count_ - 1; i >= 0; i--) {
            const auto& spr = sprite_buffer_[i];
            int screen_y = spr.y - 16;
            int screen_x = spr.x - 8;

            uint8_t tile = spr.tile;
            uint8_t attr = spr.attr;

            // In 8×16 mode, bit 0 of tile index is ignored (pairs of tiles)
            if (sprite_height == 16)
                tile &= 0xFE;

            bool flip_y = (attr & 0x40) != 0;
            bool flip_x = (attr & 0x20) != 0;
            bool bg_over_obj = (attr & 0x80) != 0;

            // Which row of the sprite are we on?
            int row = ly_ - screen_y;
            if (flip_y) row = sprite_height - 1 - row;

            // Tile data address calculation
            uint16_t tile_addr;
            if (sprite_height == 16) {
                if (row < 8)
                    tile_addr = tile * 16 + row * 2;
                else
                    tile_addr = (tile | 0x01) * 16 + (row - 8) * 2;
            } else {
                tile_addr = tile * 16 + row * 2;
            }

            if (cgb_mode_) {
                // CGB: VRAM bank from attribute bit 3, CGB palette from bits 0-2
                uint8_t cgb_pal = attr & 0x07;
                uint16_t bank_off = (attr & 0x08) ? gb_ppu::VRAM_BANK_SIZE : 0;

                uint8_t lo = vram_[bank_off + tile_addr];
                uint8_t hi = vram_[bank_off + tile_addr + 1];

                bool lcdc_bg_en = (lcdc & 0x01) != 0;

                for (int bit_pos = 0; bit_pos < 8; bit_pos++) {
                    int px = screen_x + bit_pos;
                    if (px < 0 || px >= gb_ppu::SCREEN_WIDTH) continue;

                    uint8_t bit = flip_x ? bit_pos : (7 - bit_pos);
                    uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
                    if (color_id == 0) continue;  // Transparent

                    // CGB priority: if LCDC.0=1, BG attr priority or OAM priority
                    // can cause BG to win over sprite (when BG color != 0)
                    if (lcdc_bg_en && (bg_priority_[px] || bg_over_obj) &&
                        bg_color_index_[px] != 0)
                        continue;

                    scanline_buffer_[px] = 32 + cgb_pal * 4 + color_id;
                }
            } else {
                // DMG: palette from OBP0/OBP1 via attribute bit 4
                uint8_t palette = regs_.data[(attr & 0x10) ? gb_ppu::OBP1 : gb_ppu::OBP0];

                uint8_t lo = vram_[tile_addr];
                uint8_t hi = vram_[tile_addr + 1];

                for (int bit_pos = 0; bit_pos < 8; bit_pos++) {
                    int px = screen_x + bit_pos;
                    if (px < 0 || px >= gb_ppu::SCREEN_WIDTH) continue;

                    uint8_t bit = flip_x ? bit_pos : (7 - bit_pos);
                    uint8_t color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);

                    // Color 0 is transparent for sprites
                    if (color_id == 0) continue;

                    // BG-to-OBJ priority: if bg_over_obj is set and BG pixel is non-zero, BG wins
                    if (bg_over_obj && bg_priority_[px]) continue;

                    uint8_t color = (palette >> (color_id * 2)) & 0x03;
                    scanline_buffer_[px] = color;
                }
            }
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("GB PPU — LCD Control")
            .value("LCDC", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::LCDC]; })
            .value("STAT", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::STAT]; })
            .value("Mode", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->mode_; })
            .category("GB PPU — Position")
            .value("LY", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->ly_; })
            .value("LYC", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::LYC]; })
            .value("SCX", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::SCX]; })
            .value("SCY", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::SCY]; })
            .value("WX", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::WX]; })
            .value("WY", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::WY]; })
            .category("GB PPU — Palette")
            .value("BGP", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::BGP]; })
            .value("OBP0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::OBP0]; })
            .value("OBP1", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_ppu_t*>(c)->regs_.data[gb_ppu::OBP1]; });
    }
#endif
};
