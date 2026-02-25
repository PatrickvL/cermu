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

uint8_t PPU::cpu_read(uint16_t addr, bool read_only) {
    uint8_t data = 0x00;
    
    switch (addr) {
        case 0x2000: // Control - write only
            break;
        case 0x2001: // Mask - write only
            break;
        case 0x2002: // Status
            data = (regs.status & 0xE0) | (regs.data & 0x1F);
            regs.status &= ~0x80; // Clear VBlank flag
            internal.w = false; // Reset write toggle
            break;
        case 0x2003: // OAM Address - write only
            break;
        case 0x2004: // OAM Data
            data = oam[regs.oam_addr];
            break;
        case 0x2005: // Scroll - write only
            break;
        case 0x2006: // PPU Address - write only
            break;
        case 0x2007: // PPU Data
            data = regs.data;
            regs.data = ppu_read(internal.v);
            
            // Palette reads are immediate
            if (internal.v >= 0x3F00) {
                data = regs.data;
            }
            
            // Increment VRAM address
            internal.v += (regs.ctrl & 0x04) ? 32 : 1;
            break;
    }
    
    return data;
}

void PPU::cpu_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x2000: // Control
            regs.ctrl = data;
            internal.t = (internal.t & 0xF3FF) | ((data & 0x03) << 10);
            break;
        case 0x2001: // Mask
            regs.mask = data;
            break;
        case 0x2002: // Status - read only
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
                // First write - X scroll
                internal.t = (internal.t & 0xFFE0) | ((data & 0xF8) >> 3);
                internal.x = data & 0x07;
                internal.w = true;
            } else {
                // Second write - Y scroll
                internal.t = (internal.t & 0x8FFF) | ((data & 0x07) << 12);
                internal.t = (internal.t & 0xFC1F) | ((data & 0xF8) << 2);
                internal.w = false;
            }
            break;
        case 0x2006: // PPU Address
            if (!internal.w) {
                // First write - high byte
                internal.t = (internal.t & 0x80FF) | ((data & 0x3F) << 8);
                internal.w = true;
            } else {
                // Second write - low byte
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

uint8_t PPU::ppu_read(uint16_t addr, bool read_only) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;
    
    if (cart && cart->ppu_read(addr, data)) {
        // Cartridge handled the read
    } else if (addr <= 0x1FFF) {
        // Pattern table
        data = pattern_table[addr >> 12][addr & 0x0FFF];
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables
        addr &= 0x0FFF;
        
        // Handle mirroring
        if (cart) {
            if (cart->get_mirror_vertical()) {
                // Vertical mirroring
                if (addr <= 0x03FF) data = vram[addr & 0x03FF];
                if (addr >= 0x0400 && addr <= 0x07FF) data = vram[(addr & 0x03FF) + 0x0400];
                if (addr >= 0x0800 && addr <= 0x0BFF) data = vram[addr & 0x03FF];
                if (addr >= 0x0C00 && addr <= 0x0FFF) data = vram[(addr & 0x03FF) + 0x0400];
            } else {
                // Horizontal mirroring
                if (addr <= 0x03FF) data = vram[addr & 0x03FF];
                if (addr >= 0x0400 && addr <= 0x07FF) data = vram[addr & 0x03FF];
                if (addr >= 0x0800 && addr <= 0x0BFF) data = vram[(addr & 0x03FF) + 0x0400];
                if (addr >= 0x0C00 && addr <= 0x0FFF) data = vram[(addr & 0x03FF) + 0x0400];
            }
        }
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
        // Nametables
        addr &= 0x0FFF;
        
        // Handle mirroring
        if (cart) {
            if (cart->get_mirror_vertical()) {
                // Vertical mirroring
                if (addr <= 0x03FF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0400 && addr <= 0x07FF) vram[(addr & 0x03FF) + 0x0400] = data;
                if (addr >= 0x0800 && addr <= 0x0BFF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0C00 && addr <= 0x0FFF) vram[(addr & 0x03FF) + 0x0400] = data;
            } else {
                // Horizontal mirroring
                if (addr <= 0x03FF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0400 && addr <= 0x07FF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0800 && addr <= 0x0BFF) vram[(addr & 0x03FF) + 0x0400] = data;
                if (addr >= 0x0C00 && addr <= 0x0FFF) vram[(addr & 0x03FF) + 0x0400] = data;
            }
        }
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
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    
    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
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
    mirror_horizontal = !(header.mapper1 & 0x01);
    mirror_vertical = header.mapper1 & 0x01;
    battery_backed = header.mapper1 & 0x02;
    
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
    
    // Create appropriate mapper
    switch (mapper_id) {
        case 0:
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            break;
        default:
            std::cout << "Warning: Unsupported mapper " << (int)mapper_id << std::endl;
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            break;
    }
    
    return true;
}

bool Cartridge::cpu_read(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr;
    if (mapper->cpu_map_read(addr, mapped_addr)) {
        if (mapped_addr < prg_memory.size()) {
            data = prg_memory[mapped_addr];
            return true;
        }
    }
    return false;
}

bool Cartridge::cpu_write(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr;
    if (mapper->cpu_map_write(addr, mapped_addr, data)) {
        if (mapped_addr < prg_memory.size()) {
            prg_memory[mapped_addr] = data;
            return true;
        }
    }
    return false;
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
    }
}

