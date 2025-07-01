#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// forwards
static void update_border_color_and_priority(vicii_common_t* vicii);
static void update_colors_based_on_graphics_mode_and_background_012(vicii_common_t* vicii);
static void update_graphics_mode_and_dependent_colors(vicii_common_t* vicii);
static void update_is_bad_line(vicii_common_t* vicii);
void set_main_border_flip_flop(vicii_common_t* vicii, bool main_border_flip_flop);

// Helper function to read clear (collision registers)
static uint8_t read_clear(vicii_common_t* vicii, uint8_t reg) {
    uint8_t val = vicii->registers[reg];
    vicii->registers[reg] = 0;
    return val;
}

// Register read with proper masking
uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    uint8_t reg = address & VICII_REGS_MASK;
    
    uint8_t data = ((c64_bus_t*)vicii->bus)->data; // Used for "floating" bus state for subsequent unattached reads

    switch (reg) {
        case VICII_C1:
            return (vicii->registers[VICII_C1] & 0x7F) |       //    17 $d011 Control register 1
                   ((vicii->raster_counter >> 1) & VICII_C1_RST8); //         bit 7 (RST8) reflects raster_counter bit 8 
        case VICII_RASTER:
            return vicii->raster_counter & 0xFF;               //    18 $d012 Reflects raster_counter bits 0..7
        case VICII_C2:
            return vicii->registers[VICII_C2] | (data & 0xC0); //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
        case VICII_MP:
            return vicii->registers[VICII_MP] | (data & 0x01); //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
        case VICII_IR:
            return vicii->registers[VICII_IR] | (data & 0x70); //    25 $d019 | IRQ|  - |  - |  - | ILP|IMMC|IMBC|IRST| Interrupt register
        case VICII_IE:
            return vicii->registers[VICII_IE] | (data & 0xF0); //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
        case VICII_MXM:
            return read_clear(vicii, VICII_MXM_2);             //    30 $d01e Sprite-sprite collision is cleared on read
        case VICII_MXD:
            return read_clear(vicii, VICII_MXD_2);             //    31 $d01f Sprite-data collision is cleared on read
        default:
            if (reg <= 29) {
                return vicii->registers[reg];                  //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
            } else if (reg <= 46) {
                return vicii->registers[reg] | (data & 0xF0);  // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
            } else {
                return data;                                   // 47-63 $d02f-$d03f unattached registers (many docs say: give $ff on reading)
            }
    }
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
        ir |= VICII_IR_UNUSED; // TODO : Remove this now that reads use floating bus data?
        // Store the resulting bits
        vicii->registers[VICII_IR] = ir;
        // Note/TODO : Here, it's assumed that when all interrupt bits are cleared, the
        // IR_IRQ flag is untouched - it'll be cleared later, in vicii_common_handle_raster_interrupt()
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
            // Note : Any change in C1_DEN and/or C1_YSCROLL impacts is_bad_line,
            // so update that immediately on a write to C1. Updating is_bad_line
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
            for (int i = 0; i < VICII_NUM_SPRITES; i++)
                vicii_sprite_written_to_reg_mxye(vicii, i);
            break;
        case VICII_MXDP: // $d01b Sprite data priority
            for (int i = 0; i < VICII_NUM_SPRITES; i++)
                vicii_sprite_written_to_reg_mxdp(vicii, i);
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
            for (int i = 0; i < VICII_NUM_SPRITES; i++)
                vicii_sprite_written_to_reg_mm0(vicii, i);
            break;
        case VICII_MM1: // $d026 (4 bits) Sprite multicolor 1
            for (int i = 0; i < VICII_NUM_SPRITES; i++)
                vicii_sprite_written_to_reg_mm1(vicii, i);
            break;
        case VICII_M0C: // $d027 (4 bits) Color sprite 0
        case VICII_M1C: // $d028 (4 bits) Color sprite 1
        case VICII_M2C: // $d029 (4 bits) Color sprite 2
        case VICII_M3C: // $d02a (4 bits) Color sprite 3
        case VICII_M4C: // $d02b (4 bits) Color sprite 4
        case VICII_M5C: // $d02c (4 bits) Color sprite 5
        case VICII_M6C: // $d02d (4 bits) Color sprite 6
        case VICII_M7C: // $d02e (4 bits) Color sprite 7
            vicii_sprite_written_to_reg_mxc(vicii, reg - VICII_M0C);
            break;
    }
}

