/*
 * nes_system.cpp - Complete NES System Implementation
 *
 * This file implements the complete Nintendo Entertainment System with
 * hardware-accurate components and precise timing.
 */

#include "nes_system.h"
#include "nes_nsf_player.h"
#include "nes_nsf_cartridge.h"
#include "../../core/formats/nsf_format.h"
// CPU is now a native ChipBase (via fam65xx_t<Traits> inheritance)
#include "../../core/chip.h"
#include "../../chip/memory/memory_chip.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef IMGUI_VERSION
#include "imgui.h"
#include <SDL.h>
#endif

namespace nes_system {

// NES color palette (RGB values)
static const uint32_t nes_palette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFF6ECC, 0xFF8170, 0xFF9C12,
    0xD7B000, 0xA6C100, 0x79C900, 0x5ACA8A, 0x4BC0EA, 0x424242, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

// ============================================================================
// PPU IMPLEMENTATION
// ============================================================================

bus_state_t PPU::cpu_bus_tick(bus_state_t bus) {
    const uint16_t addr = BUS_GET_ADDR(bus) & 0x2007;   // PPU mirrors every 8 bytes
    const bool is_read  = BUS_GET_BIT(bus, BUS_RW_BIT);

    if (is_read) {
        // ---- READ ----
        // Start with the data already on the bus (floating/open-bus value)
        uint8_t data = BUS_GET_DATA(bus);

        // The PPU's internal data bus latch drives open-bus bits.
        // Only readable registers override the relevant bits.
        data = ppu_data_bus_;

        switch (addr) {
            case 0x2000: // Control — write only (open bus)
                break;
            case 0x2001: // Mask — write only (open bus)
                break;
            case 0x2002: // Status
                // Top 3 bits from status, bottom 5 from PPU data bus latch
                data = (regs.status & 0xE0) | (ppu_data_bus_ & 0x1F);
                regs.status &= ~0x80; // Clear VBlank flag on read
                internal.w = false;   // Reset write toggle
                break;
            case 0x2003: // OAM Address — write only (open bus)
                break;
            case 0x2004: // OAM Data
                data = oam[regs.oam_addr];
                break;
            case 0x2005: // Scroll — write only (open bus)
                break;
            case 0x2006: // PPU Address — write only (open bus)
                break;
            case 0x2007: // PPU Data
                data = regs.data;
                regs.data = ppu_read(internal.v);

                // Palette reads are immediate (no buffering delay)
                if (internal.v >= 0x3F00) {
                    data = regs.data;
                }

                // Increment VRAM address
                internal.v += (regs.ctrl & 0x04) ? 32 : 1;
                break;
        }

        ppu_data_bus_ = data;   // Update PPU-internal data bus latch
        BUS_SET_DATA(bus, data); // Drive result onto shared system bus
    } else {
        // ---- WRITE ----
        uint8_t data = BUS_GET_DATA(bus);  // Sample data lines from CPU
        ppu_data_bus_ = data;              // Every write updates the open-bus latch

        switch (addr) {
            case 0x2000: // Control
                regs.ctrl = data;
                internal.t = (internal.t & 0xF3FF) | ((data & 0x03) << 10);
                break;
            case 0x2001: // Mask
                regs.mask = data;
                break;
            case 0x2002: // Status — read only (write is ignored, bus latch updated above)
                break;
            case 0x2003: // OAM Address
                regs.oam_addr = data;
                break;
            case 0x2004: // OAM Data
                oam[regs.oam_addr] = data;
                regs.oam_addr++;
                break;
            case 0x2005: // Scroll
                if (!internal.w) {
                    internal.t = (internal.t & 0xFFE0) | ((data & 0xF8) >> 3);
                    internal.x = data & 0x07;
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0x8FFF) | ((data & 0x07) << 12);
                    internal.t = (internal.t & 0xFC1F) | ((data & 0xF8) << 2);
                    internal.w = false;
                }
                break;
            case 0x2006: // PPU Address
                if (!internal.w) {
                    internal.t = (internal.t & 0x80FF) | ((data & 0x3F) << 8);
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0xFF00) | data;
                    internal.v = internal.t;
                    internal.w = false;
                }
                break;
            case 0x2007: // PPU Data
                ppu_write(internal.v, data);
                internal.v += (regs.ctrl & 0x04) ? 32 : 1;
                break;
        }
    }

    return bus;
}

// Read-only peek for debug/GUI — no side-effects on PPU state
uint8_t PPU::cpu_peek(uint16_t addr) const {
    addr &= 0x2007;
    switch (addr) {
        case 0x2000: return ppu_data_bus_;
        case 0x2001: return ppu_data_bus_;
        case 0x2002: return (regs.status & 0xE0) | (ppu_data_bus_ & 0x1F);
        case 0x2003: return ppu_data_bus_;
        case 0x2004: return oam[regs.oam_addr];
        case 0x2005: return ppu_data_bus_;
        case 0x2006: return ppu_data_bus_;
        case 0x2007: return regs.data;  // buffered value, don't trigger VRAM read
        default:     return ppu_data_bus_;
    }
}

// Helper: map a 12-bit nametable offset ($000-$FFF) to a VRAM index (0-$7FF)
// using the active cartridge mirroring mode.
uint16_t PPU::mirror_nametable_addr(uint16_t addr) const {
    addr &= 0x0FFF;
    uint16_t table = addr >> 10;  // 0-3
    uint16_t offset = addr & 0x03FF;

    // Default horizontal mirroring lookup table
    // H: [0,0,1,1]  V: [0,1,0,1]  1LO: [0,0,0,0]  1HI: [1,1,1,1]
    static const uint16_t h_map[4] = {0, 0, 1, 1};
    static const uint16_t v_map[4] = {0, 1, 0, 1};
    static const uint16_t lo_map[4] = {0, 0, 0, 0};
    static const uint16_t hi_map[4] = {1, 1, 1, 1};

    const uint16_t* map = h_map;  // default

    if (cart) {
        switch (cart->get_mirror_mode()) {
            case Cartridge::Mirror::HORIZONTAL:   map = h_map;  break;
            case Cartridge::Mirror::VERTICAL:     map = v_map;  break;
            case Cartridge::Mirror::ONESCREEN_LO: map = lo_map; break;
            case Cartridge::Mirror::ONESCREEN_HI: map = hi_map; break;
            case Cartridge::Mirror::FOUR_SCREEN:
                return addr & 0x07FF;  // direct mapping (needs 4KB VRAM)
        }
    }

    return map[table] * 0x0400 + offset;
}

uint8_t PPU::ppu_read(uint16_t addr, bool read_only) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;
    
    if (cart && cart->ppu_read(addr, data)) {
        // Cartridge handled the read
    } else if (addr <= 0x1FFF) {
        // Pattern table
        data = pattern_table[addr >> 12][addr & 0x0FFF];
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables — apply mirroring
        addr &= 0x0FFF;
        data = vram[mirror_nametable_addr(addr)];
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Palette RAM
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        data = palette[addr] & (regs.mask & 0x01 ? 0x30 : 0x3F);
    }
    
    return data;
}

