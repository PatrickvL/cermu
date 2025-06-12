#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// forwards
static uint8_t read_clear(vicii_common_t* vicii, uint8_t reg);
static void update_border_color_and_priority(vicii_common_t* vicii);
static void update_colors_based_on_graphics_mode_and_background_012(vicii_common_t* vicii);
static void update_graphics_mode_and_dependent_colors(vicii_common_t* vicii);
static void update_is_bad_line(vicii_common_t* vicii);
void set_main_border_flip_flop(vicii_common_t* vicii, bool main_border_flip_flop);

// Register read with proper masking
uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    uint8_t reg = address & VICII_REGS_MASK;
    
    switch (reg) {
        case VICII_C1:
            return (vicii->registers[VICII_C1] & 0x7F) |   //    17 $d011 Control register 1
                   ((vicii->raster_counter >> 1) & VICII_C1_RST8); // bit 7 (RST8) reflects RasterCounter bit 8 
        case VICII_RASTER:
            return vicii->raster_counter & 0xFF;           //    18 $d012 Reflects RasterCounter bits 0..7 (masked to u8 by caller, BusRead)
        case VICII_C2:
            return vicii->registers[VICII_C2] | 0xC0;      //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
        case VICII_MP:
            return vicii->registers[VICII_MP] | 0x01;      //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
        // IR                                                    25 $d019 Note : Default read, since BusWrite(), Initialize() already set the unconnected IR_UNUSED bits
        case VICII_IE:
            return vicii->registers[VICII_IE] | 0xF0;      //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
        case VICII_MXM:
            return read_clear(vicii, VICII_MXM_2);         //    30 $d01e Sprite-sprite collision is cleared on read
        case VICII_MXD:
            return read_clear(vicii, VICII_MXD_2);         //    31 $d01f Sprite-data collision is cleared on read
        default:
            if (reg <= 29) {
                return vicii->registers[reg];              //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
            } else {
                // Note : 'Or' doesn't change                 47-63 $d02f-$d03f unused addresses give $ff on reading, as set in Initialize()
                return vicii->registers[reg] | 0xF0;       // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
            }
    }
}

// Helper function to read clear (collision registers)
static uint8_t read_clear(vicii_common_t* vicii, uint8_t reg) {
    uint8_t val = vicii->registers[reg];
    vicii->registers[reg] = 0;
    return val;
}

// Register write with proper handling
void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    uint8_t reg = address & VICII_REGS_MASK; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    // Notes:
    // * Some not-connected bits (marked with '-') are written anyway here,
    //   because determing the mask for those would only be slower, for no benefit
    //   (and these not-connected bits are turned into 1's in MaskBusRead anyway).
    // * Writes on 4 bit color registers ARE masked, to avoid having to do that in (often repeated) reads
    // * Instead of skipping writes to MxM and MxD, their reads are rerouted to MxM_2 and MxD_2
    // * Unused register indices 47..63 are written anyway here
    //   because avoiding those would only be slower, for no benefit

    // Treat the latching Interrupt Register differently from the other registers
    if (reg == VICII_IR) { // $d019 Interrupt Register
        // Only consider the 4 actually supported interrupt bits (IRST/IMBC/IMMC/ILP)
        value &= VICII_INTERRUPTS_MASK;
        // Fetch the current Interrupt Register value
        uint8_t ir = vicii->registers[VICII_IR];
        // Clear all '1' bits in the Interrupt Register
        ir &= ~value;
        // Always set the not-connected bits high
        ir |= VICII_IR_UNUSED;
        // Store the resulting bits
        vicii->registers[VICII_IR] = ir;
        // Note/TODO : Here, it's assumed that when all interrupt bits are cleared, the
        // IR_IRQ flag is untouched - it'll be cleared later, in HandleRasterInterrupt()
        return;
    }
    
    // Mask color registers to 4 bits
    if (reg >= VICII_EC) { // $d020 (4 bits) Exterior color (Border)
        value &= 0x0F; // $d020 and up are colors - keep only lowest 4 bits
    }
    
    vicii->registers[reg] = value;
    // Handle register-specific updates
    switch (reg) {
        case VICII_C1: // $d011 Control register 1
            // Note : Any change in C1_DEN and/or C1_YSCROLL impacts IsBadLine,
            // so update that immediately on a write to C1. Updating IsBadLine
            // on a C1 write is more efficient than doing that in the much more
            // frequently called ClockPulse()
            update_is_bad_line(vicii);
            update_graphics_mode_and_dependent_colors(vicii);
            break;
        case VICII_C2: // $d016 Control register 2
            // Since there's an C1 update handler already, handling C2 here helps
            // avoiding repeated GraphicsMode determinations, so do that here too
            update_graphics_mode_and_dependent_colors(vicii);
            break;
        case VICII_MXYE: // $d017 Sprite Y expansion x
            // TODO : 
            //for (int i = 0; i < NrSprites; i++)
            //    Sprites[i].WrittenToRegMxYE();
            break;
        case VICII_MXDP: // $d01b Sprite data priority
            // TODO : 
            //for (int i = 0; i < NrSprites; i++)
            //    Sprites[i].WrittenToRegMxDP();
            break;            
        case VICII_MP: // AI $d018 Memory pointers
            // Memory pointers changed - update memory mapping
            vicii_update_bank_mapping(vicii, vicii->bank);
            break;
        case VICII_EC: // $d020 (4 bits) Exterior color (Border)
            update_border_color_and_priority(vicii);
            break;
        case VICII_B0C: // $d021 (4 bits) Background color 0
            update_border_color_and_priority(vicii);
            update_colors_based_on_graphics_mode_and_background_012(vicii);
            break;
        case VICII_B1C: // $d022 (4 bits) Background color 1
        case VICII_B2C: // $d023 (4 bits) Background color 2
            update_colors_based_on_graphics_mode_and_background_012(vicii);
            break;
        case VICII_MM0: // $d025 (4 bits) Sprite multicolor 0
            // TODO : 
            //for (int i = 0; i < NrSprites; i++)
            //    Sprites[i].WrittenToRegMM0();
            break;
        case VICII_MM1: // $d026 (4 bits) Sprite multicolor 1
            // TODO :
            //for (int i = 0; i < NrSprites; i++)
            //    Sprites[i].WrittenToRegMM1();
            break;
        case VICII_M0C: // $d027 (4 bits) Color sprite 0
            // TODO : Sprites[0].WrittenToRegMxC();
            break;
        case VICII_M1C: // $d028 (4 bits) Color sprite 1
            // TODO : Sprites[1].WrittenToRegMxC();
            break;
        case VICII_M2C: // $d029 (4 bits) Color sprite 2
            // TODO : Sprites[2].WrittenToRegMxC();
            break;
        case VICII_M3C: // $d02a (4 bits) Color sprite 3
            // TODO : Sprites[3].WrittenToRegMxC();
            break;
        case VICII_M4C: // $d02b (4 bits) Color sprite 4
            // TODO : Sprites[4].WrittenToRegMxC();
            break;
        case VICII_M5C: // $d02c (4 bits) Color sprite 5
            // TODO : Sprites[5].WrittenToRegMxC();
            break;
        case VICII_M6C: // $d02d (4 bits) Color sprite 6
            // TODO : Sprites[6].WrittenToRegMxC();
            break;
        case VICII_M7C: // $d02e (4 bits) Color sprite 7
            // TODO : Sprites[7].WrittenToRegMxC();
            break;
    }
}

