/*
 * nes_ppu.cpp — PPU (Picture Processing Unit) implementation
 *
 * Cycle-accurate PPU rendering including:
 *   - Background tile fetching and shifter logic
 *   - Sprite evaluation and rendering (8×8 and 8×16)
 *   - VRAM address management (coarse/fine scroll, nametable mirroring)
 *   - VBlank / NMI generation
 *
 * Extracted from the monolithic nes_system.cpp — behavior unchanged.
 */

// Include nes_system.h for full Cartridge definition (cart_->notify_a12, etc.)
#include "../nes_system.h"

// nes_ppu.h is transitively included via nes_system.h but be explicit
#include "nes_ppu.h"
#include "nes_palette.h"

// ============================================================================
// Optional PPU sub-component profiling — enabled with -DNES_PROFILING
// ============================================================================
#ifdef NES_PROFILING
#include <x86intrin.h>

struct PpuSubProfile {
    uint64_t bg_fetch_cycles = 0;      // BG tile fetch switch statement
    uint64_t pixel_render_cycles = 0;  // Pixel compositing + screen write
    uint64_t sprite_eval_cycles = 0;   // evaluate_sprites() + load_sprite_shifters()
    uint64_t scroll_cycles = 0;        // increment_scroll + transfer_address
    uint64_t vbl_misc_cycles = 0;      // VBL handling, cycle advance, other
    uint64_t ppu_read_cycles = 0;      // ppu_read() calls (VRAM access)
    uint64_t shifter_cycles = 0;       // update_shifters + load_background_shifters
    uint64_t total_dots = 0;

