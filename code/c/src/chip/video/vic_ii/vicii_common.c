#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declaration for decode function from bus
// Decode read/write ACIDs from encoded byte - reverse of encode_acid_rw
static inline void decode_acid_rw(uint8_t encoded, uint8_t* read_acid, uint8_t* write_acid) {
    uint8_t read_code = encoded & 0x0F;  // Extract bits [3:0]
    uint8_t write_code = (encoded >> 5) & 0x07;  // Extract bits [7:5]
    
    // Reverse the encoding logic
    // Special case: encoded 0 means I/O region, which will be resolved by memory access
    if (read_code == 0) {  // ACID_VIC_D0
        *read_acid = 0;  // I/O region - actual page will be determined by address bits
    } else {
        *read_acid = read_code + 15;  // ACID_IO2_DF
    }
    
    if (write_code == 0) {  // ACID_VIC_D0
        *write_acid = 0;  // I/O region - actual page will be determined by address bits  
    } else {
        *write_acid = write_code + 15;  // ACID_IO2_DF
    }
}

// ========================================================================================
// UTILITY FUNCTIONS AND CONSTANTS
// ========================================================================================

// C64 color palette (unchanged)
static const uint32_t c64_palette[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
    0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
    0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
    0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
};

// Cycle group lookup table - PAL timing (63 cycles per line) (Documentation section 3.6.3)
static const vic_cycle_group_t cycle_group_table_pal[64] = {
    CYCLE_GROUP_LINE_START,         // 0
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 1
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 2
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 3
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 4
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 5
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 6
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 7
    CYCLE_GROUP_SPRITE_PS_ACCESS,   // 8
    CYCLE_GROUP_REFRESH_ACCESS,     // 9
    CYCLE_GROUP_IDLE,               // 10
    CYCLE_GROUP_IDLE,               // 11
    CYCLE_GROUP_BADLINE_SETUP,      // 12
    CYCLE_GROUP_BADLINE_SETUP,      // 13
    CYCLE_GROUP_BADLINE_SETUP,      // 14
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 15
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 16
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 17
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 18
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 19
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 20
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 21
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 22
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 23
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 24
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 25
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 26
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 27
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 28
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 29
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 30
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 31
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 32
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 33
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 34
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 35
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 36
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 37
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 38
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 39
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 40
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 41
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 42
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 43
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 44
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 45
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 46
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 47
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 48
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 49
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 50
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 51
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 52
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 53
    CYCLE_GROUP_CHAR_COLOR_ACCESS,  // 54
    CYCLE_GROUP_LINE_END,           // 55
    CYCLE_GROUP_LINE_END,           // 56
    CYCLE_GROUP_LINE_END,           // 57
    CYCLE_GROUP_LINE_END,           // 58
    CYCLE_GROUP_LINE_END,           // 59
    CYCLE_GROUP_LINE_END,           // 60
    CYCLE_GROUP_LINE_END,           // 61
    CYCLE_GROUP_LINE_END,           // 62
    CYCLE_GROUP_LINE_START          // 63 (wrap-around safety)
};

uint32_t* vicii_common_get_default_palette(void) {
    return (uint32_t*)c64_palette;
}

// ========================================================================================
// MEMORY UNIT FUNCTIONS
// ========================================================================================

// Update memory mapping (Documentation section 2.4.2)
static inline void vic_memory_update_mapping(vic_memory_unit_t* memory, uint8_t mp_reg) {
    // Update addresses with bit operations
    // ""VM10-VM13 (register $d018) that specify one of four 1KB blocks within the 16KB address space""
    memory->vm_base = (mp_reg & 0xF0) << 6;  // VM10-VM13 bits * 0x400 -> << 6
    // ""CB11-CB13 (register $d018) that specify one of eight 2KB blocks within the 16KB address space""
    memory->cb_base = (mp_reg & 0x0E) << 10; // CB11-CB13 bits * 0x800 -> << 10
    
    // Character ROM accessibility check
    memory->char_rom_enabled = ((memory->bank & 0x01) == 0) &&  // bank 0 or 2
                              ((memory->cb_base == 0x1000) ||
                               (memory->cb_base == 0x9000));
}

uint8_t vic_memory_read(vicii_common_t* vicii, uint16_t address) {
    if (!vicii->bus.bus) return 0xFF;
    
    c64_bus_t* c64_bus = (c64_bus_t*)vicii->bus.bus;
    
    // OPTIMIZED VIC-II MEMORY READ - More optimal than CPU version
    // VIC-II can only read, never write, and can't access I/O regions
    // Uses only 4 banks (0-3) and 4 modes instead of CPU's 16 banks and 32 modes
    
    // Extract 4KB bank from VIC-II 14-bit address (0-3 for VIC-II's 16KB space)
    uint8_t vic_bank = (address >> 12) & 0x03;  // Only 4 banks, so mask with 0x03
    
    // Get encoded read acid for this bank in current PLA mode
    // VIC-II banking uses same mode as CPU but only needs 4 configurations
    uint8_t encoded = c64_bus->vic_encoded_rwid_per_bank_per_mode[c64_bus->pla_banking_mode][vic_bank];
    
    // Direct ACID extraction - no I/O detection needed (VIC-II can't access I/O)
    // VIC-II encoded values never use I/O regions (encoded == 0), so no special handling needed
    uint8_t read_acid = (encoded & 0x0F) + ACID_IO2_DF;
    
    // Calculate final address including VIC-II bank offset from CIA2
    uint16_t final_address = (address & 0x3FFF) | vicii->memory.bank_base;
    
    // Direct callback - single operation, no branch
    return c64_bus->read_callbacks[read_acid].read(
        c64_bus->read_callbacks[read_acid].context, 
        final_address
    );
}

// ========================================================================================
// VIDEO LOGIC UNIT FUNCTIONS
// ========================================================================================