// Initialize VIC-II to default state
void vicii_common_initialize(vicii_common_t* vicii) {
    // TODO : Set VIC-II default bank to 0 (lowest 16 Kb)

    // Set all registers to their default value :
    for (int r = 0; r < VICII_REGS_SIZE; r++) {
        switch (r) {
            case VICII_C1:
                vicii->registers[r] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                                     (VICII_C1_YSCROLL & 3); // 155:Display ENable,25-row  
                break;
            case VICII_MXE:
                vicii->registers[r] = 0;  // All sprites disabled
                break;
            case VICII_C2:
                vicii->registers[r] = VICII_C2_CSEL; // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
                break;
            case VICII_IR:
                vicii->registers[r] = VICII_IR_UNUSED; // See BusWrite; Always set the unused bits high
                break;
            case VICII_MP:
                vicii->registers[r] = VICII_MP_CB12 | VICII_MP_VM10; // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
                break;
            case VICII_EC:
                vicii->registers[r] = VICII_COLOR_LIGHT_BLUE; // 14: Border Color
                break;
            case VICII_B0C:
                vicii->registers[r] = VICII_COLOR_BLUE; // 6: Background Color 0
                break;
            case VICII_B1C:
                vicii->registers[r] = VICII_COLOR_WHITE; // 1: Background Color 1
                break;
            case VICII_B2C:
                vicii->registers[r] = VICII_COLOR_RED; // 2: Background Color 2
                break;
            case VICII_B3C:
                vicii->registers[r] = VICII_COLOR_CYAN; // 3: Background Color 3
                break;
            case VICII_MM0:
                vicii->registers[r] = VICII_COLOR_PURPLE; // 4: Sprite Multicolor 0
                break;
            case VICII_MM1:
                vicii->registers[r] = VICII_COLOR_BLACK; // 0: Sprite Multicolor 1
                break;
            case VICII_M0C:
                vicii->registers[r] = VICII_COLOR_WHITE; // 1: Sprite Color 0
                break;
            case VICII_M1C:
                vicii->registers[r] = VICII_COLOR_RED; // 2: Sprite Color 1
                break;
            case VICII_M2C:
                vicii->registers[r] = VICII_COLOR_CYAN; // 3: Sprite Color 2
                break;
            case VICII_M3C:
                vicii->registers[r] = VICII_COLOR_PURPLE; // 4: Sprite Color 3
                break;
            case VICII_M4C:
                vicii->registers[r] = VICII_COLOR_GREEN; // 5: Sprite Color 4
                break;
            case VICII_M5C:
                vicii->registers[r] = VICII_COLOR_BLUE; // 6: Sprite Color 5
                break;
            case VICII_M6C:
                vicii->registers[r] = VICII_COLOR_YELLOW; // 7: Sprite Color 6
                break;
            case VICII_M7C:
                vicii->registers[r] = VICII_COLOR_MEDIUM_GREY; // 12: Sprite Color 7
                break;
            default:
                // Set registers 47-63 $d02f-$d03f unused addresses to 0xFF (which we never overwrite)
                // so that reading them needs no separate case in default MaskBusRead() return value.
                if (r >= 47) {
                    vicii->registers[r] = 0xFF;  // Unused addresses
                } else {
                    vicii->registers[r] = 0; // SPxX,SPxY,MSIGX,etc
                }
                break;
        }
    }
    
    update_graphics_mode_and_dependent_colors(vicii);
    // Initialize border generation properly
    update_border_color_and_priority(vicii);  // Start generating border pixels

    // After above defaults, initialize the sprites using those values
    // TODO :
    //for (int i = 0; i < NrSprites; i++)
    //    Sprites[i] = new(i, this);

    // Assign color priorities just once
    vicii->colors[0].priority = VICII_PRIORITY_BACKGROUND; // "00" / "0" Use in both MC modes
    vicii->colors[1].priority = VICII_PRIORITY_BACKGROUND; // "01" Used in EmitMCPixel()
    vicii->colors[2].priority = VICII_PRIORITY_FOREGROUND; // "10" 
    vicii->colors[3].priority = VICII_PRIORITY_FOREGROUND; // "11"
    vicii->colors[4].priority = VICII_PRIORITY_FOREGROUND; // "1" Used in EmitPixel()
    
    // AI :
    
    // Initialize VIC-II memory mapping to default bank 0
    vicii_update_bank_mapping(vicii, 0);
    
    // Initialize video logic display state to start generating pixels
    vicii->video_logic_display_state = false;  // Will be enabled during bad lines
    
    // Set visible pixels per line based on timing
    if (vicii->visible_pixels_per_line == 0) {
        // Default to PAL size if not set - this should be set by wrapper create functions
        vicii->visible_pixels_per_line = VICII_PAL_VISIBLE_PIXELS;
    }
    
    // Allocate pixel buffers
    if (vicii->visible_pixels_per_line > 0) {
        vicii->pixel_line_priority = malloc(vicii->visible_pixels_per_line * sizeof(vicii_priority_t));
        vicii->pixel_line_color = malloc(vicii->visible_pixels_per_line * sizeof(uint32_t));
        
        // Initialize pixel buffers with default values (light blue border)
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;  // Default VIC-II border color
        }
    }
}

