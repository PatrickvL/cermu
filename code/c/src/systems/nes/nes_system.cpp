/*
 * nes_system.cpp - Complete NES System Implementation
 *
 * This file implements the complete Nintendo Entertainment System with
 * hardware-accurate components and precise timing.
 */

#include "nes_system.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

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
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Pattern table
        data = pattern_table[addr >> 12][addr & 0x0FFF];
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables
        addr &= 0x0FFF;
        
        // Handle mirroring
        if (cart) {
            if (cart->get_mirror_vertical()) {
                // Vertical mirroring
                if (addr >= 0x0000 && addr <= 0x03FF) data = vram[addr & 0x03FF];
                if (addr >= 0x0400 && addr <= 0x07FF) data = vram[(addr & 0x03FF) + 0x0400];
                if (addr >= 0x0800 && addr <= 0x0BFF) data = vram[addr & 0x03FF];
                if (addr >= 0x0C00 && addr <= 0x0FFF) data = vram[(addr & 0x03FF) + 0x0400];
            } else {
                // Horizontal mirroring
                if (addr >= 0x0000 && addr <= 0x03FF) data = vram[addr & 0x03FF];
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
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Pattern table (CHR-RAM)
        pattern_table[addr >> 12][addr & 0x0FFF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables
        addr &= 0x0FFF;
        
        // Handle mirroring
        if (cart) {
            if (cart->get_mirror_vertical()) {
                // Vertical mirroring
                if (addr >= 0x0000 && addr <= 0x03FF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0400 && addr <= 0x07FF) vram[(addr & 0x03FF) + 0x0400] = data;
                if (addr >= 0x0800 && addr <= 0x0BFF) vram[addr & 0x03FF] = data;
                if (addr >= 0x0C00 && addr <= 0x0FFF) vram[(addr & 0x03FF) + 0x0400] = data;
            } else {
                // Horizontal mirroring
                if (addr >= 0x0000 && addr <= 0x03FF) vram[addr & 0x03FF] = data;
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
        for (int i = 0; i < internal.sprite_scanline.size(); i++) {
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
                                           (7 - (scanline - internal.sprite_scanline[i].y) & 0x07);
                } else {
                    // Bottom half (flipped, so actually top)
                    sprite_pattern_addr_lo = ((internal.sprite_scanline[i].tile_id & 0x01) << 12) |
                                           ((internal.sprite_scanline[i].tile_id & 0xFE) << 4) |
                                           (7 - (scanline - internal.sprite_scanline[i].y) & 0x07);
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
        if (addr >= 0x8000 && addr <= 0xFFFF) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    
    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0xFFFF) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }
    
    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x0000 && addr <= 0x1FFF) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }
    
    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x0000 && addr <= 0x1FFF && chr_banks == 0) {
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

uint8_t MemoryBus::cpu_read(uint16_t addr, bool read_only) {
    uint8_t data = 0x00;
    
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CPU RAM (with mirroring)
        data = cpu_ram[addr & 0x07FF];
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // PPU registers (with mirroring)
        if (ppu) {
            data = ppu->cpu_read(addr & 0x2007, read_only);
        }
    } else if (addr >= 0x4000 && addr <= 0x4017) {
        // APU and I/O registers
        if (addr == 0x4016) {
            data = controllers[0].read();
        } else if (addr == 0x4017) {
            data = controllers[1].read();
        }
        // APU registers handled by NES6502 CPU
    } else if (addr >= 0x4020 && addr <= 0xFFFF) {
        // Cartridge space
        if (cartridge) {
            cartridge->cpu_read(addr, data);
        }
    }
    
    return data;
}

void MemoryBus::cpu_write(uint16_t addr, uint8_t data) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CPU RAM (with mirroring)
        cpu_ram[addr & 0x07FF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // PPU registers (with mirroring)
        if (ppu) {
            ppu->cpu_write(addr & 0x2007, data);
        }
    } else if (addr >= 0x4000 && addr <= 0x4017) {
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
    } else if (addr >= 0x4020 && addr <= 0xFFFF) {
        // Cartridge space
        if (cartridge) {
            cartridge->cpu_write(addr, data);
        }
    }
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
                dma_data = cpu_read((dma_page << 8) | dma_addr);
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

NESSystem::NESSystem(bool pal) : is_pal(pal) {
    // Create CPU with integrated APU
    cpu = nes6502_create();
    if (!cpu) {
        throw std::runtime_error("Failed to create NES CPU");
    }
    
    // Set APU region
    nes6502_set_apu_region(cpu, is_pal);
    
    // Create PPU
    ppu = std::make_shared<PPU>(is_pal);
    
    // Create memory bus
    bus = std::make_shared<MemoryBus>();
    bus->connect_ppu(ppu);
    
    setup_audio_timing();
    reset();
}

NESSystem::~NESSystem() {
    if (cpu) {
        nes6502_destroy(cpu);
    }
}

void NESSystem::setup_audio_timing() {
    uint32_t cpu_freq = is_pal ? nes_constants::CPU_FREQ_PAL : nes_constants::CPU_FREQ_NTSC;
    audio_samples_per_frame = (audio_sample_rate * (is_pal ? 50 : 60)) / (is_pal ? 50 : 60);
}

bool NESSystem::load_cartridge(const std::string& filename) {
    try {
        cartridge = std::make_shared<Cartridge>(filename);
        bus->connect_cartridge(cartridge);
        ppu->connect_cartridge(cartridge);
        
        // Reset system with new cartridge
        reset();
        system_ready = true;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load cartridge: " << e.what() << std::endl;
        return false;
    }
}

void NESSystem::eject_cartridge() {
    cartridge.reset();
    bus->connect_cartridge(nullptr);
    ppu->connect_cartridge(nullptr);
    system_ready = false;
}

void NESSystem::reset() {
    if (cpu) {
        bus_state_t pins = create_bus_state(0, 0, 1);
        nes6502_reset(cpu, pins);
    }
    
    if (ppu) {
        ppu->reset();
    }
    
    if (bus) {
        bus->reset();
    }
    
    if (cartridge) {
        cartridge->reset();
    }
    
    total_cycles = 0;
    residual_time = 0.0;
    audio_sample_counter = 0;
}

void NESSystem::power_cycle() {
    eject_cartridge();
    reset();
}

void NESSystem::clock() {
    // Clock the memory bus (which clocks PPU 3 times)
    bus->clock();
    
    // Clock CPU every 3 PPU cycles
    if (bus->system_clock_counter % 3 == 0) {
        // Handle DMA stall
        if (bus->dma_transfer) {
            // CPU is stalled during DMA
        } else {
            // Create bus state for CPU
            bus_state_t pins = create_bus_state(0, 0, 1);
            
            // Tick CPU (handles APU internally)
            pins = nes6502_tick(cpu, pins);
            
            // Handle CPU memory requests
            uint16_t addr = BUS_GET_ADDR(pins);
            bool is_write = BUS_GET_RW(pins) == 0;
            
            if (is_write) {
                uint8_t data = BUS_GET_DATA(pins);
                bus->cpu_write(addr, data);
            } else {
                uint8_t data = bus->cpu_read(addr);
                BUS_SET_DATA(pins, data);
            }
            
            // Handle NMI from PPU
            if (ppu->get_nmi()) {
                // Set NMI line low (NMI is active low)
                pins = BUS_SET_NMI(pins, 0);
            }
        }
        
        // Generate audio sample
        if (audio_sample_counter == 0) {
            float sample = nes6502_generate_audio_sample(cpu);
            audio_buffer.push_back(sample);
        }
        audio_sample_counter = (audio_sample_counter + 1) % (is_pal ? 33 : 37);
    }
    
    total_cycles++;
}

void NESSystem::run_frame() {
    if (!system_ready) return;
    
    ppu->frame_complete = false;
    while (!ppu->frame_complete) {
        clock();
    }
}

void NESSystem::set_controller_state(int controller, uint8_t state) {
    if (controller >= 0 && controller < 2) {
        // Set individual buttons based on state
        for (int i = 0; i < 8; i++) {
            bool pressed = (state >> i) & 1;
            Controller::Button button = static_cast<Controller::Button>(1 << i);
            bus->controllers[controller].set_button_state(button, pressed);
        }
    }
}

void NESSystem::press_button(int controller, Controller::Button button) {
    if (controller >= 0 && controller < 2) {
        bus->controllers[controller].set_button_state(button, true);
    }
}

void NESSystem::release_button(int controller, Controller::Button button) {
    if (controller >= 0 && controller < 2) {
        bus->controllers[controller].set_button_state(button, false);
    }
}

const std::vector<uint32_t>& NESSystem::get_screen() const {
    static std::vector<uint32_t> empty_screen;
    return ppu ? ppu->get_screen() : empty_screen;
}

const std::vector<uint32_t>& NESSystem::get_pattern_table(int table, uint8_t palette) const {
    static std::vector<uint32_t> empty_table;
    return ppu ? ppu->get_pattern_table(table, palette) : empty_table;
}

void NESSystem::set_audio_sample_rate(uint32_t rate) {
    audio_sample_rate = rate;
    setup_audio_timing();
}

bus_state_t NESSystem::create_bus_state(uint16_t addr, uint8_t data, bool rw) {
    bus_state_t state = 0;
    BUS_SET_ADDR(state, addr);
    BUS_SET_DATA(state, data);
    BUS_SET_RW(state, rw ? 1 : 0);
    return state;
}

bool NESSystem::save_state(const std::string& filename) const {
    // TODO: Implement save state functionality
    return false;
}

bool NESSystem::load_state(const std::string& filename) {
    // TODO: Implement load state functionality
    return false;
}

} // namespace nes_system