// Update bad line condition (Documentation section 3.5)
void vic_update_badline_condition(vicii_common_t* vicii) {
    uint16_t raster = vicii->timing.raster_counter;
    
    // ""A Bad Line Condition is given at any arbitrary clock cycle, if at the
    // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
    // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
    // DEN bit was set during an arbitrary cycle of raster line $30.""
    // Single range check instead of two comparisons
    if ((raster - 48) < 200) {  // Equivalent to raster >= 48 && raster < 248
        if (raster == 0x30) {
            if (!vicii->video_logic.was_den_set_during_raster_30) {
                vicii->video_logic.was_den_set_during_raster_30 = 
                    (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;  // Avoid > 0 comparison
            }
        }
        vicii->video_logic.is_bad_line = vicii->video_logic.was_den_set_during_raster_30 &&
                                 ((raster & 0x07) == (vicii->registers.data[VICII_C1] & VICII_C1_YSCROLL));
    } else {
        vicii->video_logic.is_bad_line = false;
    }
}

// ========================================================================================
// SEQUENCER UNIT FUNCTIONS
// ========================================================================================

static inline void vic_sequencer_update_mode(vic_sequencer_unit_t* sequencer, uint8_t c1_reg, uint8_t c2_reg) {
    sequencer->graphics_mode = ((c1_reg & (VICII_C1_ECM | VICII_C1_BMM)) |
                               (c2_reg & VICII_C2_MCM)) >> 4;
}

static inline void vic_sequencer_update_colors(vicii_common_t* vicii) {
    vic_sequencer_unit_t* sequencer = &vicii->sequencer;
    vic_registers_unit_t* regs = &vicii->registers;
    
    // Update color palette based on graphics mode and background colors
    switch (sequencer->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            sequencer->colors[0].color = regs->data[VICII_B0C] & 0x0F;
            break;
        case VICII_GM_MULTICOLOR_TEXT:
            sequencer->colors[0].color = regs->data[VICII_B0C] & 0x0F;
            sequencer->colors[1].color = regs->data[VICII_B1C] & 0x0F;
            sequencer->colors[2].color = regs->data[VICII_B2C] & 0x0F;
            break;
        case VICII_GM_MULTICOLOR_BITMAP:
            sequencer->colors[0].color = regs->data[VICII_B0C] & 0x0F;
            break;
        default:
            for (int i = 0; i < 5; i++) {
                sequencer->colors[i].color = VICII_COLOR_BLACK;
            }
            break;
    }
}

// ========================================================================================
// BORDER UNIT FUNCTIONS
// ========================================================================================

static inline void vic_border_update_limits(vic_border_unit_t* border, uint8_t c1_reg, uint8_t c2_reg) {
    border->border_top = (c1_reg & VICII_C1_RSEL) ? 
        VICII_BORDER_TOP_RSEL1 : VICII_BORDER_TOP_RSEL0;
    border->border_bottom = (c1_reg & VICII_C1_RSEL) ? 
        VICII_BORDER_BOTTOM_RSEL1 : VICII_BORDER_BOTTOM_RSEL0;
    border->border_left = (c2_reg & VICII_C2_CSEL) ? 
        VICII_BORDER_LEFT_CSEL1 : VICII_BORDER_LEFT_CSEL0;
    border->border_right = (c2_reg & VICII_C2_CSEL) ? 
        VICII_BORDER_RIGHT_CSEL1 : VICII_BORDER_RIGHT_CSEL0;
}

// Border flip-flop logic (Documentation section 3.9) - X coordinate rules only
static inline void vic_border_update_flip_flops_x(vic_border_unit_t* border, vic_timing_unit_t* timing, 
                                                  uint8_t c1_reg) {
    uint16_t raster = timing->raster_counter;
    uint16_t x_coord = timing->x_coordinate;
    bool den_set = (c1_reg & VICII_C1_DEN) != 0;
    
    // ""The flip flops are switched according to the following rules:""
    
    // Rule 1: ""If the X coordinate reaches the right comparison value, the main border flip flop is set.""
    if (x_coord == border->border_right) {
        border->main_border_flip_flop = true;
    }
    
    // Rules 4, 5, 6: Handle left coordinate checks only
    else if (x_coord == border->border_left) {
        // Rule 4: ""If the X coordinate reaches the left comparison value and the Y
        // coordinate reaches the bottom one, the vertical border flip flop is set.""
        if (raster == border->border_bottom) {
            border->vertical_border_flip_flop = true;
        }
        // Rule 5: ""If the X coordinate reaches the left comparison value and the Y
        // coordinate reaches the top one and the DEN bit in register $d011 is set,
        // the vertical border flip flop is reset.""
        else if (raster == border->border_top && den_set) {
            border->vertical_border_flip_flop = false;
        }
        
        // Rule 6: ""If the X coordinate reaches the left comparison value and the vertical
        // border flip flop is not set, the main flip flop is reset.""
        if (!border->vertical_border_flip_flop) {
            border->main_border_flip_flop = false;
        }
    }
}

// ========================================================================================
// REGISTER UNIT FUNCTIONS - Write Handlers (Called by register write)
// ========================================================================================

static inline void vic_registers_write_interrupt(vic_registers_unit_t* regs, uint8_t value) {
    // Only consider the 4 actually supported interrupt bits (IRST/IMBC/IMMC/ILP)
    value &= VICII_INTERRUPTS_MASK;
    // Fetch the current Interrupt Register value
    uint8_t ir = regs->data[VICII_IR];
    // Clear all '1' bits in the Interrupt Register
    ir &= ~value;
    // Always set the not-connected bits high
    ir |= VICII_IR_UNUSED; // TODO : Remove this now that reads use floating bus data?
    // Store the resulting bits
    regs->data[VICII_IR] = ir;
    // Note/TODO : Here, it's assumed that when all interrupt bits are cleared, the
    // IR_IRQ flag is untouched - it'll be cleared later, in vicii_common_handle_raster_interrupt()
}

// Register write function (uses all the above handlers)
void vic_registers_write(vicii_common_t* vicii, uint16_t address, uint8_t value) {
    uint8_t reg = address & VICII_REGS_MASK; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    // Notes:
    // * Some not-connected bits (marked with '-') are written anyway here,
    //   because determing the mask for those would only be slower, for no benefit
    //   (and these not-connected bits are turned into 1's in MaskBusRead anyway).
    // * Writes on 4 bit color registers ARE masked, to avoid having to do that in (often repeated) reads
    // * Instead of skipping writes to MxM and MxD, their reads are rerouted to MxM_2 and MxD_2
    // * Unused register indices 47..63 are written anyway here
    //   because avoiding those would only be slower, for no benefit

    
    // Mask color registers to 4 bits
    if (reg >= VICII_EC) { // $d020 (4 bits) Exterior color (Border)
        value &= 0x0F; // $d020 and up are colors - keep only lowest 4 bits
    }
    
    vicii->registers.data[reg] = value;
    
    // Unit-specific update handlers
    switch (reg) {
        case VICII_C1: // $d011 Control register 1
            // Update bad line condition when C1 changes (YSCROLL or DEN bit changes)
            vic_update_badline_condition(vicii);
            // Fall through to C2 case
        case VICII_C2: // $d016 Control register 2
            vic_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            vic_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            break;
        case VICII_MXE: // $d015 Sprite enabled x
            // Update sprite enabled state
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii->sprites.sprites[i].enabled = (value & (1 << i)) != 0;
            }
            break;
        case VICII_MXYE: // $d017 Sprite Y expansion x
            // ""Complex expansion flip flop logic per VIC-II documentation""
            // Optimized: set flip-flop state directly based on bit value
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                // Writing 0 sets flip-flop, writing 1 clears it (immediate effect)
                // Note: cycle 55 inversion is handled separately in vicii_common_cycle()
                vicii->sprites.sprites[i].expansion_flip_flop = !(value & (1 << i));
            }
            break;
        case VICII_MP: // AI $d018 Memory pointers
            vic_memory_update_mapping(&vicii->memory, value);
            break;
        case VICII_IR: // $d019 Interrupt Register
		    // Treat the latching Interrupt Register differently from the other registers
            vic_registers_write_interrupt(&vicii->registers, value);
            return;
        case VICII_MXDP: // $d01b Sprite data priority
            // Batch update sprite priorities
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii->sprites.sprites[i].priority = (value & (1 << i)) ? 
                    VICII_PRIORITY_SPRITE_BEHIND : VICII_PRIORITY_SPRITE_IN_FRONT;
            }
            break;
        case VICII_EC: // $d020 (4 bits) Exterior color (Border)
            vicii->border.border_pixel.color = value; // value already masked to 0x0F above
            // Fall through to B0C-B2C case
        case VICII_B0C: // $d021 (4 bits) Background color 0
        case VICII_B1C: // $d022 (4 bits) Background color 1
        case VICII_B2C: // $d023 (4 bits) Background color 2
            // Inline vic_border_update_color since priority is set only once during init
            vic_sequencer_update_colors(vicii);
            break;
        case VICII_MM0: // $d025 (4 bits) Sprite multicolor 0
        case VICII_MM1: // $d026 (4 bits) Sprite multicolor 1
            // Global sprite multicolor registers - update all sprites with pre-masked value
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                // MM0/MM1 are global multicolor registers, not per-sprite
            }
            break;
        default:
            // No special handling needed for other registers
            break;
    }
}