bool Reg_DisplayEnable(vicii_common_t* vicii) { return (vicii->registers[VICII_C1] & VICII_C1_DEN) > 0; } // C1_DEN: 17.4
// TODO : Implement private int Reg_XScroll() => Reg[C2] & C2_XSCROLL; // C2_XSCROLL:22.0-2
//internal u16 Reg_VideoMatrixBaseAddress() => (u16)((Reg[MP] & (MP_VM13 | MP_VM12 | MP_VM11 | MP_VM10)) << 6); // VM10-VM13: 24.4-7
static inline uint8_t Reg_BackgroundColor(vicii_common_t* vicii, int i) { return vicii->registers[VICII_B0C + i] & 0x0F; } // i:0-3; B0C-B3C: 33-36

// Register-write-related state updates

// Update bad line condition
static void update_is_bad_line(vicii_common_t* vicii) {
    bool eevmf = (vicii->raster_counter >= 48) && (vicii->raster_counter < 248);
    
    // A Bad Line Condition is given at any arbitrary clock cycle, if at the
    // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
    // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
    // DEN bit was set during an arbitrary cycle of raster line $30.
    if (eevmf) {
        // A Bad Line Condition can only occur if the DEN bit has been
        // set for at least one cycle somewhere in raster line $30.
        if (vicii->raster_counter == 0x30) {
            // Check if DEN was set during raster line $30
            if (!vicii->was_den_set_during_raster_30) {
                vicii->was_den_set_during_raster_30 = Reg_DisplayEnable(vicii);
            }
        }
        
        vicii->bad_line = vicii->was_den_set_during_raster_30 &&
                         ((vicii->raster_counter & 0x07) ==
                          (vicii->registers[VICII_C1] & VICII_C1_YSCROLL));
    } else {
        vicii->bad_line = false;
    }
}