void PPU::ppu_write(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;
    
    if (cart && cart->ppu_write(addr, data)) {
        // Cartridge handled the write
    } else if (addr <= 0x1FFF) {
        // Pattern table (CHR-RAM)
        pattern_table[addr >> 12][addr & 0x0FFF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables — apply mirroring
        addr &= 0x0FFF;
        vram[mirror_nametable_addr(addr)] = data;
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Palette RAM
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        palette[addr] = data;
    }
}

void PPU::clock() {
    // Lambda to get pixel color from palette
    auto get_pixel = [this](uint8_t palette_idx, uint8_t pixel) -> uint32_t {
        return nes2rgb(ppu_read(0x3F00 + (palette_idx << 2) + pixel) & 0x3F);
    };
    
    // Visible scanlines and pre-render scanline
    if (scanline >= -1 && scanline < 240) {
        
        // Pre-render scanline setup
        if (scanline == -1 && cycle == 1) {
            regs.status &= ~0x80; // Clear VBlank
            regs.status &= ~0x40; // Clear Sprite 0 Hit
            regs.status &= ~0x20; // Clear Sprite Overflow
            
            // Clear sprite shifters
            for (int i = 0; i < 8; i++) {
                internal.sprite_shifter_pattern_lo[i] = 0;
                internal.sprite_shifter_pattern_hi[i] = 0;
            }
        }
        
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            update_shifters();
            
            // Background rendering
            switch ((cycle - 1) % 8) {
                case 0:
                    load_background_shifters();
                    internal.nt_addr = 0x2000 | (internal.v & 0x0FFF);
                    break;
                case 2:
                    internal.nt_byte = ppu_read(internal.nt_addr);
                    break;
                case 4:
                    internal.at_byte = ppu_read(0x2000 | (internal.v & 0x0C00) | 0x03C0 |
                                               ((internal.v >> 4) & 0x38) | ((internal.v >> 2) & 0x07));
                    break;
                case 6:
                    internal.bg_lo_byte = ppu_read(((regs.ctrl & 0x10) << 8) +
                                                  ((uint16_t)internal.nt_byte << 4) +
                                                  (internal.v >> 12) + 0);
                    break;
                case 7:
                    internal.bg_hi_byte = ppu_read(((regs.ctrl & 0x10) << 8) +
                                                  ((uint16_t)internal.nt_byte << 4) +
                                                  (internal.v >> 12) + 8);
                    increment_scroll_x();
                    break;
            }
        }
        
        if (cycle == 256) {
            increment_scroll_y();
        }
        
        if (cycle == 257) {
            load_background_shifters();
            transfer_address_x();
        }
        
        if (cycle == 338 || cycle == 340) {
            internal.nt_byte = ppu_read(internal.nt_addr);
        }
        
        if (scanline == -1 && cycle >= 280 && cycle < 305) {
            transfer_address_y();
        }
        
        // Sprite evaluation for next scanline
        if (cycle == 257 && scanline >= 0) {
            evaluate_sprites();
        }
        
        if (cycle == 340) {
            load_sprite_shifters();
        }
    }
    
    // Render pixel
    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle < 257) {
        uint8_t bg_pixel = 0x00;
        uint8_t bg_palette = 0x00;
        
        // Background rendering
        if (regs.mask & 0x08) {
            if (!(regs.mask & 0x02) && cycle < 9) {
                // Hide leftmost 8 pixels
            } else {
                uint16_t bit_mux = 0x8000 >> internal.x;
                
                uint8_t p0_pixel = (internal.bg_shifter_pattern_lo & bit_mux) > 0;
                uint8_t p1_pixel = (internal.bg_shifter_pattern_hi & bit_mux) > 0;
                bg_pixel = (p1_pixel << 1) | p0_pixel;
                
                uint8_t bg_pal0 = (internal.bg_shifter_attrib_lo & bit_mux) > 0;
                uint8_t bg_pal1 = (internal.bg_shifter_attrib_hi & bit_mux) > 0;
                bg_palette = (bg_pal1 << 1) | bg_pal0;
            }
        }
        
        // Sprite rendering
        uint8_t fg_pixel = 0x00;
        uint8_t fg_palette = 0x00;
        uint8_t fg_priority = 0x00;
        
        if (regs.mask & 0x10) {
            if (!(regs.mask & 0x04) && cycle < 9) {
                // Hide leftmost 8 pixels
            } else {
                internal.sprite_zero_being_rendered = false;
                
                for (uint8_t i = 0; i < internal.sprite_scanline.size(); i++) {
                    if (internal.sprite_scanline[i].x == 0) {
                        uint8_t fg_pixel_lo = (internal.sprite_shifter_pattern_lo[i] & 0x80) > 0;
                        uint8_t fg_pixel_hi = (internal.sprite_shifter_pattern_hi[i] & 0x80) > 0;
                        fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;
                        
                        fg_palette = (internal.sprite_scanline[i].attributes & 0x03) + 0x04;
                        fg_priority = (internal.sprite_scanline[i].attributes & 0x20) == 0;
                        
                        if (fg_pixel != 0) {
                            if (i == 0) {
                                internal.sprite_zero_being_rendered = true;
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // Pixel selection
        uint8_t pixel = 0x00;
        uint8_t palette = 0x00;
        
        if (bg_pixel == 0 && fg_pixel == 0) {
            pixel = 0x00;
            palette = 0x00;
        } else if (bg_pixel == 0 && fg_pixel > 0) {
            pixel = fg_pixel;
            palette = fg_palette;
        } else if (bg_pixel > 0 && fg_pixel == 0) {
            pixel = bg_pixel;
            palette = bg_palette;
        } else if (bg_pixel > 0 && fg_pixel > 0) {
            if (fg_priority) {
                pixel = fg_pixel;
                palette = fg_palette;
            } else {
                pixel = bg_pixel;
                palette = bg_palette;
            }
            
            if (internal.sprite_zero_hit_possible && internal.sprite_zero_being_rendered) {
                if ((regs.mask & 0x08) && (regs.mask & 0x10)) {
                    if (!(regs.mask & 0x06) || cycle >= 9) {
                        regs.status |= 0x40;
                    }
                }
            }
        }
        
        uint32_t color = get_pixel(palette, pixel);
        screen[(scanline * nes_constants::SCREEN_WIDTH) + (cycle - 1)] = color;
    }
    
    // VBlank
    if (scanline >= 241 && scanline < (is_pal ? 311 : 261)) {
        if (scanline == 241 && cycle == 1) {
            regs.status |= 0x80;
            if (regs.ctrl & 0x80) {
                nmi = true;
            }
        }
    }
    
    // Notify cartridge mapper on scanline boundary (for MMC3 IRQ counter)
    // The real hardware clocks the counter on PPU A12 rising edge, but
    // per-scanline notification at cycle 260 is the standard approximation.
    if (cycle == 260 && scanline >= 0 && scanline < 240 && (regs.mask & 0x18)) {
        if (cart) {
            cart->scanline();
        }
    }

    // Advance cycle
    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= (is_pal ? 312 : 262)) {
            scanline = -1;
            frame_complete = true;
            frame_count++;
        }
    }

    // NTSC odd-frame cycle skip: on odd frames, if rendering is enabled,
    // skip one dot (cycle 0 of the pre-render scanline)
    if (!is_pal && scanline == -1 && cycle == 0 && (frame_count & 1) && (regs.mask & 0x18)) {
        cycle = 1;
    }
}

uint32_t PPU::nes2rgb(uint8_t nes_color) {
    return nes_palette[nes_color & 0x3F];
}

void PPU::increment_scroll_x() {
    if (regs.mask & 0x18) {
        if ((internal.v & 0x001F) == 31) {
            internal.v &= ~0x001F;
            internal.v ^= 0x0400;
        } else {
            internal.v++;
        }
    }
}

void PPU::increment_scroll_y() {
    if (regs.mask & 0x18) {
        if ((internal.v & 0x7000) != 0x7000) {
            internal.v += 0x1000;
        } else {
            internal.v &= ~0x7000;
            int y = (internal.v & 0x03E0) >> 5;
            if (y == 29) {
                y = 0;
                internal.v ^= 0x0800;
            } else if (y == 31) {
                y = 0;
            } else {
                y++;
            }
            internal.v = (internal.v & ~0x03E0) | (y << 5);
        }
    }
}

void PPU::transfer_address_x() {
    if (regs.mask & 0x18) {
        internal.v = (internal.v & ~0x041F) | (internal.t & 0x041F);
    }
}

void PPU::transfer_address_y() {
    if (regs.mask & 0x18) {
        internal.v = (internal.v & ~0x7BE0) | (internal.t & 0x7BE0);
    }
}

void PPU::load_background_shifters() {
    internal.bg_shifter_pattern_lo = (internal.bg_shifter_pattern_lo & 0xFF00) | internal.bg_lo_byte;
    internal.bg_shifter_pattern_hi = (internal.bg_shifter_pattern_hi & 0xFF00) | internal.bg_hi_byte;
    
    internal.bg_shifter_attrib_lo = (internal.bg_shifter_attrib_lo & 0xFF00) |
                                   ((internal.at_byte & 0x01) ? 0xFF : 0x00);
    internal.bg_shifter_attrib_hi = (internal.bg_shifter_attrib_hi & 0xFF00) |
                                   ((internal.at_byte & 0x02) ? 0xFF : 0x00);
}

void PPU::update_shifters() {
    if (regs.mask & 0x08) {
        internal.bg_shifter_pattern_lo <<= 1;
        internal.bg_shifter_pattern_hi <<= 1;
        internal.bg_shifter_attrib_lo <<= 1;
        internal.bg_shifter_attrib_hi <<= 1;
    }
    
    if (regs.mask & 0x10 && cycle >= 1 && cycle < 258) {
        for (size_t i = 0; i < internal.sprite_scanline.size(); i++) {
            if (internal.sprite_scanline[i].x > 0) {
                internal.sprite_scanline[i].x--;
            } else {
                internal.sprite_shifter_pattern_lo[i] <<= 1;
                internal.sprite_shifter_pattern_hi[i] <<= 1;
            }
        }
    }
}

void PPU::evaluate_sprites() {
    // Clear sprites for next scanline
    internal.sprite_scanline.clear();
    internal.sprite_scanline.resize(8);
    
    internal.sprite_zero_hit_possible = false;
    uint8_t sprite_count = 0;
    
    for (uint8_t i = 0; i < 64 && sprite_count < 9; i++) {
        uint8_t sprite_y = oam[i * 4 + 0];
        uint8_t sprite_height = (regs.ctrl & 0x20) ? 16 : 8;
        
        if ((scanline >= sprite_y) && (scanline < (sprite_y + sprite_height))) {
            if (sprite_count < 8) {
                if (i == 0) {
                    internal.sprite_zero_hit_possible = true;
                }
                
                internal.sprite_scanline[sprite_count].y = sprite_y;
                internal.sprite_scanline[sprite_count].tile_id = oam[i * 4 + 1];
                internal.sprite_scanline[sprite_count].attributes = oam[i * 4 + 2];
                internal.sprite_scanline[sprite_count].x = oam[i * 4 + 3];
            }
            sprite_count++;
        }
    }
    
    if (sprite_count > 8) {
        regs.status |= 0x20; // Set sprite overflow
    }
}

void PPU::load_sprite_shifters() {
    for (uint8_t i = 0; i < internal.sprite_scanline.size(); i++) {
        uint8_t sprite_pattern_bits_lo, sprite_pattern_bits_hi;
        uint16_t sprite_pattern_addr_lo, sprite_pattern_addr_hi;
        
        if (regs.ctrl & 0x20) {
            // 8x16 sprites
            if ((internal.sprite_scanline[i].attributes & 0x80) == 0) {
                // Not vertically flipped
                if (scanline - internal.sprite_scanline[i].y < 8) {
                    // Top half
                    sprite_pattern_addr_lo = ((internal.sprite_scanline[i].tile_id & 0x01) << 12) |
                                           ((internal.sprite_scanline[i].tile_id & 0xFE) << 4) |
                                           ((scanline - internal.sprite_scanline[i].y) & 0x07);
                } else {
                    // Bottom half
                    sprite_pattern_addr_lo = ((internal.sprite_scanline[i].tile_id & 0x01) << 12) |
                                           (((internal.sprite_scanline[i].tile_id & 0xFE) + 1) << 4) |
                                           ((scanline - internal.sprite_scanline[i].y) & 0x07);
                }
            } else {
                // Vertically flipped
                if (scanline - internal.sprite_scanline[i].y < 8) {
                    // Top half (flipped, so actually bottom)
                    sprite_pattern_addr_lo = ((internal.sprite_scanline[i].tile_id & 0x01) << 12) |
                                           (((internal.sprite_scanline[i].tile_id & 0xFE) + 1) << 4) |
                                           ((7 - (scanline - internal.sprite_scanline[i].y)) & 0x07);
                } else {
                    // Bottom half (flipped, so actually top)
                    sprite_pattern_addr_lo = ((internal.sprite_scanline[i].tile_id & 0x01) << 12) |
                                           ((internal.sprite_scanline[i].tile_id & 0xFE) << 4) |
                                           ((7 - (scanline - internal.sprite_scanline[i].y)) & 0x07);
                }
            }
        } else {
            // 8x8 sprites
            if ((internal.sprite_scanline[i].attributes & 0x80) == 0) {
                // Not vertically flipped
                sprite_pattern_addr_lo = ((regs.ctrl & 0x08) << 9) |
                                       (internal.sprite_scanline[i].tile_id << 4) |
                                       (scanline - internal.sprite_scanline[i].y);
            } else {
                // Vertically flipped
                sprite_pattern_addr_lo = ((regs.ctrl & 0x08) << 9) |
                                       (internal.sprite_scanline[i].tile_id << 4) |
                                       (7 - (scanline - internal.sprite_scanline[i].y));
            }
        }
        
        sprite_pattern_addr_hi = sprite_pattern_addr_lo + 8;
        sprite_pattern_bits_lo = ppu_read(sprite_pattern_addr_lo);
        sprite_pattern_bits_hi = ppu_read(sprite_pattern_addr_hi);
        
        if (internal.sprite_scanline[i].attributes & 0x40) {
            // Horizontally flip
            auto flip_byte = [](uint8_t b) {
                b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
                b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
                b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
                return b;
            };
            sprite_pattern_bits_lo = flip_byte(sprite_pattern_bits_lo);
            sprite_pattern_bits_hi = flip_byte(sprite_pattern_bits_hi);
        }
        
        internal.sprite_shifter_pattern_lo[i] = sprite_pattern_bits_lo;
        internal.sprite_shifter_pattern_hi[i] = sprite_pattern_bits_hi;
    }
}
void PPU::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
    cart = cartridge;
}

const std::vector<uint32_t>& PPU::get_pattern_table(int i, uint8_t palette) const {
    // Pattern tables are used for debugging - they visualize the CHR ROM/RAM tiles
    // Each pattern table is 128x128 pixels (16x16 tiles of 8x8 pixels each)
    
    // Return the appropriate pattern table buffer
    if (i < 0 || i > 1) {
        i = 0;  // Default to pattern table 0
    }
    
    // We need to render the pattern table - but this is const, so we need to
    // cast away constness for the pattern_table member. This is safe because
    // we're only updating a cache that doesn't affect the logical state.
    auto* non_const_this = const_cast<PPU*>(this);
    
    // Render the pattern table to the buffer
    for (uint16_t tile_y = 0; tile_y < 16; tile_y++) {
        for (uint16_t tile_x = 0; tile_x < 16; tile_x++) {
            uint16_t tile_offset = tile_y * 256 + tile_x * 16;
            
            // Each tile is 8x8 pixels
            for (uint16_t row = 0; row < 8; row++) {
                // Read the low and high bitplanes for this row
                uint16_t addr = (i * 0x1000) + tile_offset + row;
                uint8_t tile_lsb = non_const_this->ppu_read(addr, true);
                uint8_t tile_msb = non_const_this->ppu_read(addr + 8, true);
                
                // Render each pixel in the row
                for (uint16_t col = 0; col < 8; col++) {
                    // Get the 2-bit pixel value
                    uint8_t pixel = ((tile_lsb & 0x01) | ((tile_msb & 0x01) << 1));
                    tile_lsb >>= 1;
                    tile_msb >>= 1;
                    
                    // Get the color from the selected palette using the static palette array
                    uint8_t palette_index = non_const_this->ppu_read(0x3F00 + (palette << 2) + pixel, true) & 0x3F;
                    uint32_t color = nes_palette[palette_index];
                    
                    // Calculate screen position
                    uint16_t x = tile_x * 8 + (7 - col);
                    uint16_t y = tile_y * 8 + row;
                    
                    // Write to pattern table buffer
                    non_const_this->pattern_table[i][y * 128 + x] = color;
                }
            }
        }
    }
    
    return pattern_table[i];
}

// ============================================================================
// CARTRIDGE IMPLEMENTATION
// ============================================================================

// Mapper 000 (NROM) implementation
class Cartridge::Mapper000 : public Cartridge::Mapper {
private:
    uint8_t prg_banks;
    uint8_t chr_banks;
    
public:
    Mapper000(uint8_t prgBanks, uint8_t chrBanks) : prg_banks(prgBanks), chr_banks(chrBanks) {}
    
    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // PRG RAM sentinel
            return true;
        }
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    
    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // PRG RAM sentinel
            return true;
        }
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    
    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }
    
    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }
    
    void reset() override {}
};