// --- Sprite register write handlers ---
// "The expansion flip flop is set as long as the bit in MxYE in register $d017 corresponding to the sprite is cleared."
static void vicii_sprite_written_to_reg_mxye(vicii_common_t* vicii, int i) {
    if (!(vicii->registers[VICII_MXYE] & (1 << i)))
        vicii->sprites[i].expansion_flip_flop = true;
}
// "With the MxDP bits from register $d01b, you can separately specify for each sprite if it should be displayed in front of or behind the foreground pixels."
static void vicii_sprite_written_to_reg_mxdp(vicii_common_t* vicii, int i) {
    // Priority: 0 = in front, 1 = behind
    vicii->sprites[i].priority = (vicii->registers[VICII_MXDP] & (1 << i)) ? VICII_PRIORITY_SPRITE_BEHIND : VICII_PRIORITY_SPRITE_IN_FRONT;
}
// "MM0: Sprite multicolor 0"
static void vicii_sprite_written_to_reg_mm0(vicii_common_t* vicii, int i) {
    vicii->sprites[i].color = vicii->registers[VICII_MM0] & 0x0F;
}
// "MM1: Sprite multicolor 1"
static void vicii_sprite_written_to_reg_mm1(vicii_common_t* vicii, int i) {
    vicii->sprites[i].color = vicii->registers[VICII_MM1] & 0x0F;
}
// "MxC: Sprite color register"
static void vicii_sprite_written_to_reg_mxc(vicii_common_t* vicii, int i) {
    vicii->sprites[i].color = vicii->registers[VICII_M0C + i] & 0x0F;
}

