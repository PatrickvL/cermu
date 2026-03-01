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

// NES master palette — 64-color LUT (canonical emulator palette)
// Maps 6-bit PPU palette index ($00-$3F) to 32-bit 0xFFBBGGRR (ABGR).
// This matches the GL_RGBA / GL_UNSIGNED_BYTE convention on little-endian:
// memory byte order [R, G, B, A] → uint32_t 0xAABBGGRR.
static constexpr uint32_t NES_COLOR_TABLE[64] = {
    0xFF666666, 0xFF882A00, 0xFFA71214, 0xFFA4003B, 0xFF7E005C, 0xFF40006E, 0xFF00066C, 0xFF001D56,
    0xFF003533, 0xFF00480B, 0xFF005200, 0xFF084F00, 0xFF4D4000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFFD95F15, 0xFFFF4042, 0xFFFE2775, 0xFFCC1AA0, 0xFF7B1EB7, 0xFF2031B5, 0xFF004E99,
    0xFF006D6B, 0xFF008738, 0xFF00930C, 0xFF328F00, 0xFF8D7C00, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFB064, 0xFFFF9092, 0xFFFF76C6, 0xFFFF6AF3, 0xFFCC6EFF, 0xFF7081FF, 0xFF129CFF,
    0xFF00B0D7, 0xFF00C1A6, 0xFF00C979, 0xFF8ACA5A, 0xFFEAC04B, 0xFF424242, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFDFC0, 0xFFFFD2D3, 0xFFFFC8E8, 0xFFFFC2FB, 0xFFEAC4FE, 0xFFC5CCFE, 0xFFA5D8F7,
    0xFF94E5E4, 0xFF96EFCF, 0xFFABF4BD, 0xFFCCF3B3, 0xFFF2EBB5, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
};

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
                // Flag that $2002 was read this dot, for VBL suppression
                // race-condition detection at the next clock() commit point.
                status_read_last_dot_ = true;


                // Clear VBL on read — both internal (NMI) and external ($2002).
                // Also cancel any pending propagation since the read overtakes it.
                // NMI suppression is handled naturally by the CPU's pin-state
                // shift register: clearing vbl_flag_internal_ de-asserts NMI,
                // which feeds 0s into the shift register, preventing the
                // 3-consecutive-LOW condition.  The nmi_pending flag (set when
                // the shift register WAS full) persists independently.
                regs.status &= ~0x80;
                vbl_flag_internal_ = false;
                pending_vbl_set_ = false;
                internal.w = false;   // Reset write toggle
                update_nmi_output();  // NMI level changes (VBL cleared)
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
                regs.data = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(internal.v)));

                // Palette reads are immediate (no buffering delay)
                if (internal.v >= 0x3F00) {
                    data = regs.data;
                }

                // Increment VRAM address
                internal.v += (regs.ctrl & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;  // v is 15 bits
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
                {
                    uint8_t old_ctrl = regs.ctrl;
                    regs.ctrl = data;
                    internal.t = (internal.t & 0xF3FF) | ((data & 0x03) << 10);
                    update_nmi_output();  // NMI enable may have changed
                }
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
                    internal.t = (internal.t & 0x7FE0) | ((data & 0xF8) >> 3);
                    internal.x = data & 0x07;
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0x0FFF) | ((data & 0x07) << 12);
                    internal.t = (internal.t & 0x7C1F) | ((data & 0xF8) << 2);
                    internal.w = false;
                }
                break;
            case 0x2006: // PPU Address
                if (!internal.w) {
                    internal.t = (internal.t & 0x00FF) | ((data & 0x3F) << 8);
                    internal.w = true;
                } else {
                    internal.t = (internal.t & 0xFF00) | data;
                    internal.v = internal.t;
                    internal.w = false;
                }
                break;
            case 0x2007: // PPU Data
                ppu_write(PPU_BUS_WITH_ADDR_DATA(internal.v, data));
                internal.v += (regs.ctrl & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;  // v is 15 bits
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

ppu_bus_state_t PPU::ppu_read(ppu_bus_state_t bus, bool read_only) {
    uint16_t addr = PPU_BUS_GET_ADDR(bus);

    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU, no bus access ----
    if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        PPU_BUS_SET_DATA(bus, palette[addr] & (regs.mask & 0x01 ? 0x30 : 0x3F));
        return bus;
    }

    // ---- A12 edge detection state update (for future mapper use) ----
    // Track A12 state for mappers that need the actual edge.
    // The MMC3 scanline counter is handled separately via the
    // per-scanline cart->scanline() call in clock().
    if (!read_only) {
        last_ppu_addr_ = addr;
    }

    // ---- CHR + nametable via page pointers ----
    if (bus_ptr_) {
        PPU_BUS_SET_ADDR(bus, addr);  // write back masked address
        return bus_ptr_->ppu_read(bus);
    }

    return bus;
}