// Register read helper (used by register read)
static inline uint8_t read_clear(vic_registers_unit_t* regs, uint8_t reg) {
    uint8_t val = regs->data[reg];
    regs->data[reg] = 0;
    return val;
}

// Register read function (uses the above helper)
uint8_t vic_registers_read(vicii_common_t* vicii, uint16_t address) {
    uint8_t reg = address & VICII_REGS_MASK;
	// Used for "floating" bus state for subsequent unattached reads
    uint8_t data = ((c64_bus_t*)vicii->bus.bus)->data;
    
    // Fast path for most common registers
    switch (reg) {
        case VICII_C1:
            return (vicii->registers.data[VICII_C1] & 0x7F) |       //    17 $d011 Control register 1 
                   ((vicii->timing.raster_counter >> 1) & VICII_C1_RST8); //         bit 7 (RST8) reflects raster_counter bit 8 
        case VICII_RASTER:
            return vicii->timing.raster_counter & 0xFF;               //    18 $d012 Reflects raster_counter bits 0..7
        case VICII_C2:
            return vicii->registers.data[VICII_C2] | (data & 0xC0); //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
        case VICII_MP:
            return vicii->registers.data[VICII_MP] | (data & 0x01); //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
        case VICII_IR:
            return vicii->registers.data[VICII_IR] | (data & 0x70); //    25 $d019 | IRQ|  - |  - |  - | ILP|IMMC|IMBC|IRST| Interrupt register
        case VICII_IE:
            return vicii->registers.data[VICII_IE] | (data & 0xF0); //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
        case VICII_MXM:
            return read_clear(&vicii->registers, VICII_MXM_2);             //    30 $d01e Sprite-sprite collision is cleared on read
        case VICII_MXD:
            return read_clear(&vicii->registers, VICII_MXD_2);             //    31 $d01f Sprite-data collision is cleared on read
        default:
		    if (reg <= 29) {
                return vicii->registers.data[reg];                  //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
 		   } else if (reg <= 46) {
        		return vicii->registers.data[reg] | (data & 0xF0);  // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
		    } else {
		        return data;                                        // 47-63 $d02f-$d03f unattached registers (many docs say: give $ff on reading)
		    }
    }
}

// ========================================================================================
// TIMING UNIT FUNCTIONS
// ========================================================================================