// Initialize VIC-II to default state
void vicii_common_initialize(vicii_common_t* vicii) {
    // TODO : Set VIC-II default bank to 0 (lowest 16 Kb)
    
    // Set all registers to their default value :
    memset(vicii->registers, 0, sizeof(vicii->registers)); // SPxX,SPxY,MSIGX,etc
    vicii->registers[VICII_C1] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                            (VICII_C1_YSCROLL & 3); // 155:Display ENable,25-row  
    vicii->registers[VICII_MXE] = 0;  // All sprites disabled
    vicii->registers[VICII_C2] = VICII_C2_CSEL; // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
    vicii->registers[VICII_MP] = VICII_MP_CB12 | VICII_MP_VM10; // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
    vicii->registers[VICII_IR] = VICII_IR_UNUSED; // See BusWrite; Always set the unused bits high
    vicii->registers[VICII_EC] = VICII_COLOR_LIGHT_BLUE; // 14: Border Color
    vicii->registers[VICII_B0C] = VICII_COLOR_BLUE; // 6: Background Color 0
    vicii->registers[VICII_B1C] = VICII_COLOR_WHITE; // 1: Background Color 1
    vicii->registers[VICII_B2C] = VICII_COLOR_RED; // 2: Background Color 2
    vicii->registers[VICII_B3C] = VICII_COLOR_CYAN; // 3: Background Color 3
    vicii->registers[VICII_MM0] = VICII_COLOR_PURPLE; // 4: Sprite Multicolor 0
    vicii->registers[VICII_MM1] = VICII_COLOR_BLACK; // 0: Sprite Multicolor 1
    vicii->registers[VICII_M0C] = VICII_COLOR_WHITE; // 1: Sprite Color 0
    vicii->registers[VICII_M1C] = VICII_COLOR_RED; // 2: Sprite Color 1
    vicii->registers[VICII_M2C] = VICII_COLOR_CYAN; // 3: Sprite Color 2
    vicii->registers[VICII_M3C] = VICII_COLOR_PURPLE; // 4: Sprite Color 3
    vicii->registers[VICII_M4C] = VICII_COLOR_GREEN; // 5: Sprite Color 4
    vicii->registers[VICII_M5C] = VICII_COLOR_BLUE; // 6: Sprite Color 5
    vicii->registers[VICII_M6C] = VICII_COLOR_YELLOW; // 7: Sprite Color 6
    vicii->registers[VICII_M7C] = VICII_COLOR_MEDIUM_GREY; // 12: Sprite Color 7
    // Set registers 47-63 $d02f-$d03f unused addresses to 0xFF (which we never overwrite)
    // so that reading them needs no separate case in default MaskBusRead() return value.
    for (int r = 47; r < VICII_REGS_SIZE; r++) {
        vicii->registers[r] = 0xFF;  // Unused addresses
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

// Reactivated and adapted for new C design
// "If there is a Bad Line Condition, the BA line is set low and the transition from idle to display state occurs as soon as there is a Bad Line Condition."
void vicii_common_handle_bad_line_related_state(vicii_common_t* vicii)
{
    if (vicii->is_bad_line)
    {
        // BA.SetLow() is handled in the main cycle logic
        vicii->video_logic_display_state = true;
    }
}

// "1. If the X coordinate reaches the right comparison value, the main border flip flop is set."
void vicii_common_handle_right_border_flipflop(vicii_common_t* vicii)
{
    // BorderRightCSEL0 = 336 (should be 335) / BorderRightCSEL1 = 344
    // TODO: Make right border pixel-exact based on non-4-multiple BorderRightCSEL0 limit
    if (vicii->x_coordinate == vicii->border_right)
    {
        set_main_border_flip_flop(vicii, true);
    }
}

// "If the X coordinate reaches the left comparison value ..."
void vicii_common_handle_left_border_flipflop(vicii_common_t* vicii)
{
    if (vicii->x_coordinate == vicii->border_left)
    {
        // "4. If the X coordinate reaches the left comparison value and the Y coordinate reaches the bottom one, the vertical border flip flop is set."
        if (vicii->raster_counter == vicii->border_bottom)
            vicii->vertical_border_flip_flop = true;
        // "5. If the X coordinate reaches the left comparison value and the Y coordinate reaches the top one and the DEN bit in register $d011 is set, the vertical border flip flop is reset."
        if (vicii->raster_counter == vicii->border_top && Reg_DisplayEnable(vicii))
            vicii->vertical_border_flip_flop = false;
        // "6. If the X coordinate reaches the left comparison value and the vertical border flip flop is not set, the main flip flop is reset."
        if (!vicii->vertical_border_flip_flop)
            set_main_border_flip_flop(vicii, false);
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
    
    // "A Bad Line Condition is given at any arbitrary clock cycle, if at the
    // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
    // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
    // DEN bit was set during an arbitrary cycle of raster line $30."
    if (eevmf) {
        // "A Bad Line Condition can only occur if the DEN bit has been
        // set for at least one cycle somewhere in raster line $30."
        if (vicii->raster_counter == 0x30) { // 48
            // Check if DEN was set during raster line $30
            if (!vicii->was_den_set_during_raster_30) {
                vicii->was_den_set_during_raster_30 = Reg_DisplayEnable(vicii);
            }
        }
        
        vicii->is_bad_line = vicii->was_den_set_during_raster_30 &&
                         ((vicii->raster_counter & 0x07) ==
                          (vicii->registers[VICII_C1] & VICII_C1_YSCROLL));
    } else {
        vicii->is_bad_line = false;
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
}

// Handles sprite DMA state transitions at specific cycles
// At cycle 55, check for DMA start
static void vicii_common_handle_cycle_55_sprite_dma(vicii_common_t* vicii) {
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vicii_sprite_t* spr = &vicii->sprites[i];
        if (spr->enabled && spr->y_pos == (uint8_t)(vicii->raster_counter - 1)) {
            spr->dma_counter = 0;
            spr->mcbase = 0;
            if (spr->y_expand)
                spr->expansion_flip_flop = !spr->expansion_flip_flop;
            else
                spr->expansion_flip_flop = true;
        }
    }
}
// Handles sprite DMA state transitions at specific cycles
// At cycle 55, check for DMA start
// At cycle 56, handle expansion flip-flop (if needed)
static void vicii_common_handle_cycle_56_sprite_dma(vicii_common_t* vicii) {
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vicii_sprite_t* spr = &vicii->sprites[i];
        if (spr->y_expand) {
            spr->expansion_flip_flop = !spr->expansion_flip_flop;
        }
    }
}

// At cycle 58, set display state and MC
static void vicii_common_handle_cycle_58(vicii_common_t* vicii) {
    // Handle end of character row
    if (vicii->rc == 7) {
        if (!vicii->is_bad_line) {
            vicii->video_logic_display_state = false;
            vicii->vc_base = vicii->vc;
            vicii->rc = 0;
        }
    }
    
    if (vicii->video_logic_display_state) {
        vicii->rc = (vicii->rc + 1) & 0x07;
    }
    
    // Handles sprite display state transitions at cycle 58
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vicii_sprite_t* spr = &vicii->sprites[i];
        if (spr->enabled && spr->y_pos == (uint8_t)(vicii->raster_counter - 1)) {
            spr->display_state = true;
            spr->mc = spr->mcbase;
        }
    }
}

// Sprite sequencer and shift register update for pixel emission
// "The sprite shift register is loaded at the start of the display state and shifted each cycle."
static void vicii_common_update_sprite_sequencer(vicii_common_t* vicii) {
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vicii_sprite_t* spr = &vicii->sprites[i];
        if (spr->display_state) {
            // On the first cycle of display state, load the shift register from sprite data buffer
            if (spr->sequencer_reload) {
                // Load the shift register from the DMA-fetched sprite data buffer
                // Each sprite is 24 bits (3 bytes), loaded MSB first
                spr->shift_reg = (spr->data_buffer[0] << 16) | (spr->data_buffer[1] << 8) | spr->data_buffer[2];
                spr->sequencer_reload = false;
            } else {
                // Shift the register left (MSB first)
                spr->shift_reg <<= 1;
            }
        }
    }
}