// ============================================================================
// Mapper 001 (MMC1) — Nintendo SxROM
// Covers: Zelda, Metroid, Mega Man 2, Final Fantasy, Kid Icarus, etc.
// Features: PRG/CHR bank switching, mirroring control, PRG RAM
// ============================================================================

class Cartridge::Mapper001 : public Cartridge::Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // Shift register (serial writes)
    uint8_t shift_register_ = 0x10;  // bit 4 set = "empty"
    uint8_t write_count_ = 0;

    // Internal registers
    uint8_t reg_control_ = 0x0C;   // $8000-$9FFF — control
    uint8_t reg_chr_bank0_ = 0;    // $A000-$BFFF — CHR bank 0
    uint8_t reg_chr_bank1_ = 0;    // $C000-$DFFF — CHR bank 1
    uint8_t reg_prg_bank_ = 0;     // $E000-$FFFF — PRG bank

    // PRG RAM enable
    bool prg_ram_enabled_ = true;

    // Derived state
    Cartridge::Mirror mirror_mode_ = Cartridge::Mirror::HORIZONTAL;

    void update_mirroring() {
        switch (reg_control_ & 0x03) {
            case 0: mirror_mode_ = Cartridge::Mirror::ONESCREEN_LO; break;
            case 1: mirror_mode_ = Cartridge::Mirror::ONESCREEN_HI; break;
            case 2: mirror_mode_ = Cartridge::Mirror::VERTICAL;     break;
            case 3: mirror_mode_ = Cartridge::Mirror::HORIZONTAL;   break;
        }
    }

public:
    Mapper001(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            // PRG RAM — use a sentinel address range
            // Caller checks prg_ram vector directly
            mapped_addr = 0xFFFFFFFF;  // sentinel for PRG RAM
            return true;
        }

        if (addr >= 0x8000) {
            uint8_t prg_mode = (reg_control_ >> 2) & 0x03;

            if (prg_mode <= 1) {
                // 32KB mode: ignore low bit of bank number
                uint32_t bank = (reg_prg_bank_ & 0x0E) >> 1;
                mapped_addr = bank * 0x8000 + (addr & 0x7FFF);
            } else if (prg_mode == 2) {
                // Fix first bank at $8000, switch second at $C000
                if (addr < 0xC000) {
                    mapped_addr = addr & 0x3FFF;
                } else {
                    mapped_addr = (reg_prg_bank_ & 0x0F) * 0x4000 + (addr & 0x3FFF);
                }
            } else {
                // Fix last bank at $C000, switch first at $8000
                if (addr < 0xC000) {
                    mapped_addr = (reg_prg_bank_ & 0x0F) * 0x4000 + (addr & 0x3FFF);
                } else {
                    mapped_addr = (prg_banks_ - 1) * 0x4000 + (addr & 0x3FFF);
                }
            }
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // sentinel for PRG RAM
            return true;
        }

        if (addr >= 0x8000) {
            if (data & 0x80) {
                // Reset shift register
                shift_register_ = 0x10;
                write_count_ = 0;
                reg_control_ |= 0x0C;  // Reset to PRG bank mode 3
                update_mirroring();
            } else {
                shift_register_ >>= 1;
                shift_register_ |= (data & 0x01) << 4;
                write_count_++;

                if (write_count_ == 5) {
                    uint8_t target = (addr >> 13) & 0x03;  // Which register

                    switch (target) {
                        case 0: // $8000-$9FFF — Control
                            reg_control_ = shift_register_ & 0x1F;
                            update_mirroring();
                            break;
                        case 1: // $A000-$BFFF — CHR bank 0
                            reg_chr_bank0_ = shift_register_ & 0x1F;
                            break;
                        case 2: // $C000-$DFFF — CHR bank 1
                            reg_chr_bank1_ = shift_register_ & 0x1F;
                            break;
                        case 3: // $E000-$FFFF — PRG bank
                            reg_prg_bank_ = shift_register_ & 0x0F;
                            prg_ram_enabled_ = !(shift_register_ & 0x10);
                            break;
                    }

                    shift_register_ = 0x10;
                    write_count_ = 0;
                }
            }
            mapped_addr = 0;
            return false;  // Don't write to PRG ROM
        }
        return false;
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            if (chr_banks_ == 0) {
                // CHR RAM — direct mapping
                mapped_addr = addr;
                return true;
            }

            bool chr_mode = (reg_control_ & 0x10) != 0;

            if (!chr_mode) {
                // 8KB mode
                uint32_t bank = (reg_chr_bank0_ & 0x1E) >> 1;
                mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
            } else {
                // 4KB mode
                if (addr < 0x1000) {
                    mapped_addr = reg_chr_bank0_ * 0x1000 + (addr & 0x0FFF);
                } else {
                    mapped_addr = reg_chr_bank1_ * 0x1000 + (addr & 0x0FFF);
                }
            }
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks_ == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    Mirror mirror() override { return mirror_mode_; }

    void reset() override {
        shift_register_ = 0x10;
        write_count_ = 0;
        reg_control_ = 0x0C;
        reg_chr_bank0_ = 0;
        reg_chr_bank1_ = 0;
        reg_prg_bank_ = 0;
        prg_ram_enabled_ = true;
        update_mirroring();
    }
};

// ============================================================================
// Mapper 002 (UxROM) — Simple PRG bank switching
// Covers: Castlevania, Contra, Metal Gear, Mega Man, etc.
// ============================================================================

class Cartridge::Mapper002 : public Cartridge::Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

public:
    Mapper002(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x8000 && addr <= 0xBFFF) {
            // Switchable bank at $8000
            mapped_addr = prg_bank_select_ * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        if (addr >= 0xC000) {
            // Fixed last bank at $C000
            mapped_addr = (prg_banks_ - 1) * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x0F;
        }
        return false;  // No actual ROM write
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks_ == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    void reset() override { prg_bank_select_ = 0; }
};

// ============================================================================
// Mapper 003 (CNROM) — Simple CHR bank switching
// Covers: Galaxian, Gradius, Arkista's Ring, etc.
// ============================================================================

class Cartridge::Mapper003 : public Cartridge::Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper003(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks_ > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x8000) {
            chr_bank_select_ = data & 0x03;
        }
        return false;  // No actual ROM write
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = chr_bank_select_ * 0x2000 + addr;
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        // CNROM uses CHR ROM, no writes
        return false;
    }

    void reset() override { chr_bank_select_ = 0; }
};

