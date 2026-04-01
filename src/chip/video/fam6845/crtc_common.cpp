/*
 * crtc_common.cpp — Unified CRTC/VDC chip family implementation
 *
 * Cycle-accurate implementation of the MC6845 CRTC timing generator,
 * extended with MOS 8563/8568 VDC functionality: private DRAM subsystem,
 * block copy DMA, smooth scroll, attribute handling, and RGBI output.
 *
 * The CRTC core (R0-R17) generates timing and addresses identically across
 * all family members.  The VDC extensions are gated on traits_->has_private_dram.
 */

#include "chip/video/fam6845/crtc_common.hpp"

#include <cstdio>
#include <cstring>

using namespace fam6845::reg;

// ============================================================================
// INITIALIZATION / RESET
// ============================================================================

void crtc_base_t::init() {
    reset();
}

void crtc_base_t::reset() {
    address_register = 0;
    regs_.clear();

    // Horizontal state
    h_char_counter   = 0;
    h_sync_counter   = 0;
    h_sync_active    = false;
    h_display_active = false;

    // Vertical state
    v_row_counter      = 0;
    v_scanline_counter = 0;
    v_adjust_counter   = 0;
    v_sync_counter     = 0;
    v_sync_active      = false;
    v_display_active   = false;
    in_adjust          = false;

    // Address
    linear_address    = 0;
    row_start_address = 0;

    // Cursor
    cursor_blink_counter = 0;
    cursor_visible       = true;

    // Light pen
    light_pen_latched = false;
    light_pen_address = 0;

    frame_count = 0;

    // VDC-specific reset
    if (traits_ && traits_->has_private_dram) {
        update_addr_      = 0;
        read_latch_       = 0;
        ready_            = true;
        dram_wait_        = 0;
        block_copy_armed_ = false;
        block_src_addr_   = 0;
        block_count_      = 0;
        dma_active_       = false;
        dma_remaining_    = 0;
        dma_latch_        = 0;
        hscroll_latched_  = 0;
        vscroll_latched_  = 0;
        pixel_x_          = 0;
        pixels_per_line_  = 0;

        // Set ready bit in status
        status_register |= fam6845::STATUS_READY;
    }
}

// ============================================================================
// BUS INTERFACE — on_bus_read / on_bus_write
// ============================================================================

bus_state_t crtc_base_t::on_bus_read(bus_state_t bus) noexcept {
    BUS_SET_DATA(bus, read(BUS_GET_ADDR(bus)));
    return bus;
}

bus_state_t crtc_base_t::on_bus_write(bus_state_t bus) noexcept {
    write(BUS_GET_ADDR(bus), BUS_GET_DATA(bus));
    return bus;
}

// ============================================================================
// REGISTER ACCESS
// ============================================================================

uint8_t crtc_base_t::read(uint16_t addr) {
    if (traits_ && traits_->has_private_dram) {
        // VDC two-step protocol: RS=0 reads status, RS=1 reads data
        if ((addr & 1) == 0) {
            // RS=0: Status register
            return status_register;
        }
        // RS=1: Read from selected register
        return vdc_read_register(address_register);
    }

    // Plain CRTC: addr bit 0 selects address (0) vs data register (1)
    if ((addr & 1) == 0) {
        // Address register port — write-only on most variants, return 0
        return 0;
    }
    return crtc_read_register(address_register);
}

void crtc_base_t::write(uint16_t addr, uint8_t data) {
    if ((addr & 1) == 0) {
        // RS=0 / addr bit 0 = 0: write address register
        address_register = data & traits_->address_mask;
        return;
    }

    // RS=1 / addr bit 1 = 1: write to selected data register
    if (traits_ && traits_->has_private_dram) {
        vdc_write_register(address_register, data);
    } else {
        crtc_write_register(address_register, data);
    }
}

// ── Plain CRTC register access ───────────────────────────────────────

uint8_t crtc_base_t::crtc_read_register(uint8_t reg) {
    if (!traits_) return 0;

    // Check readable mask — most registers are write-only on the 6845
    if (reg < 32 && !((traits_->readable_mask >> reg) & 1))
        return 0;

    switch (reg) {
        case R16_LPEN_HI: return static_cast<uint8_t>(light_pen_address >> 8);
        case R17_LPEN_LO: return static_cast<uint8_t>(light_pen_address & 0xFF);
        default:
            if (reg < traits_->num_registers)
                return regs_[reg];
            return 0;
    }
}

