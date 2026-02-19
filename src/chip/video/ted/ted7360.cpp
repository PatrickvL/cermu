/*
 * ted7360.cpp — TED 7360/8360 stub implementation
 *
 * Provides register read/write, three countdown timers with IRQ generation,
 * raster counter tracking, ROM/RAM banking state, keyboard scanning,
 * and a solid-color framebuffer fill (border color).
 *
 * This is a foundation stub — display rendering, sound output, and precise
 * timing will be added incrementally.
 */

#include "ted7360.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

// ============================================================================
// TED 7360 COLOR PALETTE — 128 colors (16 hues × 8 luminances)
// ============================================================================
// The TED generates 121 unique colors from 16 hues at 8 luminance levels,
// but hue 0 (black) is luminance-independent, giving 1 + 15×8 = 121 colors.
// Register format: high nibble = luminance (0-7 in bits 6-4), low nibble = hue.
//
// Placeholder palette: basic 16 colors repeated at each luminance for now.
// A proper palette requires accurate hue/luminance mixing curves.

static const uint32_t ted_base_colors[16] = {
    0xFF000000,  // 0: Black
    0xFFFFFFFF,  // 1: White
    0xFF8E3C97,  // 2: Red
    0xFF72DB87,  // 3: Cyan
    0xFF4F44D8,  // 4: Purple
    0xFF3DAC29,  // 5: Green
    0xFFC94B48,  // 6: Blue
    0xFF5DD9E8,  // 7: Yellow
    0xFF8A4A00,  // 8: Orange
    0xFFAC7E3C,  // 9: Brown
    0xFFDB8B8A,  // 10: Yellow-Green
    0xFF94B6E0,  // 11: Pink
    0xFF868686,  // 12: Blue-Green
    0xFFC9E29E,  // 13: Light Blue
    0xFF5CC9B5,  // 14: Dark Blue
    0xFFBDBDBD,  // 15: Light Green
};

// Convert TED color register value (luminance | hue) to RGBA
static uint32_t ted_color_to_rgba(uint8_t color_reg) {
    uint8_t hue = color_reg & 0x0F;
    // uint8_t lum = (color_reg >> 4) & 0x07;
    // TODO: Apply luminance scaling to the base hue color
    // For now, just return the base color ignoring luminance
    return ted_base_colors[hue];
}

// ============================================================================
// CREATE / DESTROY
// ============================================================================

ted7360_t* ted7360_create(const ted7360_desc_t* desc) {
    ted7360_t* ted = (ted7360_t*)calloc(1, sizeof(ted7360_t));
    if (!ted) return nullptr;

    ted->is_pal = desc ? desc->is_pal : true;
    ted->keyboard_scan = desc ? desc->keyboard_scan : nullptr;
    ted->keyboard_user_data = desc ? desc->keyboard_user_data : nullptr;

    if (ted->is_pal) {
        ted->lines_per_frame = TED_PAL_LINES_PER_FRAME;
        ted->cycles_per_line = TED_PAL_CYCLES_PER_LINE;
    } else {
        ted->lines_per_frame = TED_NTSC_LINES_PER_FRAME;
        ted->cycles_per_line = TED_NTSC_CYCLES_PER_LINE;
    }

    ted7360_reset(ted);
    return ted;
}

void ted7360_destroy(ted7360_t* ted) {
    if (ted) free(ted);
}

// ============================================================================
// RESET
// ============================================================================

void ted7360_reset(ted7360_t* ted) {
    if (!ted) return;

    memset(ted->regs, 0, sizeof(ted->regs));

    // Default register values after reset
    ted->regs[TED_REG_CONTROL1] = 0x00;  // Display disabled
    ted->regs[TED_REG_CONTROL2] = 0x00;
    ted->regs[TED_REG_IRQ_STATUS] = 0x00;
    ted->regs[TED_REG_IRQ_MASK] = 0x00;
    ted->regs[TED_REG_BORDER] = 0x00;    // Black border
    ted->regs[TED_REG_COLOR_BG0] = 0x00; // Black background
    ted->regs[TED_REG_ROM_RAM] = 0x00;

    // Timers: reset to $FFFF (max value)
    ted->timer1 = 0xFFFF;
    ted->timer2 = 0xFFFF;
    ted->timer3 = 0xFFFF;
    ted->timer1_latch = 0xFFFF;
    ted->timer2_latch = 0xFFFF;
    ted->timer3_latch = 0xFFFF;

    // Raster
    ted->raster_line = 0;
    ted->raster_compare = 0;
    ted->h_counter = 0;
    ted->frame_cycle = 0;

    // IRQ
    ted->irq_status = 0;
    ted->irq_mask = 0;

    // Memory banking: ROM enabled after reset
    ted->rom_enabled = true;
    ted->mem_config = 0;

    // Keyboard
    ted->keyboard_latch = 0xFF;

    // Flash
    ted->flash_counter = 0;
}