// ============================================================================
// Mapper 004 (MMC3/TxROM) — Advanced PRG/CHR switching with scanline IRQ
// Covers: Super Mario Bros. 2/3, Kirby's Adventure, Mega Man 3-6, etc.
// The most complex of the common mappers; scanline counter is critical.
// ============================================================================

class Cartridge::Mapper004 : public Cartridge::Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // Bank registers
    uint8_t target_register_ = 0;    // R0-R7 selection
    bool prg_bank_mode_ = false;     // false = $8000 swappable, true = $C000 swappable
    bool chr_inversion_ = false;     // false = 2KB banks at $0000, true = 2KB banks at $1000
    uint8_t registers_[8] = {};      // R0-R7 bank values

    // PRG RAM protect
    bool prg_ram_enabled_ = true;
    bool prg_ram_write_protect_ = false;

    // Mirroring
    Cartridge::Mirror mirror_mode_ = Cartridge::Mirror::HORIZONTAL;

    // IRQ
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    bool irq_reload_ = false;

    // Derived banks for fast lookup
    uint32_t prg_bank_[4] = {};  // 4 × 8KB PRG banks
    uint32_t chr_bank_[8] = {};  // 8 × 1KB CHR banks

    void update_prg_banks() {
        uint32_t last_bank = (prg_banks_ * 2) - 1;  // Total 8KB banks - 1

        if (!prg_bank_mode_) {
            prg_bank_[0] = (registers_[6] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[1] = (registers_[7] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[2] = (last_bank - 1) % (prg_banks_ * 2);
            prg_bank_[3] = last_bank % (prg_banks_ * 2);
        } else {
            prg_bank_[0] = (last_bank - 1) % (prg_banks_ * 2);
            prg_bank_[1] = (registers_[7] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[2] = (registers_[6] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[3] = last_bank % (prg_banks_ * 2);
        }
    }

    void update_chr_banks() {
        uint32_t chr_size = chr_banks_ == 0 ? 8 : chr_banks_ * 8; // in 1KB units

        if (!chr_inversion_) {
            chr_bank_[0] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank_[1] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank_[2] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank_[3] = ((registers_[1] & 0xFE) + 1) % chr_size;
            chr_bank_[4] = registers_[2] % chr_size;
            chr_bank_[5] = registers_[3] % chr_size;
            chr_bank_[6] = registers_[4] % chr_size;
            chr_bank_[7] = registers_[5] % chr_size;
        } else {
            chr_bank_[0] = registers_[2] % chr_size;
            chr_bank_[1] = registers_[3] % chr_size;
            chr_bank_[2] = registers_[4] % chr_size;
            chr_bank_[3] = registers_[5] % chr_size;
            chr_bank_[4] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank_[5] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank_[6] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank_[7] = ((registers_[1] & 0xFE) + 1) % chr_size;
        }
    }

public:
    Mapper004(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            // PRG RAM
            mapped_addr = 0xFFFFFFFF;  // sentinel
            return true;
        }

        if (addr >= 0x8000) {
            uint8_t slot = (addr >> 13) & 0x03;  // 0-3 for $8000/$A000/$C000/$E000
            mapped_addr = prg_bank_[slot] * 0x2000 + (addr & 0x1FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // sentinel
            return true;  // Write to PRG RAM
        }

        if (addr >= 0x8000) {
            bool even = !(addr & 0x0001);

            if (addr <= 0x9FFF) {
                if (even) {
                    // Bank select ($8000)
                    target_register_ = data & 0x07;
                    prg_bank_mode_ = (data & 0x40) != 0;
                    chr_inversion_ = (data & 0x80) != 0;
                    update_prg_banks();
                    update_chr_banks();
                } else {
                    // Bank data ($8001)
                    registers_[target_register_] = data;
                    update_prg_banks();
                    update_chr_banks();
                }
            } else if (addr <= 0xBFFF) {
                if (even) {
                    // Mirroring ($A000)
                    mirror_mode_ = (data & 0x01) ? Cartridge::Mirror::HORIZONTAL
                                                 : Cartridge::Mirror::VERTICAL;
                } else {
                    // PRG RAM protect ($A001)
                    prg_ram_enabled_ = (data & 0x80) != 0;
                    prg_ram_write_protect_ = (data & 0x40) != 0;
                }
            } else if (addr <= 0xDFFF) {
                if (even) {
                    // IRQ latch ($C000)
                    irq_reload_value_ = data;
                } else {
                    // IRQ reload ($C001)
                    irq_counter_ = 0;
                    irq_reload_ = true;
                }
            } else {
                if (even) {
                    // IRQ disable ($E000) — also acknowledges
                    irq_enabled_ = false;
                    irq_active_ = false;
                } else {
                    // IRQ enable ($E001)
                    irq_enabled_ = true;
                }
            }
            return false;  // Don't write to PRG ROM
        }
        return false;
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            uint8_t slot = addr >> 10;  // 0-7 for each 1KB bank
            mapped_addr = chr_bank_[slot] * 0x0400 + (addr & 0x03FF);
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks_ == 0) {
            uint8_t slot = addr >> 10;
            mapped_addr = chr_bank_[slot] * 0x0400 + (addr & 0x03FF);
            return true;
        }
        return false;
    }

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override { return irq_active_; }

    void irq_clear() override { irq_active_ = false; }

    void scanline() override {
        if (irq_counter_ == 0 || irq_reload_) {
            irq_counter_ = irq_reload_value_;
            irq_reload_ = false;
        } else {
            irq_counter_--;
        }

        if (irq_counter_ == 0 && irq_enabled_) {
            irq_active_ = true;
        }
    }

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        prg_ram_enabled_ = true;
        prg_ram_write_protect_ = false;
        irq_counter_ = 0;
        irq_reload_value_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
        irq_reload_ = false;
        mirror_mode_ = Cartridge::Mirror::HORIZONTAL;
        update_prg_banks();
        update_chr_banks();
    }
};

// ============================================================================
// Cartridge IRQ / scanline / mirroring delegation
// ============================================================================

bool Cartridge::irq_state() const {
    return mapper ? mapper->irq_state() : false;
}

void Cartridge::irq_clear() {
    if (mapper) mapper->irq_clear();
}

void Cartridge::scanline() {
    if (mapper) mapper->scanline();
}

// ============================================================================
// Battery-backed SRAM persistence
// ============================================================================

std::string Cartridge::sram_path_for_rom(const std::string& rom_path) const {
    // Replace .nes extension with .sav
    std::string sav = rom_path;
    auto dot = sav.rfind('.');
    if (dot != std::string::npos) {
        sav = sav.substr(0, dot);
    }
    sav += ".sav";
    return sav;
}

bool Cartridge::load_sram(const std::string& sav_path) {
    if (prg_ram.empty()) return false;
    std::ifstream f(sav_path, std::ios::binary);
    if (!f.is_open()) return false;
    f.read(reinterpret_cast<char*>(prg_ram.data()),
           static_cast<std::streamsize>(prg_ram.size()));
    printf("NES: Loaded SRAM from %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
    return true;
}

bool Cartridge::save_sram(const std::string& sav_path) const {
    if (prg_ram.empty() || !battery_backed) return false;
    std::ofstream f(sav_path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(prg_ram.data()),
            static_cast<std::streamsize>(prg_ram.size()));
    printf("NES: Saved SRAM to %s (%zu bytes)\n", sav_path.c_str(), prg_ram.size());
    return true;
}

// ============================================================================

Cartridge::Cartridge(const std::string& filename) {
    if (!load_from_file(filename)) {
        throw std::runtime_error("Failed to load cartridge: " + filename);
    }
}

bool Cartridge::load_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(Header));
    
    // Verify iNES header
    if (header.name[0] != 'N' || header.name[1] != 'E' || 
        header.name[2] != 'S' || header.name[3] != 0x1A) {
        return false;
    }
    
    // Extract mapper ID
    mapper_id = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
    mirror_mode = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    battery_backed = (header.mapper1 & 0x02) != 0;
    
    prg_banks = header.prg_rom_chunks;
    chr_banks = header.chr_rom_chunks;
    
    // Skip trainer if present
    if (header.mapper1 & 0x04) {
        file.seekg(512, std::ios::cur);
    }
    
    // Load PRG ROM
    uint32_t prg_size = prg_banks * 16384;
    prg_memory.resize(prg_size);
    file.read(reinterpret_cast<char*>(prg_memory.data()), prg_size);
    
    // Load CHR ROM/RAM
    if (chr_banks == 0) {
        // CHR RAM
        chr_memory.resize(8192, 0);
    } else {
        // CHR ROM
        uint32_t chr_size = chr_banks * 8192;
        chr_memory.resize(chr_size);
        file.read(reinterpret_cast<char*>(chr_memory.data()), chr_size);
    }

    // Allocate PRG RAM (8KB, used by MMC1/MMC3 and others)
    uint32_t prg_ram_size = header.prg_ram_size ? header.prg_ram_size * 8192 : 8192;
    prg_ram.resize(prg_ram_size, 0);
    
    // Create appropriate mapper
    switch (mapper_id) {
        case 0:
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            break;
        case 1:
            mapper = std::make_unique<Mapper001>(prg_banks, chr_banks);
            break;
        case 2:
            mapper = std::make_unique<Mapper002>(prg_banks, chr_banks);
            break;
        case 3:
            mapper = std::make_unique<Mapper003>(prg_banks, chr_banks);
            break;
        case 4:
            mapper = std::make_unique<Mapper004>(prg_banks, chr_banks);
            break;
        default:
            std::cout << "Warning: Unsupported mapper " << (int)mapper_id
                      << ", falling back to NROM" << std::endl;
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            break;
    }

    // Store ROM path for SRAM persistence
    rom_filepath_ = filename;

    // Load battery-backed SRAM if present
    if (battery_backed) {
        load_sram(sram_path_for_rom(filename));
    }
    
    return true;
}

