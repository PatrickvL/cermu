/*
 * nes_ppu.cpp — PPU (Picture Processing Unit) implementation
 *
 * Cycle-accurate PPU rendering including:
 *   - Background tile fetching and shifter logic
 *   - Sprite evaluation and rendering (8×8 and 8×16)
 *   - VRAM address management (coarse/fine scroll, nametable mirroring)
 *   - VBlank / NMI generation
 *   - Pattern table debug visualization
 *
 * Extracted from the monolithic nes_system.cpp — behavior unchanged.
 */

// Include nes_system.h for full Cartridge definition (cart->ppu_read, etc.)
#include "../nes_system.h"

// nes_ppu.h is transitively included via nes_system.h but be explicit
#include "nes_ppu.h"
#include "nes_ppu_palette.h"

namespace nes_system {

// ============================================================================
// PPU — CPU BUS INTERFACE
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

// ============================================================================
// PPU — Debug peek (no side-effects)
// ============================================================================

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

// ============================================================================
// PPU — Nametable mirroring
// ============================================================================

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

// ============================================================================
// PPU — VRAM read / write
// ============================================================================

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

// ============================================================================
// PPU — Main clock (one PPU dot)
// ============================================================================

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
        uint8_t palette_val = 0x00;
        
        if (bg_pixel == 0 && fg_pixel == 0) {
            pixel = 0x00;
            palette_val = 0x00;
        } else if (bg_pixel == 0 && fg_pixel > 0) {
            pixel = fg_pixel;
            palette_val = fg_palette;
        } else if (bg_pixel > 0 && fg_pixel == 0) {
            pixel = bg_pixel;
            palette_val = bg_palette;
        } else if (bg_pixel > 0 && fg_pixel > 0) {
            if (fg_priority) {
                pixel = fg_pixel;
                palette_val = fg_palette;
            } else {
                pixel = bg_pixel;
                palette_val = bg_palette;
            }
            
            if (internal.sprite_zero_hit_possible && internal.sprite_zero_being_rendered) {
                if ((regs.mask & 0x08) && (regs.mask & 0x10)) {
                    if (!(regs.mask & 0x06) || cycle >= 9) {
                        regs.status |= 0x40;
                    }
                }
            }
        }
        
        uint32_t color = get_pixel(palette_val, pixel);
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

// ============================================================================
// PPU — Color conversion
// ============================================================================

uint32_t PPU::nes2rgb(uint8_t nes_color) {
    return nes_palette::COLOR_TABLE[nes_color & 0x3F];
}

// ============================================================================
// PPU — Scroll / address helpers
// ============================================================================

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

// ============================================================================
// PPU — Shifter / sprite helpers
// ============================================================================

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

// ============================================================================
// PPU — Cartridge connection + pattern table debug
// ============================================================================

void PPU::connect_cartridge(std::shared_ptr<Cartridge> cartridge) {
    cart = cartridge;
}

const std::vector<uint32_t>& PPU::get_pattern_table(int i, uint8_t palette) const {
    // Pattern tables are used for debugging - they visualize the CHR ROM/RAM tiles
    // Each pattern table is 128x128 pixels (16x16 tiles of 8x8 pixels each)
    
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
                    
                    // Get the color from the selected palette
                    uint8_t palette_index = non_const_this->ppu_read(0x3F00 + (palette << 2) + pixel, true) & 0x3F;
                    uint32_t color = nes_palette::COLOR_TABLE[palette_index];
                    
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

} // namespace nes_system