ppu_bus_state_t PPU::ppu_write(ppu_bus_state_t bus) {
    uint16_t addr = PPU_BUS_GET_ADDR(bus);
    uint8_t data = PPU_BUS_GET_DATA(bus);

    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU ----
    if (addr >= 0x3F00) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        palette[addr] = data;
        return bus;
    }

    // ---- A12 edge detection (writes also put address on bus) ----
    {
        bool a12_rising = (addr & 0x1000) && !(last_ppu_addr_ & 0x1000);
        if (a12_rising && cart) {
            cart->scanline();
        }
        last_ppu_addr_ = addr;
    }

    // ---- CHR + nametable via page pointers ----
    if (bus_ptr_) {
        PPU_BUS_SET_ADDR(bus, addr);  // write back masked address
        return bus_ptr_->ppu_write(bus);
    }

    return bus;
}

// ============================================================================
// PPU — Color conversion
// ============================================================================

inline uint32_t nes2rgb(uint8_t nes_color) {
    return NES_COLOR_TABLE[nes_color & 0x3F];
}

// ============================================================================
// PPU — Main clock (one PPU dot)
// ============================================================================

void PPU::clock() {
    // ---- Commit pending VBL flag changes (1-dot propagation delay) ----
    // These were queued on the previous dot; now propagate to regs.status
    // so that $2002 reads reflect the updated value.
    if (pending_vbl_set_) {
        // VBL suppression race condition (nesdev wiki):
        //   Reading $2002 1 PPU clock before the flag becomes VISIBLE in
        //   $2002 prevents VBL from being set that frame.
        //
        // The visible VBL time is NOW (the commit point, 1 dot after the
        // internal VBL was set).  "1 dot before visible" corresponds to
        // the previous dot — the same dot where vbl_flag_internal_ was set.
        // status_read_last_dot_ was set by cpu_bus_tick on that same dot.
        if (status_read_last_dot_) {
            // $2002 was read on the internal VBL dot — suppress entirely.
            // Cancel the pending set AND clear internal state and NMI.
            pending_vbl_set_ = false;
            vbl_flag_internal_ = false;
            vbl_was_suppressed_ = true;
            update_nmi_output();
        } else {
            regs.status |= 0x80;
            pending_vbl_set_ = false;
        }
    }
    if (pending_vbl_clear_) {
        regs.status &= ~0x80;
        regs.status &= ~0x40; // Clear Sprite 0 Hit
        regs.status &= ~0x20; // Clear Sprite Overflow
        pending_vbl_clear_ = false;
    }

    // Lambda to get pixel color from palette
    auto get_pixel = [this](uint8_t palette_idx, uint8_t pixel) -> uint32_t {
        return nes2rgb(PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(0x3F00 + (palette_idx << 2) + pixel))) & 0x3F);
    };
    
    // Visible scanlines and pre-render scanline
    if (scanline >= -1 && scanline < 240) {
        
        // Pre-render scanline setup — dot 1.
        // VBL internal flag is cleared immediately (de-asserts NMI at dot 1);
        // regs.status bit 7 clear is deferred via pending_vbl_clear_ and
        // committed at the start of the NEXT clock() call (dot 2 visibility).
        if (scanline == -1 && cycle == 1) {
            vbl_flag_internal_ = false;     // NMI de-asserts immediately
            pending_vbl_clear_ = true;      // $2002 visible next dot
            vbl_was_suppressed_ = false;    // Reset suppression for new frame
            
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
                    internal.nt_byte = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(internal.nt_addr)));
                    break;
                case 4:
                    internal.at_byte = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(
                        0x2000 | (internal.v & 0x0C00) | 0x03C0 |
                        ((internal.v >> 4) & 0x38) | ((internal.v >> 2) & 0x07))));
                    // Shift to the correct quadrant's 2-bit palette selector
                    if (internal.v & 0x0002) internal.at_byte >>= 2;
                    if (internal.v & 0x0040) internal.at_byte >>= 4;
                    break;
                case 6:
                    internal.bg_lo_byte = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(
                        ((regs.ctrl & 0x10) << 8) +
                        ((uint16_t)internal.nt_byte << 4) +
                        ((internal.v >> 12) & 0x07) + 0)));
                    break;
                case 7:
                    internal.bg_hi_byte = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(
                        ((regs.ctrl & 0x10) << 8) +
                        ((uint16_t)internal.nt_byte << 4) +
                        ((internal.v >> 12) & 0x07) + 8)));
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
            internal.nt_byte = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(internal.nt_addr)));
        }
        
        if (scanline == -1 && cycle >= 280 && cycle < 305) {
            transfer_address_y();
        }
        
        // Sprite evaluation for next scanline
        if (cycle == 257 && scanline >= 0) {
            evaluate_sprites();
        }

        // Mapper scanline counter (MMC3) — clock once per scanline.
        // On real hardware, the MMC3 monitors PPU A12 rising edges and
        // its internal filter ensures exactly one count during the
        // BG→sprite pattern-fetch transition (~dot 260).  Since our
        // sprite fetches are batched at dot 340 (after BG pre-fetch),
        // the genuine A12 edge is not visible.  We call cart->scanline()
        // directly at dot 260 when rendering is enabled.
        if (cycle == 260 && (regs.mask & 0x18)) {
            if (cart) cart->scanline();
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
        screen[(scanline * 256) + (cycle - 1)] = color;
    }

    // VBlank flag set — (scanline 241, dot 1)
    //
    // The internal VBL state (vbl_flag_internal_) and NMI output assert
    // immediately at dot 1.  The $2002-readable flag (regs.status bit 7)
    // is deferred by 1 PPU clock via pending_vbl_set_, committed at the
    // start of the next clock() call.  Suppression is also checked at
    // commit time — see the pending_vbl_set_ block at the top of clock().
    if (scanline == nes_constants::VBLANK_SCANLINE && cycle == 1) {
        vbl_flag_internal_ = true;     // NMI asserts immediately
        pending_vbl_set_ = true;       // $2002 visible next dot
        vbl_was_suppressed_ = false;
    }

    // Drive /NMI output level (updated every dot for correctness)
    update_nmi_output();

    // (A12 edge detection for mapper IRQ counters is now handled inside
    //  ppu_read() — see the rising-edge check on bit 12 of the address.)

    // Advance cycle
    cycle++;

    // NTSC odd-frame cycle skip — evaluated at the END of dot 339 of
    // the pre-render scanline (cycle has just advanced to 340).
    // If rendering is enabled on an odd frame, the would-be dot 340 is
    // skipped: the pre-render line becomes 340 dots instead of 341.
    // Advancing cycle to 341 triggers the normal scanline-end wrap below.
    //
    // Blargg's 10-even_odd_timing verifies this exact boundary: the
    // rendering-enabled check must see writes to $2001 that land at
    // dot 339 but NOT those at dot 340.
    if (!is_pal && scanline == -1 && cycle == 340 &&
        (frame_count & 1) && (regs.mask & 0x18)) {
        cycle++;  // 340 → 341, caught by the >= check below
    }

    if (cycle >= nes_constants::DOTS_PER_SCANLINE) {
        cycle = 0;
        scanline++;
        if (scanline >= (is_pal ? nes_constants::TOTAL_SCANLINES_PAL - 1
                                    : nes_constants::TOTAL_SCANLINES_NTSC - 1)) {
            scanline = -1;
            frame_complete = true;
            frame_count++;
        }
    }
    status_read_last_dot_ = false;  // Consumed; clear for next dot
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
        // XOR-AND-XOR bitmix (3 ops) — merge t's coarse X + nametable X into v
        internal.v = internal.v ^ ((internal.v ^ internal.t) & 0x041F);
    }
}