bus_state_t Cartridge::cpu_bus_tick(bus_state_t bus, bool& handled) {
    const uint16_t addr   = BUS_GET_ADDR(bus);
    const bool     is_read = BUS_GET_BIT(bus, BUS_RW_BIT);
    handled = false;

    if (is_read) {
        // ---- READ ----
        uint32_t mapped_addr;
        if (mapper->cpu_map_read(addr, mapped_addr)) {
            if (mapped_addr == 0xFFFFFFFF) {
                // PRG RAM ($6000-$7FFF)
                uint16_t ram_offset = addr & 0x1FFF;
                if (ram_offset < prg_ram.size()) {
                    BUS_SET_DATA(bus, prg_ram[ram_offset]);
                }
                handled = true;
                return bus;
            }
            if (mapped_addr < prg_memory.size()) {
                BUS_SET_DATA(bus, prg_memory[mapped_addr]);
                handled = true;
                return bus;
            }
        }
    } else {
        // ---- WRITE ----
        uint8_t data = BUS_GET_DATA(bus);
        uint32_t mapped_addr;
        if (mapper->cpu_map_write(addr, mapped_addr, data)) {
            if (mapped_addr == 0xFFFFFFFF) {
                // PRG RAM ($6000-$7FFF)
                uint16_t ram_offset = addr & 0x1FFF;
                if (ram_offset < prg_ram.size()) {
                    prg_ram[ram_offset] = data;
                }
                handled = true;
                return bus;
            }
            if (mapped_addr < prg_memory.size()) {
                prg_memory[mapped_addr] = data;
                handled = true;
                return bus;
            }
        }
        // Even if mapper returned false, the write may have updated mapper state
        // (e.g. MMC1 shift register, UxROM bank select).  Update mirroring.
        mirror_mode = mapper->mirror();
    }

    return bus;
}

bool Cartridge::ppu_read(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr;
    if (mapper->ppu_map_read(addr, mapped_addr)) {
        if (mapped_addr < chr_memory.size()) {
            data = chr_memory[mapped_addr];
            return true;
        }
    }
    return false;
}

bool Cartridge::ppu_write(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr;
    if (mapper->ppu_map_write(addr, mapped_addr)) {
        if (mapped_addr < chr_memory.size()) {
            chr_memory[mapped_addr] = data;
            return true;
        }
    }
    return false;
}

void Cartridge::reset() {
    if (mapper) {
        mapper->reset();
        mirror_mode = mapper->mirror();
    }
}

uint8_t Cartridge::peek(uint16_t addr) const {
    if (!mapper) return 0;
    uint32_t mapped_addr;
    // cpu_map_read is logically const for all mappers (no state mutation)
    if (const_cast<Mapper*>(mapper.get())->cpu_map_read(addr, mapped_addr)) {
        if (mapped_addr == 0xFFFFFFFF) {
            uint16_t ram_offset = addr & 0x1FFF;
            if (ram_offset < prg_ram.size()) return prg_ram[ram_offset];
            return 0;
        }
        if (mapped_addr < prg_memory.size()) return prg_memory[mapped_addr];
    }
    return 0;
}

// ============================================================================
// MEMORY BUS IMPLEMENTATION
// ============================================================================

bus_state_t MemoryBus::mem_tick(bus_state_t bus) {
    uint16_t addr = BUS_GET_ADDR(bus);
    const bool is_read = BUS_GET_BIT(bus, BUS_RW_BIT);

    // ========================================================================
    // The data lines on `bus` retain their last value (floating bus).
    // Each device that claims the address either drives (read) or samples
    // (write) via the shared bus_state_t — no intermediate variables.
    // ========================================================================

    if (!is_read) {
        // ---- WRITE ---- (RW=0 per 6502 convention)
        uint8_t data = BUS_GET_DATA(bus);

        if (addr <= 0x1FFF) {
            // CPU RAM (with mirroring)
            cpu_ram[addr & 0x07FF] = data;
        } else if (addr <= 0x3FFF) {
            // PPU registers (with mirroring) — pass full bus through
            if (ppu) {
                bus = ppu->cpu_bus_tick(bus);
            }
        } else if (addr <= 0x4017) {
            // APU and I/O registers
            if (addr == 0x4014) {
                // OAM DMA
                dma_page = data;
                dma_addr = 0x00;
                dma_transfer = true;
            } else if (addr == 0x4016) {
                controllers[0].write(data);
                controllers[1].write(data);
            }
            // APU registers ($4000-$4013, $4015, $4017) handled by CPU PHI1
        } else {
            // Cartridge space ($4020-$FFFF) — pass full bus through
            if (cartridge) {
                bool handled = false;
                bus = cartridge->cpu_bus_tick(bus, handled);
            }
        }
    } else {
        // ---- READ ----
        // Data lines carry whatever was last driven (floating).
        // Each device that recognises the address overwrites the data field.

        if (addr <= 0x1FFF) {
            // CPU RAM (with mirroring)
            BUS_SET_DATA(bus, cpu_ram[addr & 0x07FF]);
        } else if (addr <= 0x3FFF) {
            // PPU registers (with mirroring) — pass full bus through
            if (ppu) {
                bus = ppu->cpu_bus_tick(bus);
            }
        } else if (addr <= 0x4017) {
            // APU and I/O registers
            if (addr == 0x4016) {
                BUS_SET_DATA(bus, controllers[0].read());
            } else if (addr == 0x4017) {
                BUS_SET_DATA(bus, controllers[1].read());
            }
            // APU registers ($4000-$4013, $4015) handled by CPU PHI1
        } else {
            // Cartridge space ($4020-$FFFF) — pass full bus through
            if (cartridge) {
                bool handled = false;
                bus = cartridge->cpu_bus_tick(bus, handled);
                // If cartridge didn't claim, data lines stay floating
            }
        }
    }

    return bus;
}

void MemoryBus::reset() {
    std::fill(cpu_ram.begin(), cpu_ram.end(), 0);
    system_clock_counter = 0;
    dma_transfer = false;
    dma_dummy = true;
}

void MemoryBus::clock() {
    if (ppu) {
        ppu->clock();
    }
    
    // Handle OAM DMA
    if (dma_transfer) {
        if (dma_dummy) {
            if (system_clock_counter % 2 == 1) {
                dma_dummy = false;
            }
        } else {
            if (system_clock_counter % 2 == 0) {
                // DMA read — construct a read bus_state_t and service it
                bus_state_t dma_bus = 0;
                BUS_SET_ADDR(dma_bus, (dma_page << 8) | dma_addr);
                // RW bit clear = read
                dma_bus = mem_tick(dma_bus);
                dma_data = BUS_GET_DATA(dma_bus);
            } else {
                ppu->oam[dma_addr] = dma_data;
                dma_addr++;
                if (dma_addr == 0x00) {
                    dma_transfer = false;
                    dma_dummy = true;
                }
            }
        }
    }
    
    system_clock_counter++;
}

// ============================================================================
// MAIN NES SYSTEM IMPLEMENTATION
// ============================================================================

// Hardware traits definition
static HardwareTraits create_nes_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - NES PPU
    traits.display.native_width = 256;
    traits.display.native_height = 240;
    traits.display.visible_width = 256;
    traits.display.visible_height = 240;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 64;       // 64 colors
    traits.display.pixel_aspect_ratio = 8.0f / 7.0f;  // NTSC pixel aspect
    traits.display.has_overscan = true;
    
    // NES palette (simplified - first 16 colors)
    const uint32_t nes_colors[16] = {
        0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC,
        0x940084, 0xA80020, 0xA81000, 0x881400,
        0x503000, 0x007800, 0x006800, 0x005800,
        0x004058, 0x000000, 0x000000, 0x000000
    };
    
    for (int i = 0; i < 16; i++) {
        uint32_t c = nes_colors[i];
        traits.display.default_palette.push_back(
            PaletteColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255)
        );
    }
    
    // Audio traits - NES APU (2A03)
    traits.audio.format = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz = 44100;
    traits.audio.channels = 1;
    traits.audio.chip_name = "RP2A03 APU";
    
    // Timing - NTSC version
    traits.timing.cpu_frequency_hz = 1789773;   // ~1.79 MHz
    traits.timing.video_frequency_hz = 5369318; // PPU is 3x CPU
    traits.timing.audio_sample_rate_hz = 44100;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = 29829;     // 1789773 / 60
    traits.timing.standard = VideoStandard::NTSC;
    
    // Memory options (NES has fixed 2KB RAM)
    traits.memory_options.push_back({
        "2KB RAM (Standard)",
        2048,
        0,
        true
    });
    
    // Region options
    traits.video_standard_configs.push_back({
        "NTSC",
        VideoStandard::NTSC,
        traits.timing,
        true
    });
    
    SystemTiming pal_timing = traits.timing;
    pal_timing.cpu_frequency_hz = 1662607;      // ~1.66 MHz (PAL)
    pal_timing.video_frequency_hz = 4987821;    // PPU is 3x CPU
    pal_timing.target_fps = 50;
    pal_timing.cycles_per_frame = 33252;        // 1662607 / 50
    pal_timing.standard = VideoStandard::PAL;
    
    traits.video_standard_configs.push_back({
        "PAL",
        VideoStandard::PAL,
        pal_timing,
        false
    });
    
    return traits;
}

// File detection callback
static float nes_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".nes") == 0 || strcmp(ext, ".NES") == 0) {
            // Check for iNES header
            if (size >= 16 && data[0] == 'N' && data[1] == 'E' &&
                data[2] == 'S' && data[3] == 0x1A) {
                return 1.0f;  // Perfect match
            }
            return 0.9f;  // .nes extension but no header
        }
        // NSF music files
        if (strcmp(ext, ".nsf") == 0 || strcmp(ext, ".NSF") == 0) {
            if (size >= 128 && data[0] == 'N' && data[1] == 'E' &&
                data[2] == 'S' && data[3] == 'M' && data[4] == 0x1A) {
                return 1.0f;  // Perfect NSF match
            }
            return 0.9f;  // .nsf extension but no header
        }
    }
    
    // Check for iNES header without extension
    if (size >= 16 && data[0] == 'N' && data[1] == 'E' &&
        data[2] == 'S' && data[3] == 0x1A) {
        return 0.95f;
    }

    // Check for NSF header without extension
    if (size >= 128 && data[0] == 'N' && data[1] == 'E' &&
        data[2] == 'S' && data[3] == 'M' && data[4] == 0x1A) {
        return 0.95f;
    }
    
    return 0.0f;
}

// ============================================================================
// NintendoSystem static descriptor (function-local static, same pattern as TED)
// ============================================================================

template<NintendoVariant V>
const SystemDescriptor& NintendoSystem<V>::static_descriptor() {
    static const format_descriptor_t* const formats[] = {
        &NSF_FORMAT_DESCRIPTOR, nullptr
    };
    static const SystemDescriptor desc = {
        Traits::full_name,
        Traits::short_id,
        Traits::description,
        formats,
        create_nes_hardware_traits(),
        nes_can_load_file
    };
    return desc;
}

