#ifndef VICII_COMMON_H
#define VICII_COMMON_H

#include "../../core/chip.h"
#include <stdint.h>
#include <stdbool.h>

// VIC-II Register Constants
#define VICII_REGS_SIZE 64
#define VICII_REGS_MASK 63

// Register indices
#define VICII_M0X     0   // $d000 X coordinate sprite 0
#define VICII_M0Y     1   // $d001 Y coordinate sprite 0
#define VICII_M1X     2   // $d002 X coordinate sprite 1
#define VICII_M1Y     3   // $d003 Y coordinate sprite 1
#define VICII_M2X     4   // $d004 X coordinate sprite 2
#define VICII_M2Y     5   // $d005 Y coordinate sprite 2
#define VICII_M3X     6   // $d006 X coordinate sprite 3
#define VICII_M3Y     7   // $d007 Y coordinate sprite 3
#define VICII_M4X     8   // $d008 X coordinate sprite 4
#define VICII_M4Y     9   // $d009 Y coordinate sprite 4
#define VICII_M5X     10  // $d00a X coordinate sprite 5
#define VICII_M5Y     11  // $d00b Y coordinate sprite 5
#define VICII_M6X     12  // $d00c X coordinate sprite 6
#define VICII_M6Y     13  // $d00d Y coordinate sprite 6
#define VICII_M7X     14  // $d00e X coordinate sprite 7
#define VICII_M7Y     15  // $d00f Y coordinate sprite 7
#define VICII_MX8     16  // $d010 MSB X coordinate sprite i
#define VICII_C1      17  // $d011 Control register 1
#define VICII_RASTER  18  // $d012 Raster counter
#define VICII_LPX     19  // $d013 Light pen X
#define VICII_LPY     20  // $d014 Light pen Y
#define VICII_MXE     21  // $d015 Sprite enabled x
#define VICII_C2      22  // $d016 Control register 2
#define VICII_MXYE    23  // $d017 Sprite Y expansion x
#define VICII_MP      24  // $d018 Memory pointers
#define VICII_IR      25  // $d019 Interrupt Register
#define VICII_IE      26  // $d01a Interrupt Enabled
#define VICII_MXDP    27  // $d01b Sprite data priority x
#define VICII_MXMC    28  // $d01c Sprite multicolor x select
#define VICII_MXXE    29  // $d01d Sprite X expansion x
#define VICII_MXM     30  // $d01e Sprite-sprite collision x
#define VICII_MXD     31  // $d01f Sprite-data collision x
#define VICII_EC      32  // $d020 Exterior color (Border)
#define VICII_B0C     33  // $d021 Background color 0
#define VICII_B1C     34  // $d022 Background color 1
#define VICII_B2C     35  // $d023 Background color 2
#define VICII_B3C     36  // $d024 Background color 3
#define VICII_MM0     37  // $d025 Sprite multicolor 0
#define VICII_MM1     38  // $d026 Sprite multicolor 1
#define VICII_M0C     39  // $d027 Color sprite 0
#define VICII_M1C     40  // $d028 Color sprite 1
#define VICII_M2C     41  // $d029 Color sprite 2
#define VICII_M3C     42  // $d02a Color sprite 3
#define VICII_M4C     43  // $d02b Color sprite 4
#define VICII_M5C     44  // $d02c Color sprite 5
#define VICII_M6C     45  // $d02d Color sprite 6
#define VICII_M7C     46  // $d02e Color sprite 7

// Control register 1 ($d011) bit masks
#define VICII_C1_YSCROLL  0x07  // Smooth Scroll to Y Pos
#define VICII_C1_RSEL     0x08  // Select 24/25 Row Text Display
#define VICII_C1_DEN      0x10  // Display Enable
#define VICII_C1_BMM      0x20  // Bitmap Mode
#define VICII_C1_ECM      0x40  // Extended Color Mode
#define VICII_C1_RST8     0x80  // Raster bit 8

// Control register 2 ($d016) bit masks
#define VICII_C2_XSCROLL  0x07  // Smooth Scroll to X Pos
#define VICII_C2_CSEL     0x08  // Select 38/40 Column Text Display
#define VICII_C2_MCM      0x10  // Multi-Color Mode
#define VICII_C2_RES      0x20  // Reserved (always 0)

// Memory pointers ($d018) bit masks
#define VICII_MP_CB11     0x02  // Character Dot-Data Base Address
#define VICII_MP_CB12     0x04
#define VICII_MP_CB13     0x08
#define VICII_MP_VM10     0x10  // Video Matrix Base Address
#define VICII_MP_VM11     0x20
#define VICII_MP_VM12     0x40
#define VICII_MP_VM13     0x80