void vic_timing_advance(vicii_common_t* vicii) {
    if (++vicii->timing.x_cycle >= vicii->timing.cycles_per_line) {
        // Reset line state - combine operations
        vicii->pixel.pixel_line_index = 0;
        vicii->timing.x_cycle = 0;
        vicii->timing.cycle_group = CYCLE_GROUP_LINE_START;
        
        if (++vicii->timing.raster_counter >= vicii->timing.total_lines) {
            if (!vicii->pixel.framebuffer || vicii->timing.raster_counter >= vicii->pixel.framebuffer_height) {
                vicii->timing.raster_counter = 0;
                // Batch reset video logic state
                vicii->video_logic.was_den_set_during_raster_30 = false;
                vicii->video_logic.is_bad_line = false;
                vicii->video_logic.vcbase = 0;
            }
        }
        
        // VC/RC Rule 1: Reset VCBASE outside display range (Documentation section 3.7.2)
        // ""Once somewhere outside of the range of raster lines $30-$f7 (i.e.
        // outside of the Bad Line range), VCBASE is reset to zero. This is
        // presumably done in raster line 0, the exact moment cannot be determined
        // and is irrelevant.""
        if (vicii->timing.raster_counter < 0x30 || vicii->timing.raster_counter > 0xf7) {
            vicii->video_logic.vcbase = 0;
        }
        
        // Reset refresh counter in raster line 0 (Documentation section 3.13)
        // ""The counter is reset to $ff in raster line 0""
        if (vicii->timing.raster_counter == 0) {
            vicii->video_logic.refresh_counter = 0xFF;
        }
    }
    
    // Ultra-fast cycle group lookup - single array access
    vicii->timing.cycle_group = cycle_group_table_pal[vicii->timing.x_cycle];
    
    // Update X coordinate for border logic (8 pixels per cycle)
    vicii->timing.x_coordinate = vicii->timing.x_cycle * 8;
    
    // Handle cycle-specific video logic updates
    switch (vicii->timing.x_cycle) {
        case 14:
            // VC/RC Rule 2: Handle cycle 14 updates (Documentation section 3.7.2)
            // ""In the first phase of cycle 14 of each line, VC is loaded from VCBASE
            // (VCBASE->VC) and VMLI is cleared. If there is a Bad Line Condition in
            // this phase, RC is also reset to zero.""
            vicii->video_logic.vc = vicii->video_logic.vcbase;
            vicii->video_logic.vmli = 0;
            if (vicii->video_logic.is_bad_line) {
                vicii->video_logic.rc = 0;
            }
            break;
        case 58:
            // VC/RC Rule 5: Handle cycle 58 updates (Documentation section 3.7.2)
            // ""In the first phase of cycle 58, the VIC checks if RC=7. If so, the video
            // logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE). If
            // the video logic is in display state afterwards (this is always the case
            // if there is a Bad Line Condition), RC is incremented.""
            if (vicii->video_logic.rc == 7) {
                vicii->video_logic.display_state = false;  // Go to idle state
                vicii->video_logic.vcbase = vicii->video_logic.vc;  // VC->VCBASE
            }
            
            // If still in display state after the check, increment RC
            if (vicii->video_logic.display_state) {
                vicii->video_logic.rc++;
            }
            break;
        case 63:
            // Border Rules 2 & 3: Y coordinate checks in cycle 63 (Documentation section 3.9)
            {
                uint16_t raster = vicii->timing.raster_counter;
                bool den_set = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
                
                // Rule 2: ""If the Y coordinate reaches the bottom comparison value in cycle 63, the
                // vertical border flip flop is set.""
                if (raster == vicii->border.border_bottom) {
                    vicii->border.vertical_border_flip_flop = true;
                }
                // Rule 3: ""If the Y coordinate reaches the top comparison value in cycle 63 and the
                // DEN bit in register $d011 is set, the vertical border flip flop is reset.""
                else if (raster == vicii->border.border_top && den_set) {
                    vicii->border.vertical_border_flip_flop = false;
                }
            }
            break;
    }
}

// ========================================================================================
// PIXEL UNIT FUNCTIONS - Basic pixel operations
// ========================================================================================

// Helper function to emit a single pixel
static inline void vic_pixel_emit_single(vic_pixel_unit_t* pixel, const vicii_pixel_t* pixel_data) {
    if (pixel->pixel_line_index < pixel->visible_pixels_per_line) {
        uint16_t idx = pixel->pixel_line_index;
        pixel->pixel_line_priority[idx] = pixel_data->priority;
        pixel->pixel_line_color[idx] = pixel_data->color;
        pixel->pixel_line_index++;
    }
}

static inline void vic_pixel_set_framebuffer(vic_pixel_unit_t* pixel, uint32_t* framebuffer, 
                               int width, int height) {
    pixel->framebuffer = framebuffer;
    pixel->framebuffer_width = width;
    pixel->framebuffer_height = height;
}

void vic_pixel_flush_line(vicii_common_t* vicii, uint32_t* palette, int y) {
    if (!vicii->pixel.framebuffer || !palette || y >= vicii->pixel.framebuffer_height) return;
    
    vic_pixel_unit_t* pixel = &vicii->pixel;  // Used multiple times - KEEP
    
    int row_address = y * pixel->framebuffer_width;
    uint8_t border_color_index = vicii->registers.data[VICII_EC] & 0x0F;  // Direct access - single use
    uint32_t border_color = palette[border_color_index];
    
    // Fill entire line with border color
    uint32_t* row_ptr = &pixel->framebuffer[row_address];
    for (int x = 0; x < pixel->framebuffer_width; x++) {
        row_ptr[x] = border_color;
    }
    
    // Copy VIC-II pixels if available
    int pixels_to_copy = (pixel->pixel_line_index < pixel->visible_pixels_per_line) ?
                        pixel->pixel_line_index : pixel->visible_pixels_per_line;
    
    if (pixel->pixel_line_color && pixels_to_copy > 0) {
        int offset_x = (pixel->framebuffer_width - pixel->visible_pixels_per_line) >> 1;
        
        for (int x = 0; x < pixels_to_copy; x++) {
            int fb_x = offset_x + x;
            if (fb_x >= 0 && fb_x < pixel->framebuffer_width) {
                uint8_t color_index = pixel->pixel_line_color[x] & 0x0F;
                row_ptr[fb_x] = palette[color_index];
            }
        }
    }
    
    pixel->pixel_line_index = 0;
}

// ========================================================================================
// PIXEL EMISSION FUNCTIONS - Using the common pixel emission helper
// ========================================================================================

static inline void vic_border_emit_pixels(vicii_common_t* vicii) {
    vic_pixel_emit_single(&vicii->pixel, &vicii->border.border_pixel);
}