template<NintendoVariant V>
NintendoSystem<V>::NintendoSystem()
    : EmulatedSystem()
    , cpu_(nullptr)
    , pins_(0)
    , is_pal_(false)
    , system_ready_(false)
    , cycles_per_frame_(29829)
    , initialized_(false)
    , audio_sample_rate_(44100)
    , audio_sample_counter_(0)
    , residual_time_(0.0)
{
    hardware_traits_ = create_nes_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
}

template<NintendoVariant V>
NintendoSystem<V>::~NintendoSystem() {
    shutdown();
}

template<NintendoVariant V>
const SystemDescriptor& NintendoSystem<V>::get_descriptor() const {
    return static_descriptor();
}

template<NintendoVariant V>
bool NintendoSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    
    // Check if region changed and update cached target FPS
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        cached_target_fps_ = hardware_traits_.video_standard_configs[config_.region_option_index].timing.target_fps;
        bool new_is_pal = (hardware_traits_.video_standard_configs[config_.region_option_index].standard == VideoStandard::PAL);
        if (new_is_pal != is_pal_) {
            is_pal_ = new_is_pal;
            // Need to recreate system with new region
            if (initialized_) {
                shutdown();
                initialize();
            }
        }
    }
    
    return true;
}

template<NintendoVariant V>
bool NintendoSystem<V>::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
    }
    
    return true;
}

// ============================================================================
// Auto-detect optimal configuration from iNES header
// ============================================================================
template<NintendoVariant V>
SystemConfiguration NintendoSystem<V>::detect_optimal_configuration(
    const char* filepath, const uint8_t* data, size_t size) {

    SystemConfiguration config = EmulatedSystem::detect_optimal_configuration(filepath, data, size);

    if (!data || size < 16) return config;

    // Verify iNES header magic: "NES\x1A"
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A)
        return config;

    // Check for iNES 2.0 format (bits 2-3 of byte 7 == 0b10)
    bool is_ines2 = ((data[7] & 0x0C) == 0x08);

    bool detected_pal = false;

    if (is_ines2) {
        // iNES 2.0: byte 12, bits 0-1 encode the CPU/PPU timing mode
        //   0 = NTSC, 1 = PAL, 2 = Multi-region, 3 = Dendy
        uint8_t timing = data[12] & 0x03;
        if (timing == 1) {
            detected_pal = true;
            printf("%s: iNES 2.0 header indicates PAL timing\n", Traits::name);
        } else {
            printf("%s: iNES 2.0 header indicates %s timing\n", Traits::name,
                   timing == 0 ? "NTSC" : (timing == 2 ? "Multi-region" : "Dendy"));
        }
    } else {
        // iNES 1.0: byte 9, bit 0 — unofficial but widely used
        //   0 = NTSC, 1 = PAL
        if (data[9] & 0x01) {
            detected_pal = true;
            printf("%s: iNES 1.0 header byte 9 indicates PAL\n", Traits::name);
        }
    }

    if (detected_pal) {
        // PAL is region option index 1 in create_nes_hardware_traits()
        if (hardware_traits_.video_standard_configs.size() > 1) {
            config.region_option_index = 1;
        }
    }

    return config;
}