    void report() const {
        if (total_dots == 0) return;
        uint64_t total = bg_fetch_cycles + pixel_render_cycles + sprite_eval_cycles +
                         scroll_cycles + vbl_misc_cycles + shifter_cycles;
        auto pct = [total](uint64_t c) { return total ? 100.0 * c / total : 0.0; };
        printf("\n  === PPU Sub-Component Breakdown (rdtsc) ===\n");
        printf("  BG fetch:        %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               bg_fetch_cycles, pct(bg_fetch_cycles), (double)bg_fetch_cycles / total_dots);
        printf("  Pixel render:    %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               pixel_render_cycles, pct(pixel_render_cycles), (double)pixel_render_cycles / total_dots);
        printf("  Sprite eval:     %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               sprite_eval_cycles, pct(sprite_eval_cycles), (double)sprite_eval_cycles / total_dots);
        printf("  Shifter update:  %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               shifter_cycles, pct(shifter_cycles), (double)shifter_cycles / total_dots);
        printf("  Scroll:          %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               scroll_cycles, pct(scroll_cycles), (double)scroll_cycles / total_dots);
        printf("  VBL/misc:        %12lu cycles  %5.1f%%  (%.1f cy/dot)\n",
               vbl_misc_cycles, pct(vbl_misc_cycles), (double)vbl_misc_cycles / total_dots);
        printf("  -----------------------------------------\n");
        printf("  Total measured:  %12lu rdtsc cycles\n", total);
    }

    void reset() { *this = {}; }
};

PpuSubProfile g_ppu_subprofile;
#define PPU_PROF_START(var)       uint64_t ppu_##var##_t0 = __rdtsc()
#define PPU_PROF_END(counter, var) g_ppu_subprofile.counter += __rdtsc() - ppu_##var##_t0
#define PPU_PROF_DOT()            g_ppu_subprofile.total_dots++
#else
#define PPU_PROF_START(var)       ((void)0)
#define PPU_PROF_END(counter, var) ((void)0)
#define PPU_PROF_DOT()            ((void)0)
#endif

namespace nes_system {

// ============================================================================
// PPU — A12 transition forwarding (out-of-line to avoid Cartridge include in header)
// ============================================================================

void PPU::forward_a12_transition(bool a12_high) {
    if (cart_) cart_->notify_a12(a12_high, ppu_dot_count_);
}

// ============================================================================
// PPU — CPU BUS INTERFACE
// ============================================================================

std::pair<bus_state_t, ppu_bus_state_t> PPU::service_cpu_bus(
        bus_state_t bus, ppu_bus_state_t ppu_bus) {
    const uint16_t addr = BUS_GET_ADDR(bus) & 0x2007;   // PPU mirrors every 8 bytes
    const bool is_read  = BUS_GET_BIT(bus, BUS_RW_BIT);

    if (is_read) {
        // ---- READ ----
        // CPU is not driving D0-D7.  Load the decayed PPU data latch
        // onto the bus, preserving addr/control.
        bus = apply_open_bus_decay(bus);

        switch (addr) {
            case 0x2000: // Control — write only (decayed data carries through)
            case 0x2001: // Mask — write only
            case 0x2003: // OAM Address — write only
            case 0x2005: // Scroll — write only
            case 0x2006: // PPU Address — write only
                break;
            case 0x2002: // Status
                // PPU drives bits 7-5 from status; bits 4-0 carry through.
                BUS_SET_DATA(bus, (regs[PPUSTATUS] & 0xE0) | (BUS_GET_DATA(bus) & 0x1F));
                refresh_open_bus_timestamps(bus, 0xE0);

                // Flag that $2002 was read this dot, for VBL suppression
                // race-condition detection at the next clock() commit point.
                status_read_last_dot_ = true;

                // Clear VBL on read — both internal (NMI) and external ($2002).
                // Also cancel any pending propagation since the read overtakes it.
                regs[PPUSTATUS] &= ~0x80;
                vbl_flag_internal_ = false;
                pending_vbl_set_ = false;
                internal.w = false;   // Reset write toggle
                update_nmi_output(ppu_bus);  // NMI level changes (VBL cleared)
                break;
            case 0x2004: { // OAM Data
                uint8_t data = oam[regs[OAMADDR]];
                // Attribute byte (offset 2 in each 4-byte entry): bits 2-4
                // are unimplemented in hardware and always read back as 0.
                if ((regs[OAMADDR] & 3) == 2) data &= 0xE3;
                BUS_SET_DATA(bus, data);
                refresh_open_bus_timestamps(bus);
                break;
            }
            case 0x2007: { // PPU Data
                uint8_t data = regs[PPUDATA];
                regs[PPUDATA] = ppu_read_byte(internal.v);

                // Palette reads are immediate (no buffering delay).
                if (internal.v >= 0x3F00) {
                    data = regs[PPUDATA] & (regs[PPUMASK] & 0x01 ? 0x30 : 0x3F);
                    // Palette read: PPU drives bits 5-0; bits 7-6 carry through.
                    BUS_SET_DATA(bus, (BUS_GET_DATA(bus) & 0xC0) | (data & 0x3F));
                    refresh_open_bus_timestamps(bus, 0x3F);
                } else {
                    BUS_SET_DATA(bus, data);
                    refresh_open_bus_timestamps(bus);
                }

                // Increment VRAM address
                internal.v += (regs[PPUCTRL] & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;
                notify_a12(internal.v, ppu_bus);
                break;
            }
        }
    } else {
        // ---- WRITE ----
        // CPU drives all 8 data lines — refresh latch + timestamps.
        refresh_open_bus_timestamps(bus);
        const uint8_t data = BUS_GET_DATA(bus);

        switch (addr) {
            case 0x2000: // Control
                {
                    uint8_t old_ctrl = regs[PPUCTRL];
                    regs[PPUCTRL] = data;
                    internal.t = (internal.t & 0xF3FF) | ((data & 0x03) << 10);
                    update_nmi_output(ppu_bus);  // NMI enable may have changed
                }
                break;
            case 0x2001: // Mask
                regs[PPUMASK] = data;
                active_palette_ = nullptr;  // Invalidate pixel LUT
                break;
            case 0x2002: // Status — read only (write is ignored, bus latch updated above)
                break;
            case 0x2003: // OAM Address
                regs[OAMADDR] = data;
                break;
            case 0x2004: // OAM Data
                oam[regs[OAMADDR]] = data;
                regs[OAMADDR]++;
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
                    // v now drives the PPU address bus — update A12 tracking.
                    // Games (and test ROMs) can clock the MMC3 counter by
                    // toggling A12 via $2006 writes.
                    notify_a12(internal.v, ppu_bus);
                }
                break;
            case 0x2007: // PPU Data
                ppu_write_byte(internal.v, data);
                internal.v += (regs[PPUCTRL] & 0x04) ? 32 : 1;
                internal.v &= 0x7FFF;  // v is 15 bits
                // Post-increment address drives the PPU bus — A12 may change.
                notify_a12(internal.v, ppu_bus);
                break;
        }
    }

    // Return both bus states — the caller stores them as snapshots.
    return {bus, ppu_bus};
}

// ============================================================================
// PPU — Debug peek (no side-effects)
// ============================================================================

uint8_t PPU::cpu_peek(uint16_t addr) const {
    const uint8_t decayed = decayed_latch_data();
    switch (addr & 0x2007) {
        case 0x2002: return (regs[PPUSTATUS] & 0xE0) | (decayed & 0x1F);
        case 0x2004: return oam[regs[OAMADDR]];
        case 0x2007: return regs[PPUDATA];
        default:     return decayed;
    }
}

// ============================================================================
// PPU — VRAM read / write
// ============================================================================

uint8_t PPU::ppu_read_byte(uint16_t addr) const {
    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU, no bus access ----
    // Returns the raw palette byte.  Greyscale masking (PPUMASK bit 0) is
    // handled by the precalculated palette cache for rendering, and applied
    // explicitly in the $2007 CPU-read handler for CPU-visible reads.
    if (unlikely(addr >= 0x3F00)) {
        return palette[pal_mirror_[addr & 0x1F]];
    }

    // ---- CHR + nametable via block dispatch ----
    if (bus_ptr_) {
        uint16_t block = bus_ptr_->ppu_read_block[addr >> nes_bus::PPU_PAGE_SHIFT];
        if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
            return bus_ptr_->ppu_block_read(block, addr);
        }
    }

    return 0;
}

void PPU::ppu_write_byte(uint16_t addr, uint8_t data) {
    // ---- Palette RAM ($3F00-$3FFF) — internal to PPU ----
    if (unlikely(addr >= 0x3F00)) {
        palette[pal_mirror_[addr & 0x1F]] = data;
        active_palette_ = nullptr;  // Invalidate pixel LUT
        return;
    }

    // ---- CHR + nametable via block dispatch ----
    if (bus_ptr_) {
        uint16_t block = bus_ptr_->ppu_write_block[addr >> nes_bus::PPU_PAGE_SHIFT];
        if (likely(block < nes_bus::BLOCK_SENTINEL_MIN)) {
            bus_ptr_->ppu_block_write(block, addr, data);
        }
    }
}

// ============================================================================
// PPU — Main clock (one PPU dot)
// ============================================================================

ppu_bus_state_t PPU::clock(ppu_bus_state_t ppu_bus) {
    ++ppu_dot_count_;  // Monotonic counter for A12 filter timing

    // Cache mask register — accessed many times per dot; one read beats ten.
    const uint8_t mask = regs[PPUMASK];

    // ---- Commit pending VBL flag changes (1-dot propagation delay) ----
    // These were queued on the previous dot; now propagate to regs[PPUSTATUS]
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
            regs[PPUSTATUS] |= 0x80;
            pending_vbl_set_ = false;
        }
    }
    if (unlikely(pending_vbl_clear_)) {
        // Clear VBL (bit 7), Sprite 0 Hit (bit 6), Sprite Overflow (bit 5)
        regs[PPUSTATUS] &= ~0xE0;
        pending_vbl_clear_ = false;
    }

    // Visible scanlines and pre-render scanline
    if (scanline >= -1 && scanline < 240) {

        // Pre-render scanline setup — dot 1.
        // VBL internal flag is cleared immediately (de-asserts NMI at dot 1);
        // regs[PPUSTATUS] bit 7 clear is deferred via pending_vbl_clear_ and
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
            update_shifters();

            // Precompute values shared by BG pattern-byte fetches (cases 6 & 7)
            const uint16_t bg_base  = (uint16_t)(regs[PPUCTRL] & 0x10) << 8;
            const uint16_t fine_y   = (internal.v >> 12) & 0x07;
            const uint16_t tile_row = (uint16_t)internal.nt_byte << 4;

            // Background rendering
            switch ((cycle - 1) & 7) {
                case 0:
                    load_background_shifters();
                    internal.nt_addr = 0x2000 | (internal.v & 0x0FFF);
                    break;
                case 2:
                    internal.nt_byte = fast_vram_read(internal.nt_addr, ppu_bus);
                    break;
                case 4:
                    internal.at_byte = fast_vram_read(
                        0x2000 | (internal.v & 0x0C00) | 0x03C0 |
                        ((internal.v >> 4) & 0x38) | ((internal.v >> 2) & 0x07), ppu_bus);
                    // Shift to the correct quadrant's 2-bit palette selector.
                    // Combine both conditional shifts into a single operation.
                    internal.at_byte >>= ((internal.v & 0x0002) ? 2 : 0)
                                      | ((internal.v & 0x0040) ? 4 : 0);
                    break;
                case 6:
                    internal.bg_lo_byte = fast_vram_read(bg_base + tile_row + fine_y, ppu_bus);
                    break;
                case 7:
                    internal.bg_hi_byte = fast_vram_read(bg_base + tile_row + fine_y + 8, ppu_bus);
                    increment_scroll_x();
                    break;
            }
        }

        // Point-event dispatch — one comparison per dot (whichever case is active).
        // scanline_event_ advances monotonically through each scanline and is
        // reset to 0 at the scanline wrap below.  On visible scanlines, case 3
        // skips case 4 entirely (+= 2) since transfer_address_y is pre-render only.
        switch (scanline_event_) {
            case 0: // cycle 0-255
                scanline_event_ += (cycle == 256 - 1);
                break;
            case 1: // cycle 256
                increment_scroll_y(); 
                scanline_event_++;
                break;
            case 2: // cycle == 257
                load_background_shifters();
                transfer_address_x();
                // Sprite evaluation only occurs when rendering is enabled.
                // When rendering is off ($2001 & $18 == 0), no evaluation
                // happens — overflow flag won't be set, sprite data stale.
                if (scanline >= 0 && (regs[PPUMASK] & 0x18)) evaluate_sprites();
                // Sprite 0, sub-cycle 0: garbage nametable read.
                // Starts the 64-cycle sprite fetch window (257-320).
                fast_vram_read(0x2000 | (internal.v & 0x0FFF), ppu_bus);
                scanline_event_++;
                break;
            case 3: // cycles 258-320 — per-cycle sprite pattern fetches
            {
                // Real hardware: 8 sprites × 8 cycles = 64 cycles (257-320).
                // Each sprite's 8-cycle window has 4 memory reads:
                //   +0 garbage NT  +2 garbage AT  +4 pattern lo  +6 pattern hi
                // Sub-cycle 0 of sprite 0 was done in case 2 (cycle 257).
                if (cycle <= 320) {
                    const uint16_t fc  = cycle - 257;  // 1–63
                    const uint8_t  idx = fc >> 3;      // sprite slot 0–7
                    const uint8_t  sub = fc & 7;       // sub-cycle within slot

                    switch (sub) {
                        case 0: // Garbage nametable byte (A12 = 0)
                            fast_vram_read(0x2000 | (internal.v & 0x0FFF), ppu_bus);
                            break;
                        case 2: // Garbage attribute byte (A12 = 0)
                            fast_vram_read(0x23C0 | (internal.v & 0x0C00) |
                                           ((internal.v >> 4) & 0x38) |
                                           ((internal.v >> 2) & 0x07), ppu_bus);
                            break;
                        case 4: { // Sprite pattern table low byte
                            uint16_t addr = compute_sprite_pattern_addr(idx);
                            uint8_t  data = fast_vram_read(addr, ppu_bus);
                            if (idx < internal.sprite_count) {
                                if (internal.sprite_scanline[idx].attributes & 0x40)
                                    data = flip_byte(data);
                                internal.sprite_shifter_pattern_lo[idx] = data;
                            }
                            break;
                        }
                        case 6: { // Sprite pattern table high byte
                            uint16_t addr = compute_sprite_pattern_addr(idx) + 8;
                            uint8_t  data = fast_vram_read(addr, ppu_bus);
                            if (idx < internal.sprite_count) {
                                if (internal.sprite_scanline[idx].attributes & 0x40)
                                    data = flip_byte(data);
                                internal.sprite_shifter_pattern_hi[idx] = data;
                            }
                            break;
                        }
                    }

                    // Pre-render: transfer_address_y overlaps with sprite
                    // fetch window (cycles 280-304 ⊂ 258-320).
                    if (scanline == -1 && cycle >= 280 && cycle <= 304) {
                        transfer_address_y();
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
            case 5: // cycle 338
                internal.nt_byte = fast_vram_read(internal.nt_addr, ppu_bus);
                scanline_event_++;
                break;
            case 6: // cycles 339-340
                if (cycle == 340) {
                    internal.nt_byte = fast_vram_read(internal.nt_addr, ppu_bus);
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

                for (uint8_t i = 0; i < internal.sprite_count; i++) {
                    if (internal.sprite_scanline[i].x == 0) {
                        // Shift-and-mask: bit 7 >> 7 gives 0 or 1
                        const uint8_t fg_pixel_lo = (internal.sprite_shifter_pattern_lo[i] >> 7) & 1;
                        const uint8_t fg_pixel_hi = (internal.sprite_shifter_pattern_hi[i] >> 7) & 1;
                        fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;

                        fg_palette  = (internal.sprite_scanline[i].attributes & 0x03) + 0x04;
                        fg_priority = (internal.sprite_scanline[i].attributes & 0x20) == 0;

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
            if (!(regs[PPUSTATUS] & 0x40) &&
                internal.sprite_zero_hit_possible &&
                internal.sprite_zero_being_rendered &&
                (mask & 0x18) == 0x18 &&
                x != 255 &&
                ((mask & 0x06) == 0x06 || cycle >= 9)) {
                regs[PPUSTATUS] |= 0x40;
            }
        }

        // Lazy pixel LUT rebuild — coalesces rapid palette/mask writes
        if (unlikely(!active_palette_)) rebuild_pixel_lut();

        screen[(scanline * 256) + x] = pixel_lut_[((palette_val << 2) | pixel) & 0x1F];
    }

    // VBlank flag set — (scanline 241, dot 1)
    //
    // The internal VBL state (vbl_flag_internal_) and NMI output assert
    // immediately at dot 1.  The $2002-readable flag (regs[PPUSTATUS] bit 7)
    // is deferred by 1 PPU clock via pending_vbl_set_, committed at the
    // start of the next clock() call.  Suppression is also checked at
    // commit time — see the pending_vbl_set_ block at the top of clock().
    if (scanline == nes_constants::VBLANK_SCANLINE && cycle == 1) {
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

    if (cycle >= nes_constants::DOTS_PER_SCANLINE) {
        cycle           = 0;
        scanline_event_ = 0;  // Reset event counter for new scanline
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

// ============================================================================
// PPU — Scroll / address helpers
// ============================================================================

void PPU::increment_scroll_x() {
    if (regs[PPUMASK] & 0x18) {
        if ((internal.v & 0x001F) == 31) {
            internal.v &= ~0x001F;
            internal.v ^= 0x0400;
        } else {
            internal.v++;
        }
    }
}

void PPU::increment_scroll_y() {
    if (regs[PPUMASK] & 0x18) {
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
    if (regs[PPUMASK] & 0x18) {
        // XOR-AND-XOR bitmix (3 ops) — merge t's coarse X + nametable X into v
        internal.v = internal.v ^ ((internal.v ^ internal.t) & 0x041F);
    }
}

void PPU::transfer_address_y() {
    if (regs[PPUMASK] & 0x18) {
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

    // Branchless attribute expansion: -(bit & 1) yields 0x0000 or 0xFFFF
    // (two's complement negate: 0→0, 1→0xFFFF, truncated to low byte = 0xFF)
    internal.bg_shifter_attrib_lo = (internal.bg_shifter_attrib_lo & 0xFF00) |
                                   (-(internal.at_byte & 0x01) & 0xFF);
    internal.bg_shifter_attrib_hi = (internal.bg_shifter_attrib_hi & 0xFF00) |
                                   (-((internal.at_byte >> 1) & 0x01) & 0xFF);
}

void PPU::update_shifters() {
    if (regs[PPUMASK] & 0x08) {
        internal.bg_shifter_pattern_lo <<= 1;
        internal.bg_shifter_pattern_hi <<= 1;
        internal.bg_shifter_attrib_lo <<= 1;
        internal.bg_shifter_attrib_hi <<= 1;
    }
    
    if (regs[PPUMASK] & 0x10 && cycle < 258) {
        for (uint8_t i = 0; i < internal.sprite_count; i++) {
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
    internal.sprite_scanline.fill({0xFF, 0xFF, 0xFF, 0xFF});
    internal.sprite_count = 0;
    
    internal.sprite_zero_hit_possible = false;
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < 64 && count < 9; i++) {
        uint8_t sprite_y = oam[i * 4 + 0];
        uint8_t sprite_height = (regs[PPUCTRL] & 0x20) ? 16 : 8;
        
        if ((scanline >= sprite_y) && (scanline < (sprite_y + sprite_height))) {
            if (count < 8) {
                if (i == 0) {
                    internal.sprite_zero_hit_possible = true;
                }
                
                internal.sprite_scanline[count].y = sprite_y;
                internal.sprite_scanline[count].tile_id = oam[i * 4 + 1];
                internal.sprite_scanline[count].attributes = oam[i * 4 + 2];
                internal.sprite_scanline[count].x = oam[i * 4 + 3];
            }
            count++;
        }
    }
    
    internal.sprite_count = (count > 8) ? 8 : count;
    if (count > 8) {
        regs[PPUSTATUS] |= 0x20; // Set sprite overflow
    }
}

// load_sprite_shifters() — removed; sprite pattern fetches are now
// per-cycle during clock() cases 3 (cycles 258-320), matching real
// hardware timing where each of 8 sprites takes 8 PPU cycles.

// ============================================================================
// PPU — Cartridge connection
// ============================================================================

void PPU::connect_cartridge(Cartridge* cartridge) {
    cart_ = cartridge;
}

} // namespace nes_system