// Graphics sequencer (Documentation section 3.7.3)
void vic_graphics_sequencer(vicii_common_t* vicii, uint8_t graphics_data) {
    if (vicii->pixel.pixel_line_index >= vicii->pixel.visible_pixels_per_line) return;
    
    // Reset on mode change or line start
    if (vicii->sequencer.graphics_mode != vicii->sequencer.last_mode || vicii->pixel.pixel_line_index == 0) {
        vicii->sequencer.shift_reg = 0;
        vicii->sequencer.xscroll_counter = 7; // Default XSCROLL
        vicii->sequencer.last_mode = vicii->sequencer.graphics_mode;
    }
    
    // Handle XSCROLL and data loading
    if (vicii->sequencer.xscroll_counter > 0) {
        vicii->sequencer.xscroll_counter--;
        vicii->sequencer.shift_reg <<= 1;
    } else {
        vicii->sequencer.shift_reg = graphics_data;
    }
    
    // Optimized pixel emission with fewer operations
    uint8_t color_index;
    
    // Use bit operations to determine multicolor mode and extract pixels
    bool is_multicolor = (vicii->sequencer.graphics_mode & VICII_MULTICOLOR_MODE_MASK) &&
                        ((vicii->sequencer.graphics_mode != VICII_GM_MULTICOLOR_TEXT) ||
                         (vicii->video_data.video_color_line[0] & 0x08)); // MC flag for text mode
    
    if (is_multicolor) {
        color_index = vicii->sequencer.shift_reg >> 6;  // Top 2 bits
        vicii->sequencer.shift_reg <<= 2;  // Shift by 2 for multicolor
    } else {
        color_index = (vicii->sequencer.shift_reg >> 7) << 2;  // Convert bit 7 to index 0 or 4
        vicii->sequencer.shift_reg <<= 1;  // Shift by 1 for standard
    }
    
    // Use the common pixel emission helper
    vic_pixel_emit_single(&vicii->pixel, &vicii->sequencer.colors[color_index]);
}

static inline void vic_sprite_emit_pixels(vicii_common_t* vicii, int sprite_index) {
    vic_sprite_unit_t* sprite = &vicii->sprites.sprites[sprite_index];
    
    if (!sprite->display_state) return;
    
    // Sprites overlay the CURRENT pixel position (graphics sequencer just advanced the index)
    uint16_t pixel_idx = vicii->pixel.pixel_line_index - 1;
    if (pixel_idx >= vicii->pixel.visible_pixels_per_line) return;
    
    // Check sprite pixel - transparent sprites don't affect display or collisions
    if (!(sprite->shift_reg & 0x800000)) return;
    
    // Get current pixel state for collision detection (Documentation section 3.8.2)
    vicii_priority_t curr_priority = vicii->pixel.pixel_line_priority[pixel_idx];
    
    // Sprite-sprite collision detection
    // ""A collision of sprites among themselves is detected as soon as two or more
    // sprite data sequencers output a non-transparent pixel""
    if (curr_priority == VICII_PRIORITY_SPRITE_IN_FRONT || 
        curr_priority == VICII_PRIORITY_SPRITE_BEHIND) {
        vicii->registers.data[VICII_MXM_2] |= (1 << sprite_index);
    }
    
    // Sprite-graphics collision detection  
    // ""A collision of sprites and other graphics data is detected as soon as one
    // or more sprite data sequencers output a non-transparent pixel and the
    // graphics data sequencer outputs a foreground pixel""
    if (curr_priority == VICII_PRIORITY_FOREGROUND) {
        vicii->registers.data[VICII_MXD_2] |= (1 << sprite_index);
    }
    
    // Priority check and pixel overwrite
    // ""The sprites have a rigid hierarchy among themselves: Sprite 0 has the
    // highest and sprite 7 the lowest priority""
    bool sprite_wins = false;
    if (sprite->priority == VICII_PRIORITY_SPRITE_IN_FRONT) {
        // Sprite in front of graphics
        sprite_wins = true;
    } else if (sprite->priority == VICII_PRIORITY_SPRITE_BEHIND) {
        // Sprite behind graphics - only wins over background
        sprite_wins = (curr_priority <= VICII_PRIORITY_BACKGROUND);
    }
    
    if (sprite_wins) {
        vicii_color_t sprite_color = vicii->registers.data[VICII_M0C + sprite_index];
        vicii->pixel.pixel_line_priority[pixel_idx] = sprite->priority;
        vicii->pixel.pixel_line_color[pixel_idx] = sprite_color; // already masked to 0x0F on write
    }
}

// Sprite sequencer (Documentation section 3.8)
void vic_sprite_sequencer(vicii_common_t* vicii) {
    // Process sprites in order 0..7 (sprite 0 = highest priority)
    for (int i = 0; i < VICII_NUM_SPRITES; ++i) {
        vic_sprite_emit_pixels(vicii, i);
    }
}

// ========================================================================================
// BUS UNIT FUNCTIONS
// ========================================================================================

