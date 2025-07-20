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

// MxM and MxD storage is moved outside the 0..63 range
#define VICII_MXM_2   64  // Shadow register for MxM $d01e Sprite-sprite collision x
#define VICII_MXD_2   65  // Shadow register for MxD $d01f Sprite-data collision x

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
typedef enum vicii_color_e vicii_color_t;

// Priority levels for sprite/background collision
enum vicii_priority_e {
    VICII_PRIORITY_BACKGROUND = 0,
    VICII_PRIORITY_SPRITE_BEHIND = 1,
    VICII_PRIORITY_FOREGROUND = 2,
    VICII_PRIORITY_SPRITE_IN_FRONT = 3,
    VICII_PRIORITY_BORDER = 4
};
typedef enum vicii_priority_e vicii_priority_t;

// Pixel structure
struct vicii_pixel_s {
    vicii_priority_t priority;
    vicii_color_t color;
};
typedef struct vicii_pixel_s vicii_pixel_t;

// MOS6569 PAL VIC-II timing constants 
#define VICII_PAL_CYCLES_PER_LINE    63 // aka MOS6569_CYCLES_PER_LINE
#define VICII_PAL_TOTAL_LINES        312 // aka MOS6569_TOTAL_LINES / VIC_LINES_PER_FRAME 
#define VICII_PAL_VISIBLE_PIXELS     403

// MOS6567 NTSC VIC-II timing constants 
#define VICII_NTSC_CYCLES_PER_LINE   65 // aka MOS6567_CYCLES_PER_LINE
#define VICII_NTSC_TOTAL_LINES       262 // aka MOS6567_TOTAL_LINES
#define VICII_NTSC_VISIBLE_PIXELS    411

// #define VIC_CYCLES_PER_FRAME (VIC_CYCLES_PER_LINE * VIC_LINES_PER_FRAME)

// Border limits
#define VICII_BORDER_TOP_RSEL1       51
#define VICII_BORDER_TOP_RSEL0       55
#define VICII_BORDER_BOTTOM_RSEL0    247
#define VICII_BORDER_BOTTOM_RSEL1    251
#define VICII_BORDER_LEFT_CSEL1      24
#define VICII_BORDER_LEFT_CSEL0      31
#define VICII_BORDER_RIGHT_CSEL0     335
#define VICII_BORDER_RIGHT_CSEL1     344

// VIC-II access types (Documentation section 3.6.2)
#define VIC_ACCESS_IDLE         0  // i-access - idle access to $3fff
#define VIC_ACCESS_REFRESH      1  // r-access - DRAM refresh
#define VIC_ACCESS_P            2  // p-access - sprite data pointers
#define VIC_ACCESS_S            3  // s-access - sprite data
#define VIC_ACCESS_C            4  // c-access - video matrix and Color RAM
#define VIC_ACCESS_G            5  // g-access - character generator or bitmap

// Interrupt mask
#define VICII_INTERRUPTS_MASK (VICII_IR_ILP | VICII_IR_IMMC | VICII_IR_IMBC | VICII_IR_IRST)

// Number of sprites
#define VICII_NUM_SPRITES 8

// ========================================================================================
// CHIP CONFIGURATION STRUCTURE
// ========================================================================================

// Configuration struct for different MOS 656x chip variants
typedef struct {
    // Timing parameters
    uint8_t cycles_per_line;
    uint16_t total_lines;
    uint16_t pixels_per_line;
    uint16_t visible_pixels_per_line;
    uint16_t base_offset;
    
    // Border coordinates for RSEL=0 (24-row mode)
    uint16_t border_top_rsel0;
    uint16_t border_bottom_rsel0;
    
    // Border coordinates for RSEL=1 (25-row mode)
    uint16_t border_top_rsel1;
    uint16_t border_bottom_rsel1;
    
    // Border coordinates for CSEL=0 (38-column mode)
    uint16_t border_left_csel0;
    uint16_t border_right_csel0;
    
    // Border coordinates for CSEL=1 (40-column mode)
    uint16_t border_left_csel1;
    uint16_t border_right_csel1;
    
    // Display area bounds
    uint16_t display_start_x;
    uint16_t display_end_x;
    uint16_t framebuffer_start_x;
    uint16_t framebuffer_end_x;
    
    // Chip name for debugging
    const char* chip_name;
} vicii_chip_config_t;

// ========================================================================================
// CYCLE TABLE ENTRY TYPE (needed for timing unit)
// ========================================================================================
typedef struct vicii_s vicii_t;
typedef uint8_t (*vicii_cycle_func_t)(vicii_t*, int);

typedef struct vicii_cycle_entry_t {
    vicii_cycle_func_t func;
    int param;
} vicii_cycle_entry_t;

// ========================================================================================
// TOPIC-SPECIFIC UNIT STRUCTURES
// ========================================================================================

// Register Unit - All VIC-II register state
typedef struct {
    uint8_t data[VICII_REGS_SIZE + 2];  // +2 for shadow collision registers
} vicii_registers_unit_t;