void crtc_base_t::crtc_write_register(uint8_t reg, uint8_t data) {
    if (!traits_) return;
    if (reg >= traits_->num_registers) return;

    // R16-R17 are read-only (light pen)
    if (reg >= R16_LPEN_HI && reg <= R17_LPEN_LO) return;

    regs_[reg] = data;
}

// ── VDC register access ─────────────────────────────────────────────

uint8_t crtc_base_t::vdc_read_register(uint8_t reg) {
    if (reg >= fam6845::reg::NUM_VDC_REGISTERS) return 0xFF;

    switch (reg) {
        case R16_LPEN_HI:
            light_pen_latched = false;  // Reading R16 clears latch
            return static_cast<uint8_t>(light_pen_address >> 8);
        case R17_LPEN_LO:
            return static_cast<uint8_t>(light_pen_address & 0xFF);

        case R31_DATA_PORT: {
            // Read from data port: return read_latch_, auto-increment address
            uint8_t val = read_latch_;
            update_addr_ = (update_addr_ + 1) & vram_addr_mask_;
            regs_[R18_UPDATE_ADDR_HI] = static_cast<uint8_t>(update_addr_ >> 8);
            regs_[R19_UPDATE_ADDR_LO] = static_cast<uint8_t>(update_addr_ & 0xFF);
            // Start prefetch of next byte
            ready_ = false;
            dram_wait_ = 9;  // ~9 character clocks for DRAM cycle
            return val;
        }

        default:
            return regs_[reg];
    }
}

void crtc_base_t::vdc_write_register(uint8_t reg, uint8_t data) {
    if (reg >= fam6845::reg::NUM_VDC_REGISTERS) return;

    // R16-R17 are read-only (light pen)
    if (reg == R16_LPEN_HI || reg == R17_LPEN_LO) return;

    regs_[reg] = data;

    // ── Side effects of specific register writes ─────────────────────

    switch (reg) {
        case R19_UPDATE_ADDR_LO:
            // Writing R19 loads the internal address register and starts prefetch
            update_addr_ = update_address();
            prefetch();
            break;

        case R18_UPDATE_ADDR_HI:
            // High byte write — update internal addr but no prefetch yet
            // (prefetch fires when R19 is written)
            update_addr_ = update_address();
            break;

        case R24_VSCROLL:
            // Bit 7 arms/disarms block copy mode
            block_copy_armed_ = (data & 0x80) != 0;
            break;

        case R30_WORD_COUNT:
            // Store byte count for DMA (actual count = R30 + 1)
            block_count_ = static_cast<uint16_t>(data) + 1;
            break;

        case R31_DATA_PORT:
            if (block_copy_armed_ && !dma_active_) {
                // Block copy/fill: writing R31 fires the DMA
                block_src_addr_ = block_source_address();
                dma_active_     = true;
                dma_remaining_  = block_count_;
                ready_          = false;
                // First read from source address
                if (vram_) {
                    dma_latch_ = vram_[block_src_addr_ & vram_addr_mask_];
                }
            } else {
                // Normal write: store data byte, auto-increment
                if (vram_) {
                    vram_[update_addr_ & vram_addr_mask_] = data;
                }
                update_addr_ = (update_addr_ + 1) & vram_addr_mask_;
                regs_[R18_UPDATE_ADDR_HI] = static_cast<uint8_t>(update_addr_ >> 8);
                regs_[R19_UPDATE_ADDR_LO] = static_cast<uint8_t>(update_addr_ & 0xFF);
                // Insert DRAM wait
                ready_ = false;
                dram_wait_ = 9;
            }
            break;

        case R32_BLOCK_SRC_HI:
        case R33_BLOCK_SRC_LO:
            block_src_addr_ = block_source_address();
            break;

        default:
            break;
    }
}

// ============================================================================
// VDC DRAM OPERATIONS
// ============================================================================

void crtc_base_t::prefetch() {
    // Start asynchronous prefetch of VRAM[update_addr_] into read_latch_.
    // The CPU must poll status bit 7 before reading R31.
    ready_     = false;
    dram_wait_ = 9;  // DRAM cycle takes ~9 character clocks
}