// ============================================================================
// TICK — one TED cycle (2× CPU clock rate)
// ============================================================================

void ted7360_tick(ted7360_t* ted) {
    if (!ted) return;

    // ---- Horizontal counter ----
    ted->h_counter++;
    if (ted->h_counter >= ted->cycles_per_line) {
        ted->h_counter = 0;

        // ---- New scanline ----
        ted->raster_line++;
        if (ted->raster_line >= ted->lines_per_frame) {
            ted->raster_line = 0;
            ted->flash_counter++;

            // Fill framebuffer with border color at end of frame (stub rendering)
            if (ted->framebuffer && ted->fb_width > 0 && ted->fb_height > 0) {
                uint32_t border_rgba = ted_color_to_rgba(ted->regs[TED_REG_BORDER]);
                int total_pixels = ted->fb_width * ted->fb_height;
                for (int i = 0; i < total_pixels; i++) {
                    ted->framebuffer[i] = border_rgba;
                }
            }
        }

        // Raster compare IRQ
        if (ted->raster_line == ted->raster_compare) {
            ted->irq_status |= TED_IRQ_RASTER;
        }
    }

    // ---- Timer 1 (counts down every TED cycle) ----
    if (ted->timer1 == 0) {
        ted->irq_status |= TED_IRQ_TIMER1;
        ted->timer1 = ted->timer1_latch;  // Reload
    } else {
        ted->timer1--;
    }

    // ---- Timer 2 (counts down every TED cycle) ----
    if (ted->timer2 == 0) {
        ted->irq_status |= TED_IRQ_TIMER2;
        ted->timer2 = ted->timer2_latch;
    } else {
        ted->timer2--;
    }

    // ---- Timer 3 (counts down every TED cycle) ----
    if (ted->timer3 == 0) {
        ted->irq_status |= TED_IRQ_TIMER3;
        ted->timer3 = ted->timer3_latch;
    } else {
        ted->timer3--;
    }

    ted->frame_cycle++;
}

// ============================================================================
// REGISTER READ
// ============================================================================

uint8_t ted7360_read_register(ted7360_t* ted, uint8_t reg) {
    if (!ted) return 0xFF;

    reg &= 0x3F;  // Mirror: $FF00-$FF3F

    switch (reg) {
        // Timer reads return current counter value (not latch)
        case TED_REG_TIMER1_LO:  return ted->timer1 & 0xFF;
        case TED_REG_TIMER1_HI:  return (ted->timer1 >> 8) & 0xFF;
        case TED_REG_TIMER2_LO:  return ted->timer2 & 0xFF;
        case TED_REG_TIMER2_HI:  return (ted->timer2 >> 8) & 0xFF;
        case TED_REG_TIMER3_LO:  return ted->timer3 & 0xFF;
        case TED_REG_TIMER3_HI:  return (ted->timer3 >> 8) & 0xFF;

        case TED_REG_KEYBOARD:
            // Scan keyboard matrix using the column pattern in the latch
            if (ted->keyboard_scan) {
                return ted->keyboard_scan(ted->keyboard_user_data, ted->keyboard_latch);
            }
            return 0xFF;  // No keys pressed

        case TED_REG_IRQ_STATUS:
            // Bit 7 = any IRQ pending (OR of status & mask)
            return ted->irq_status | ((ted->irq_status & ted->irq_mask) ? TED_IRQ_ANY : 0);

        case TED_REG_IRQ_MASK:
            return ted->irq_mask;

        case TED_REG_RASTER_LO:
            return ted->raster_line & 0xFF;

        case TED_REG_CHARPOS_HI:
            // Bit 0 = raster line bit 8
            return (ted->regs[reg] & 0xFE) | ((ted->raster_line >> 8) & 0x01);

        case TED_REG_HPOS:
            return ted->h_counter;

        case TED_REG_VPOS:
            return ted->raster_line & 0xFF;  // Simplified

        case TED_REG_FLASH:
            return (ted->flash_counter & 0x3F) | (ted->regs[reg] & 0xC0);

        // Banking latches: $FF3E/$FF3F read as open bus
        case 0x3E:
        case 0x3F:
            return 0xFF;

        default:
            if (reg < TED_NUM_REGS)
                return ted->regs[reg];
            // Mirrored registers ($20-$3D mirror $00-$1D)
            return ted->regs[reg & 0x1F];
    }
}

