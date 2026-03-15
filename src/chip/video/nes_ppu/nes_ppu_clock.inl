/*
 * nes_ppu_clock.inl — PPU hot-path inline implementations
 *
 * Included at the end of nes_ppu.hpp, INSIDE namespace nes_system.
 *
 * These functions are called 89,342 times per NTSC frame (once per PPU dot).
 * Keeping them in the header allows the compiler to inline them into the
 * system tick loop, eliminating function-call overhead and enabling
 * cross-function optimization (register allocation, invariant hoisting).
 *
 * Non-hot-path functions (service_cpu_bus, ppu_read_byte, ppu_write_byte,
 * connect_cartridge, register_debug_fields) remain in nes_ppu.cpp.
 */

// ============================================================================
// PPU — Scroll / address helpers
// ============================================================================

inline void PPU::increment_scroll_x(uint8_t mask) {
    if (mask & 0x18) {
        if ((internal.v & 0x001F) == 31) {
            internal.v &= ~0x001F;
            internal.v ^= 0x0400;
        } else {
            internal.v++;
        }
    }
}

inline void PPU::increment_scroll_y(uint8_t mask) {
    if (mask & 0x18) {
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

inline void PPU::transfer_address_x(uint8_t mask) {
    if (mask & 0x18) {
        // XOR-AND-XOR bitmix (3 ops) — merge t's coarse X + nametable X into v
        internal.v = internal.v ^ ((internal.v ^ internal.t) & 0x041F);
    }
}

inline void PPU::transfer_address_y(uint8_t mask) {
    if (mask & 0x18) {
        // XOR-AND-XOR bitmix (3 ops) — merge t's fine Y + coarse Y + nametable Y into v
        internal.v = internal.v ^ ((internal.v ^ internal.t) & 0x7BE0);
    }
}

// ============================================================================
// PPU — Shifter / sprite helpers
// ============================================================================

inline void PPU::load_background_shifters() {
    internal.bg_shifter_pattern_lo = (internal.bg_shifter_pattern_lo & 0xFF00) | internal.bg_lo_byte;
    internal.bg_shifter_pattern_hi = (internal.bg_shifter_pattern_hi & 0xFF00) | internal.bg_hi_byte;

    // Branchless attribute expansion: -(bit & 1) yields 0x0000 or 0xFFFF
    // (two's complement negate: 0→0, 1→0xFFFF, truncated to low byte = 0xFF)
    internal.bg_shifter_attrib_lo = (internal.bg_shifter_attrib_lo & 0xFF00) |
                                   (-(internal.at_byte & 0x01) & 0xFF);
    internal.bg_shifter_attrib_hi = (internal.bg_shifter_attrib_hi & 0xFF00) |
                                   (-((internal.at_byte >> 1) & 0x01) & 0xFF);
}

inline void PPU::update_shifters(uint8_t mask) {
    if (mask & 0x08) {
        internal.bg_shifter_pattern_lo <<= 1;
        internal.bg_shifter_pattern_hi <<= 1;
        internal.bg_shifter_attrib_lo <<= 1;
        internal.bg_shifter_attrib_hi <<= 1;
    }
    
    if (mask & 0x10 && cycle < 258) {
        auto& front = internal.sec_oam_[internal.sec_oam_front_];
        for (uint8_t i = 0; i < internal.sprite_count; i++) {
            if (front.entries[i].x > 0) {
                front.entries[i].x--;
            } else {
                internal.sprite_shifter_pattern_lo[i] <<= 1;
                internal.sprite_shifter_pattern_hi[i] <<= 1;
            }
        }
    }
}

// ============================================================================
// Commit secondary OAM — called at cycle 257.
//
// During cycles 65-256, sprite_eval_step() builds the secondary OAM one
// byte at a time in the back buffer.  At cycle 257 we swap the double-buffer
// index so the sprite-fetch window (258-320) and the next scanline's pixel
// compositor read from the freshly-built buffer.  No memcpy — just toggle.
// ============================================================================
inline void PPU::commit_sprite_eval() {
    auto& ev = internal.sprite_eval;
    internal.sec_oam_front_            = 1 - internal.sec_oam_front_;
    internal.sprite_count              = (ev.state & SE_WR) >> 2;
    internal.sprite_zero_hit_possible  = sprite_masks_[scanline] & 1u;

    // Precompute sprite pattern addresses for all 8 slots.
    // Avoids recomputing the branchy address calc twice per slot
    // during the sprite fetch window (sub-cycles 4 and 6).
    for (uint8_t i = 0; i < 8; i++)
        internal.sprite_pattern_addr[i] = compute_sprite_pattern_addr(i);
}

// ============================================================================
// Sprite evaluation state machine — one step per 2 PPU cycles.
//
// On real hardware, sprite evaluation runs during cycles 65-256 (192 PPU
// cycles = 96 read/write pairs).  We advance the state machine on even
// cycles so the overflow flag is set at the correct dot.
//
// Templated on sprite Height (8 or 16) so comparisons use constexpr and
// the compiler can fully unroll inner loops.
//
// Finding:  iterate primary OAM via bitmask; copy matching sprites to sec OAM.
// Overflow: check remaining sprites for range with hardware byte-offset bug.
// Done:     SE_DONE set → single-bit early-out; also triggered naturally by n==64.
//
// Packed state layout (uint16_t):
//   [15]   = done (n==64 sentinel)   [14:9] = sprite index   [8:7] = byte offset
//   [5]    = overflow (0=finding)    [4:0]  = sec OAM write pointer
//
// sec OAM write pointer carry (31→32) naturally sets SE_OVF  (finding→overflow).
// byte offset carry           (3→0)   naturally increments sprite index.
// sprite index carry          (63→64) naturally sets SE_DONE.
// state=0 is valid cold start.
// ============================================================================
inline void PPU::sprite_eval_step() {
    auto& ev = internal.sprite_eval;

    if (ev.state & SE_DONE) return;

    const uint8_t oam_idx = ev.state >> SE_OAM_SHF;
    uint16_t addend = SE_INC;  // most common: mid-copy

    if (!(ev.state & SE_OVF)) {
        // ---- Finding: copy visible sprites to sec OAM (bitmask-accelerated) ----
        if ((ev.state & SE_BYTE) || (sprite_masks_[scanline] & (1ULL << (ev.state >> SE_SPRITE_SHF)))) {
            internal.sec_oam_back().bytes[ev.state & SE_WR] = oam.bytes[oam_idx];
            // addend already SE_INC
        } else {
            addend = SE_SPRITE_INC;  // not visible — sprite++, byte offset stays 0
        }
        // carry from write pointer [4:0] → SE_OVF  when sec OAM fills  (natural)
        // carry from byte offset   [8:7] → sprite++ when copy ends      (natural)
        // carry from sprite index [14:9] → SE_DONE when n==64           (natural)
    } else {
        // ---- Overflow check: OAM[n*4+m] with hardware byte-offset bug ----
        const uint8_t y = oam.bytes[oam_idx];
        if (static_cast<unsigned>(scanline - y) < ev.sprite_height) {
            regs_[PPUSTATUS] |= 0x20u;
            addend = SE_DONE;
        } else {
            // sprite index and byte offset increment independently — the hardware bug.
            // when byte offset==3, +0x80 carries naturally into sprite index;
            // otherwise explicit sprite_inc is needed.
            addend = 0x0080u | (uint16_t)(((oam_idx & 3u) != 3u) << 9);
        }
    }

    ev.state += addend;
}

// ============================================================================
// PPU — Main clock (one PPU dot)
// ============================================================================

inline ppu_bus_state_t PPU::clock(ppu_bus_state_t ppu_bus) {
    // PPU timing constants (duplicated from nes_constants to avoid
    // system header dependency in the chip header).
    constexpr int32_t  VBLANK_SCANLINE  = 241;
    constexpr uint32_t DOTS_PER_SCANLINE = 341;

    ++ppu_dot_count_;  // Monotonic counter for A12 filter timing

    // Capture data from PPU bus — placed by cartridge's ppu_memory_tick()
    // between this dot and the previous one.  Used on odd sub-cycles
    // (1, 3, 5, 7) to latch tile/sprite pattern data.
    vram_data_latch_ = PPU_BUS_GET_DATA(ppu_bus);

    // Cache mask register — accessed many times per dot; one read beats ten.
    const uint8_t mask = regs_[PPUMASK];

    // ---- Commit pending VBL flag changes (1-dot propagation delay) ----
    // These were queued on the previous dot; now propagate to regs_[PPUSTATUS]
    // so that $2002 reads reflect the updated value.
    if (unlikely(pending_vbl_set_)) {
        // VBL suppression race condition (nesdev wiki):
        //   Reading $2002 1 PPU clock before the flag becomes VISIBLE in
        //   $2002 prevents VBL from being set that frame.
        //
        // The visible VBL time is NOW (the commit point, 1 dot after the
        // internal VBL was set).  "1 dot before visible" corresponds to
        // the previous dot — the same dot where vbl_flag_internal_ was set.
        // status_read_last_dot_ was set by service_cpu_bus on that same dot.
        if (status_read_last_dot_) {
            // $2002 was read on the internal VBL dot — suppress entirely.
            // Cancel the pending set AND clear internal state and NMI.
            pending_vbl_set_ = false;
            vbl_flag_internal_ = false;
            vbl_was_suppressed_ = true;
            update_nmi_output(ppu_bus);
        } else {
            regs_[PPUSTATUS] |= 0x80;
            pending_vbl_set_ = false;
        }
    }
    if (unlikely(pending_vbl_clear_)) {
        // Clear VBL (bit 7), Sprite 0 Hit (bit 6), Sprite Overflow (bit 5)
        regs_[PPUSTATUS] &= ~0xE0;
        pending_vbl_clear_ = false;
    }

    // Visible scanlines and pre-render scanline
    if (scanline >= -1 && scanline < 240) {

        // Pre-render scanline setup — dot 1.
        // VBL internal flag is cleared immediately (de-asserts NMI at dot 1);
        // regs_[PPUSTATUS] bit 7 clear is deferred via pending_vbl_clear_ and
        // committed at the start of the NEXT clock() call (dot 2 visibility).
        if (scanline == -1 && cycle == 1) {
            vbl_flag_internal_ = false;     // NMI de-asserts immediately
            update_nmi_output(ppu_bus);            // Drive /NMI HIGH immediately
            pending_vbl_clear_ = true;      // $2002 visible next dot
            vbl_was_suppressed_ = false;    // Reset suppression for new frame

            // Clear sprite shifters and stale sprite-0 hit flag from previous frame
            memset(internal.sprite_shifter_pattern_lo, 0, sizeof(internal.sprite_shifter_pattern_lo));
            memset(internal.sprite_shifter_pattern_hi, 0, sizeof(internal.sprite_shifter_pattern_hi));
            internal.sprite_zero_hit_possible = false;
        }

        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            update_shifters(mask);

            // Background tile fetch — bus-mediated pipeline.
            // Even sub-cycles (0,2,4,6): output address on PPU bus.
            // Odd sub-cycles (1,3,5,7): capture data from VRAM data latch.
            // The cartridge services the bus between dots (ppu_memory_tick),
            // performing block dispatch + A12 edge detection.
            switch ((cycle - 1) & 7) {
                case 0:
                    load_background_shifters();
                    // Output nametable address for this tile
                    internal.nt_addr = 0x2000 | (internal.v & 0x0FFF);
                    PPU_BUS_SET_ADDR(ppu_bus, internal.nt_addr);
                    break;
                case 1:
                    // Capture nametable byte from bus
                    internal.nt_byte = vram_data_latch_;
                    break;
                case 2:
                    // Output attribute table address
                    PPU_BUS_SET_ADDR(ppu_bus,
                        0x2000 | (internal.v & 0x0C00) | 0x03C0 |
                        ((internal.v >> 4) & 0x38) | ((internal.v >> 2) & 0x07));
                    break;
                case 3:
                    // Capture attribute byte + quadrant shift
                    internal.at_byte = vram_data_latch_;
                    internal.at_byte >>= ((internal.v & 0x0002) ? 2 : 0)
                                      | ((internal.v & 0x0040) ? 4 : 0);
                    break;
                case 4: {
                    // Output BG pattern table low byte address
                    const uint16_t bg_base  = (uint16_t)(regs_[PPUCTRL] & 0x10) << 8;
                    const uint16_t fine_y   = (internal.v >> 12) & 0x07;
                    const uint16_t tile_row = (uint16_t)internal.nt_byte << 4;
                    internal.bg_pattern_lo_addr = bg_base + tile_row + fine_y;
                    PPU_BUS_SET_ADDR(ppu_bus, internal.bg_pattern_lo_addr);
                    break;
                }
                case 5:
                    // Capture BG pattern low byte from bus
                    internal.bg_lo_byte = vram_data_latch_;
                    break;
                case 6:
                    // Output BG pattern table high byte address (low + 8)
                    PPU_BUS_SET_ADDR(ppu_bus, internal.bg_pattern_lo_addr + 8);
                    break;
                case 7:
                    // Capture BG pattern high byte from bus
                    internal.bg_hi_byte = vram_data_latch_;
                    increment_scroll_x(mask);
                    break;
            }
        }

        // Point-event dispatch — one comparison per dot (whichever case is active).
        // scanline_event_ advances monotonically through each scanline and is
        // reset to 0 at the scanline wrap below.  On visible scanlines, case 3
        // skips case 4 entirely (+= 2) since transfer_address_y is pre-render only.
        switch (scanline_event_) {
            case 0: // cycle 0-255
                // Sprite evaluation state machine — initialize at cycle 0,
                // step on even cycles 66-254.  Real hardware evaluates during
                // cycles 65-256 with one read/write pair per 2 PPU cycles.
                if (scanline >= 0 && (mask & 0x18)) {
                    if (cycle == 0) {
                        // Flush any deferred sprite mask updates before latching.
                        flush_sprite_mask_dirty();
                        // Clear back secondary OAM and latch visibility mask.
                        std::memset(internal.sec_oam_back().bytes, 0xFF, 32);
                        internal.sprite_eval = {
                            0,                                          // state: finding, n=0, m=0, sec_wr=0
                            (uint8_t)(regs_[PPUCTRL] & 0x20 ? 16 : 8)  // sprite_height latched from PPUCTRL
                        };
                    } else if (cycle >= 66 && (cycle & 1) == 0) {
                        sprite_eval_step();
                    }
                }
                scanline_event_ += (cycle == 256 - 1);
                break;
            case 1: // cycle 256
                increment_scroll_y(mask);
                // Sprite eval step at cycle 256
                if (scanline >= 0 && (mask & 0x18))
                    sprite_eval_step();
                scanline_event_++;
                break;
            case 2: // cycle == 257
                load_background_shifters();
                transfer_address_x(mask);
                // Commit the secondary OAM built by the per-cycle evaluator
                // during dots 65-256.  When rendering is off ($2001 & $18 == 0),
                // no evaluation ran — secondary OAM stays $FF (offscreen).
                if (scanline >= 0 && (regs_[PPUMASK] & 0x18)) {
                    // Last sprite evaluation step at cycle 257
                    sprite_eval_step();
                    commit_sprite_eval();
                }
                // Sprite 0, sub-cycle 0: output garbage nametable address (A12 = 0).
                // Starts the 64-cycle sprite fetch window (257-320).
                PPU_BUS_SET_ADDR(ppu_bus, 0x2000 | (internal.v & 0x0FFF));
                scanline_event_++;
                break;
            case 3: // cycles 258-320 — per-cycle sprite pattern fetches
            {
                // Real hardware: 8 sprites × 8 cycles = 64 cycles (257-320).
                // Bus-mediated pipeline — same even/odd pattern as BG fetch:
                //   +0 output garbage NT addr   +1 (capture, discard)
                //   +2 output garbage AT addr   +3 (capture, discard)
                //   +4 output pattern lo addr   +5 capture pattern lo
                //   +6 output pattern hi addr   +7 capture pattern hi
                // Sub-cycle 0 of sprite 0 was done in case 2 (cycle 257).
                if (cycle <= 320) {
                    const uint16_t fc  = cycle - 257;  // 1–63
                    const uint8_t  idx = fc >> 3;      // sprite slot 0–7
                    const uint8_t  sub = fc & 7;       // sub-cycle within slot

                    switch (sub) {
                        case 0: // Output garbage nametable address (A12 = 0)
                            PPU_BUS_SET_ADDR(ppu_bus, 0x2000 | (internal.v & 0x0FFF));
                            break;
                        case 2: // Output garbage attribute address (A12 = 0)
                            PPU_BUS_SET_ADDR(ppu_bus,
                                0x23C0 | (internal.v & 0x0C00) |
                                ((internal.v >> 4) & 0x38) |
                                ((internal.v >> 2) & 0x07));
                            break;
                        case 4: // Output sprite pattern table low byte address
                            PPU_BUS_SET_ADDR(ppu_bus, internal.sprite_pattern_addr[idx]);
                            break;
                        case 5: { // Capture sprite pattern low byte
                            uint8_t data = vram_data_latch_;
                            if (idx < internal.sprite_count) {
                                if (internal.sec_oam_front().entries[idx].attributes & 0x40)
                                    data = flip_byte(data);
                                internal.sprite_shifter_pattern_lo[idx] = data;
                            }
                            break;
                        }
                        case 6: // Output sprite pattern table high byte address
                            PPU_BUS_SET_ADDR(ppu_bus, internal.sprite_pattern_addr[idx] + 8);
                            break;
                        case 7: { // Capture sprite pattern high byte
                            uint8_t data = vram_data_latch_;
                            if (idx < internal.sprite_count) {
                                if (internal.sec_oam_front().entries[idx].attributes & 0x40)
                                    data = flip_byte(data);
                                internal.sprite_shifter_pattern_hi[idx] = data;
                            }
                            break;
                        }
                    }

                    // Pre-render: transfer_address_y overlaps with sprite
                    // fetch window (cycles 280-304 ⊂ 258-320).
                    if (scanline == -1 && cycle >= 280 && cycle <= 304) {
                        transfer_address_y(mask);
                    }

                    if (cycle == 320) scanline_event_++;
                    break;
                }
                scanline_event_++;
                [[fallthrough]];
            }
            case 4: // cycles 321-337 — BG prefetch handled by main fetch loop
                scanline_event_ += (cycle == 337);
                break;
            case 5: // cycle 338 — first dummy NT fetch
                // Capture data from the nametable read output at cycle 337
                internal.nt_byte = vram_data_latch_;
                // Output nametable address for the second dummy read
                PPU_BUS_SET_ADDR(ppu_bus, internal.nt_addr);
                scanline_event_++;
                break;
            case 6: // cycles 339-340 — second dummy NT fetch
                if (cycle == 340) {
                    internal.nt_byte = vram_data_latch_;
                    scanline_event_++;  // case 7+ is empty
                }
                break;
        }
    }

    // Render pixel
    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle < 257) {
        // Precompute x once; used for array index and bit mux below.
        const int x = cycle - 1;

        uint8_t bg_pixel   = 0x00;
        uint8_t bg_palette = 0x00;

        // Background rendering
        if (mask & 0x08) {
            if ((mask & 0x02) || cycle >= 9) {  // Hide leftmost 8 pixels unless bit set
                // Extract BG pixel bits via shift-and-mask (avoids > 0 comparison)
                const uint8_t shift = 15 - internal.x;
                const uint8_t p0 = (internal.bg_shifter_pattern_lo >> shift) & 1;
                const uint8_t p1 = (internal.bg_shifter_pattern_hi >> shift) & 1;
                bg_pixel = (p1 << 1) | p0;

                const uint8_t a0 = (internal.bg_shifter_attrib_lo >> shift) & 1;
                const uint8_t a1 = (internal.bg_shifter_attrib_hi >> shift) & 1;
                bg_palette = (a1 << 1) | a0;
            }
        }

        // Sprite rendering
        uint8_t fg_pixel    = 0x00;
        uint8_t fg_palette  = 0x00;
        uint8_t fg_priority = 0x00;

        if ((mask & 0x10) && internal.sprite_count > 0) {
            if ((mask & 0x04) || cycle >= 9) {  // Hide leftmost 8 pixels unless bit set
                internal.sprite_zero_being_rendered = false;
                const auto& front = internal.sec_oam_front();

                for (uint8_t i = 0; i < internal.sprite_count; i++) {
                    if (front.entries[i].x == 0) {
                        // Shift-and-mask: bit 7 >> 7 gives 0 or 1
                        const uint8_t fg_pixel_lo = (internal.sprite_shifter_pattern_lo[i] >> 7) & 1;
                        const uint8_t fg_pixel_hi = (internal.sprite_shifter_pattern_hi[i] >> 7) & 1;
                        fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;

                        fg_palette  = (front.entries[i].attributes & 0x03) + 0x04;
                        fg_priority = (front.entries[i].attributes & 0x20) == 0;

                        if (fg_pixel != 0) {
                            if (i == 0) internal.sprite_zero_being_rendered = true;
                            break;
                        }
                    }
                }
            }
        }

        // Pixel priority selection — combines BG and sprite with priority logic.
        // Uses a streamlined branch structure:
        //   - If both transparent → backdrop
        //   - If only one is opaque → use that one
        //   - If both opaque → priority decides, plus sprite-0 hit check
        uint8_t pixel;
        uint8_t palette_val;

        if (bg_pixel == 0) {
            // No BG pixel — use sprite or backdrop
            pixel       = fg_pixel;
            palette_val = (fg_pixel != 0) ? fg_palette : 0;
        } else if (fg_pixel == 0) {
            // No sprite pixel — use BG
            pixel = bg_pixel;
            palette_val = bg_palette;
        } else {
            // Both opaque — priority decides winner (ternary avoids branch pair)
            const bool fg_wins = fg_priority;
            pixel       = fg_wins ? fg_pixel   : bg_pixel;
            palette_val = fg_wins ? fg_palette : bg_palette;

            // Sprite-0 hit detection (only when both BG and sprite are opaque).
            // Once detected (status bit 6 set), skip for rest of frame.
            // Hardware never sets hit at x=255 (cycle 256).
            if (!(regs_[PPUSTATUS] & 0x40) &&
                internal.sprite_zero_hit_possible &&
                internal.sprite_zero_being_rendered &&
                (mask & 0x18) == 0x18 &&
                x != 255 &&
                ((mask & 0x06) == 0x06 || cycle >= 9)) {
                regs_[PPUSTATUS] |= 0x40;
            }
        }

        // Store palette index in scanline buffer — deferred to flush at cycle 257.
        scanline_color_line_[x] = ((palette_val << 2) | pixel) & 0x1F;
    }

    // Flush remaining pixels of the visible scanline to screen buffer.
    // Pixels [0, scanline_flush_x_) were already flushed by mid-scanline
    // palette/mask changes; flush the tail [scanline_flush_x_, 256).
    if (scanline >= 0 && scanline < 240 && cycle == 257) {
        if (unlikely(!active_palette_)) rebuild_pixel_lut();
        scanline_pixel_.flush_indexed_line_range(
            scanline, pixel_lut_, scanline_flush_x_, 256);
        scanline_flush_x_ = 256;  // Prevent re-flush from HBlank palette writes
    }

    // VBlank flag set — (scanline 241, dot 1)
    //
    // The internal VBL state (vbl_flag_internal_) and NMI output assert
    // immediately at dot 1.  The $2002-readable flag (regs_[PPUSTATUS] bit 7)
    // is deferred by 1 PPU clock via pending_vbl_set_, committed at the
    // start of the next clock() call.  Suppression is also checked at
    // commit time — see the pending_vbl_set_ block at the top of clock().
    if (scanline == VBLANK_SCANLINE && cycle == 1) {
        vbl_flag_internal_ = true;     // NMI asserts immediately
        update_nmi_output(ppu_bus);           // Drive /NMI LOW immediately
        pending_vbl_set_ = true;       // $2002 visible next dot
        vbl_was_suppressed_ = false;
    }

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
        (frame_count & 1) && (mask & 0x18)) {
        cycle++;  // 340 → 341, caught by the >= check below
    }

    if (cycle >= DOTS_PER_SCANLINE) {
        cycle            = 0;
        scanline_event_  = 0;  // Reset event counter for new scanline
        scanline_flush_x_ = 0; // Reset partial-flush cursor for new scanline
        scanline++;
        if (scanline >= total_scanlines_minus_one_) {
            scanline = -1;
            frame_complete = true;
            frame_count++;
        }
    }
    status_read_last_dot_ = false;  // Consumed; clear for next dot

    return ppu_bus;
}