void crtc_base_t::dram_cycle() {
    // Called once per character clock to service DRAM wait states.
    if (!traits_ || !traits_->has_private_dram) return;

    if (dma_active_) {
        dma_step();
        return;
    }

    if (dram_wait_ > 0) {
        dram_wait_--;
        if (dram_wait_ == 0) {
            // DRAM operation complete — prefetch into read latch
            if (vram_) {
                read_latch_ = vram_[update_addr_ & vram_addr_mask_];
            }
            ready_ = true;
            status_register |= fam6845::STATUS_READY;
        }
    }
}

void crtc_base_t::dma_step() {
    // Execute one step of block copy/fill DMA.
    // The VDC copies one byte per ~2 character clocks at full DRAM speed.

    if (dma_remaining_ == 0) {
        dma_active_       = false;
        block_copy_armed_ = false;
        ready_            = true;
        status_register |= fam6845::STATUS_READY;
        // Clear R24 block copy bit
        regs_[R24_VSCROLL] &= 0x7F;
        return;
    }

    // Write current latch byte to destination
    if (vram_) {
        vram_[update_addr_ & vram_addr_mask_] = dma_latch_;
    }

    // Advance destination
    update_addr_ = (update_addr_ + 1) & vram_addr_mask_;

    // Read next source byte (for block copy — for fill this re-reads same addr)
    block_src_addr_ = (block_src_addr_ + 1) & vram_addr_mask_;
    if (vram_) {
        dma_latch_ = vram_[block_src_addr_ & vram_addr_mask_];
    }

    dma_remaining_--;

    if (dma_remaining_ == 0) {
        // DMA complete
        dma_active_       = false;
        block_copy_armed_ = false;
        ready_            = true;
        status_register |= fam6845::STATUS_READY;
        regs_[R24_VSCROLL] &= 0x7F;

        // Update address registers to final positions
        regs_[R18_UPDATE_ADDR_HI] = static_cast<uint8_t>(update_addr_ >> 8);
        regs_[R19_UPDATE_ADDR_LO] = static_cast<uint8_t>(update_addr_ & 0xFF);
        regs_[R32_BLOCK_SRC_HI]   = static_cast<uint8_t>(block_src_addr_ >> 8);
        regs_[R33_BLOCK_SRC_LO]   = static_cast<uint8_t>(block_src_addr_ & 0xFF);
    }
}

// ============================================================================
// VDC PIXEL RENDERING
// ============================================================================

void crtc_base_t::render_char(uint16_t screen_addr, uint8_t scanline) {
    // Render one character cell to the RGBI video output.
    // Called during active display for each character position.

    if (!video_out_ || !vram_) return;

    // Read character code from screen RAM
    uint8_t char_code = vram_[screen_addr & vram_addr_mask_];

    // Read character glyph row from charset in VRAM
    uint16_t charset = charset_base();
    uint8_t char_height = regs_[R9_MAX_SCANLINE] + 1;
    uint16_t glyph_addr = charset + (static_cast<uint16_t>(char_code) * char_height) + scanline;
    uint8_t pixel_row = vram_[glyph_addr & vram_addr_mask_];

    // Determine foreground/background colours
    uint8_t fg, bg;

    if (traits_->has_attribute_ram && (regs_[R25_HSCROLL] & 0x40)) {
        // Attribute mode enabled: read attribute from attribute RAM
        uint16_t attr_base = attribute_address();
        uint16_t attr_addr = attr_base + (screen_addr - start_address());
        uint8_t attr = vram_[attr_addr & vram_addr_mask_];
        fg = (attr >> 4) & 0x0F;
        bg = attr & 0x0F;

        // Underline: check R29
        if (scanline == regs_[R29_UNDERLINE]) {
            pixel_row = 0xFF;  // Full underline
        }

        // Blink: bit 4 of attribute (alternating)
        if ((attr & 0x10) && (frame_count & 0x10)) {
            pixel_row = 0x00;  // Blanked during blink-off phase
        }

        // Reverse: bit 6 of attribute
        if (attr & 0x80) {
            pixel_row = ~pixel_row;
        }
    } else {
        // No attribute RAM: use R26 default colours
        fg = (regs_[R26_FGBG_COLOR] >> 4) & 0x0F;
        bg = regs_[R26_FGBG_COLOR] & 0x0F;
    }

    // Global reverse (R24 bit 6)
    if (regs_[R24_VSCROLL] & 0x40) {
        pixel_row = ~pixel_row;
    }

    // Cursor overlay: invert if at cursor position and cursor visible
    uint16_t cursor_pos = cursor_address();
    if (cursor_visible && screen_addr == cursor_pos) {
        uint8_t cursor_start = regs_[R10_CURSOR_START] & 0x1F;
        uint8_t cursor_end   = regs_[R11_CURSOR_END] & 0x1F;
        if (scanline >= cursor_start && scanline <= cursor_end) {
            pixel_row = ~pixel_row;
        }
    }

    // Emit 8 pixels to RGBI output (or 16 in double-pixel mode)
    bool double_pixel = (regs_[R25_HSCROLL] & 0x10) != 0;
    int pixel_count = double_pixel ? 16 : 8;

    for (int bit = 7; bit >= 0; --bit) {
        uint8_t rgbi = (pixel_row & (1 << bit)) ? fg : bg;
        RGBIVideoSample sample;
        sample.rgbi  = rgbi;
        sample.flags = SyncFlag::None;

        // Add sync flags during blanking
        if (h_sync_active)
            sample.flags = sample.flags | SyncFlag::HSync;
        if (v_sync_active)
            sample.flags = sample.flags | SyncFlag::VSync;
        if (!h_display_active || !v_display_active)
            sample.flags = sample.flags | SyncFlag::Blank;

        video_out_->drive(sample);

        if (double_pixel) {
            // Double each pixel
            video_out_->drive(sample);
        }
    }
}