// ============================================================================
// MEMORY BUS IMPLEMENTATION
// ============================================================================

bus_state_t MemoryBus::mem_tick(bus_state_t bus) {
    uint16_t addr = BUS_GET_ADDR(bus);
    const bool is_read = BUS_GET_BIT(bus, BUS_RW_BIT);
    
    if (!is_read) {
        // ---- WRITE ---- (RW=0 per 6502 convention)
        uint8_t data = BUS_GET_DATA(bus);
        
        if (addr <= 0x1FFF) {
            // CPU RAM (with mirroring)
            cpu_ram[addr & 0x07FF] = data;
        } else if (addr <= 0x3FFF) {
            // PPU registers (with mirroring)
            if (ppu) {
                ppu->cpu_write(addr & 0x2007, data);
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
            // APU registers handled by NES6502 CPU
        } else {
            // Cartridge space ($4020-$FFFF)
            if (cartridge) {
                cartridge->cpu_write(addr, data);
            }
        }
    } else {
        // ---- READ ----
        uint8_t data = 0x00;
        
        if (addr <= 0x1FFF) {
            // CPU RAM (with mirroring)
            data = cpu_ram[addr & 0x07FF];
        } else if (addr <= 0x3FFF) {
            // PPU registers (with mirroring)
            if (ppu) {
                data = ppu->cpu_read(addr & 0x2007);
            }
        } else if (addr <= 0x4017) {
            // APU and I/O registers
            if (addr == 0x4016) {
                data = controllers[0].read();
            } else if (addr == 0x4017) {
                data = controllers[1].read();
            }
            // APU registers handled by NES6502 CPU
        } else {
            // Cartridge space ($4020-$FFFF)
            if (cartridge) {
                cartridge->cpu_read(addr, data);
            }
        }
        
        BUS_SET_DATA(bus, data);
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
    cpu_ = nes6502_create();
    if (!cpu_) {
        printf("%s: Failed to create CPU\n", Traits::name);
        return false;
    }
    
    // Set APU region
    nes6502_set_apu_region(cpu_, is_pal_);
    
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
    if (cpu_) {
        printf("%s: Shutting down system\n", Traits::name);
        nes6502_destroy(cpu_);
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
    nes6502_reset(cpu_, pins_);
    
    if (ppu_) {
        ppu_->reset();
    }
    
    if (bus_) {
        bus_->reset();
    }
    
    if (cartridge_) {
        cartridge_->reset();
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
    register_chip(nes6502_as_chip_base(cpu),
        "Ricoh 2A03 (6502 + APU)", "2A03", "CPU", 0x0000);

    // PPU (Ricoh 2C02) — native ChipBase, registered directly
    register_chip(ppu_.get(),
        "Ricoh 2C02 PPU", "PPU", "Video", 0x2000);

    // APU (built into 2A03) — native ChipBase, registered directly
    register_chip(nes6502_get_apu(cpu),
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
            pins_ = nes6502_tick(cpu_, pins_);
            
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
            
            // PHI1: CPU internal operations (including APU clock)
            pins_ = nes6502_tick_phi1(cpu_, pins_);
        }
        
        // Generate audio sample
        if (audio_sample_counter_ == 0) {
            float sample = nes6502_generate_audio_sample(cpu_);
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
    // TODO: Implement save state functionality
    return false;
}

template<NintendoVariant V>
bool NintendoSystem<V>::load_state(const std::string& filename) {
    // TODO: Implement load state functionality
    return false;
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