/*
 * VIC-II TIMING ADVANCEMENT - VIC-II owns the master clock
 * Updates cycle_group only when transitioning to different groups
 */
void vic_ii_advance_timing(vicii_common_t* vicii) {
    // Count X position
    vicii->x_coordinate += 8;
    vicii->x_cycle++;
    
    // Handle horizontal retrace
    // Check for end of raster line or update cycle group
    if (vicii->x_cycle >= vicii->cycles_per_line) {
        // Flush current pixel line to framebuffer before advancing to next line
        // Always flush if we have a framebuffer, even for raster lines beyond VIC-II's normal range
        if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
            vicii_common_flush_pixel_line_to_output(vicii,
                vicii_common_get_default_palette(), vicii->raster_counter);
        }
    
        vicii->pixel_line_index = 0;  // Reset pixel line index
        vicii->x_coordinate = 0;
        vicii->x_cycle = 0;
        vicii->cycle_group = CYCLE_GROUP_LINE_START;  // Reset to line start
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
                vicii->is_bad_line = false;
                vicii->vc_base = 0;
                vicii->lp_edge_detected = false;
                // Handle frame completion directly
                //c64_handle_frame_complete(c64);
            }
        } else {
            update_is_bad_line(vicii);
            vicii_common_handle_raster_interrupt(vicii);
        }
    }
    
    switch (vicii->x_cycle) {
        case 1:
            vicii->cycle_group = CYCLE_GROUP_SPRITES;
            break;
        case 9:
            vicii->cycle_group = CYCLE_GROUP_REFRESH;
            break;
        case 10:
            vicii->cycle_group = CYCLE_GROUP_NORMAL;
            break;
        case 12:
            vicii->cycle_group = CYCLE_GROUP_BADLINE_WARNING;
            break;
        case 15:
            vicii->cycle_group = CYCLE_GROUP_CHAR_AND_COLOR;
            // Handle display logic control (separate from pixel generation)
            // Reset video counters at start of display window
            vicii->vc = vicii->vc_base;
            vicii->vmli = 0;
            if (vicii->is_bad_line) {
                vicii->rc = 0;
            }
            break;

        case 55:
            vicii->cycle_group = CYCLE_GROUP_LINE_END;
            // Call sprite DMA/display state handler at the appropriate cycle
            vicii_common_handle_cycle_55_sprite_dma(vicii);
            break;
        case 56:
            vicii_common_handle_cycle_56_sprite_dma(vicii);
            break;
        case 58:
            vicii_common_handle_cycle_58(vicii);
            break;
        // No other cases - cycle_group stays the same for intermediate values
    }
}

/*
 * VIC-II BUS ACCESS: Consolidated bus access logic
 * This implements the character and color data fetching described in the documentation
 */