// ============================================================================
// INDEXED CHARACTER RENDERING (for non-VDC variants)
// ============================================================================

void crtc_base_t::render_indexed_char(uint16_t screen_offset, uint8_t scanline, bool at_cursor) {
    if (!char_render_rom_ || !char_render_vram_ || !char_render_indices_) return;

    screen_offset &= char_render_vram_mask_;
    uint8_t char_code = char_render_vram_[screen_offset];

    // Invert handling (e.g. PET uses bit 7)
    bool inverted = char_render_invert_bit_ &&
                    (char_code & char_render_invert_bit_) != 0;
    uint8_t glyph_index = char_code & static_cast<uint8_t>(~char_render_invert_bit_);
    uint8_t pixel_row = char_render_rom_[(glyph_index * char_render_char_h_) + scanline];

    if (inverted) pixel_row = ~pixel_row;
    if (at_cursor) pixel_row = ~pixel_row;

    // Calculate framebuffer position
    uint32_t char_col = screen_offset % static_cast<uint32_t>(char_render_cols_);
    uint32_t char_row = screen_offset / static_cast<uint32_t>(char_render_cols_);
    uint32_t pixel_x = char_col * 8;
    uint32_t pixel_y = char_row * static_cast<uint32_t>(char_render_char_h_) + scanline;
    int fb_w = char_render_fb_w_;

    if (pixel_y < static_cast<uint32_t>(char_render_fb_h_)) {
        uint8_t* row_ptr = char_render_indices_ + pixel_y * fb_w;
        uint8_t fg_idx = char_render_fg_;
        uint8_t bg_idx = char_render_bg_;
        for (int bit = 7; bit >= 0; --bit) {
            uint32_t px = pixel_x + static_cast<uint32_t>(7 - bit);
            if (px < static_cast<uint32_t>(fb_w)) {
                row_ptr[px] = (pixel_row & (1 << bit)) ? fg_idx : bg_idx;
            }
        }
    }
}

// ============================================================================
// TICK — ONE CHARACTER CLOCK
// ============================================================================