// ============================================================================
// C INTERFACE IMPLEMENTATION
// ============================================================================

extern "C" {

struct nes_system_t {
    nes_system::NESSystem* system;
};

nes_system_t* nes_system_create(bool is_pal) {
    try {
        nes_system_t* handle = new nes_system_t;
        handle->system = new nes_system::NESSystem(is_pal);
        return handle;
    } catch (...) {
        return nullptr;
    }
}

void nes_system_destroy(nes_system_t* system) {
    if (system) {
        delete system->system;
        delete system;
    }
}

bool nes_system_load_cartridge(nes_system_t* system, const char* filename) {
    return system && system->system ? 
           system->system->load_cartridge(std::string(filename)) : false;
}

void nes_system_eject_cartridge(nes_system_t* system) {
    if (system && system->system) {
        system->system->eject_cartridge();
    }
}

void nes_system_reset(nes_system_t* system) {
    if (system && system->system) {
        system->system->reset();
    }
}

void nes_system_power_cycle(nes_system_t* system) {
    if (system && system->system) {
        system->system->power_cycle();
    }
}

void nes_system_clock(nes_system_t* system) {
    if (system && system->system) {
        system->system->clock();
    }
}

void nes_system_run_frame(nes_system_t* system) {
    if (system && system->system) {
        system->system->run_frame();
    }
}

void nes_system_set_controller_state(nes_system_t* system, int controller, uint8_t state) {
    if (system && system->system) {
        system->system->set_controller_state(controller, state);
    }
}

void nes_system_press_button(nes_system_t* system, int controller, uint8_t button) {
    if (system && system->system) {
        system->system->press_button(controller, static_cast<nes_system::Controller::Button>(button));
    }
}

void nes_system_release_button(nes_system_t* system, int controller, uint8_t button) {
    if (system && system->system) {
        system->system->release_button(controller, static_cast<nes_system::Controller::Button>(button));
    }
}

const uint32_t* nes_system_get_screen(nes_system_t* system) {
    return system && system->system ? 
           system->system->get_screen().data() : nullptr;
}

const float* nes_system_get_audio_buffer(nes_system_t* system, uint32_t* sample_count) {
    if (system && system->system) {
        const auto& buffer = system->system->get_audio_buffer();
        if (sample_count) {
            *sample_count = static_cast<uint32_t>(buffer.size());
        }
        return buffer.data();
    }
    if (sample_count) {
        *sample_count = 0;
    }
    return nullptr;
}

void nes_system_clear_audio_buffer(nes_system_t* system) {
    if (system && system->system) {
        system->system->clear_audio_buffer();
    }
}

void nes_system_set_audio_sample_rate(nes_system_t* system, uint32_t rate) {
    if (system && system->system) {
        system->system->set_audio_sample_rate(rate);
    }
}

bool nes_system_is_cartridge_loaded(nes_system_t* system) {
    return system && system->system ? 
           system->system->is_cartridge_loaded() : false;
}

uint64_t nes_system_get_total_cycles(nes_system_t* system) {
    return system && system->system ? 
           system->system->get_total_cycles() : 0;
}

bool nes_system_is_ready(nes_system_t* system) {
    return system && system->system ? 
           system->system->is_system_ready() : false;
}

} // extern "C"