// Interrupt Register ($d019) bit masks
#define VICII_IR_IRST     0x01  // Raster Compare occurred
#define VICII_IR_IMBC     0x02  // Sprite-data Collision occurred
#define VICII_IR_IMMC     0x04  // Sprite to Sprite Collision occurred
#define VICII_IR_ILP      0x08  // Light-Pen occurred
#define VICII_IR_UNUSED   0x70  // Unused bits - always high
#define VICII_IR_IRQ      0x80  // Set on Any Enabled VIC IRQ Condition

// Interrupt Enabled ($d01a) bit masks
#define VICII_IE_ERST     0x01  // Raster interrupt enabled
#define VICII_IE_EMBC     0x02  // Sprite-data collision interrupt enabled
#define VICII_IE_EMMC     0x04  // Sprite-sprite collision interrupt enabled
#define VICII_IE_ELP      0x08  // Light pen interrupt enabled

// Graphics modes
#define VICII_GM_STANDARD_TEXT      0  // ECM/BMM/MCM=0/0/0
#define VICII_GM_MULTICOLOR_TEXT    1  // ECM/BMM/MCM=0/0/1
#define VICII_GM_STANDARD_BITMAP    2  // ECM/BMM/MCM=0/1/0
#define VICII_GM_MULTICOLOR_BITMAP  3  // ECM/BMM/MCM=0/1/1
#define VICII_GM_ECM_TEXT           4  // ECM/BMM/MCM=1/0/0
#define VICII_GM_INVALID_TEXT       5  // ECM/BMM/MCM=1/0/1
#define VICII_GM_INVALID_BITMAP1    6  // ECM/BMM/MCM=1/1/0
#define VICII_GM_INVALID_BITMAP2    7  // ECM/BMM/MCM=1/1/1

// Mode bitmasks
#define VICII_MULTICOLOR_MODE_MASK    1  // MCM=1
#define VICII_BITMAP_MODE_MASK        2  // BMM=1
#define VICII_EXTENDED_COLOR_MODE_MASK 4  // ECM=1

// Forward declarations for types
typedef enum vicii_color_e vicii_color_t;
typedef enum vicii_priority_e vicii_priority_t;
typedef struct vicii_pixel_s vicii_pixel_t;

// VIC-II Colors
enum vicii_color_e {
    VICII_COLOR_BLACK = 0,
    VICII_COLOR_WHITE = 1,
    VICII_COLOR_RED = 2,
    VICII_COLOR_CYAN = 3,
    VICII_COLOR_PURPLE = 4,
    VICII_COLOR_GREEN = 5,
    VICII_COLOR_BLUE = 6,
    VICII_COLOR_YELLOW = 7,
    VICII_COLOR_ORANGE = 8,
    VICII_COLOR_BROWN = 9,
    VICII_COLOR_LIGHT_RED = 10,
    VICII_COLOR_DARK_GREY = 11,
    VICII_COLOR_MEDIUM_GREY = 12,
    VICII_COLOR_LIGHT_GREEN = 13,
    VICII_COLOR_LIGHT_BLUE = 14,
    VICII_COLOR_LIGHT_GREY = 15
};

// Priority levels for sprite/background collision
enum vicii_priority_e {
    VICII_PRIORITY_BACKGROUND = 0,
    VICII_PRIORITY_SPRITE_BEHIND = 1,
    VICII_PRIORITY_FOREGROUND = 2,
    VICII_PRIORITY_SPRITE_IN_FRONT = 3,
    VICII_PRIORITY_BORDER = 4
};

// Pixel structure
struct vicii_pixel_s {
    vicii_priority_t priority;
    vicii_color_t color;
};

// VIC-II timing constants
#define VICII_PAL_CYCLES_PER_LINE    63
#define VICII_PAL_TOTAL_LINES        312
#define VICII_PAL_VISIBLE_PIXELS     403
#define VICII_NTSC_CYCLES_PER_LINE   65
#define VICII_NTSC_TOTAL_LINES       262
#define VICII_NTSC_VISIBLE_PIXELS    411

// Border limits
#define VICII_BORDER_TOP_RSEL1       51
#define VICII_BORDER_TOP_RSEL0       55
#define VICII_BORDER_BOTTOM_RSEL0    247
#define VICII_BORDER_BOTTOM_RSEL1    251
#define VICII_BORDER_LEFT_CSEL1      24
#define VICII_BORDER_LEFT_CSEL0      32
#define VICII_BORDER_RIGHT_CSEL0     336
#define VICII_BORDER_RIGHT_CSEL1     344

// Interrupt mask
#define VICII_INTERRUPTS_MASK (VICII_IR_ILP | VICII_IR_IMMC | VICII_IR_IMBC | VICII_IR_IRST)