void crtc_base_t::tick() {
    bool is_vdc = traits_ && traits_->has_private_dram;

    // ── VDC: service DRAM arbitration ────────────────────────────────
    if (is_vdc) {
        dram_cycle();

        // Update status register sync bits
        if (v_sync_active)
            status_register |= fam6845::STATUS_VSYNC;
        else
            status_register &= ~fam6845::STATUS_VSYNC;

        if (ready_)
            status_register |= fam6845::STATUS_READY;
        else
            status_register &= ~fam6845::STATUS_READY;
    }

    // ====================================================================
    // DISPLAY CHARACTER — active area processing
    // ====================================================================

    if (h_display_active && v_display_active) {
        if (is_vdc) {
            // VDC: render character directly to RGBI output
            render_char(linear_address, v_scanline_counter);
        } else {
            // Plain CRTC: indexed rendering + callback
            bool at_cursor = cursor_visible &&
                             (linear_address == cursor_address()) &&
                             (v_scanline_counter >= (regs_[R10_CURSOR_START] & 0x1F)) &&
                             (v_scanline_counter <= regs_[R11_CURSOR_END]);

            render_indexed_char(linear_address, v_scanline_counter, at_cursor);

            if (on_display_char) {
                on_display_char(linear_address, v_scanline_counter, at_cursor);
            }
        }
    } else if (is_vdc && video_out_) {
        // VDC: emit blank/sync pixels outside active area
        RGBIVideoSample sample;
        sample.rgbi  = 0;  // Black during blanking
        sample.flags = SyncFlag::Blank;
        if (h_sync_active)
            sample.flags = sample.flags | SyncFlag::HSync;
        if (v_sync_active)
            sample.flags = sample.flags | SyncFlag::VSync;

        // Emit pixels for one character cell width
        bool double_pixel = (regs_[R25_HSCROLL] & 0x10) != 0;
        int pixel_count = double_pixel ? 16 : 8;
        for (int i = 0; i < pixel_count; ++i)
            video_out_->drive(sample);
    }

    // ====================================================================
    // HORIZONTAL COUNTER ADVANCE
    // ====================================================================

    if (h_display_active && v_display_active) {
        linear_address++;
    }

    h_char_counter++;

    // Horizontal display enable
    if (h_char_counter >= regs_[R1_HDISPLAYED]) {
        h_display_active = false;
    }

    // Horizontal sync
    if (h_char_counter == regs_[R2_HSYNC_POS]) {
        h_sync_active  = true;
        h_sync_counter = 0;
    }
    if (h_sync_active) {
        h_sync_counter++;
        uint8_t hsync_width = regs_[R3_SYNC_WIDTHS] & 0x0F;
        if (hsync_width == 0) hsync_width = 16;
        if (h_sync_counter >= hsync_width) {
            h_sync_active = false;
        }
    }

    // ── End of horizontal line ───────────────────────────────────────
    if (h_char_counter > regs_[R0_HTOTAL]) {
        h_char_counter   = 0;
        h_display_active = true;

        // VDC: latch smooth scroll at HBLANK
        if (is_vdc) {
            hscroll_latched_ = (regs_[R25_HSCROLL] >> 4) & 0x0F;
        }

        if (on_hsync) on_hsync();

        // ════════════════════════════════════════════════════════════
        // VERTICAL COUNTER ADVANCE (once per horizontal line)
        // ════════════════════════════════════════════════════════════

        if (in_adjust) {
            v_adjust_counter++;
            if (v_adjust_counter >= regs_[R5_VADJUST]) {
                // End of frame — start new frame
                in_adjust = false;
                v_row_counter      = 0;
                v_scanline_counter = 0;
                v_display_active   = true;
                linear_address     = start_address();
                row_start_address  = linear_address;
                frame_count++;

                // VDC: latch vertical scroll at frame start
                if (is_vdc) {
                    vscroll_latched_ = regs_[R24_VSCROLL] & 0x1F;
                }

                // Cursor blink
                uint8_t blink_mode = (regs_[R10_CURSOR_START] >> 5) & 0x03;
                switch (blink_mode) {
                    case 0: cursor_visible = true; break;
                    case 1: cursor_visible = false; break;
                    case 2:
                        cursor_blink_counter++;
                        cursor_visible = (cursor_blink_counter & 0x10) == 0;
                        break;
                    case 3:
                        cursor_blink_counter++;
                        cursor_visible = (cursor_blink_counter & 0x20) == 0;
                        break;
                }

                if (on_vsync) on_vsync();

                // VDC: signal frame end for video output
                if (is_vdc && video_out_) {
                    RGBIVideoSample fe;
                    fe.rgbi  = 0;
                    fe.flags = SyncFlag::FrameEnd;
                    video_out_->drive(fe);
                }

                // Flush indexed character rendering at end of frame
                if (char_render_display_ && char_render_indices_ && char_render_palette_) {
                    char_render_display_->flush_frame(
                        char_render_indices_, char_render_palette_);
                }
            }
        } else {
            v_scanline_counter++;

            if (v_scanline_counter > regs_[R9_MAX_SCANLINE]) {
                v_scanline_counter = 0;
                v_row_counter++;
                row_start_address = linear_address;

                if (v_row_counter >= regs_[R6_VDISPLAYED]) {
                    v_display_active = false;
                }

                if (v_row_counter == regs_[R7_VSYNC_POS]) {
                    v_sync_active  = true;
                    v_sync_counter = 0;
                }

                if (v_row_counter > regs_[R4_VTOTAL]) {
                    in_adjust        = true;
                    v_adjust_counter = 0;
                    v_sync_active    = false;
                }
            } else {
                // Same character row, next scan line — reset to row start
                linear_address = row_start_address;
            }
        }

        // VSYNC duration tracking
        if (v_sync_active) {
            v_sync_counter++;
            uint8_t vsync_width;
            if (traits_ && traits_->r3_has_vsync_width) {
                vsync_width = (regs_[R3_SYNC_WIDTHS] >> 4) & 0x0F;
            } else {
                vsync_width = 16;  // Original MC6845: fixed 16 lines
            }
            if (vsync_width == 0) vsync_width = 16;
            if (v_sync_counter >= vsync_width) {
                v_sync_active = false;
            }
        }
    }
}