template<NintendoVariant V>
bool NintendoSystem<V>::initialize() {
    if (initialized_) {
        return true;
    }
    
    printf("%s: Initializing system (%s)\n", Traits::name,
           is_pal_ ? "PAL" : "NTSC");
    
    // Create CPU with integrated APU
    cpu_ = new RICOH_2A03();
    if (!cpu_) {
        printf("%s: Failed to create CPU\n", Traits::name);
        return false;
    }
    
    // Initialize CPU
    cpu_->init();
    
    // Set APU region
    cpu_->set_apu_region(is_pal_);
    
    // Create PPU
    ppu_ = std::make_shared<PPU>(is_pal_);
    
    // Create memory bus
    bus_ = std::make_shared<MemoryBus>();
    bus_->connect_ppu(ppu_);
    
    setup_audio_timing();
    setup_connector_ports();

    // Register chips for the Hardware menu and debug windows
    register_nes_chips();

    initialized_ = true;
    
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::shutdown() {
    // Save battery-backed SRAM before shutdown
    if (cartridge_ && cartridge_->battery_backed) {
        cartridge_->save_sram(cartridge_->sram_path_for_rom(cartridge_->get_rom_filepath()));
    }
    if (cpu_) {
        printf("%s: Shutting down system\n", Traits::name);
        delete cpu_;
        cpu_ = nullptr;
    }
    initialized_ = false;
    system_ready_ = false;
}

template<NintendoVariant V>
void NintendoSystem<V>::reset() {
    if (!cpu_) return;
    
    printf("%s: Resetting system\n", Traits::name);
    
    pins_ = NES_BUS_DEFAULT_STATE;
    cpu_->reset(pins_);
    
    if (ppu_) {
        ppu_->reset();
    }
    
    if (bus_) {
        bus_->reset();
    }
    
    if (cartridge_) {
        cartridge_->reset();
    }
    
    // Read reset vector from $FFFC/$FFFD and set CPU PC
    // (fam65xx::reset() leaves PC at 0 — the caller must load it)
    if (bus_) {
        uint8_t lo = peek_memory(0xFFFC);
        uint8_t hi = peek_memory(0xFFFD);
        uint16_t reset_vector = lo | (hi << 8);
        cpu_->set(REG_PC, reset_vector);
        cpu_->set(REG_AB, reset_vector);
        printf("%s: Reset vector $%04X\n", Traits::name, reset_vector);
    }
    
    total_cycles_ = 0;
    residual_time_ = 0.0;
    audio_sample_counter_ = 0;
}

template<NintendoVariant V>
void NintendoSystem<V>::tick() {
    if (!cpu_) return;
    
    clock();
    // total_cycles_ is updated in clock()
}

template<NintendoVariant V>
void NintendoSystem<V>::run_frame() {
    if (!system_ready_ || !ppu_) return;
    
    ppu_->frame_complete = false;
    while (!ppu_->frame_complete) {
        clock();
    }

    // Tick all attached peripheral devices
    tick_peripherals();
}

template<NintendoVariant V>
bool NintendoSystem<V>::load_file(const char* filepath) {
    if (!cpu_) {
        if (!initialize()) {
            return false;
        }
    }
    
    printf("%s: Loading file: %s\n", Traits::name, filepath);

    // =========================================================================
    // NSF FILE — Use the format system to parse, then launch NSF player
    // =========================================================================
    const char* ext = strrchr(filepath, '.');
    bool is_nsf = (ext && (strcmp(ext, ".nsf") == 0 || strcmp(ext, ".NSF") == 0));

    // Also check by header magic for extensionless files
    if (!is_nsf) {
        std::ifstream probe(filepath, std::ios::binary);
        uint8_t magic[5] = {};
        if (probe.read(reinterpret_cast<char*>(magic), 5)) {
            if (magic[0] == 'N' && magic[1] == 'E' && magic[2] == 'S' &&
                magic[3] == 'M' && magic[4] == 0x1A) {
                is_nsf = true;
            }
        }
    }

    if (is_nsf) {
        // Read entire file
        size_t file_size = 0;
        uint8_t* file_data = format_read_entire_file(filepath, &file_size);
        if (!file_data) {
            printf("%s: Failed to read NSF file\n", Traits::name);
            return false;
        }

        // Parse NSF header
        nsf_header_t header;
        if (!nsf_parse_header(file_data, file_size, &header)) {
            printf("%s: Invalid NSF header\n", Traits::name);
            free(file_data);
            return false;
        }

        // Extract payload
        const uint8_t* payload = file_data + 128;
        size_t payload_size = file_size - 128;

        program_data_t prog = {};
        prog.data = const_cast<uint8_t*>(payload);  // Temporary, won't be freed
        prog.data_size = payload_size;
        prog.load_addr = header.load_addr;

        // Compute 0-based subtune index from 1-based start_song
        uint16_t subtune = header.start_song;
        if (subtune > 0) subtune--;

        // Launch NSF player
        nsf_cartridge_ = nes_apply_nsf_load(
            cpu_, ppu_.get(), bus_.get(),
            &header, &prog, subtune, is_pal_);

        if (!nsf_cartridge_) {
            printf("%s: Failed to apply NSF load\n", Traits::name);
            free(file_data);
            return false;
        }

        // Save state for subtune switching
        active_nsf_header_ = header;
        active_nsf_data_.assign(payload, payload + payload_size);
        active_nsf_subtune_ = subtune;
        nsf_player_active_ = true;
        system_ready_ = true;

        // Set program title from NSF header (strings are already UTF-8
        // after parsing — Latin-1→UTF-8 conversion happens in nsf_parse_header).
        program_title_ = header.name;
        if (header.artist[0]) {
            program_title_ += " - ";
            program_title_ += header.artist;
        }

        printf("%s: NSF player active — \"%s\" by %s\n",
               Traits::name, header.name, header.artist);
        free(file_data);
        return true;
    }

    // =========================================================================
    // STANDARD PATH — iNES ROM cartridge
    // =========================================================================
    nsf_player_active_ = false;
    nsf_cartridge_.reset();
    
    try {
        cartridge_ = std::make_shared<Cartridge>(filepath);
        bus_->connect_cartridge(cartridge_);
        ppu_->connect_cartridge(cartridge_);
        
        // Reset system with new cartridge
        reset();
        system_ready_ = true;

        // Set program title to bare filename
        const char* name = filepath;
        const char* sep = strrchr(filepath, '/');
        if (!sep) sep = strrchr(filepath, '\\');
        if (sep) name = sep + 1;
        program_title_ = name;
        
        printf("%s: Cartridge loaded successfully\n", Traits::name);
        return true;
    } catch (const std::exception& e) {
        printf("%s: Failed to load cartridge: %s\n", Traits::name, e.what());
        return false;
    }
}

template<NintendoVariant V>
uint32_t* NintendoSystem<V>::get_framebuffer() {
    if (!ppu_ || !rgba_framebuffer_) return rgba_framebuffer_;
    
    // Get NES screen buffer and copy to our framebuffer
    const std::vector<uint32_t>& nes_screen = ppu_->get_screen();
    if (!nes_screen.empty() && rgba_framebuffer_) {
        // NES screen is 256x240, copy directly
        memcpy(rgba_framebuffer_, nes_screen.data(), 256 * 240 * sizeof(uint32_t));
    }
    
    return rgba_framebuffer_;
}

template<NintendoVariant V>
void NintendoSystem<V>::get_display_dimensions(int* width, int* height) const {
    *width = 256;
    *height = 240;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

template<NintendoVariant V>
void NintendoSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
#ifdef IMGUI_VERSION
    if (!cpu_) return;
    
    // Map keyboard to NES controller buttons
    uint8_t button = 0;
    bool mapped = true;
    
    switch (key) {
        case SDLK_z:      button = 0x40; break;  // B
        case SDLK_x:      button = 0x80; break;  // A
        case SDLK_RETURN: button = 0x10; break;  // Start
        case SDLK_RSHIFT: button = 0x20; break;  // Select
        case SDLK_UP:     button = 0x08; break;  // Up
        case SDLK_DOWN:   button = 0x04; break;  // Down
        case SDLK_LEFT:   button = 0x02; break;  // Left
        case SDLK_RIGHT:  button = 0x01; break;  // Right
        default: mapped = false; break;
    }
    
    if (mapped) {
        if (pressed) {
            press_button(0, static_cast<Controller::Button>(button));
        } else {
            release_button(0, static_cast<Controller::Button>(button));
        }
    }
#else
    (void)key;
    (void)pressed;
#endif
}

template<NintendoVariant V>
void NintendoSystem<V>::handle_controller_event(int controller, int button, bool pressed) {
    if (!bus_) return;
    
    if (pressed) {
        press_button(controller, static_cast<Controller::Button>(button));
    } else {
        release_button(controller, static_cast<Controller::Button>(button));
    }
}

// =============================================================================
// NSF Player — Extended keyboard handler with subtune selection
// =============================================================================

template<NintendoVariant V>
void NintendoSystem<V>::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode,
                                          uint16_t mod, bool pressed, bool repeat) {
    // NSF player subtune selection — intercept before controller mapping
    if (nsf_player_active_ && pressed && !repeat) {
        if (handle_nsf_player_key(key)) return;
    }

    // Fall through to standard keyboard handling
    if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

// =============================================================================
// NSF Player — Subtune selection via keyboard
// =============================================================================
//
// Digits 1-9:  select subtune 1-9 directly (0-based index 0-8)
// Digit 0:     select subtune 10 (0-based index 9)
// Right arrow:  next subtune (wraps from last → first)
// Left arrow:   previous subtune (wraps from first → last)
// ESC:          exit application
// =============================================================================

template<NintendoVariant V>
bool NintendoSystem<V>::handle_nsf_player_key(SDL_Keycode key) {
    if (!cpu_ || active_nsf_header_.num_songs == 0) return false;

    const uint16_t num_songs = active_nsf_header_.num_songs;
    int new_subtune = -1;

    // Digit keys: 1→subtune 1, ..., 9→subtune 9, 0→subtune 10
    if (key >= SDLK_0 && key <= SDLK_9) {
        int digit = (key == SDLK_0) ? 10 : (key - SDLK_0);
        if (digit <= num_songs) {
            new_subtune = digit - 1;  // Convert to 0-based
        }
    }
    // Cursor right = next subtune (with wrapping)
    else if (key == SDLK_RIGHT) {
        new_subtune = (active_nsf_subtune_ + 1) % num_songs;
    }
    // Cursor left = previous subtune (with wrapping)
    else if (key == SDLK_LEFT) {
        new_subtune = (active_nsf_subtune_ == 0) ? (num_songs - 1)
                                                   : (active_nsf_subtune_ - 1);
    }
    // ESC = exit application while in NSF player mode
    else if (key == SDLK_ESCAPE) {
        request_quit();
        return true;
    }

    if (new_subtune < 0) return false;
    if (static_cast<uint16_t>(new_subtune) == active_nsf_subtune_) return true;

    active_nsf_subtune_ = static_cast<uint16_t>(new_subtune);
    nes_nsf_switch_subtune(cpu_, ppu_.get(), bus_.get(),
                            nsf_cartridge_.get(), &active_nsf_header_,
                            active_nsf_data_.data(), active_nsf_data_.size(),
                            active_nsf_subtune_, is_pal_);
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::render_system_menu_items() {
#ifdef IMGUI_VERSION
    char reset_label[32];
    snprintf(reset_label, sizeof(reset_label), "Reset %s", Traits::name);
    if (ImGui::MenuItem(reset_label)) {
        reset();
    }
    
    if (ImGui::MenuItem("Eject Cartridge", nullptr, false, is_cartridge_loaded())) {
        eject_cartridge();
    }
#endif
}

template<NintendoVariant V>
const char* NintendoSystem<V>::get_mode_label() const {
    return nsf_player_active_ ? "NSF Player" : nullptr;
}

template<NintendoVariant V>
std::string NintendoSystem<V>::get_subtitle_info() const {
    if (!nsf_player_active_ || active_nsf_header_.num_songs <= 1) return {};
    return "[" + std::to_string(active_nsf_subtune_ + 1) + "/" +
           std::to_string(active_nsf_header_.num_songs) + "]";
}

// ============================================================================
// Chip Registration — populate registered_chips_ for Hardware menu + debug
// ============================================================================

template<NintendoVariant V>
void NintendoSystem<V>::register_nes_chips() {
    auto* cpu = cpu_;

    // CPU (Ricoh 2A03) — native ChipBase, registered directly
    register_chip(static_cast<ChipBase*>(cpu_),
        "Ricoh 2A03 (6502 + APU)", "2A03", "CPU", 0x0000);

    // PPU (Ricoh 2C02) — native ChipBase, registered directly
    register_chip(ppu_.get(),
        "Ricoh 2C02 PPU", "PPU", "Video", 0x2000);

    // APU (built into 2A03) — native ChipBase, registered directly
    register_chip(cpu_->get_apu(),
        "APU (built-in 2A03)", "APU", "Audio", 0x4000);

    // RAM — MemoryChip with layout rendering
    register_chip(std::make_unique<MemoryChip>(
        ChipInfo{"SRAM", "Various"}, 2048, MemoryChip::SRAM, &pins_,
        "RAM", 0x0000));

    // Cartridge (no suitable chip type — mapper + ROM + optional RAM)
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Cartridge", "Various"}, "Cartridge", "Cart", "Memory", 0x4020));
}

template<NintendoVariant V>
void NintendoSystem<V>::render_configuration_ui() {
#ifdef IMGUI_VERSION
    ImGui::Text("%s Configuration", Traits::name);
    ImGui::Separator();
    
    // Region configuration
    ImGui::Text("Video Region:");
    for (size_t i = 0; i < hardware_traits_.video_standard_configs.size(); i++) {
        bool selected = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.video_standard_configs[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.region_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
    
    ImGui::Separator();
    
    // Cartridge info
    if (is_cartridge_loaded()) {
        ImGui::Text("Cartridge: Loaded");
    } else {
        ImGui::TextDisabled("Cartridge: None");
    }
#endif
}

template<NintendoVariant V>
void NintendoSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

template<NintendoVariant V>
uint32_t NintendoSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    uint32_t avail = static_cast<uint32_t>(audio_buffer_.size());
    uint32_t to_copy = avail < max_samples ? avail : max_samples;
    if (to_copy > 0) {
        memcpy(buffer, audio_buffer_.data(), to_copy * sizeof(float));
        // Remove consumed samples (shift remainder to front)
        audio_buffer_.erase(audio_buffer_.begin(),
                            audio_buffer_.begin() + to_copy);
    }
    return to_copy;
}

template<NintendoVariant V>
void NintendoSystem<V>::setup_audio_timing() {
    uint32_t cpu_freq = is_pal_ ? nes_constants::CPU_FREQ_PAL : nes_constants::CPU_FREQ_NTSC;
    audio_samples_per_frame_ = (audio_sample_rate_ * (is_pal_ ? 50 : 60)) / (is_pal_ ? 50 : 60);
}

template<NintendoVariant V>
void NintendoSystem<V>::eject_cartridge() {
    // Save battery-backed SRAM before ejecting
    if (cartridge_ && cartridge_->battery_backed) {
        cartridge_->save_sram(cartridge_->sram_path_for_rom(cartridge_->get_rom_filepath()));
    }
    cartridge_.reset();
    if (bus_) {
        bus_->connect_cartridge(nullptr);
    }
    if (ppu_) {
        ppu_->connect_cartridge(nullptr);
    }
    system_ready_ = false;
}

template<NintendoVariant V>
void NintendoSystem<V>::clock() {
    // Clock the memory bus (which clocks PPU 3 times)
    bus_->clock();
    
    // Clock CPU every 3 PPU cycles
    if (bus_->system_clock_counter % 3 == 0) {
        // Handle DMA stall
        if (bus_->dma_transfer) {
            // CPU is stalled during DMA
        } else {
            // PHI2: CPU sets up bus (address, R/W)
            pins_ = cpu_->tick<RICOH_2A03::Phase::PHI2>(pins_);
            
            // Service CPU memory request via bus (between phases)
            pins_ = bus_->mem_tick(pins_);
            
            // Handle NMI from PPU — NMI is edge-sensitive (active low)
            if (ppu_->get_nmi()) {
                // Assert NMI: drive pin LOW (bit 34 = 0)
                BUS_CLR_BIT(pins_, BUS_NMI_BIT);
            } else {
                // Deassert NMI: release pin HIGH (bit 34 = 1)
                // Required for edge detection — next NMI needs a new HIGH→LOW
                BUS_SET_BIT(pins_, BUS_NMI_BIT);
            }
            
            // Handle IRQ from cartridge (e.g. MMC3 scanline counter)
            // and APU — IRQ is level-sensitive (active low)
            bool irq_asserted = false;
            if (cartridge_ && cartridge_->irq_state()) {
                irq_asserted = true;
                cartridge_->irq_clear();
            }
            if (cpu_->apu_irq()) {
                irq_asserted = true;
            }
            if (irq_asserted) {
                BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
            } else {
                BUS_SET_BIT(pins_, BUS_IRQ_BIT);
            }

            // PHI1: CPU internal operations (including APU clock)
            pins_ = cpu_->tick<RICOH_2A03::Phase::PHI1>(pins_);
        }
        
        // Generate audio sample
        if (audio_sample_counter_ == 0) {
            float sample = cpu_->generate_audio_sample();
            audio_buffer_.push_back(sample);
        }
        audio_sample_counter_ = (audio_sample_counter_ + 1) % (is_pal_ ? 33 : 37);
    }
    
    total_cycles_++;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_controller_state(int controller, uint8_t state) {
    if (!bus_ || controller < 0 || controller >= 2) return;
    
    // Set individual buttons based on state
    for (int i = 0; i < 8; i++) {
        bool pressed = (state >> i) & 1;
        Controller::Button button = static_cast<Controller::Button>(1 << i);
        bus_->controllers[controller].set_button_state(button, pressed);
    }
}

template<NintendoVariant V>
void NintendoSystem<V>::press_button(int controller, Controller::Button button) {
    if (!bus_ || controller < 0 || controller >= 2) return;
    bus_->controllers[controller].set_button_state(button, true);
}

template<NintendoVariant V>
void NintendoSystem<V>::release_button(int controller, Controller::Button button) {
    if (!bus_ || controller < 0 || controller >= 2) return;
    bus_->controllers[controller].set_button_state(button, false);
}

template<NintendoVariant V>
const std::vector<uint32_t>& NintendoSystem<V>::get_screen() const {
    static std::vector<uint32_t> empty_screen;
    return ppu_ ? ppu_->get_screen() : empty_screen;
}

template<NintendoVariant V>
const std::vector<uint32_t>& NintendoSystem<V>::get_pattern_table(int table, uint8_t palette) const {
    static std::vector<uint32_t> empty_table;
    return ppu_ ? ppu_->get_pattern_table(table, palette) : empty_table;
}

template<NintendoVariant V>
void NintendoSystem<V>::set_audio_sample_rate(uint32_t rate) {
    audio_sample_rate_ = rate;
    setup_audio_timing();
}

template<NintendoVariant V>
bool NintendoSystem<V>::save_state(const std::string& filename) const {
    if (!cpu_ || !ppu_ || !bus_) return false;

    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;

    // Magic + version
    const char magic[4] = {'C','S','S','1'};  // cermu save state v1
    f.write(magic, 4);

    // CPU state — save pins and the full register file
    f.write(reinterpret_cast<const char*>(&pins_), sizeof(pins_));

    // PPU state
    f.write(reinterpret_cast<const char*>(&ppu_->regs), sizeof(ppu_->regs));
    f.write(reinterpret_cast<const char*>(ppu_->vram.data()), ppu_->vram.size());
    f.write(reinterpret_cast<const char*>(ppu_->oam.data()), ppu_->oam.size());
    f.write(reinterpret_cast<const char*>(ppu_->palette.data()), ppu_->palette.size());
    f.write(reinterpret_cast<const char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl = ppu_->scanline; f.write(reinterpret_cast<const char*>(&sl), sizeof(sl));
    uint16_t cy = ppu_->cycle;   f.write(reinterpret_cast<const char*>(&cy), sizeof(cy));
    uint64_t fc = ppu_->frame_count; f.write(reinterpret_cast<const char*>(&fc), sizeof(fc));

    // Bus state — CPU RAM
    f.write(reinterpret_cast<const char*>(bus_->cpu_ram.data()), bus_->cpu_ram.size());
    f.write(reinterpret_cast<const char*>(&bus_->dma_page), 1);
    f.write(reinterpret_cast<const char*>(&bus_->dma_addr), 1);
    f.write(reinterpret_cast<const char*>(&bus_->dma_data), 1);
    uint8_t dma_flags = (bus_->dma_transfer ? 1 : 0) | (bus_->dma_dummy ? 2 : 0);
    f.write(reinterpret_cast<const char*>(&dma_flags), 1);
    f.write(reinterpret_cast<const char*>(&bus_->system_clock_counter), sizeof(bus_->system_clock_counter));

    // PRG RAM (if present)
    if (cartridge_ && !cartridge_->prg_ram.empty()) {
        uint32_t ram_size = static_cast<uint32_t>(cartridge_->prg_ram.size());
        f.write(reinterpret_cast<const char*>(&ram_size), sizeof(ram_size));
        f.write(reinterpret_cast<const char*>(cartridge_->prg_ram.data()), ram_size);
    } else {
        uint32_t zero = 0;
        f.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
    }

    printf("%s: Saved state to %s\n", Traits::name, filename.c_str());
    return true;
}

template<NintendoVariant V>
bool NintendoSystem<V>::load_state(const std::string& filename) {
    if (!cpu_ || !ppu_ || !bus_) return false;

    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;

    char magic[4];
    f.read(magic, 4);
    if (magic[0] != 'C' || magic[1] != 'S' || magic[2] != 'S' || magic[3] != '1') {
        printf("%s: Invalid save state file\n", Traits::name);
        return false;
    }

    // CPU pins
    f.read(reinterpret_cast<char*>(&pins_), sizeof(pins_));

    // PPU state
    f.read(reinterpret_cast<char*>(&ppu_->regs), sizeof(ppu_->regs));
    f.read(reinterpret_cast<char*>(ppu_->vram.data()), ppu_->vram.size());
    f.read(reinterpret_cast<char*>(ppu_->oam.data()), ppu_->oam.size());
    f.read(reinterpret_cast<char*>(ppu_->palette.data()), ppu_->palette.size());
    f.read(reinterpret_cast<char*>(&ppu_->internal), sizeof(ppu_->internal));
    int16_t sl; f.read(reinterpret_cast<char*>(&sl), sizeof(sl)); ppu_->scanline = sl;
    uint16_t cy; f.read(reinterpret_cast<char*>(&cy), sizeof(cy)); ppu_->cycle = cy;
    uint64_t fc; f.read(reinterpret_cast<char*>(&fc), sizeof(fc)); ppu_->frame_count = fc;

    // Bus state
    f.read(reinterpret_cast<char*>(bus_->cpu_ram.data()), bus_->cpu_ram.size());
    f.read(reinterpret_cast<char*>(&bus_->dma_page), 1);
    f.read(reinterpret_cast<char*>(&bus_->dma_addr), 1);
    f.read(reinterpret_cast<char*>(&bus_->dma_data), 1);
    uint8_t dma_flags; f.read(reinterpret_cast<char*>(&dma_flags), 1);
    bus_->dma_transfer = (dma_flags & 1) != 0;
    bus_->dma_dummy = (dma_flags & 2) != 0;
    f.read(reinterpret_cast<char*>(&bus_->system_clock_counter), sizeof(bus_->system_clock_counter));

    // PRG RAM
    uint32_t ram_size = 0;
    f.read(reinterpret_cast<char*>(&ram_size), sizeof(ram_size));
    if (ram_size > 0 && cartridge_ && cartridge_->prg_ram.size() >= ram_size) {
        f.read(reinterpret_cast<char*>(cartridge_->prg_ram.data()), ram_size);
    }

    printf("%s: Loaded state from %s\n", Traits::name, filename.c_str());
    return true;
}

template<NintendoVariant V>
void NintendoSystem<V>::power_cycle() {
    eject_cartridge();
    reset();
}

// ============================================================================
// CONNECTOR PORT SETUP — NES
// ============================================================================
// NES has: 2× front controller ports (7-pin) and 1× bottom expansion port (48-pin).
// Controller ports use a serial shift-register protocol (LATCH + CLK + D0).

static const ConnectorDefinition nes_controller_1_def = {
    ConnectorType::CONTROLLER_NES,
    "Controller Port 1",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition nes_controller_2_def = {
    ConnectorType::CONTROLLER_NES,
    "Controller Port 2",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition nes_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    ConnectorSignals::NES_EXPANSION_SIGNALS,
    ConnectorSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

// Famicom-specific: hardwired controllers (not removable)
static const ConnectorDefinition fc_controller_1_def = {
    ConnectorType::CONTROLLER_NES,
    "Controller I (hardwired)",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition fc_controller_2_def = {
    ConnectorType::CONTROLLER_NES,
    "Controller II (hardwired, microphone)",
    ConnectorSignals::NES_CONTROLLER_SIGNALS,
    ConnectorSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition fc_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port (15-pin)",
    ConnectorSignals::NES_EXPANSION_SIGNALS,
    ConnectorSignals::NES_EXPANSION_SIGNAL_COUNT,
    false, false
};

template<NintendoVariant V>
void NintendoSystem<V>::setup_connector_ports() {
    connector_ports_.clear();

    if constexpr (Traits::is_famicom) {
        // Famicom: hardwired controllers, 15-pin expansion port
        add_connector_port(fc_controller_1_def, 1);
        add_connector_port(fc_controller_2_def, 2);
        add_connector_port(fc_expansion_def, 0);
        printf("%s: Created %zu connector ports\n", Traits::name, connector_ports_.size());
    } else {
        // NES: removable controller ports, bottom expansion
        add_connector_port(nes_controller_1_def, 1);
        add_connector_port(nes_controller_2_def, 2);
        add_connector_port(nes_expansion_def, 0);
        printf("%s: Created %zu connector ports\n", Traits::name, connector_ports_.size());
    }
}

// ============================================================================
// Debug / Test harness helpers
// ============================================================================

template<NintendoVariant V>
uint8_t NintendoSystem<V>::peek_memory(uint16_t addr) const {
    if (!bus_) return 0;

    // $0000-$1FFF: CPU RAM (mirrored every 2KB)
    if (addr < 0x2000) {
        return bus_->cpu_ram[addr & 0x07FF];
    }

    // $2000-$3FFF: PPU registers (read-only peek)
    if (addr >= 0x2000 && addr <= 0x3FFF && ppu_) {
        return ppu_->cpu_peek(addr);
    }

    // $6000-$FFFF: Cartridge space (PRG RAM + PRG ROM)
    if (addr >= 0x6000 && cartridge_) {
        return cartridge_->peek(addr);
    }

    return 0;
}

template<NintendoVariant V>
uint16_t NintendoSystem<V>::get_cpu_pc() const {
    if (!cpu_) return 0;
    return static_cast<uint16_t>(cpu_->get(REG_PC));
}

} // namespace nes_system

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

template class nes_system::NintendoSystem<nes_system::NintendoVariant::NES>;
template class nes_system::NintendoSystem<nes_system::NintendoVariant::FAMICOM>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(nes_system::NESSystem::static_descriptor(), []() {
    return std::make_unique<nes_system::NESSystem>();
})

REGISTER_SYSTEM(nes_system::FamicomSystem::static_descriptor(), []() {
    return std::make_unique<nes_system::FamicomSystem>();
})