// Number of sprites
#define VICII_NUM_SPRITES 8

// VIC-II Sprite structure
typedef struct {
    uint8_t x_pos;
    uint8_t y_pos;
    bool enabled;
    bool multicolor;
    bool x_expand;
    bool y_expand;
    bool priority;
    vicii_color_t color;
    uint8_t data_pointer;
    uint8_t mcbase;  // multicolor base
    uint8_t mc;      // multicolor counter
} vicii_sprite_t;

// Main VIC-II state structure
typedef struct {
    chip_descriptor_t* desc;
    uint8_t registers[VICII_REGS_SIZE + 2];  // +2 for shadow collision registers
    
    // Timing state
    uint8_t x_cycle;
    uint16_t x_coordinate;
    uint16_t raster_counter;
    
    // Video logic state
    bool video_logic_display_state;
    bool bad_line;
    bool was_den_set_during_raster_30;
    bool vertical_border_flip_flop;
    
    // Video counters
    uint16_t vc_base;     // Video Counter Base (10 bits)
    uint16_t vc;          // Video Counter (10 bits)
    uint8_t rc;           // Row Counter (3 bits)
    uint8_t vmli;         // Video Matrix Line Index (6 bits)
    
    // Video data buffers
    uint8_t video_matrix_line[40];
    vicii_color_t video_color_line[40];
    
    // Graphics mode and colors
    uint8_t graphics_mode;
    vicii_pixel_t colors[5];
    vicii_pixel_t border_pixel;
    
    // Border limits (updated based on RSEL/CSEL)
    uint16_t border_top;
    uint16_t border_bottom;
    uint16_t border_left;
    uint16_t border_right;
    
    // Sprites
    vicii_sprite_t sprites[VICII_NUM_SPRITES];
    
    // Light pen state
    bool lp_edge_detected;
    
    // Bus and memory access
    void* bus;
    uint8_t bank;
    void (*bank_change)(void* context, uint8_t bank);
    
    // VIC-II memory mapping
    vicii_memory_map_t memory_map;
    
    // Per-standard timing (set by wrapper create functions)
    uint8_t cycles_per_line;
    uint16_t total_lines;
    uint16_t visible_pixels_per_line;
    
    // Frame statistics
    uint32_t frame_count;
    
    // Display output (for pixel rendering)
    vicii_priority_t* pixel_line_priority;
    uint32_t* pixel_line_color;
    uint16_t pixel_line_index;
} vicii_common_t;

// Factory and lifecycle
vicii_common_t* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t));
void vicii_common_system_destroy(void* chip);

// Bus attachment
void vicii_common_bus_attach(void* chip, void* bus);

// Register I/O
uint8_t vicii_common_registers_read(void* chip, uint16_t address);
void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void vicii_common_bank_change(void* chip, uint8_t bank);

// Cycle logic, parameterized by cycles_per_line and total_lines
void vicii_common_cycle(vicii_common_t* vicii);

// Internal helper functions
void vicii_common_initialize(vicii_common_t* vicii);
void vicii_common_update_graphics_mode(vicii_common_t* vicii);
void vicii_common_update_border_limits(vicii_common_t* vicii);
void vicii_common_update_bad_line(vicii_common_t* vicii);
void vicii_common_handle_raster_interrupt(vicii_common_t* vicii);

// VIC-II memory access functions
uint8_t vicii_memory_read_cycle(vicii_common_t* vicii, uint16_t address);
void vicii_update_bank_mapping(vicii_common_t* vicii, uint8_t bank);

// Character and graphics access functions
void vicii_common_c_access(vicii_common_t* vicii);
void vicii_common_g_access(vicii_common_t* vicii);

// Pixel emission functions
void vicii_common_emit_border_pixels(vicii_common_t* vicii);
void vicii_common_emit_graphics_pixels(vicii_common_t* vicii, uint8_t data);

// VIC-II banking constants
#define VICII_BANK_0_BASE    0x0000  // Bank 0: $0000-$3FFF
#define VICII_BANK_1_BASE    0x4000  // Bank 1: $4000-$7FFF
#define VICII_BANK_2_BASE    0x8000  // Bank 2: $8000-$BFFF
#define VICII_BANK_3_BASE    0xC000  // Bank 3: $C000-$FFFF

// VIC-II memory mapping structure
typedef struct {
    uint16_t bank_base;         // Base address of current 16KB VIC bank
    uint16_t video_matrix_base; // Video matrix base within VIC bank
    uint16_t char_base;         // Character ROM base within VIC bank
    bool char_rom_enabled;      // Whether character ROM is accessible
} vicii_memory_map_t;

#endif // VICII_COMMON_H