// Memory access function (Documentation section 3.6.2)
void vic_memory_access(vicii_common_t* vicii, uint8_t access_type, uint8_t access_param) {
    if (!vicii->bus.bus) return;
    
    uint16_t address;
    uint8_t data = 0;
    
    switch (access_type) {
        case VIC_ACCESS_P:
            // p-access: ""To the sprite data pointers; 8 bytes after the end of the video matrix,
            // that select one out of 256 blocks of 64 bytes within the VIC address
            // space for each sprite.""
            address = vicii->memory.vm_base + 0x3F8 + access_param;
            data = vic_memory_read(vicii, address);
            vicii->sprites.sprites[access_param].data_pointer = data;
            break;
        case VIC_ACCESS_S:
            // s-access: ""To the sprite data; an area of 63 bytes containing the pixel data of the
            // sprites which can be moved in steps of 64 bytes with the sprite data
            // pointers independently for each sprite.""
            if (vicii->sprites.sprites[access_param].mc_counter < 3) {
                address = vicii->sprites.sprites[access_param].data_pointer * 64 + 
                         vicii->sprites.sprites[access_param].mc_counter;
                data = vic_memory_read(vicii, address);
                vicii->sprites.sprites[access_param].data_buffer[vicii->sprites.sprites[access_param].mc_counter] = data;
                vicii->sprites.sprites[access_param].mc_counter++;
            }
            break;
        case VIC_ACCESS_C:
            // c-access: ""To the video matrix; an area of 1000 video addresses (40×25, 12 bits each)
            // that can be moved in 1KB steps within the 16KB address space of the VIC
            // with the bits VM10-VM13 of register $d018. It stores the character codes
            // and their color for the text modes and some of the color information of
            // 8×8 pixel blocks for the bitmap modes. The Color RAM is part of the
            // video matrix, it delivers the upper 4 bits of the 12 bit matrix.""
            {
                c64_bus_t* bus = (c64_bus_t*)vicii->bus.bus;
                // FIRST φ PHASE (φ2 low): VIC accesses Color RAM simultaneously with video matrix
                // ""The VIC has a 12 bit wide data bus over which the VIC accesses the memory. The
                // lower 8 bits are connected to the main memory and the processor data bus, the upper 4 bits are
                // connected to a special 4 bit wide static memory (1024 addresses, A0-A9) used for storing color
                // information, the Color RAM.""
                address = 0xD800 + vicii->video_logic.vc;
                // TODO : What about = 0xD800 + (vicii->video_counter & 0x3FF) + access_param;
                // Color RAM uses same addressing as character data (lower 10 bits)
                data = bus->read_callbacks[ACID_COLORRAM_D8].read(
                    bus->read_callbacks[ACID_COLORRAM_D8].context, address);
                vicii->video_data.video_color_line[vicii->video_logic.vmli] = data & 0x0F; // Upper 4 bits of 12-bit matrix
                
                // SAME φ PHASE: Video matrix access (lower 8 bits of 12-bit matrix)
                // ""The VIC accesses in the first phase (φ2 low), the processor in the second phase (φ2 high)""
                address = vicii->memory.vm_base;
                data = vic_memory_read(vicii, address);
                bus->data = data; // Set bus data for next access
                vicii->video_data.video_matrix_line[vicii->video_logic.vmli] = data; // Lower 8 bits of 12-bit matrix
            }
            // Fall through to g-access case
        case VIC_ACCESS_G:
            // g-access follows: ""In idle state, only g-accesses occur. The access is always to address $3fff""
            // ""In display state, c- and g-accesses take place, the addresses and interpretation of the data depend on the selected display mode""
            {
                uint8_t char_code = data; // Character code from c-access
                
                if (vicii->video_logic.display_state) {
                    // Display state: g-access address calculation (Documentation section 3.7.3.1)
                    if (vicii->registers.data[VICII_C1] & VICII_C1_BMM) {
                        // Bitmap mode: ""CB13| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|""
                        address = vicii->memory.cb_base | 
                                   ((vicii->video_logic.vc & 0x3FF) << 3) | 
                                   (vicii->video_logic.rc & 0x07);
                    } else {
                        // Text mode: ""CB13|CB12|CB11| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|""
                        address = vicii->memory.cb_base | 
                                   (char_code << 3) | 
                                   (vicii->video_logic.rc & 0x07);
                    }
                } else {
                    // Idle state: ""The access is always to address $3fff ($39ff when the ECM bit in register $d016 is set)""
                    address = (vicii->registers.data[VICII_C1] & VICII_C1_ECM) ? 0x39ff : 0x3fff;
                }
                
                // NEXT φ PHASE or SAME CYCLE: g-access reads character/bitmap data or idle data
                // ""Bad Line Condition... the VIC also needs the bus sometimes during the second phase.
                // In this case, BA goes low three cycles before the VIC access. After that,
                // AEC remains low during the second phase and the VIC performs the accesses.""
                uint8_t graphics_data = vic_memory_read(vicii, address);
                
                // Process graphics data for sequencer
                vic_graphics_sequencer(vicii, graphics_data);
                
                data = graphics_data; // Return graphics data on bus
            }
            break;
        case VIC_ACCESS_REFRESH:
            // r-access: ""Accesses for refreshing the dynamic RAM, 5 read accesses per raster
            // line. The VIC does five read accesses in every raster line for the refresh of the
            // dynamic RAM. An 8 bit refresh counter (REF) is used to generate 256 DRAM
            // row addresses. The counter is reset to $ff in raster line 0 and decremented
            // by 1 after each refresh access.""
            if (vicii->enable_hardware_accurate_reads) {
                address = vicii->memory.vm_base | 0x3F00 | vicii->video_logic.refresh_counter;
                data = vic_memory_read(vicii, address);
            } else {
                // Skip read for performance - refresh counter still maintained for timing accuracy
                data = 0xFF; // Typical floating bus value
            }
            vicii->video_logic.refresh_counter--;  // Always decrement counter
            break;
        default:
            // i-access: ""Idle accesses. As described, the VIC accesses in every first clock phase
            // although there are some cycles in which no other of the above mentioned
            // accesses is pending. In this case, the VIC does an idle access; a read
            // access to video address $3fff (i.e. to $3fff, $7fff, $bfff or $ffff
            // depending on the VIC bank) of which the result is discarded.""
            if (vicii->enable_hardware_accurate_reads) {
                data = vic_memory_read(vicii, 0x3FFF);
            } else {
                // Skip read for performance - result discarded anyway per documentation
                data = 0xFF; // Typical floating bus value
            }
            break;
    }
    
    ((c64_bus_t*)vicii->bus.bus)->data = data;
}

// ========================================================================================
// MAIN CYCLE FUNCTION - High-level coordination of all units
// ========================================================================================

