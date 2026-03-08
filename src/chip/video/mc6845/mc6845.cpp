/**
 * mc6845.cpp — Motorola MC6845 CRT Controller Implementation
 *
 * Cycle-accurate implementation of the MC6845 CRTC timing generator.
 * Each tick() call advances the internal counters by one character clock,
 * generating horizontal and vertical sync signals, display enable, and
 * memory addresses for the host system's character rendering logic.
 *
 * The CRTC itself does not produce video — it provides addressing and
 * timing.  The host system interprets the addresses to look up screen
 * RAM and character ROM and produce pixel output.
 */

#include "mc6845.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// INITIALIZATION / RESET
// ============================================================================

void mc6845_t::init() {
    reset();
}

void mc6845_t::reset() {
    address_register = 0;
    memset(regs, 0, sizeof(regs));

    // Horizontal state
    h_char_counter = 0;
    h_sync_counter = 0;
    h_sync_active = false;
    h_display_active = false;

    // Vertical state
    v_row_counter = 0;
    v_scanline_counter = 0;
    v_adjust_counter = 0;
    v_sync_counter = 0;
    v_sync_active = false;
    v_display_active = false;
    in_adjust = false;

    // Address
    linear_address = 0;
    row_start_address = 0;

    // Cursor
    cursor_blink_counter = 0;
    cursor_visible = true;

    // Light pen
    light_pen_latched = false;
    light_pen_address = 0;

    frame_count = 0;
}

// ============================================================================
// REGISTER ACCESS
// ============================================================================

uint8_t mc6845_t::read(uint16_t addr) {
    if ((addr & 1) == 0) {
        // Address register — write-only on most variants, return 0
        return 0;
    }

    // Data register — only R14-R17 are readable
    switch (address_register) {
        case MC6845_R14_CURSOR_HI:  return regs[MC6845_R14_CURSOR_HI];
        case MC6845_R15_CURSOR_LO:  return regs[MC6845_R15_CURSOR_LO];
        case MC6845_R16_LPEN_HI:    return static_cast<uint8_t>(light_pen_address >> 8);
        case MC6845_R17_LPEN_LO:    return static_cast<uint8_t>(light_pen_address & 0xFF);
        default: return 0;  // Non-readable registers return 0
    }
}

void mc6845_t::write(uint16_t addr, uint8_t data) {
    if ((addr & 1) == 0) {
        // Address register: select which R0–R17 register to access
        address_register = data & 0x1F;  // 5-bit register select
        return;
    }

    // Data register: write to selected register
    if (address_register >= MC6845_NUM_REGISTERS) return;

    // R16-R17 are read-only (light pen)
    if (address_register >= MC6845_R16_LPEN_HI) return;

    regs[address_register] = data;
}

// ============================================================================
// TICK — ONE CHARACTER CLOCK
// ============================================================================

void mc6845_t::tick() {
    // ====================================================================
    // DISPLAY CHARACTER CALLBACK
    // ====================================================================
    // If we're in the active display area, notify the host system to render
    // the character at the current address and scan line.
    if (h_display_active && v_display_active && on_display_char) {
        // Check cursor position
        bool at_cursor = cursor_visible && (linear_address == cursor_address()) &&
                         (v_scanline_counter >= (regs[MC6845_R10_CURSOR_START] & 0x1F)) &&
                         (v_scanline_counter <= regs[MC6845_R11_CURSOR_END]);

        on_display_char(linear_address, v_scanline_counter, at_cursor);
    }

    // ====================================================================
    // HORIZONTAL COUNTER ADVANCE
    // ====================================================================

    // Advance display address during active display
    if (h_display_active && v_display_active) {
        linear_address++;
    }

    h_char_counter++;

    // --- Horizontal display enable ---
    // Display is active for chars 0 through R1-1
    if (h_char_counter >= regs[MC6845_R1_HDISPLAYED]) {
        h_display_active = false;
    }

    // --- Horizontal sync ---
    if (h_char_counter == regs[MC6845_R2_HSYNC_POS]) {
        h_sync_active = true;
        h_sync_counter = 0;
    }
    if (h_sync_active) {
        h_sync_counter++;
        uint8_t hsync_width = regs[MC6845_R3_SYNC_WIDTHS] & 0x0F;
        if (hsync_width == 0) hsync_width = 16;  // 0 means 16 characters wide
        if (h_sync_counter >= hsync_width) {
            h_sync_active = false;
        }
    }

    // --- End of horizontal line (character counter == R0) ---
    if (h_char_counter > regs[MC6845_R0_HTOTAL]) {
        // Reset horizontal counter for new line
        h_char_counter = 0;
        h_display_active = true;

        // Notify host of new scan line
        if (on_hsync) on_hsync();

        // ================================================================
        // VERTICAL COUNTER ADVANCE (once per horizontal line)
        // ================================================================

        if (in_adjust) {
            // Vertical adjust phase: extra scan lines after last character row
            v_adjust_counter++;
            if (v_adjust_counter >= regs[MC6845_R5_VADJUST]) {
                // End of frame — start new frame
                in_adjust = false;
                v_row_counter = 0;
                v_scanline_counter = 0;
                v_display_active = true;
                linear_address = start_address();
                row_start_address = linear_address;
                frame_count++;

                // Update cursor blink state
                uint8_t blink_mode = (regs[MC6845_R10_CURSOR_START] >> 5) & 0x03;
                switch (blink_mode) {
                    case 0:  // No blink — always visible
                        cursor_visible = true;
                        break;
                    case 1:  // Cursor disabled
                        cursor_visible = false;
                        break;
                    case 2:  // Blink at 1/16 frame rate
                        cursor_blink_counter++;
                        cursor_visible = (cursor_blink_counter & 0x10) == 0;
                        break;
                    case 3:  // Blink at 1/32 frame rate
                        cursor_blink_counter++;
                        cursor_visible = (cursor_blink_counter & 0x20) == 0;
                        break;
                }

                // VSYNC callback
                if (on_vsync) on_vsync();
            }
        } else {
            // Normal scan line advance within character row
            v_scanline_counter++;

            if (v_scanline_counter > regs[MC6845_R9_MAX_SCANLINE]) {
                // End of character row — advance to next row
                v_scanline_counter = 0;
                v_row_counter++;

                // Save start address for this new row
                row_start_address = linear_address;

                // --- Vertical display enable ---
                if (v_row_counter >= regs[MC6845_R6_VDISPLAYED]) {
                    v_display_active = false;
                }

                // --- Vertical sync ---
                if (v_row_counter == regs[MC6845_R7_VSYNC_POS]) {
                    v_sync_active = true;
                    v_sync_counter = 0;
                }

                // --- End of frame (row counter == R4) ---
                if (v_row_counter > regs[MC6845_R4_VTOTAL]) {
                    // Enter vertical adjust phase
                    in_adjust = true;
                    v_adjust_counter = 0;
                    v_sync_active = false;
                }
            } else {
                // Same character row, next scan line — reset address to row start
                linear_address = row_start_address;
            }
        }

        // VSYNC duration tracking (in scan lines)
        if (v_sync_active) {
            v_sync_counter++;
            uint8_t vsync_width = (regs[MC6845_R3_SYNC_WIDTHS] >> 4) & 0x0F;
            if (vsync_width == 0) vsync_width = 16;
            if (v_sync_counter >= vsync_width) {
                v_sync_active = false;
            }
        }
    }
}