void vic_ii_bus_access(vicii_common_t* vicii, c64_bus_t* bus, 
                       uint8_t access_type, uint8_t access_param) {
    uint16_t address;
    uint8_t data;
    
    switch (access_type) {
        case VIC_ACCESS_SPRITE_PTR:
            // Fetch sprite pointer (p-access from documentation)
            address = vicii->memory_map.video_matrix_base + 0x3F8 + vicii->vc;
            // TODO : What about = vicii->screen_base + 0x3F8 + access_param;
            data = vicii_memory_read(vicii, address);
            vicii->sprites[access_param].data_pointer = data;
            break;
            
        case VIC_ACCESS_SPRITE_DATA:
            // Fetch sprite data byte (s-access from documentation)
            if (vicii->sprites[access_param].dma_counter < 3) {
                address = vicii->sprites[access_param].data_pointer * 64 + 
                         vicii->sprites[access_param].dma_counter;
                data = vicii_memory_read(vicii, address);
                // Store DMA-fetched sprite data in per-sprite buffer
                vicii->sprites[access_param].data_buffer[vicii->sprites[access_param].dma_counter] = data;
                vicii->sprites[access_param].dma_counter++;
            }
            break;
            
        case VIC_ACCESS_CHAR_DATA:
            // if (!vicii->video_logic_display_state) return;

            // Handle simultaneous color access during bad line
            // Read color from color RAM (always at $D800-$DBFF, independent of VIC banking)
            address = 0xD800 + vicii->vc;
            // TODO : What about = 0xD800 + (vicii->video_counter & 0x3FF) + access_param;
            // Color RAM uses same addressing as character data (lower 10 bits)
            data = bus->read_callbacks[ACID_COLORRAM_D8].read(
                bus->read_callbacks[ACID_COLORRAM_D8].context, address);
            vicii->video_color_line[vicii->vmli] = data & 0x0F; // Color RAM returns only 4 bits (on pins D8-D11)

            // Character access (c-access) - reads from video matrix
            // Fetch character code (c-access from documentation)
            // This reads from video matrix using VC (Video Counter)
            // Calculate video matrix address
            address = vicii->memory_map.video_matrix_base | vicii->vc;
            // TODO : What about = vicii->screen_base + vicii->video_counter + access_param; ?
            // Read character code from video matrix
            data = vicii_memory_read(vicii, address);
            vicii->video_matrix_line[vicii->vmli] = data;
            break;
            
        case VIC_ACCESS_REFRESH:
            // DRAM refresh (r-access from documentation)
            address = vicii->memory_map.video_matrix_base | 0x3F00 | (0xFF & (-1 - 5 * vicii->raster_counter));
            // TODO : What about = vicii->video_bank_base | 0x3F00 | (0xFF & (-1 - 5 * vicii->raster_counter));
            data = vicii_memory_read(vicii, address);
            break;
            
        default:
            // Idle access (i-access from documentation)
            data = vicii_memory_read(vicii, 0x3FFF);
            break;
    }
    
    bus->data = data;
}

/*
 * HANDLE SPRITE BUS REQUIREMENTS FOR CURRENT CYCLE
 * Returns access type only - bus control can be derived from return value
 */
inline uint8_t vic_ii_handle_sprite_requirements(vicii_common_t* vicii, uint8_t sprite_num, uint8_t target_line) {
    if ((vicii->sprites[sprite_num].enabled) &&
        (target_line >= vicii->sprites[sprite_num].y_pos) &&
        (target_line <= vicii->sprites[sprite_num].y_pos + 
         (vicii->sprites[sprite_num].y_expand ? 42 : 21))) {
        
        if (vicii->x_cycle & 1) {
            // Odd cycles: sprite pointer fetches
            return VIC_ACCESS_SPRITE_PTR;
        } else {
            // Even cycles: sprite data fetches (if DMA active)
            if (vicii->sprites[sprite_num].dma_counter < 3) {
                return VIC_ACCESS_SPRITE_DATA;
            }
        }
    }
    return VIC_ACCESS_IDLE;
}

// Emit sprite pixels for the current cycle
// "Sprite pixels are emitted according to the sprite sequencer and priority logic."
static void vicii_common_emit_sprite_pixels(vicii_common_t* vicii) {
    // Track which sprite (if any) has already set a pixel at this position
    int sprite_drawn = -1;
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vicii_sprite_t* spr = &vicii->sprites[i];
        if (!spr->display_state)
            continue;
        // Emit a pixel if the MSB of the shift register is set
        uint8_t sprite_pixel = (spr->shift_reg & 0x800000) ? 1 : 0; // 24-bit shift reg, MSB first
        if (!sprite_pixel || vicii->pixel_line_index >= vicii->visible_pixels_per_line)
            continue;

        // Check for sprite-sprite collision
        if (sprite_drawn != -1) {
            // Set sprite-sprite collision bits for both sprites
            vicii->registers[VICII_MXM_2] |= (1 << i) | (1 << sprite_drawn);
        } else {
            sprite_drawn = i;
        }

        // Priority logic: check if sprite is in front or behind graphics
        vicii_priority_t bg_priority = vicii->pixel_line_priority[vicii->pixel_line_index];
        bool sprite_in_front = (spr->priority == VICII_PRIORITY_SPRITE_IN_FRONT);
        bool bg_is_background = (bg_priority == VICII_PRIORITY_BACKGROUND || bg_priority == VICII_PRIORITY_BORDER);

        if (sprite_in_front || bg_is_background) {
            // Sprite is in front, or background pixel: draw sprite
            vicii->pixel_line_priority[vicii->pixel_line_index] = VICII_PRIORITY_SPRITE_IN_FRONT;
            vicii->pixel_line_color[vicii->pixel_line_index] = spr->color;
        } else {
            // Sprite is behind graphics, and graphics is not background: do not draw, but check collision
            // Set sprite-data (sprite-background) collision bit
            vicii->registers[VICII_MXD_2] |= (1 << i);
        }
    }
}