void vicii_common_cycle(vicii_common_t* vicii) {
    vic_cycle_group_t cycle_group = vicii->timing.cycle_group;  // Used multiple times - KEEP
    uint8_t access_type = VIC_ACCESS_IDLE;
    uint8_t access_param = 0;
    bool ba_low = false;
    uint8_t cycle = vicii->timing.x_cycle;
    
    // Determine bus access requirements based on cycle group
    switch (cycle_group) {
        case CYCLE_GROUP_SPRITE_PS_ACCESS:
            // Handle sprite access cycles (cycles 1-8)
            access_param = ((cycle - 1) >> 1) + 3;
            if (vicii->sprites.sprites[access_param].enabled) {  // Direct access
                access_type = (cycle & 1) ? VIC_ACCESS_P : VIC_ACCESS_S;
                ba_low = true;
            }
            break;
            
        case CYCLE_GROUP_REFRESH_ACCESS:
            access_type = VIC_ACCESS_REFRESH;
            break;
            
        case CYCLE_GROUP_BADLINE_SETUP:
            if (vicii->video_logic.is_bad_line) {  // Direct access
                ba_low = true;
            }
            break;
            
        case CYCLE_GROUP_CHAR_COLOR_ACCESS:
            if (vicii->video_logic.is_bad_line) {  // c-access happens on bad lines regardless of display state
                ba_low = true;
                access_type = VIC_ACCESS_C;
                access_param = cycle - 15;
            }
            break;
            
        case CYCLE_GROUP_LINE_END:
            // Handle late sprite access cycles (cycles 55-60)
            if (cycle >= 55 && cycle <= 60) {
                access_param = (cycle - 55) >> 1;
                if (vicii->sprites.sprites[access_param].enabled) {  // Direct access
                    access_type = (cycle & 1) ? VIC_ACCESS_P : VIC_ACCESS_S;
                    ba_low = true;
                }
            }
            
            // Cycle 55: Handle sprite Y expansion flip flop inversion (Documentation section 3.8.1)
            // ""If the MxYE bit is set in the first phase of cycle 55, the expansion
            // flip flop is inverted.""
            if (cycle == 55) {
                uint8_t mxye_reg = vicii->registers.data[VICII_MXYE];  // Direct access - single use
                for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                    if (mxye_reg & (1 << i)) {
                        // If MxYE bit is set in cycle 55, invert the expansion flip flop
                        vicii->sprites.sprites[i].expansion_flip_flop = !vicii->sprites.sprites[i].expansion_flip_flop;  // Direct access
                    }
                }
            }
            break;
            
        case CYCLE_GROUP_IDLE:
        case CYCLE_GROUP_LINE_START:
        default:
            // No special bus access needed
            break;
    }
    
    // Bus control - combine operations
    c64_bus_t* c64_bus = (c64_bus_t*)vicii->bus.bus;  // Used multiple times - KEEP
    if (ba_low) {
        // ""BA will then go low 3 cycles before the VIC takes over the bus completely
        // (3 cycles is the maximum number of successive write accesses of the 6510).
        // After 3 cycles, AEC stays low during the second clock phase so that the VIC can output its addresses.""
        c64_bus->control_lines &= ~BA_LINE;
        // Inlined vic_video_logic_handle_bad_line
        if (vicii->video_logic.is_bad_line) {  // Direct access
            vicii->video_logic.display_state = true;  // Direct access
        }
    } else {
        // ""The VIC accesses in the first phase (φ2 low), the processor in the second phase (φ2 high)""
        c64_bus->control_lines |= BA_LINE;
    }
    
    // Perform bus access
    if (access_type > VIC_ACCESS_REFRESH) {
        // ""AEC stays low during the second clock phase so that the VIC can output its addresses""
        // This blocks the CPU by tri-stating its address lines
        c64_bus->control_lines &= ~AEC_LINE;
        vic_memory_access(vicii, access_type, access_param);
        
        // VC/RC Rule 4: Increment VC and VMLI after g-access in display state (Documentation section 3.7.2)
        if (access_type == VIC_ACCESS_C) {
            // ""VC and VMLI are incremented after each g-access in display state.""
            if (vicii->video_logic.display_state) {
                vicii->video_logic.vc++;
                vicii->video_logic.vmli++;
            }
        }
    } else {
        // ""AEC is normally low during the first clock phase (φ2 low) and high during the
        // second phase so that the VIC can access the bus during the first phase and the 6510 during the second phase""
        c64_bus->control_lines |= AEC_LINE;
    }
    
    // Pixel processing based on cycle group
    if (vicii->pixel.framebuffer && vicii->timing.raster_counter < vicii->pixel.framebuffer_height) {
        int cycles_needed = (vicii->pixel.framebuffer_width + 7) >> 3;
        
        if (cycle < cycles_needed) {
            bool in_display_area = (vicii->timing.raster_counter < vicii->timing.total_lines) &&
                                  (cycle_group == CYCLE_GROUP_CHAR_COLOR_ACCESS) &&
                                  (vicii->timing.raster_counter >= vicii->border.border_top &&
                                   vicii->timing.raster_counter <= vicii->border.border_bottom);
            
            if (in_display_area) {
                // Graphics data now comes from g-access above, so don't pass dummy data
                // vic_graphics_sequencer(vicii, graphics_data) is called in g-access section
                vic_sprite_sequencer(vicii);
            } else {
                // Border display logic (Documentation section 3.9)
                // ""If it is set, the VIC displays the color stored in register $d020""
                if (vicii->border.main_border_flip_flop) {
                    vic_border_emit_pixels(vicii);
                } else {
                    // Display background graphics or sprites
                    vic_sprite_sequencer(vicii);
                }
            }
        }
    }
    
    // Advance timing and update bad line logic
    vic_timing_advance(vicii);
    vic_update_badline_condition(vicii);
    
    // Update border flip-flops for X coordinate rules only (Documentation section 3.9)
    // Y coordinate rules (2 & 3) are handled in cycle 63 switch case above
    vic_border_update_flip_flops_x(&vicii->border, &vicii->timing, 
                                  vicii->registers.data[VICII_C1]);
    
    // Flush pixel line if end of line
    if (vicii->timing.x_cycle == 0 && vicii->pixel.framebuffer) {
        vic_pixel_flush_line(vicii, vicii_common_get_default_palette(), 
                           (vicii->timing.raster_counter - 1) % vicii->timing.total_lines);
    }
}

// ========================================================================================
// INITIALIZATION FUNCTIONS
// ========================================================================================