// ============================================================================
// REGISTER WRITE
// ============================================================================

void ted7360_write_register(ted7360_t* ted, uint8_t reg, uint8_t data) {
    if (!ted) return;

    reg &= 0x3F;  // Mirror: $FF00-$FF3F

    switch (reg) {
        // Timer writes go to latch, timer reloads on underflow
        case TED_REG_TIMER1_LO:
            ted->timer1_latch = (ted->timer1_latch & 0xFF00) | data;
            ted->regs[reg & 0x1F] = data;
            break;
        case TED_REG_TIMER1_HI:
            ted->timer1_latch = (ted->timer1_latch & 0x00FF) | (data << 8);
            // Writing high byte also loads counter immediately
            ted->timer1 = ted->timer1_latch;
            ted->regs[reg & 0x1F] = data;
            break;

        case TED_REG_TIMER2_LO:
            ted->timer2_latch = (ted->timer2_latch & 0xFF00) | data;
            ted->regs[reg & 0x1F] = data;
            break;
        case TED_REG_TIMER2_HI:
            ted->timer2_latch = (ted->timer2_latch & 0x00FF) | (data << 8);
            ted->timer2 = ted->timer2_latch;
            ted->regs[reg & 0x1F] = data;
            break;

        case TED_REG_TIMER3_LO:
            ted->timer3_latch = (ted->timer3_latch & 0xFF00) | data;
            ted->regs[reg & 0x1F] = data;
            break;
        case TED_REG_TIMER3_HI:
            ted->timer3_latch = (ted->timer3_latch & 0x00FF) | (data << 8);
            ted->timer3 = ted->timer3_latch;
            ted->regs[reg & 0x1F] = data;
            break;

        case TED_REG_KEYBOARD:
            ted->keyboard_latch = data;
            ted->regs[reg] = data;
            break;

        case TED_REG_IRQ_STATUS:
            // Writing 1 to bits clears the corresponding IRQ source
            ted->irq_status &= ~(data & 0x7E);  // Only bits 1-6 are clearable
            break;

        case TED_REG_IRQ_MASK:
            ted->irq_mask = data & 0x7E;  // Only bits 1-6 are maskable
            break;

        case TED_REG_CHARPOS_HI:
            // Bit 0 = raster compare bit 8
            ted->regs[reg] = data;
            ted->raster_compare = (ted->raster_compare & 0xFF) | ((data & 0x01) << 8);
            break;

        case TED_REG_FLASH:
            // Bits 7-6: raster compare bits 0-1 ... actually bits 0-7 of raster compare
            // are in another register.  Simplified for stub:
            ted->regs[reg] = data;
            ted->raster_compare = (ted->raster_compare & 0x100) | data;
            break;

        // ROM/RAM banking latches
        case 0x3E:
            // Write to $FF3E = switch to ROM
            ted->rom_enabled = true;
            break;
        case 0x3F:
            // Write to $FF3F = switch to RAM
            ted->rom_enabled = false;
            break;

        default:
            if (reg < TED_NUM_REGS)
                ted->regs[reg] = data;
            break;
    }
}

// ============================================================================
// IRQ QUERY
// ============================================================================

bool ted7360_irq_pending(const ted7360_t* ted) {
    if (!ted) return false;
    // IRQ is asserted when any enabled source is active
    return (ted->irq_status & ted->irq_mask) != 0;
}

// ============================================================================
// FRAMEBUFFER
// ============================================================================

void ted7360_set_framebuffer(ted7360_t* ted, uint32_t* buffer, int width, int height) {
    if (!ted) return;
    ted->framebuffer = buffer;
    ted->fb_width = width;
    ted->fb_height = height;
}

// ============================================================================
// CHIP DESCRIPTOR
// ============================================================================

chip_descriptor_t ted7360_descriptor = {
    .description = "TED 7360 (C16/Plus4)",
    .create = [](chip_descriptor_t* desc) -> void* {
        ted7360_desc_t d = {};
        d.is_pal = true;
        return ted7360_create(&d);
    },
    .destroy = [](void* chip) {
        ted7360_destroy(static_cast<ted7360_t*>(chip));
    },
    .bus_attach = nullptr,
    .bank_change = nullptr,
#ifdef IMGUI_VERSION
    .render_debug_window = nullptr,   // TODO: TED debug window
    .render_settings_window = nullptr
#endif
};
