#pragma once
/*
 * nes_ppu.h — NES PPU (RP2C02 / RP2C07) : ChipBase
 *
 * Picture Processing Unit — generates the NES video output.
 *
 * This header declares the PPU class extracted from the monolithic
 * nes_system.h.  The implementation remains in nes_system.cpp during
 * Phase 1 of the migration; it will move to ppu/nes_ppu.cpp in step 1.5.
 *
 * The PPU is a ChipBase subclass with:
 *   - CPU bus interface (register reads/writes at $2000-$2007)
 *   - Internal PPU memory bus (VRAM, palette, CHR via cartridge)
 *   - Cycle-accurate scanline rendering (background + sprites)
 *   - Debug/settings/layout rendering through ChipBase GUI virtuals
 */

#include <cstdint>
#include <memory>
#include <vector>
#include <array>

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include "../bus/nes_bus.h"

// Forward declarations
namespace nes_system {
    class Cartridge;
}

namespace nes_system {

class PPU : public ChipBase {
public:
    // Screen dimensions (duplicated here for self-containment)
    static constexpr uint32_t SCREEN_W = 256;
    static constexpr uint32_t SCREEN_H = 240;
    // PPU registers (memory-mapped at $2000-$2007)
    struct Registers {
        uint8_t ctrl;       // $2000 - PPUCTRL
        uint8_t mask;       // $2001 - PPUMASK
        uint8_t status;     // $2002 - PPUSTATUS
        uint8_t oam_addr;   // $2003 - OAMADDR
        uint8_t oam_data;   // $2004 - OAMDATA
        uint8_t scroll;     // $2005 - PPUSCROLL
        uint8_t addr;       // $2006 - PPUADDR
        uint8_t data;       // $2007 - PPUDATA
    } regs = {};

    // PPU memory
    std::vector<uint8_t> vram;      // 2KB VRAM (CIRAM — nametable RAM)
    std::vector<uint8_t> oam;       // 256 bytes OAM (Object Attribute Memory)
    std::vector<uint8_t> palette;   // 32 bytes palette RAM

    // Internal state
    struct InternalState {
        uint16_t v = 0;       // Current VRAM address (15 bits)
        uint16_t t = 0;       // Temporary VRAM address (15 bits)
        uint8_t x = 0;        // Fine X scroll (3 bits)
        bool w = false;       // First or second write toggle
        uint8_t fine_y = 0;   // Fine Y scroll

        // Background rendering
        uint16_t nt_addr = 0;     // Nametable address
        uint8_t nt_byte = 0;      // Nametable byte
        uint8_t at_byte = 0;      // Attribute table byte
        uint8_t bg_lo_byte = 0;   // Background pattern table low
        uint8_t bg_hi_byte = 0;   // Background pattern table high

        // Background shift registers
        uint16_t bg_shifter_pattern_lo = 0;
        uint16_t bg_shifter_pattern_hi = 0;
        uint16_t bg_shifter_attrib_lo = 0;
        uint16_t bg_shifter_attrib_hi = 0;

        // Sprite rendering
        struct Sprite {
            uint8_t y = 0;
            uint8_t tile_id = 0;
            uint8_t attributes = 0;
            uint8_t x = 0;
        };

        std::vector<Sprite> sprite_scanline;  // Sprites for current scanline
        uint8_t sprite_shifter_pattern_lo[8] = {};
        uint8_t sprite_shifter_pattern_hi[8] = {};

        bool sprite_zero_hit_possible = false;
        bool sprite_zero_being_rendered = false;
    } internal = {};

    // Timing
    int16_t scanline = -1;    // Current scanline (-1 to 260)
    uint16_t cycle = 0;       // Current cycle (0 to 340)
    uint64_t frame_count = 0; // Frame counter
    bool frame_complete = false;
    bool nmi = false;

    // Open bus data latch — PPU data bus retains last value
    uint8_t ppu_data_bus_ = 0;

    // Region
    bool is_pal = false;

    // Frame buffer (RGB888)
    std::vector<uint32_t> screen;

    // Pattern tables (for debugging)
    std::vector<uint32_t> pattern_table[2];

public:
    PPU(bool pal = false) : is_pal(pal) {
        info_ = ChipInfo{pal ? "RP2C07" : "RP2C02", "Ricoh"};
        // Initialize PPU memory
        vram.resize(2048, 0);
        oam.resize(256, 0);
        palette.resize(32, 0);
        screen.resize(SCREEN_W * SCREEN_H, 0);
        pattern_table[0].resize(128 * 128, 0);
        pattern_table[1].resize(128 * 128, 0);
        internal.sprite_scanline.resize(8);

        reset();
    }

    void reset() {
        regs = {};
        internal = {};
        scanline = -1;
        cycle = 0;
        frame_count = 0;
        frame_complete = false;
        nmi = false;

        // Clear memory
        std::fill(vram.begin(), vram.end(), 0);
        std::fill(oam.begin(), oam.end(), 0);
        std::fill(palette.begin(), palette.end(), 0);
        std::fill(screen.begin(), screen.end(), 0);
    }

    // CPU bus interface — the PPU is a bus device; it samples A0-A2, R/W
    // and drives/samples D0-D7 via the shared bus_state_t.  Open-bus behavior
    // emerges naturally because the data lines retain their last value.
    bus_state_t cpu_bus_tick(bus_state_t bus);

    // Read-only peek for debug/GUI (no side-effects on PPU state)
    uint8_t cpu_peek(uint16_t addr) const;

    // PPU memory access — bus_state_t receiving/returning pattern
    ppu_bus_state_t ppu_read(ppu_bus_state_t bus, bool read_only = false);
    ppu_bus_state_t ppu_write(ppu_bus_state_t bus);

    // Main PPU tick - called 3 times per CPU cycle
    void clock();

    // Connect cartridge for CHR data access and mapper interaction
    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

    // Connect bus for page-pointer VRAM access
    void connect_bus(nes_bus::nes_bus_t* bus) { bus_ptr_ = bus; }

    // Get frame buffer
    const std::vector<uint32_t>& get_screen() const { return screen; }

    // Get pattern tables (for debugging)
    const std::vector<uint32_t>& get_pattern_table(int i, uint8_t palette) const;

    // NMI status
    bool get_nmi() { bool temp = nmi; nmi = false; return temp; }

private:
    std::shared_ptr<Cartridge> cart;
    nes_bus::nes_bus_t* bus_ptr_ = nullptr;   // Page-pointer bus for VRAM reads

    // A12 edge detection — tracks last PPU address for mapper IRQ (MMC3)
    uint16_t last_ppu_addr_ = 0;

    // Internal rendering functions
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_address_x();
    void transfer_address_y();
    void load_background_shifters();
    void update_shifters();

    // Color generation
    uint32_t get_color_from_palette_ram(uint8_t palette, uint8_t pixel);
    uint32_t nes2rgb(uint8_t nes_color);

    // Nametable mirroring helper
    uint16_t mirror_nametable_addr(uint16_t addr) const;

    // Sprite evaluation
    void evaluate_sprites();
    void load_sprite_shifters();

    // --- ChipBase interface ---
public:
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;
};

} // namespace nes_system