static inline void vicii_common_initialize(vicii_common_t* vicii) {
    // Zero all units
    memset(&vicii->registers, 0, sizeof(vic_registers_unit_t));
    memset(&vicii->timing, 0, sizeof(vic_timing_unit_t));
    memset(&vicii->video_logic, 0, sizeof(vic_video_logic_unit_t));
    memset(&vicii->video_data, 0, sizeof(vic_video_data_unit_t));
    memset(&vicii->sequencer, 0, sizeof(vic_sequencer_unit_t));
    memset(&vicii->border, 0, sizeof(vic_border_unit_t));
    memset(&vicii->memory, 0, sizeof(vic_memory_unit_t));
    memset(&vicii->sprites, 0, sizeof(vic_sprites_unit_t));
    memset(&vicii->pixel, 0, sizeof(vic_pixel_unit_t));
    memset(&vicii->bus, 0, sizeof(vic_bus_unit_t));
    
    // Set default register values
    vicii->registers.data[VICII_C1] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                            (VICII_C1_YSCROLL & 3); // 155:Display ENable,25-row  
    vicii->registers.data[VICII_MXE] = 0;  // All sprites disabled
    vicii->registers.data[VICII_C2] = VICII_C2_CSEL; // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
    vicii->registers.data[VICII_MP] = VICII_MP_CB12 | VICII_MP_VM10; // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
    vicii->registers.data[VICII_IR] = VICII_IR_UNUSED; // See BusWrite; Always set the unused bits high
    
    // Set default colors
    vicii->registers.data[VICII_EC] = VICII_COLOR_LIGHT_BLUE; // 14: Border Color
    vicii->registers.data[VICII_B0C] = VICII_COLOR_BLUE; // 6: Background Color 0
    vicii->registers.data[VICII_B1C] = VICII_COLOR_WHITE; // 1: Background Color 1
    vicii->registers.data[VICII_B2C] = VICII_COLOR_RED; // 2: Background Color 2
    vicii->registers.data[VICII_B3C] = VICII_COLOR_CYAN; // 3: Background Color 3
    vicii->registers.data[VICII_MM0] = VICII_COLOR_PURPLE; // 4: Sprite Multicolor 0
    vicii->registers.data[VICII_MM1] = VICII_COLOR_BLACK; // 0: Sprite Multicolor 1
    vicii->registers.data[VICII_M0C] = VICII_COLOR_WHITE; // 1: Sprite Color 0
    vicii->registers.data[VICII_M1C] = VICII_COLOR_RED; // 2: Sprite Color 1
    vicii->registers.data[VICII_M2C] = VICII_COLOR_CYAN; // 3: Sprite Color 2
    vicii->registers.data[VICII_M3C] = VICII_COLOR_PURPLE; // 4: Sprite Color 3
    vicii->registers.data[VICII_M4C] = VICII_COLOR_GREEN; // 5: Sprite Color 4
    vicii->registers.data[VICII_M5C] = VICII_COLOR_BLUE; // 6: Sprite Color 5
    vicii->registers.data[VICII_M6C] = VICII_COLOR_YELLOW; // 7: Sprite Color 6
    vicii->registers.data[VICII_M7C] = VICII_COLOR_MEDIUM_GREY; // 12: Sprite Color 7
    
    // Initialize color priorities
    vicii->sequencer.colors[0].priority = VICII_PRIORITY_BACKGROUND; // "00" / "0" Use in both MC modes
    vicii->sequencer.colors[1].priority = VICII_PRIORITY_BACKGROUND; // "01" Used in EmitMCPixel()
    vicii->sequencer.colors[2].priority = VICII_PRIORITY_FOREGROUND; // "10"
    vicii->sequencer.colors[3].priority = VICII_PRIORITY_FOREGROUND; // "11"
    vicii->sequencer.colors[4].priority = VICII_PRIORITY_FOREGROUND; // "1" Used in EmitPixel()
    
    // Initialize sequencer
    vicii->sequencer.shift_reg = 0;
    vicii->sequencer.xscroll_counter = 0;
    vicii->sequencer.last_mode = 0xFF;
    
    // Update units based on register values
    vic_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    vic_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    // Initialize border priority once (color will be updated by register writes)
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = vicii->registers.data[VICII_EC] & 0x0F;
    // Initialize border flip-flops (Documentation section 3.9)
    vicii->border.main_border_flip_flop = true;      // Start with border on
    vicii->border.vertical_border_flip_flop = true;  // Start with vertical border on
    vic_memory_update_mapping(&vicii->memory, vicii->registers.data[VICII_MP]);
    
    // Initialize refresh counter (Documentation section 3.13)
    vicii->video_logic.refresh_counter = 0xFF;
    
    // Set timing defaults (should be set by wrapper create functions)
    vicii->timing.cycles_per_line = VICII_PAL_CYCLES_PER_LINE;
    vicii->timing.total_lines = VICII_PAL_TOTAL_LINES;
    vicii->pixel.visible_pixels_per_line = VICII_PAL_VISIBLE_PIXELS;
    
    // Allocate pixel buffers
    if (vicii->pixel.visible_pixels_per_line > 0) {
        vicii->pixel.pixel_line_priority = malloc(vicii->pixel.visible_pixels_per_line * sizeof(vicii_priority_t));
        vicii->pixel.pixel_line_color = malloc(vicii->pixel.visible_pixels_per_line * sizeof(uint32_t));
        
        memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->pixel.visible_pixels_per_line);
        for (int i = 0; i < vicii->pixel.visible_pixels_per_line; i++) {
            vicii->pixel.pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}

// ========================================================================================
// LEGACY COMPATIBILITY WRAPPERS (Highest level functions)
// ========================================================================================

vicii_common_t* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t)) {
    vicii_common_t* vicii = (vicii_common_t*)calloc(1, sizeof(vicii_common_t));
    if (!vicii) return NULL;
    
    vicii->desc = desc;
    vicii->bus.bank_change = bank_change;
    vicii_common_initialize(vicii);
    
    return vicii;
}

void vicii_common_system_destroy(void* chip) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    if (vicii) {
        free(vicii->pixel.pixel_line_priority);
        free(vicii->pixel.pixel_line_color);
        free(vicii);
    }
}

void vicii_common_bus_attach(void* chip, void* bus) {
    ((vicii_common_t*)chip)->bus.bus = bus;
}

uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    return vic_registers_read(vicii, address);
}

void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vic_registers_write(vicii, address, value);
}

void vicii_common_bank_change(void* chip, uint8_t bank) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    bank = 3 - (bank & 0x03);  // Invert bank
    vicii->memory.bank = bank;
    
    const uint16_t bank_bases[] = {
        VICII_BANK_0_BASE, VICII_BANK_1_BASE,
        VICII_BANK_2_BASE, VICII_BANK_3_BASE
    };
    vicii->memory.bank_base = bank_bases[bank];
    vic_memory_update_mapping(&vicii->memory, vicii->registers.data[VICII_MP]);
}

void vicii_common_set_framebuffer(vicii_common_t* vicii, uint32_t* framebuffer, int width, int height) {
    vic_pixel_set_framebuffer(&vicii->pixel, framebuffer, width, height);
    
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = VICII_COLOR_LIGHT_BLUE;
    
    if (vicii->pixel.pixel_line_color && vicii->pixel.visible_pixels_per_line > 0) {
        memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->pixel.visible_pixels_per_line);
        for (int i = 0; i < vicii->pixel.visible_pixels_per_line; i++) {
            vicii->pixel.pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}