// ============================================================================
// DEBUG FIELDS
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void crtc_base_t::register_debug_fields() {
    using M = const crtc_base_t;

    wire_debug_registers(FAM6845_REG_INFO);
    debug_registry_.set_decl_entries(FAM6845_DECL_ENTRIES.data(), FAM6845_DECL_ENTRIES.size());

    debug_registry_.category("Registers")
        .value("Addr Reg", +[](const ChipBase* c) -> uint32_t {
            return static_cast<M*>(c)->address_register;
        });

    // --- Counters ---
    debug_registry_.category("Counters", false)
        .counter("H Char Counter",
                 +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_char_counter; },
                 +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->chars_per_line() - 1); })
        .counter("V Row Counter",
                 +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_row_counter; },
                 +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->rows_per_frame() - 1); })
        .counter("V Scanline",
                 +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_scanline_counter; },
                 +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->scanlines_per_row() - 1); })
        .value("V Adjust Counter",
               +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_adjust_counter; });

    // --- Sync & Display ---
    debug_registry_.category("Sync & Display", false)
        .flag("HSYNC",     +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_sync_active; })
        .flag("VSYNC",     +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_sync_active; })
        .flag("H Display", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_display_active; })
        .flag("V Display", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_display_active; })
        .flag("In Adjust", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->in_adjust; });

    // --- Address ---
    debug_registry_.category("Address", false)
        .address("Linear Addr", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->linear_address; })
        .address("Row Start",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->row_start_address; })
        .address("Start Addr",  +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->start_address(); })
        .address("Cursor Addr", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->cursor_address(); })
        .flag("Cursor Vis",     +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->cursor_visible; });

    // --- Light Pen ---
    debug_registry_.category("Light Pen", false)
        .flag("Latched",    +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->light_pen_latched; })
        .address("Address", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->light_pen_address; });

    // --- Frame ---
    debug_registry_.category("Frame", false)
        .value("Frame Count", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->frame_count; }, 32);

    // --- VDC-specific debug fields ---
    if (traits_ && traits_->has_private_dram) {
        debug_registry_.category("VDC — DRAM", false)
            .address("Update Addr",    +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->update_addr_; })
            .value("Read Latch",       +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->read_latch_; })
            .flag("Ready",             +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->ready_; })
            .value("DRAM Wait",        +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->dram_wait_; })
            .value("Status",           +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->status_register; });

        debug_registry_.category("VDC — DMA", false)
            .flag("Copy Armed",        +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->block_copy_armed_; })
            .flag("DMA Active",        +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->dma_active_; })
            .address("Block Src",      +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->block_src_addr_; })
            .value("DMA Remaining",    +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->dma_remaining_; });

        debug_registry_.category("VDC — Scroll", false)
            .value("H Scroll Latch",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->hscroll_latched_; })
            .value("V Scroll Latch",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->vscroll_latched_; });
    }
}
#endif