// Update graphics mode and dependent colors
static void update_graphics_mode_and_dependent_colors(vicii_common_t* vicii) {
    // Update the graphics mode
    // This results in VICII_GM_STANDARD_TEXT, VICII_GM_MULTICOLOR_TEXT,
    // VICII_GM_MULTICOLOR_BITMAP, VICII_GM_INVALID_TEXT, VICII_GM_INVALID_BITMAP1
    // or VICII_GM_INVALID_BITMAP2  based on the current register settings.
    vicii->graphics_mode = ((vicii->registers[VICII_C1] & (VICII_C1_ECM | VICII_C1_BMM)) |
                            (vicii->registers[VICII_C2] & VICII_C2_MCM)) >> 4;
    
    // Update border limits
    vicii->border_top = (vicii->registers[VICII_C1] & VICII_C1_RSEL) == 0 ?
                        VICII_BORDER_TOP_RSEL0 : VICII_BORDER_TOP_RSEL1;
    vicii->border_bottom = (vicii->registers[VICII_C1] & VICII_C1_RSEL) == 0 ?
                           VICII_BORDER_BOTTOM_RSEL0 : VICII_BORDER_BOTTOM_RSEL1;
    vicii->border_left = (vicii->registers[VICII_C2] & VICII_C2_CSEL) == 0 ?
                         VICII_BORDER_LEFT_CSEL0 : VICII_BORDER_LEFT_CSEL1;
    vicii->border_right = (vicii->registers[VICII_C2] & VICII_C2_CSEL) == 0 ?
                          VICII_BORDER_RIGHT_CSEL0 : VICII_BORDER_RIGHT_CSEL1;

     update_colors_based_on_graphics_mode_and_background_012(vicii);
}
    
// Update graphics mode and dependent colors
static void update_colors_based_on_graphics_mode_and_background_012(vicii_common_t* vicii) {
    // Update those colors[] that are dictated purely by graphics_mode
    // and/or the value of Background Color registers 0 and 2.
    // The values for other colors[] indices are updated in g_access().
    switch (vicii->graphics_mode) {
        case VICII_GM_STANDARD_TEXT: // ECM/BMM/MCM=0/0/0
            vicii->colors[0].color = Reg_BackgroundColor(vicii, 0); // VICII_B0C // Reg[B0C]; // $d021
            break;
        case VICII_GM_MULTICOLOR_TEXT: // ECM/BMM/MCM=0/0/1
            vicii->colors[0].color = Reg_BackgroundColor(vicii, 0); // VICII_B0C // Reg[B0C]; // $d021
            // Note : colors[1], colors[2] and [3] are only used when MC_flag > 0
            vicii->colors[1].color = Reg_BackgroundColor(vicii, 1); // VICII_B1C // Reg[B1C]; // $d022
            vicii->colors[2].color = Reg_BackgroundColor(vicii, 2); // VICII_B2C // Reg[B2C]; // $d023
            // Note : colors[4] is only used when MC_flag == 0
            // Note : colors[3] and colors[4] are updated in g_access()
            break;
        // case VICII_GM_STANDARD_BITMAP: // ECM/BMM/MCM=0/1/0
        // updates both color[0] and [4] in g_access()
        case VICII_GM_MULTICOLOR_BITMAP: // ECM/BMM/MCM=0/1/1
            vicii->colors[0].color = Reg_BackgroundColor(vicii, 0); // VICII_B0C // Reg[B0C]; // $d021
            // Note : colors[1], [2] and [3] are updated in g_access()
            break;
        // case VICII_GM_ECM_TEXT: // ECM/BMM/MCM=1/0/0
        // updates both color[0] and [4] in g_access()
        case VICII_GM_INVALID_TEXT: // unused // ECM/BMM/MCM=1/0/1
            vicii->colors[0].color = VICII_COLOR_BLACK;
            // Note : Colors[2] and [3] are only used when MC_flag = 1
            vicii->colors[1].color = VICII_COLOR_BLACK;
            vicii->colors[2].color = VICII_COLOR_BLACK;
            vicii->colors[3].color = VICII_COLOR_BLACK;
            // Note : Colors[4] is only used when MC_flag = 0
            vicii->colors[4].color = VICII_COLOR_BLACK;
            break;
        case VICII_GM_INVALID_BITMAP1: // unused // ECM/BMM/MCM=1/1/0
            vicii->colors[0].color = VICII_COLOR_BLACK;
            vicii->colors[4].color = VICII_COLOR_BLACK;
            break;
        case VICII_GM_INVALID_BITMAP2: // unused // ECM/BMM/MCM=1/1/1
            vicii->colors[0].color = VICII_COLOR_BLACK;
            vicii->colors[1].color = VICII_COLOR_BLACK;
            vicii->colors[2].color = VICII_COLOR_BLACK;
            vicii->colors[3].color = VICII_COLOR_BLACK;
            break;
    }
}

// Update BorderPixel.Color (and .Priority) by setting MainBorderFlipFlop state to itself
static void update_border_color_and_priority(vicii_common_t* vicii) {
    set_main_border_flip_flop(vicii,
        vicii->border_pixel.priority > VICII_PRIORITY_BACKGROUND); // == Priority.Border
}