void PPU::transfer_address_y() {
    if (regs.mask & 0x18) {
        // XOR-AND-XOR bitmix (3 ops) — merge t's fine Y + coarse Y + nametable Y into v
        internal.v = internal.v ^ ((internal.v ^ internal.t) & 0x7BE0);
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
    // Clear secondary OAM — on real hardware this fills with $FF.
    // y=$FF places sprites offscreen; x=$FF ensures sprite counters
    // never reach 0 during visible dots, preventing unused slots
    // from rendering garbage tile-0 pixels at the left edge.
    internal.sprite_scanline.clear();
    internal.sprite_scanline.resize(8, {0xFF, 0xFF, 0xFF, 0xFF});
    
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
        sprite_pattern_bits_lo = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(sprite_pattern_addr_lo)));
        sprite_pattern_bits_hi = PPU_BUS_GET_DATA(ppu_read(PPU_BUS_WITH_ADDR(sprite_pattern_addr_hi)));
        
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
                uint8_t tile_lsb = PPU_BUS_GET_DATA(non_const_this->ppu_read(PPU_BUS_WITH_ADDR(addr), true));
                uint8_t tile_msb = PPU_BUS_GET_DATA(non_const_this->ppu_read(PPU_BUS_WITH_ADDR(addr + 8), true));
                
                // Render each pixel in the row
                for (uint16_t col = 0; col < 8; col++) {
                    // Get the 2-bit pixel value
                    uint8_t pixel = ((tile_lsb & 0x01) | ((tile_msb & 0x01) << 1));
                    tile_lsb >>= 1;
                    tile_msb >>= 1;
                    
                    // Get the color from the selected palette
                    uint8_t palette_index = PPU_BUS_GET_DATA(non_const_this->ppu_read(
                        PPU_BUS_WITH_ADDR(0x3F00 + (palette << 2) + pixel), true));
                    uint32_t color = nes2rgb(palette_index);
                    
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