// Render graphics and sprite pixels for the current cycle using only intermediate storage
void vicii_common_render_pixels(vicii_common_t* vicii) {
    vicii_common_update_sprite_sequencer(vicii);
    // Use intermediate storage (video_matrix_line, video_color_line, etc.)
    // Graphics pixel emission
    uint8_t char_code = vicii->video_matrix_line[vicii->vmli];
    vicii_color_t color_code = vicii->video_color_line[vicii->vmli];
    uint8_t graphics_data = 0; // Should be set from a pre-fetched buffer
    vicii_common_emit_graphics_pixels(vicii, graphics_data);
    vicii_common_emit_sprite_pixels(vicii);
}

void vic_ii_process_display_data(vicii_common_t* vicii, c64_bus_t* bus) {
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
                vicii_common_render_pixels(vicii);
            } else {
                // Outside display area, display disabled, or beyond normal VIC range - emit border pixels
                vicii_common_emit_border_pixels(vicii);
            }
        }
    }
}

// Main cycle function with character and graphics access
void vicii_common_cycle(vicii_common_t* vicii) {
    // Direct bus control variables
    uint8_t access_type = VIC_ACCESS_IDLE;
    uint8_t access_param = 0;
    bool ba_low = false;
    
    switch (vicii->cycle_group) {
        case CYCLE_GROUP_SPRITES: // Cycles 1-8
        case CYCLE_GROUP_LINE_END: // Cycles 55-62 (for PAL, 55-64 for NTSC)
            // Handle both sprite cases together
            {
                uint8_t sprite_num;
                uint8_t sprite_line;
                if (vicii->cycle_group == CYCLE_GROUP_SPRITES) {
                    // Cycles 1-8: sprites 3-7 (current line)
                    sprite_num = ((vicii->x_cycle - 1) / 2) + 3;
                    sprite_line = vicii->raster_counter;
                } else {
                    if (vicii->x_cycle >= 61) break;  // Only cycles 55-60 used
                    // Cycles 55-60: sprites 0-2 (next line)
                    sprite_num = (vicii->x_cycle - 55) / 2;  // 0, 1, 2
                    sprite_line = (vicii->raster_counter + 1) % vicii->total_lines;
                }
                
                // Get sprite access requirements
                access_type = vic_ii_handle_sprite_requirements(vicii, sprite_num, sprite_line);
                if (access_type != VIC_ACCESS_IDLE) {
                    ba_low = true;
                    access_param = sprite_num;  // access_param is always sprite_num
                }
            }
            break;
            
        case CYCLE_GROUP_REFRESH:
            // Cycle 9: Refresh doesn't require BA/AEC control
            access_type = VIC_ACCESS_REFRESH;
            break;
            
        case CYCLE_GROUP_BADLINE_WARNING:
            // Cycles 12-14: BA warning but VIC doesn't have full control yet
            if (vicii->is_bad_line) {
                ba_low = true;
            }
            break;
            
        case CYCLE_GROUP_CHAR_AND_COLOR:
            // Cycles 15-54: Bad line character/color fetches
            if (vicii->video_logic_display_state && vicii->is_bad_line) {
                ba_low = true;
                access_type = VIC_ACCESS_CHAR_DATA;
                access_param = vicii->x_cycle - 15;
            }
            break;
            
        default:
            break;
    }

    c64_bus_t* bus = vicii->bus;
    
    // Handle bad line related state
    // Set BA line directly
    if (ba_low) {
        // Set BA low during bad line (cycles 12-54)
        // Set BA low if VIC has bus control or is in bad line state
        bus->control_lines &= ~BA_LINE;
        // The transition from idle to display state occurs
        // as soon as there is a Bad Line Condition
        vicii->video_logic_display_state = true;
    } else {
        // Set BA high if VIC doesn't have bus control
        bus->control_lines |= BA_LINE;
    }

    // === PHI1 PHASE ===
    // Perform VIC bus access if it has control and wants to access
    // Derive vic_has_bus_control from access_type
    if (access_type > VIC_ACCESS_REFRESH) {
        // Set AEC low (VIC has full control)
        bus->control_lines &= ~AEC_LINE;
        vic_ii_bus_access(vicii, bus, access_type, access_param);
        // AEC stays low for phi2 when VIC has control (no change needed)
    } else {
        // === PHI2 PHASE (when VIC doesn't access) ===
        // Set AEC high (CPU can access) since VIC doesn't have control
        bus->control_lines |= AEC_LINE;
    }
    
    // Process display data and check interrupts
    vic_ii_process_display_data(vicii, bus);

    if (vic_ii_raster_irq_triggered(vicii, bus)) {
        vicii->registers[VICII_IR] |= VICII_IR_IRQ;
    }
        
    vic_ii_advance_timing(vicii);
}