// Helper function to set main border flip flop
void set_main_border_flip_flop(vicii_common_t* vicii, bool main_border_flip_flop) {
    // Note: MainBorderFlipFlop state is not stored itself, but instead BorderPixel
    // .Priority and .Color are updated, so this is avoided in EmitBorderPixels()
    // Border : Either Priority.Background (0) or Priority.Border (4)
    if (main_border_flip_flop) {
        vicii->border_pixel.priority = VICII_PRIORITY_BORDER;
        vicii->border_pixel.color = vicii->registers[VICII_EC] & 0x0F;
    } else {
        vicii->border_pixel.priority = VICII_PRIORITY_BACKGROUND;
        vicii->border_pixel.color = Reg_BackgroundColor(vicii, 0);
    }
    
    // AI: Update pixel line buffers if they exist
    if (vicii->pixel_line_color && vicii->visible_pixels_per_line > 0) {
        uint8_t border_color = vicii->registers[VICII_EC] & 0x0F;
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = border_color;
        }
    }
}

// Main cycle function with character and graphics access
void vicii_common_cycle(vicii_common_t* vicii) {
    // Count X position
    vicii->x_coordinate += 8;
    vicii->x_cycle++;
    
    // Handle horizontal retrace
    if (vicii->x_cycle >= vicii->cycles_per_line) {
        // Flush current pixel line to framebuffer before advancing to next line
        // Always flush if we have a framebuffer, even for raster lines beyond VIC-II's normal range
        if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
            vicii_common_flush_pixel_line_to_output(vicii,
                vicii_common_get_default_palette(), vicii->raster_counter);
        }
        
        vicii->x_cycle = 0;
        vicii->x_coordinate = 0;
        vicii->pixel_line_index = 0;  // Reset pixel line index
        vicii->raster_counter++;
        
        // Handle vertical retrace - but continue generating lines until framebuffer is full
        if (vicii->raster_counter >= vicii->total_lines) {
            // If we haven't filled the entire framebuffer height, continue generating border lines
            if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
                // Continue with border-only lines until framebuffer is complete
                // Don't reset VIC-II state, just keep generating border pixels
            } else {
                // Normal VIC-II vertical retrace
                vicii->raster_counter = 0;
                vicii->frame_count++;
                vicii->was_den_set_during_raster_30 = false;
                vicii->bad_line = false;
                vicii->vc_base = 0;
                vicii->lp_edge_detected = false;
            }
        } else {
            update_is_bad_line(vicii);
            vicii_common_handle_raster_interrupt(vicii);
        }
    }
    
    // Handle bad line related state
    if (vicii->bad_line) {
        // Set BA low during bad line (cycles 12-54)
        if (vicii->bus && vicii->x_cycle >= 12 && vicii->x_cycle <= 54) {
            ((c64_bus_t*)vicii->bus)->control_lines &= ~BA_LINE;
        }
        vicii->video_logic_display_state = true;
    } else {
        if (vicii->bus) {
            ((c64_bus_t*)vicii->bus)->control_lines |= BA_LINE;
        }
    }
    
    // VIC-II pixel generation - emit pixels for all framebuffer lines
    // Generate pixels to properly fill the framebuffer with correct centering
    if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
        // Calculate how many cycles we need to fill the framebuffer width
        int cycles_needed = (vicii->framebuffer_width + 7) / 8;  // Round up to cover full width
        
        // Generate pixels for the full width, starting from cycle 0 to ensure proper centering
        if (vicii->x_cycle < cycles_needed) {
            bool in_normal_vic_range = vicii->raster_counter < vicii->total_lines;
            bool in_display_area = in_normal_vic_range &&
                                  (vicii->x_cycle >= 15 && vicii->x_cycle <= 54) &&
                                  (vicii->raster_counter >= vicii->border_top &&
                                   vicii->raster_counter <= vicii->border_bottom);
            
            if (in_display_area) {
                // In display area - emit graphics/text pixels (even in idle state for background color)
                vicii_common_g_access(vicii);
            } else {
                // Outside display area, display disabled, or beyond normal VIC range - emit border pixels
                vicii_common_emit_border_pixels(vicii);
            }
        }
    }
    
    // Handle display logic control (separate from pixel generation)
    if (vicii->x_cycle == 14) {
        // Reset video counters at start of display window
        vicii->vc = vicii->vc_base;
        vicii->vmli = 0;
        if (vicii->bad_line) {
            vicii->rc = 0;
        }
    }
    
    // Character access during bad lines (cycles 15-54)
    if (vicii->x_cycle >= 15 && vicii->x_cycle <= 54) {
        if (vicii->video_logic_display_state && vicii->bad_line) {
            vicii_common_c_access(vicii);
        }
    }
    
    // Handle end of character row
    if (vicii->x_cycle == 58) {
        if (vicii->rc == 7) {
            if (!vicii->bad_line) {
                vicii->video_logic_display_state = false;
                vicii->vc_base = vicii->vc;
                vicii->rc = 0;
            }
        }
        
        if (vicii->video_logic_display_state) {
            vicii->rc = (vicii->rc + 1) & 0x07;
        }
    }
}