// Timing Unit - All timing-related state
typedef struct {
    // Primary counter is x_coordinate (pixel-level precision)
    uint16_t x_coordinate;               // Primary counter: 0-511 (9-bit, wraps)
    uint16_t display_x_coordinate;       // Display coordinate with 12-pixel pipeline delay

    // Derived counters
    uint8_t x_cycle;                     // Derived from x_coordinate (x_coordinate / 8)
    uint16_t raster_counter;             // Current raster line (0-total_lines)
    uint32_t frame_count;                // Frame counter

    // Precalculated timing parameters
    const vicii_cycle_entry_t* cycle_table; // Precalculated cycle table pointer
    uint16_t base_offset;                // Precalculated x_coordinate base offset
    uint16_t pixels_per_line;            // Total pixels per line (504 PAL, 520 NTSC)
    uint8_t cycles_per_line;             // Cycles per line (63 PAL, 65 NTSC)
    uint16_t total_lines;                // Total lines per frame
} vicii_timing_unit_t;

// Video Logic Unit - Display state and bad line logic (Documentation section 3.7)
typedef struct {
    bool display_state;
    bool is_bad_line;
    bool was_den_set_during_raster_30;
    uint16_t vcbase;     // VCBASE - Video Counter Base (10 bits) (Documentation section 3.7.2)
    uint16_t vc;         // VC - Video Counter (10 bits) (Documentation section 3.7.2)
    uint8_t rc;          // RC - Row Counter (3 bits) (Documentation section 3.7.2)
    uint8_t vmli;        // VMLI - Video Matrix Line Index (6 bits) (Documentation section 3.7.2)
    uint8_t refresh_counter; // REF - 8 bit refresh counter (Documentation section 3.13)
} vicii_video_logic_unit_t;

// Video Data Unit - Character and color line buffers
typedef struct {
    uint8_t video_matrix_line[40];
    vicii_color_t video_color_line[40];
} vicii_video_data_unit_t;

// Graphics Sequencer Unit - Graphics pixel generation state
typedef struct {
    uint8_t graphics_mode;    // Current graphics mode
    uint8_t last_mode;        // Last graphics mode for change detection
    uint8_t shift_reg;        // Graphics shift register
    uint8_t xscroll_counter;  // XSCROLL delay counter
    uint8_t char_index;       // Current character index (0-39)
    uint8_t pixel_in_char;    // Current pixel within character (0-7)
    vicii_pixel_t colors[5];  // Color palette for current mode
} vicii_sequencer_unit_t;

// Border Unit - Border generation and limits (Documentation section 3.9)
typedef struct {
    vicii_pixel_t border_pixel;
    
    // SCREEN POSITION DECODES 6567 NTSC
    // NTSC: https://gist.githubusercontent.com/SaxxonPike/50aca1d91234ca4980d84b795a31c6e4/raw/8eb80835e27a5f759b1c423cad58967ada0862fd/6567-datasheet-timing.txt
    // PAL : https://www.lemon64.com/forum/viewtopic.php?t=70525
    // HORIZONTAL DECODES
    //         NTSC  NTSC  PAL   PAL
    // NAME    SET  CLEAR  SET  CLEAR             FUNCTION
    // -----   ---   ---   ---   ---   ---------------------------------
    // SPBA    336   376   ???   ???   Buss avail for sprite #0 fetch
    // EOL     340   346   ???   ???   End   line (internal clock)
    // HBLANK  396   496   ???   ???   Blanks video during horiz retrace
    // VINC    404   412   394?  404?  Increment vertical counter
    // HSYNC   416   452   408   444   Horizontal sync pulse
    // HEQ2    434   452   426   444   Horizontal equalization pulse 2
    // BURST   456   492   448?  ???   Gates reference color burst
    // REFW    484    12   ???   ???   Enable dynamic ram refresh
    // VMBA    496   332   ???   ???   Buss avail for character fetch
    // BOL     508     4   ???   ???   Begin line (internal clock)
    // CW       12   332   ???   ???   Enable character fetch
    // BKDE40   28   348   ???   ???   Enables 40 column background
    // BKDE38   35   339   ???   ???   Enables 38 column background
    // HEQ1    178   196   174   192   Horizontal equalization pulse 1
    //
    //     VERTICAL DECODES
    //                NTSC  NTSC  PAL   PAL
    //     NAME       SET  CLEAR  SET  CLEAR             FUNCTION
    //     -----      ---   ---   ---   ---   ---------------------------------
    //bool VBLANK; //  13    24   300   311   Blanks video during vert retrace
    //bool VEQ; //     14    23   301   310   Enables vertical equalization
    //bool VSYNC; //   17    20   304   307   Enables vertical sync
    //bool EEVMF; //     48   248    48   248   Enables character fetch [Enable ?E? Video Matrix Fetch]
    //bool VSW25; //   51   251    51   251   Enables 25 row screen window
    //bool VSW24; //   55   247    55   247   Enables 24 row screen window
    //bool VRESET; //   261   n/a   312?  n/a   Resets vertical count to zero [See NrOfLines]

    // Border limits (updated based on RSEL/CSEL)
    uint16_t border_top;
    uint16_t border_bottom;
    uint16_t border_left;
    uint16_t border_right;
    bool main_border_flip_flop;      // Main border flip flop (Documentation section 3.9)
    bool vertical_border_flip_flop;  // Vertical border flip flop (Documentation section 3.9)
} vicii_border_unit_t;