/*
 * VIC-II Graphics access (g-access) - reads character/bitmap data
 */
void vicii_common_g_access(vicii_common_t* vicii) {
    // [In idle-state,] the sequencer uses "0" bits for the video matrix data
    uint8_t char_code = 0;
    vicii_color_t color_code = VICII_COLOR_BLACK; // = 0
    uint16_t address;
    if (vicii->video_logic_display_state) {
        // ...data is internally read from the position specified by VMLI
        // ...on each g-access in display state.
        // Get data from video matrix line (set by c-access)
        char_code = vicii->video_matrix_line[vicii->vmli];
        color_code = vicii->video_color_line[vicii->vmli];
        // In display state, [..] the addresses [..] depend on the selected display mode
        // Calculate graphics data address based on mode
        if ((vicii->graphics_mode & VICII_BITMAP_MODE_MASK) == 0) {
            // Text mode: address = char_base + (char_code * 8) + row
            address = vicii->memory_map.char_base + (char_code << 3) + vicii->rc;
        } else {
            // Bitmap mode: address = char_base + (vc * 8) + row
            address = vicii->memory_map.char_base + (vicii->vc << 3) + vicii->rc;
        }
    } else {
        // In idle state, [..] access is always to address
        // $3fff ($39ff when the ECM bit in register $d016 is set). The graphics
        // are displayed by the sequencer exactly as in display state, but with
        // the video matrix data treated as "0" bits.
        address = 0x3fff;
    }
    
    // Handle Extended Color Mode (ECM)
    // If the ECM bit is set
    if (vicii->graphics_mode & VICII_EXTENDED_COLOR_MODE_MASK) {
        // the address generator always holds the address lines 9 and 10 low
        address &= ~0x0600;
/* TODO : Translate :

#if HACK_PARTIAL_GRAPHICS_FIX
        // TODO : Figure out and fix the cause for "border-250.prg" graphics corruption
        // HACK : Somehow, reading graphics data from RAM with bit 15 set solves ""border-250.prg"
        // but regresses "Tetris" intro screeen; The latter seems more important, so this is disabled.
        if ((GraphicsMode & GM.BitMapModeMask) > 0) // non-text mode 
            address |= 0x4000); // bypass CharROM and use adjusted address

#endif
        A0toA13.PinnValue = (uint)address;
    }

    private void graph()
    {
*/
    }
    
    // Read graphics data
    // Read g-access Data bits and decode into pixels
    uint8_t graphics_data = vicii_memory_read(vicii, address);
// This proves pixels ARE drawn:    graphics_data = (uint8_t)(vicii->x_cycle ^ vicii->frame_count); // For testing, replace with actual read
    
    // For MulticolorTextMode (ECM/BMM/MCM=0/0/1) and InvalidTextMode (ECM/BMM/MCM=1/0/1)
    uint8_t mc_flag;
    if ((vicii->graphics_mode & 3) == 1) {
        // Bit D11 indicates multi-color pixels
        mc_flag = (uint8_t)color_code >> 3;
    } else {
        // Otherwise, multi-color pixels depend on the MCM bit
        mc_flag = vicii->graphics_mode & VICII_MULTICOLOR_MODE_MASK;
    }
    // MC flag 0: 8 pixels with 1 bit color; 1: 4 double-width pixels with 2 bits color

    // Update colors based on graphics mode and character/color data
    // Decode c-access Data bits into colors not already set in UpdateColorsBasedOnGraphicsModeAndBackground012()
    switch (vicii->graphics_mode) {
        case VICII_GM_STANDARD_TEXT: // ECM/BMM/MCM=0/0/0
            // Already set: Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
            vicii->colors[4].color = color_code; // Color from bits 8-11 of c-data
            break;            
        case VICII_GM_MULTICOLOR_TEXT: { // ECM/BMM/MCM=0/0/1
            // Already set : Colors[0].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
            if (mc_flag) {
                // Already set : Colors[0b01].Color = Reg_BackgroundColor(1); // Reg[B1C]; // $d022
                // Already set : Colors[0b10].Color = Reg_BackgroundColor(2); // Reg[B2C]; // $d023
                vicii->colors[3].color = color_code & 0x07; // Color from bits 8-10 of c-data
            } else {
                vicii->colors[4].color = color_code; // Color from bits 8-10 of c-data (11th is 0 here, so no masking needed)
            }
            break;
        }
        case VICII_GM_STANDARD_BITMAP: // ECM/BMM/MCM=0/1/0
            vicii->colors[0].color = char_code & 0x0F; // Color from bits 0-3 of c-data
            vicii->colors[4].color = char_code >> 4; // Color from bits 4-7 of c-data
            break;            
        case VICII_GM_MULTICOLOR_BITMAP: // ECM/BMM/MCM=0/1/1
            // Already set : Colors[0b00].Color = Reg_BackgroundColor(0); // Reg[B0C]; // $d021
            vicii->colors[1].color = char_code >> 4; // Color from bits 4-7 of c-data
            vicii->colors[2].color = char_code & 0x0F; // Color from bits 0-3 of c-data
            vicii->colors[3].color = color_code; // Color from bits 8-11 of c-data
            break;
        case VICII_GM_ECM_TEXT: // ECM/BMM/MCM=1/0/0
            // Select either VICII_B0C, VICII_B1C, VICII_B2C or VICII_B3C based on char_code bits
            vicii->colors[0].color = Reg_BackgroundColor(vicii, char_code >> 6); // B0C/B1C/B2C/B3C, depending bits 6&7 of c-data
            vicii->colors[4].color = color_code; // Color from bits 8-11 of c-data
            break;
        // case GM.InvalidTextMode: // unused // ECM/BMM/MCM=1/0/1 (optional MC mode via MC_flag)
        // case GM.InvalidBitmapMode1: // unused // ECM/BMM/MCM=1/1/0
        // case GM.InvalidBitmapMode2: // unused // ECM/BMM/MCM=1/1/1
        // Note : Colors for invalid modes are all set to Color.Black
        // in UpdateColorsBasedOnGraphicsModeAndBackground012()
    }
    
    // Emit pixels based on graphics data and mode
    vicii_common_emit_graphics_pixels(vicii, graphics_data);
    // Emit sprite pixels (after graphics, so sprite priority/collision can be handled)
    vicii_common_emit_sprite_pixels(vicii);
    
    // 4. VC and VMLI are incremented after each g-access
    vicii->vc = (vicii->vc + 1) & 0x3FF; // TODO : implement wrapping in u10
    vicii->vmli = (vicii->vmli + 1) & 0x3F; // TODO : implement wrapping in u6
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
uint8_t vicii_memory_read(vicii_common_t* vicii, uint16_t address) {
    if (!vicii->bus) return 0xFF;
    
    c64_bus_t* bus = (c64_bus_t*)vicii->bus;

    // TODO : Replace below with proper vic_read_callbacks, initialized
    // similarly to c64_bus_generate_all_pla_modes()

    // VIC-II only sees 14-bit addresses (16KB banks)
    // The top 2 bits come from CIA2 port A (inverted)
    uint16_t vic_address = (address & 0x3FFF) | vicii->memory_map.bank_base;
    
    // Special handling for character ROM access
    // Character ROM is visible in VIC bank when:
    // 1. Address is in range $1000-$1FFF or $9000-$9FFF
    // 2. Character ROM is enabled (determined by memory setup register)
    if (vicii->memory_map.char_rom_enabled) {
        uint16_t char_check = address & 0xF000;
        if (char_check == 0x1000 || char_check == 0x9000) {
            // Access character ROM directly (bypass banking)
            return bus->read_callbacks[ACID_CHARROM].read(
                bus->read_callbacks[ACID_CHARROM].context,
                (address & 0x0FFF) | 0xD000  // Map to $D000-$DFFF range
            );
        }
    }
    
    // Use bus memory read for all other accesses
    return c64_bus_memory_read(bus, vic_address);
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