// Handle raster interrupt
void vicii_common_handle_raster_interrupt(vicii_common_t* vicii) {
    // Raster interrupt line reached?
    uint16_t raster_compare = ((vicii->registers[VICII_C1] & VICII_C1_RST8) << 1) |
                              vicii->registers[VICII_RASTER];
    
    if (vicii->raster_counter == raster_compare) {
        vicii->registers[VICII_IR] |= VICII_IR_IRST;
    }
    
    // Check if any interrupt is both signaled and enabled
    uint8_t interrupt_status = vicii->registers[VICII_IR] & vicii->registers[VICII_IE];
    if ((interrupt_status & VICII_INTERRUPTS_MASK) > 0) {
        vicii->registers[VICII_IR] |= VICII_IR_IRQ;
        // TODO: Signal IRQ to CPU via bus
    } else {
        if ((vicii->registers[VICII_IR] & VICII_IR_IRQ) > 0) {
            vicii->registers[VICII_IR] &= ~VICII_IR_IRQ;
        }
    }
}

// VIC-II system creation
vicii_common_t* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t)) {
    vicii_common_t* vicii = (vicii_common_t*)calloc(1, sizeof(vicii_common_t));
    if (!vicii) return NULL;
    
    vicii->desc = desc;
    vicii->bank_change = bank_change;
    
    // Initialize registers to default values
    vicii_common_initialize(vicii);
    
    return vicii;
}

void vicii_common_system_destroy(void* chip) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    if (vicii) {
        if (vicii->pixel_line_priority) free(vicii->pixel_line_priority);
        if (vicii->pixel_line_color) free(vicii->pixel_line_color);
        free(vicii);
    }
}

void vicii_common_bus_attach(void* chip, void* bus) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii->bus = bus;
}

void vicii_common_bank_change(void* chip, uint8_t bank) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii_update_bank_mapping(vicii, bank);
}

// VIC-II specific memory read function with proper banking
uint8_t vicii_memory_read_cycle(vicii_common_t* vicii, uint16_t address) {
    if (!vicii->bus) return 0xFF;
    
    // VIC-II only sees 14-bit addresses (16KB banks)
    // The top 2 bits come from CIA2 port A (inverted)
    uint16_t vic_address = (address & 0x3FFF) | vicii->memory_map.bank_base;
    
    // Special handling for character ROM access
    // Character ROM is visible in VIC bank when:
    // 1. Address is in range $1000-$1FFF or $9000-$9FFF
    // 2. Character ROM is enabled (determined by memory setup register)
    if (vicii->memory_map.char_rom_enabled) {
        uint16_t char_check = address & 0x3000;
        if (char_check == 0x1000 || char_check == 0x9000) {
            // Access character ROM directly (bypass banking)
            c64_bus_t* bus = (c64_bus_t*)vicii->bus;
            return bus->read_callbacks[ACID_CHARROM].read(
                bus->read_callbacks[ACID_CHARROM].context,
                (address & 0x0FFF) | 0xD000  // Map to $D000-$DFFF range
            );
        }
    }
    
    // Use bus memory read for all other accesses
    return c64_bus_memory_read((c64_bus_t*)vicii->bus, vic_address);
}

// Update VIC-II bank mapping when CIA2 changes the bank
void vicii_update_bank_mapping(vicii_common_t* vicii, uint8_t bank) {
    // VIC-II bank is inverted: 0=bank3, 1=bank2, 2=bank1, 3=bank0
    bank = 3 - (bank & 0x03);
    vicii->bank = bank;
    
    switch (bank) {
        case 0: vicii->memory_map.bank_base = VICII_BANK_0_BASE; break;
        case 1: vicii->memory_map.bank_base = VICII_BANK_1_BASE; break;
        case 2: vicii->memory_map.bank_base = VICII_BANK_2_BASE; break;
        case 3: vicii->memory_map.bank_base = VICII_BANK_3_BASE; break;
    }
    
    // Update video matrix and character base addresses
    uint8_t mp_reg = vicii->registers[VICII_MP];
    vicii->memory_map.video_matrix_base = ((mp_reg & 0xF0) >> 4) * 0x400;
    vicii->memory_map.char_base = ((mp_reg & 0x0E) >> 1) * 0x800;
    
    // Character ROM is accessible in banks 0 and 2 when char_base points to $1000/$9000
    vicii->memory_map.char_rom_enabled = (bank == 0 || bank == 2) &&
                                        (vicii->memory_map.char_base == 0x1000 ||
                                         vicii->memory_map.char_base == 0x9000);
}

// Character access (c-access) - reads from video matrix
void vicii_common_c_access(vicii_common_t* vicii) {
    if (!vicii->video_logic_display_state) return;
    
    // Calculate video matrix address
    uint16_t address = vicii->memory_map.video_matrix_base | vicii->vc;
    
    // Read character code from video matrix
    vicii->video_matrix_line[vicii->vmli] = vicii_memory_read_cycle(vicii, address);
    
    // Read color from color RAM (always at $D800-$DBFF, independent of VIC banking)
    if (vicii->bus) {
        c64_bus_t* bus = (c64_bus_t*)vicii->bus;
        vicii->video_color_line[vicii->vmli] =
            bus->read_callbacks[ACID_COLORRAM_D8].read(
                bus->read_callbacks[ACID_COLORRAM_D8].context,
                0xD800 + vicii->vc
            ) & 0x0F;  // Color RAM is only 4 bits
    }
}