// Memory Mapping Unit - VIC-II memory access configuration (Documentation section 2.4.2)
typedef struct {
    uint16_t bank_base;         // Base address of current 16KB VIC bank
    uint16_t vm_base;           // VM10-VM13 bits - Video Matrix base within VIC bank
    uint16_t cb_base;           // CB11-CB13 bits - Character Base within VIC bank
    uint8_t bank;              // Current bank (0-3)
} vicii_memory_unit_t;

// Sprite Unit - Single sprite state (Documentation section 3.8 + VIC-Addendum)
typedef struct {
    uint8_t x_pos;
    uint8_t y_pos;
    bool enabled;
    bool multicolor;
    bool x_expand;
    bool y_expand;
    vicii_priority_t priority;
    uint8_t data_pointer;
    
    // VIC-Addendum sprite crunch support
    uint8_t mcbase;               // MCBASE - MOB Data Counter Base (Documentation section 3.8.1)
    uint8_t mc;                   // MC - MOB Data Counter (Documentation section 3.8.1)
    bool dma_enabled;             // Sprite DMA state (disabled when MCBASE == 63)
    
    uint8_t mc_counter;           // More descriptive than dma_counter
    bool expansion_flip_flop;
    bool display_state;
    bool sequencer_reload;
    uint32_t shift_reg;          // 24-bit shift register
    uint8_t shift_register[3];
    uint8_t data_buffer[3];
} vicii_sprite_unit_t;

// Sprites System Unit - All sprite management
typedef struct {
    vicii_sprite_unit_t sprites[VICII_NUM_SPRITES];
} vicii_sprites_unit_t;

// Pixel Output Unit - Pixel line generation and framebuffer
typedef struct {
    vicii_priority_t* pixel_line_priority;
    uint32_t* pixel_line_color;
    uint16_t pixel_line_index;
    uint16_t visible_pixels_per_line;
    
    // X-coordinate driven rendering support
    uint16_t display_start_x;            // Start of display area
    uint16_t display_end_x;              // End of display area
    uint16_t framebuffer_start_x;        // Start of framebuffer area  
    uint16_t framebuffer_end_x;          // End of framebuffer area
    
    uint32_t* framebuffer;
    int framebuffer_width;
    int framebuffer_height;
} vicii_pixel_unit_t;

// Bus Interface Unit - External bus communication
typedef struct {
    void* bus;
    void (*bank_change)(void* context, uint8_t bank);
    bool lp_edge_detected;
} vicii_bus_unit_t;

// Main VIC-II structure composed of units
struct vicii_s {
    chip_descriptor_t* desc;

    // Feature toggles
    bool enable_hardware_accurate_reads;  // Enable VIC idle/refresh memory reads for $DE00 data bus tricks and cycle accuracy (default: false for performance)

    // Chip configuration (set at initialization)
    const vicii_chip_config_t* config;

    // Topic-specific units
    vicii_registers_unit_t registers;
    vicii_timing_unit_t timing;
    vicii_video_logic_unit_t video_logic;
    vicii_video_data_unit_t video_data;
    vicii_sequencer_unit_t sequencer;
    vicii_border_unit_t border;
    vicii_memory_unit_t memory;
    vicii_sprites_unit_t sprites;
    vicii_pixel_unit_t pixel;
    vicii_bus_unit_t bus;
};

// ========================================================================================
// PUBLIC API FUNCTION PROTOTYPES
// ========================================================================================

// Only externally-visible (non-static/non-inline) functions need declarations

// Main cycle function with unified bus state threading
bus_cycle_t vicii_advance_cycle(vicii_t* vicii, bus_cycle_t bus_state);

// Unified register I/O
bus_cycle_t vicii_read(vicii_t* vicii, bus_cycle_t bus_state);
bus_cycle_t vicii_write(vicii_t* vicii, bus_cycle_t bus_state);

// Factory and lifecycle
vicii_t* vicii_system_create(chip_descriptor_t* desc, const vicii_chip_config_t* config, void (*bank_change)(void*, uint8_t));
void vicii_system_destroy(void* chip);

// Configuration helpers
const vicii_chip_config_t* vicii_get_default_config(bool is_pal);

// Bus attachment
void vicii_bus_attach(void* chip, void* bus);

// Register I/O (legacy wrappers)
uint8_t vicii_registers_read(void* chip, uint16_t address);
void vicii_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void vicii_bank_change(void* chip, uint8_t bank);

// Utility functions
void vicii_set_framebuffer(vicii_t* vicii, uint32_t* framebuffer, int width, int height);

#endif // VICII_COMMON_H