// Graphics access (g-access) - reads character/bitmap data
void vicii_common_g_access(vicii_common_t* vicii) {
    uint16_t address;
    uint8_t char_code = 0;
    vicii_color_t color_code = VICII_COLOR_BLACK;
    
    if (vicii->video_logic_display_state) {
        // Get data from video matrix line (set by c-access)
        char_code = vicii->video_matrix_line[vicii->vmli];
        color_code = vicii->video_color_line[vicii->vmli];
        
        // Calculate graphics data address based on mode
        if ((vicii->graphics_mode & VICII_BITMAP_MODE_MASK) == 0) {
            // Text mode: address = char_base + (char_code * 8) + row
            address = vicii->memory_map.char_base + (char_code << 3) + vicii->rc;
        } else {
            // Bitmap mode: address = char_base + (vc * 8) + row
            address = vicii->memory_map.char_base + (vicii->vc << 3) + vicii->rc;
        }
    } else {
        // Idle state: always access $3fff
        address = 0x3fff;
    }
    
    // Handle Extended Color Mode (ECM)
    if (vicii->graphics_mode & VICII_EXTENDED_COLOR_MODE_MASK) {
        // In ECM, address lines 9 and 10 are forced low
        address &= ~0x0600;
    }
    
    // Read graphics data
    uint8_t graphics_data = vicii_memory_read_cycle(vicii, address);
    
    // Update colors based on graphics mode and character/color data
    switch (vicii->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            vicii->colors[4].color = color_code;
            break;
            
        case VICII_GM_MULTICOLOR_TEXT: {
            uint8_t mc_flag = (color_code >> 3) & 1;  // Bit 3 determines multicolor
            if (mc_flag) {
                vicii->colors[3].color = color_code & 0x07;  // Use bits 0-2
            } else {
                vicii->colors[4].color = color_code;
            }
            break;
        }
        
        case VICII_GM_STANDARD_BITMAP:
            vicii->colors[0].color = char_code & 0x0F;       // Lower nibble
            vicii->colors[4].color = (char_code >> 4) & 0x0F; // Upper nibble
            break;
            
        case VICII_GM_MULTICOLOR_BITMAP:
            vicii->colors[1].color = (char_code >> 4) & 0x0F; // Upper nibble
            vicii->colors[2].color = char_code & 0x0F;        // Lower nibble
            vicii->colors[3].color = color_code;              // Color RAM
            break;
            
        case VICII_GM_ECM_TEXT:
            // Select either VICII_B0C, VICII_B1C, VICII_B2C or VICII_B3C based on char_code bits
            vicii->colors[0].color = vicii->registers[VICII_B0C + ((char_code >> 6) & 3)] & 0x0F;
            vicii->colors[4].color = color_code;
            break;
    }
    
    // Emit pixels based on graphics data and mode
    vicii_common_emit_graphics_pixels(vicii, graphics_data);
    
    // Increment video counters
    vicii->vc = (vicii->vc + 1) & 0x3FF;  // 10-bit counter
    vicii->vmli = (vicii->vmli + 1) & 0x3F; // 6-bit counter
}

// Emit border pixels
void vicii_common_emit_border_pixels(vicii_common_t* vicii) {
    // Emit 8 border pixels (one character width)
    for (int i = 0; i < 8; i++) {
        if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
            vicii->pixel_line_priority[vicii->pixel_line_index] = vicii->border_pixel.priority;
            vicii->pixel_line_color[vicii->pixel_line_index] = vicii->border_pixel.color;
            vicii->pixel_line_index++;
        }
    }
}

// Emit graphics pixels based on graphics data
void vicii_common_emit_graphics_pixels(vicii_common_t* vicii, uint8_t graphics_data) {
    // Check if we're in multicolor mode
    bool multicolor = false;
    
    // Note : Because of below differences, we cannot check for VICII_MULTICOLOR_MODE_MASK alone
    switch (vicii->graphics_mode) {
        case VICII_GM_MULTICOLOR_TEXT: {
            uint8_t color_code = vicii->video_color_line[vicii->vmli];
            multicolor = (color_code >> 3) & 1;
            break;
        }
        case VICII_GM_MULTICOLOR_BITMAP:
            multicolor = true;
            break;
    }
    
    if (multicolor) {
        // Multicolor mode: 4 double-width pixels, 2 bits per pixel
        for (int i = 0; i < 4; i++) {
            uint8_t color_index = (graphics_data >> (6 - i * 2)) & 0x03;
            vicii_pixel_t pixel = vicii->colors[color_index];
            
            // Emit double-width pixel
            for (int j = 0; j < 2; j++) {
                if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
                    vicii->pixel_line_priority[vicii->pixel_line_index] = pixel.priority;
                    vicii->pixel_line_color[vicii->pixel_line_index] = pixel.color;
                    vicii->pixel_line_index++;
                }
            }
        }
    } else {
        // Standard mode: 8 single-width pixels, 1 bit per pixel
        for (int i = 0; i < 8; i++) {
            uint8_t bit = (graphics_data >> (7 - i)) & 1;
            vicii_pixel_t pixel = vicii->colors[bit ? 4 : 0];  // Foreground or background
            
            if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
                vicii->pixel_line_priority[vicii->pixel_line_index] = pixel.priority;
                vicii->pixel_line_color[vicii->pixel_line_index] = pixel.color;
                vicii->pixel_line_index++;
            }
        }
    }
}

// Macro to create RGBA color values for OpenGL GL_RGBA format
#define RGBA_COLOR(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

// C64 color palette (16 colors) in RGBA format for OpenGL
static const uint32_t c64_palette[16] = {
    RGBA_COLOR(0x00, 0x00, 0x00, 0xFF),  // 0: Black
    RGBA_COLOR(0xFF, 0xFF, 0xFF, 0xFF),  // 1: White
    RGBA_COLOR(0x68, 0x37, 0x2B, 0xFF),  // 2: Red
    RGBA_COLOR(0x70, 0xA4, 0xB2, 0xFF),  // 3: Cyan
    RGBA_COLOR(0x6F, 0x3D, 0x86, 0xFF),  // 4: Purple/Violet
    RGBA_COLOR(0x58, 0x8D, 0x43, 0xFF),  // 5: Green
    RGBA_COLOR(0x35, 0x28, 0x79, 0xFF),  // 6: Blue
    RGBA_COLOR(0xB8, 0xC7, 0x6F, 0xFF),  // 7: Yellow
    RGBA_COLOR(0x6F, 0x4F, 0x25, 0xFF),  // 8: Orange
    RGBA_COLOR(0x43, 0x39, 0x00, 0xFF),  // 9: Brown
    RGBA_COLOR(0x9A, 0x67, 0x59, 0xFF),  // 10: Light Red
    RGBA_COLOR(0x44, 0x44, 0x44, 0xFF),  // 11: Dark Grey
    RGBA_COLOR(0x6C, 0x6C, 0x6C, 0xFF),  // 12: Grey
    RGBA_COLOR(0x9A, 0xD2, 0x84, 0xFF),  // 13: Light Green
    RGBA_COLOR(0x6C, 0x5E, 0xB5, 0xFF),  // 14: Light Blue
    RGBA_COLOR(0x95, 0x95, 0x95, 0xFF)   // 15: Light Grey
};

// Get default C64 color palette
uint32_t* vicii_common_get_default_palette(void) {
    return (uint32_t*)c64_palette;
}

// Set framebuffer for VIC-II output
void vicii_common_set_framebuffer(vicii_common_t* vicii, uint32_t* framebuffer, int width, int height) {
    vicii->framebuffer = framebuffer;
    vicii->framebuffer_width = width;
    vicii->framebuffer_height = height;
    
    printf("VIC-II: Connected to framebuffer (%dx%d)\n", width, height);
    
    // Initialize VIC-II state to generate proper colors, but don't pre-fill framebuffer
    // Let the VIC-II cycle function generate the actual content
    vicii->border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border_pixel.color = VICII_COLOR_LIGHT_BLUE;
    
    // Initialize pixel line buffers with proper border colors
    if (vicii->pixel_line_color && vicii->visible_pixels_per_line > 0) {
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}

// Flush pixel line to framebuffer (equivalent to C# FlushPixelLineToOutput)
void vicii_common_flush_pixel_line_to_output(vicii_common_t* vicii, uint32_t* palette, int y) {
    if (!vicii->framebuffer || !palette || y >= vicii->framebuffer_height) {
        return;
    }
    
    // Calculate row address in framebuffer
    int row_address = y * vicii->framebuffer_width;
    
    // Always use current border color from register (this ensures proper color updates)
    uint8_t border_color_index = vicii->registers[VICII_EC] & 0x0F;
    uint32_t border_color = palette[border_color_index];
    
    // First, fill the entire line with border color
    for (int x = 0; x < vicii->framebuffer_width; x++) {
        vicii->framebuffer[row_address + x] = border_color;
    }
    
    // Then, overwrite with VIC-II generated pixels if any, centered in the framebuffer
    int pixels_to_copy = (vicii->pixel_line_index < vicii->visible_pixels_per_line) ?
                        vicii->pixel_line_index : vicii->visible_pixels_per_line;
    
    if (vicii->pixel_line_color && pixels_to_copy > 0) {
        // Center VIC-II visible area within the framebuffer
        int offset_x = (vicii->framebuffer_width - vicii->visible_pixels_per_line) / 2;
        
        for (int x = 0; x < pixels_to_copy; x++) {
            int fb_x = offset_x + x;
            if (fb_x >= 0 && fb_x < vicii->framebuffer_width) {
                uint8_t color_index = vicii->pixel_line_color[x] & 0x0F;  // Ensure 4-bit color
                vicii->framebuffer[row_address + fb_x] = palette[color_index];
            }
        }
    }
    
    // Reset pixel line index for next line
    vicii->pixel_line_index = 0;
}