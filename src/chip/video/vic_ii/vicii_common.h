#pragma once

#include "../../../core/chip.h"
#include <cstdint>
#include "../../../core/system_lines.h" // For bus_state_t
#include "../../../chip/memory/mos2114.h"  // For MOS2114
#include "../video_pixel_unit.h"
// ============================================================================
// VIC-II REGISTER TABLE — single source of truth
// 66 entries: 47 named (0-46) + 17 unused (47-63) + 2 shadows (64-65)
// ============================================================================

// X(addr, symbol, description)
#define VICII_REG_TABLE(X) \
    X( 0, M0X,    "Sprite 0 X pos")       \
    X( 1, M0Y,    "Sprite 0 Y pos")       \
    X( 2, M1X,    "Sprite 1 X pos")       \
    X( 3, M1Y,    "Sprite 1 Y pos")       \
    X( 4, M2X,    "Sprite 2 X pos")       \
    X( 5, M2Y,    "Sprite 2 Y pos")       \
    X( 6, M3X,    "Sprite 3 X pos")       \
    X( 7, M3Y,    "Sprite 3 Y pos")       \
    X( 8, M4X,    "Sprite 4 X pos")       \
    X( 9, M4Y,    "Sprite 4 Y pos")       \
    X(10, M5X,    "Sprite 5 X pos")       \
    X(11, M5Y,    "Sprite 5 Y pos")       \
    X(12, M6X,    "Sprite 6 X pos")       \
    X(13, M6Y,    "Sprite 6 Y pos")       \
    X(14, M7X,    "Sprite 7 X pos")       \
    X(15, M7Y,    "Sprite 7 Y pos")       \
    X(16, MX8,    "Sprite X pos MSB")     \
    X(17, C1,     "Y-scroll/DEN/BMM/ECM") \
    X(18, RASTER, "Raster counter")       \
    X(19, LPX,    "Light pen X")          \
    X(20, LPY,    "Light pen Y")          \
    X(21, MXE,    "Sprite enable")        \
    X(22, C2,     "X-scroll/CSEL/MCM")    \
    X(23, MXYE,   "Sprite Y expand")      \
    X(24, MP,     "Memory pointers")      \
    X(25, IR,     "Interrupt request")     \
    X(26, IE,     "Interrupt enable")      \
    X(27, MXDP,   "Sprite data priority") \
    X(28, MXMC,   "Sprite multicolor")    \
    X(29, MXXE,   "Sprite X expand")      \
    X(30, MXM,    "Sprite-sprite coll")   \
    X(31, MXD,    "Sprite-data coll")     \
    X(32, EC,     "Border color")         \
    X(33, B0C,    "Background color 0")   \
    X(34, B1C,    "Background color 1")   \
    X(35, B2C,    "Background color 2")   \
    X(36, B3C,    "Background color 3")   \
    X(37, MM0,    "Sprite mcolor 0")      \
    X(38, MM1,    "Sprite mcolor 1")      \
    X(39, M0C,    "Sprite 0 color")       \
    X(40, M1C,    "Sprite 1 color")       \
    X(41, M2C,    "Sprite 2 color")       \
    X(42, M3C,    "Sprite 3 color")       \
    X(43, M4C,    "Sprite 4 color")       \
    X(44, M5C,    "Sprite 5 color")       \
    X(45, M6C,    "Sprite 6 color")       \
    X(46, M7C,    "Sprite 7 color")       \
    X(47, R47,    "-") X(48, R48, "-") X(49, R49, "-") X(50, R50, "-") \
    X(51, R51,    "-") X(52, R52, "-") X(53, R53, "-") X(54, R54, "-") \
    X(55, R55,    "-") X(56, R56, "-") X(57, R57, "-") X(58, R58, "-") \
    X(59, R59,    "-") X(60, R60, "-") X(61, R61, "-") X(62, R62, "-") \
    X(63, R63,    "-") \
    X(64, MXM_2,  "Sprite-sprite shd")    \
    X(65, MXD_2,  "Sprite-data shd")

// VIC-II Register Constants - Modern C++ constexpr
namespace vicii_regs {
    constexpr uint8_t SIZE = 64;
    constexpr uint8_t MASK = 63;

    // Register indices from X-macro
    #define VICII_X_CONST_(a, s, l) constexpr uint8_t s = a;
    VICII_REG_TABLE(VICII_X_CONST_)
    #undef VICII_X_CONST_

    // Absolute memory-mapped I/O addresses (C64: $D000-based)
    constexpr uint16_t ADDR_D011 = 0xD011;  // Control register 1
    constexpr uint16_t ADDR_D012 = 0xD012;  // Raster counter
    constexpr uint16_t ADDR_D016 = 0xD016;  // Control register 2
    constexpr uint16_t ADDR_D018 = 0xD018;  // Memory pointers
    constexpr uint16_t ADDR_D019 = 0xD019;  // Interrupt register
    constexpr uint16_t ADDR_D01A = 0xD01A;  // Interrupt enabled
    constexpr uint16_t ADDR_D020 = 0xD020;  // Border color
    constexpr uint16_t ADDR_D021 = 0xD021;  // Background color 0
}

// --- Extract register info array (66 entries) ---
#define VICII_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry VICII_REG_INFO[] = { VICII_REG_TABLE(VICII_X_INFO_) };
#undef VICII_X_INFO_

// Legacy macro compatibility - can be removed once all code is updated
#define VICII_REGS_SIZE vicii_regs::SIZE
#define VICII_REGS_MASK vicii_regs::MASK
#define VICII_M0X     vicii_regs::M0X
#define VICII_M0Y     vicii_regs::M0Y
#define VICII_M1X     vicii_regs::M1X
#define VICII_M1Y     vicii_regs::M1Y
#define VICII_M2X     vicii_regs::M2X
#define VICII_M2Y     vicii_regs::M2Y
#define VICII_M3X     vicii_regs::M3X
#define VICII_M3Y     vicii_regs::M3Y
#define VICII_M4X     vicii_regs::M4X
#define VICII_M4Y     vicii_regs::M4Y
#define VICII_M5X     vicii_regs::M5X
#define VICII_M5Y     vicii_regs::M5Y
#define VICII_M6X     vicii_regs::M6X
#define VICII_M6Y     vicii_regs::M6Y
#define VICII_M7X     vicii_regs::M7X
#define VICII_M7Y     vicii_regs::M7Y
#define VICII_MX8     vicii_regs::MX8
#define VICII_C1      vicii_regs::C1
#define VICII_RASTER  vicii_regs::RASTER
#define VICII_LPX     vicii_regs::LPX
#define VICII_LPY     vicii_regs::LPY
#define VICII_MXE     vicii_regs::MXE
#define VICII_C2      vicii_regs::C2
#define VICII_MXYE    vicii_regs::MXYE
#define VICII_MP      vicii_regs::MP
#define VICII_IR      vicii_regs::IR
#define VICII_IE      vicii_regs::IE
#define VICII_MXDP    vicii_regs::MXDP
#define VICII_MXMC    vicii_regs::MXMC
#define VICII_MXXE    vicii_regs::MXXE
#define VICII_MXM     vicii_regs::MXM
#define VICII_MXD     vicii_regs::MXD
#define VICII_EC      vicii_regs::EC
#define VICII_B0C     vicii_regs::B0C
#define VICII_B1C     vicii_regs::B1C
#define VICII_B2C     vicii_regs::B2C
#define VICII_B3C     vicii_regs::B3C
#define VICII_MM0     vicii_regs::MM0
#define VICII_MM1     vicii_regs::MM1
#define VICII_M0C     vicii_regs::M0C
#define VICII_M1C     vicii_regs::M1C
#define VICII_M2C     vicii_regs::M2C
#define VICII_M3C     vicii_regs::M3C
#define VICII_M4C     vicii_regs::M4C
#define VICII_M5C     vicii_regs::M5C
#define VICII_M6C     vicii_regs::M6C
#define VICII_M7C     vicii_regs::M7C
#define VICII_MXM_2   vicii_regs::MXM_2
#define VICII_MXD_2   vicii_regs::MXD_2

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
#define VICII_C2_UNUSED   0xC0  // Unused bits - always high

// Memory pointers ($d018) bit masks
#define VICII_MP_UNUSED   0x01  // Unused bits - always high
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
#define VICII_IR_UNUSED   0x70  // Unused bits - floating, mostly set high
#define VICII_IR_IRQ      0x80  // Set on Any Enabled VIC IRQ Condition

// Interrupt Enabled ($d01a) bit masks
#define VICII_IE_ERST     0x01  // Raster interrupt enabled
#define VICII_IE_EMBC     0x02  // Sprite-data collision interrupt enabled
#define VICII_IE_EMMC     0x04  // Sprite-sprite collision interrupt enabled
#define VICII_IE_ELP      0x08  // Light pen interrupt enabled
#define VICII_IE_UNUSED   0xF0  // Unused bits - always high

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
using vicii_color_t = vicii_color_e;

// Priority levels for sprite/background collision
enum vicii_priority_e {
    VICII_PRIORITY_BACKGROUND = 0,
    VICII_PRIORITY_SPRITE_BEHIND = 1,
    VICII_PRIORITY_FOREGROUND = 2,
    VICII_PRIORITY_SPRITE_IN_FRONT = 3,
    VICII_PRIORITY_BORDER = 4
};
using vicii_priority_t = uint8_t;  // Must be 1 byte for memset compatibility (values 0-4)

// Pixel structure
struct vicii_pixel_t {
    vicii_priority_t priority;
    vicii_color_t color;
};

// VIC-II border coordinates (identical across all chip variants)
// From vic-ii.txt lines 751-759
constexpr uint16_t VICII_BORDER_TOP_RSEL1 = 51;
constexpr uint16_t VICII_BORDER_TOP_RSEL0 = 55;
constexpr uint16_t VICII_BORDER_BOTTOM_RSEL0 = 247;
constexpr uint16_t VICII_BORDER_BOTTOM_RSEL1 = 251;
constexpr uint16_t VICII_BORDER_LEFT_CSEL1 = 24; // 0x18
constexpr uint16_t VICII_BORDER_LEFT_CSEL0 = 31; // 0x1F (7 pixels later)
constexpr uint16_t VICII_BORDER_RIGHT_CSEL0 = 335; // 0x14F (9 pixels earlier)
constexpr uint16_t VICII_BORDER_RIGHT_CSEL1 = 344; // 0x158

// VIC-II access types (Documentation section 3.6.2)
// PHI1 = PHI2 low
#define VIC_ACCESS_IDLE         0  // PHI1     : i-access - idle access to $3fff
#define VIC_ACCESS_REFRESH      1  // PHI1     : r-access - DRAM refresh
#define VIC_ACCESS_P            2  // PHI1     : p-access - sprite data pointers
#define VIC_ACCESS_S            3  // PHI1/PHI2: s-access - sprite data (PHI2 when sprite active)
#define VIC_ACCESS_C            4  //      PHI2: c-access - video matrix and Color RAM (in bad lines)
#define VIC_ACCESS_G            5  // PHI1     : g-access - character generator or bitmap (always with c-access, never alone)
#define VIC_ACCESS_REFRESH_C    6  // PHI1: r-access, PHI2: c-access (spec cycle 15 only)

// Interrupt mask
#define VICII_INTERRUPTS_MASK (VICII_IR_ILP | VICII_IR_IMMC | VICII_IR_IMBC | VICII_IR_IRST)

// Number of sprites
#define VICII_NUM_SPRITES 8

// Display geometry
#define VICII_CHARS_PER_LINE        40       // Characters per screen line

// Memory address constants
#define VICII_IDLE_ADDRESS          0x3FFF   // Idle bus read target (top of bank)
#define VICII_VC_MASK               0x3FF    // 10-bit Video Counter mask
#define VICII_SPRITE_DATA_BLOCK     64       // Bytes per sprite data block
#define VICII_SPRITE_MC_MAX         63       // Sprite MC final value (63 bytes)



// ========================================================================================
// CHIP CONFIGURATION STRUCTURE
// ========================================================================================

// Configuration struct for different MOS 656x chip variants
struct vicii_chip_config_t {
    // Timing parameters
    uint16_t total_lines;
    uint16_t visible_lines;
    uint8_t cycles_per_line;
    uint16_t visible_pixels_per_line;
    uint16_t first_vblank_line;
    uint16_t last_vblank_line;
    uint16_t first_x_coord;
    uint16_t first_visible_x_coord;
    uint16_t last_visible_x_coord;
    
    // Framebuffer area bounds
    uint16_t framebuffer_start_x;
    uint16_t framebuffer_end_x;
    
    // Chip name for debugging
    const char* chip_name;
};

// ========================================================================================
// CYCLE TABLE ENTRY TYPE (needed for timing unit)
// ========================================================================================
struct vicii_t; // Forward declaration
using vicii_cycle_func_t = uint8_t (*)(vicii_t*, int);

struct vicii_cycle_entry_t {
    vicii_cycle_func_t func;
    int param;
};

// ========================================================================================
// TOPIC-SPECIFIC UNIT STRUCTURES
// ========================================================================================

// Register Unit - All VIC-II register state
struct vicii_registers_unit_t {
    uint8_t data[VICII_REGS_SIZE + 2];  // +2 for shadow collision registers
};

// Timing Unit - All timing-related state
struct vicii_timing_unit_t {
    uint16_t prev_raster_compare;        // Previous raster compare value for edge detection
    // Hardware counters matching VIC-II documentation
    uint8_t x_cycle;                     // Horizontal cycle counter (0 to cycles_per_line-1)
    uint16_t x_coordinate;               // X position in sprite coordinate system (0-503 PAL)
    uint16_t raster_counter;             // Vertical raster counter (0 to total_lines-1)
    uint32_t frame_count;                // Frame counter

    // Precalculated timing parameters
    const vicii_cycle_entry_t* cycle_table; // Precalculated cycle table pointer
};

// Video Logic Unit - Display state and bad line logic (Documentation section 3.7)
struct vicii_video_logic_unit_t {
    bool display_state;
    bool is_bad_line;
    bool bad_line_occurred;    // Latch: true if is_bad_line was true at ANY point on current raster line.
                               // Once set, stays set until the raster counter advances.
                               // The VIC-II's c-access state machine, once triggered by a bad line
                               // condition, runs to completion regardless of subsequent D011 writes.
                               // Used for BA prediction and c-access cycle functions instead of
                               // is_bad_line, so CPU D011 writes can't cancel an in-progress steal.
    bool was_den_set_during_raster_30;
    bool ba_low_for_bad_line;  // BA warning signal for upcoming bad line (3 cycles ahead)
    uint16_t vcbase;     // VCBASE - Video Counter Base (10 bits) (Documentation section 3.7.2)
    uint16_t vc;         // VC - Video Counter (10 bits) (Documentation section 3.7.2)
    uint8_t rc;          // RC - Row Counter (3 bits) (Documentation section 3.7.2)
    uint8_t vmli;        // VMLI - Video Matrix Line Index (6 bits) (Documentation section 3.7.2)
    uint8_t refresh_counter; // REF - 8 bit refresh counter (Documentation section 3.13)
};

// Video Data Unit - Character and color line buffers
struct vicii_video_data_unit_t {
    uint8_t video_matrix_line[VICII_CHARS_PER_LINE];
    vicii_color_t video_color_line[VICII_CHARS_PER_LINE];
};

// Graphics Sequencer Unit - Graphics pixel generation state
struct vicii_sequencer_unit_t {
    uint8_t graphics_mode;    // Current graphics mode
    uint8_t last_mode;        // Last graphics mode for change detection
    uint8_t shift_reg;        // Graphics shift register
    uint8_t xscroll_counter;  // XSCROLL delay counter
    uint8_t graphics_line[VICII_CHARS_PER_LINE]; // Graphics data buffer for current scanline (40 characters)
    uint8_t pixel_in_char;    // Current pixel within character (0-7), drives SR reload timing
    uint8_t current_vmli_for_display; // VMLI value from g-access (before increment)
    uint8_t display_vmli;     // Display-side column counter (0-39), next column to load into SR
    uint8_t active_display_column; // Column index whose data is currently in the shift register
};

// Border Unit - Border generation and limits (Documentation section 3.9)
struct vicii_border_unit_t {
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

    // Border limits in hardware coordinate space (VIC-II X coordinates 0-503)
    uint16_t border_top;
    uint16_t border_bottom;
    uint16_t border_left;
    uint16_t border_right;
    
    bool main_border_flip_flop;      // Main border flip flop (Documentation section 3.9)
    bool vertical_border_flip_flop;  // Vertical border flip flop (Documentation section 3.9)
    bool set_vertical_border_flip_flop; // Two-stage vborder latch (VICE: set_vborder)
                                        // Updated per-cycle, transferred to vertical_border_flip_flop
                                        // at left border position and start of line.
    
    // VICE-compatible border_state pipeline: VICE's draw_border8() uses a 1-cycle delayed
    // border_state variable. It checks CSEL=0 and CSEL=1 right borders at DIFFERENT cycles
    // (CSEL=0 at cycle 56, CSEL=1 at cycle 57 in 1-based PAL). Our pixel-level comparisons
    // happen within a single cycle. To match VICE's effective timing, we defer the right
    // border main_border SET by 1 cycle AND re-validate the comparison at the deferred
    // cycle using the CURRENT CSEL value. If CSEL changed between the comparison and the
    // deferred application (the CSEL side-border-opening trick), the deferred set is
    // cancelled — matching VICE's behavior where the CSEL=1 check at cycle 57 fires with
    // the (now changed) CSEL=0 value and misses.
    bool deferred_right_border;      // Pending main_border=true from previous cycle's right border check
    uint16_t deferred_right_border_x; // The x position that triggered the deferred set
};

// Memory Mapping Unit - VIC-II memory access configuration (Documentation section 2.4.2)
struct vicii_memory_unit_t {
    uint16_t bank_base;         // Base address of current 16KB VIC bank
    uint16_t vm_base;           // VM10-VM13 bits - Video Matrix base within VIC bank
    uint16_t cb_base;           // CB11-CB13 bits - Character Base within VIC bank
};

// Sprite Unit - Single sprite state (Documentation section 3.8 + VIC-Addendum)
struct vicii_sprite_unit_t {
    uint8_t x_pos;
    uint8_t y_pos;
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
};

// Sprites System Unit - All sprite management
struct vicii_sprites_unit_t {
    vicii_sprite_unit_t sprites[VICII_NUM_SPRITES];
    uint8_t pending_mxye_crunch;  // Bitmask of sprites needing crunch in cycle 15 PHI2
};

// Pixel Output Unit - Pixel line generation and framebuffer
struct vicii_pixel_unit_t : VideoPixelUnit {
    // Single line buffers for pixel generation
    vicii_priority_t* pixel_line_priority = nullptr;
    // color_line is inherited from VideoPixelUnit — stores color INDICES (0-15)
    
    // Collision detection buffers (independent of display priority)
    // The VIC-II detects collisions based on raw sequencer output, not display.
    // Documentation (VIC-II-Updated2025.txt section 3.8.2):
    //   MxM: "two or more sprite data sequencers output a non-transparent pixel"
    //   MxD: "sprite non-transparent AND graphics data sequencer outputs foreground"
    // These are parallel, independent circuits from the display priority multiplexer.
    uint8_t* sprite_collision_line = nullptr;
    bool* graphics_fg_line = nullptr;
};

// Lightpen Unit - LP pin edge detection and latch state
// The VIC-II has a single LP input pin (active LOW, directly from Control Port 1 pin 6).
// On a 1→0 (negative) edge, it latches the current raster position into LPX/LPY
// and sets the ILP interrupt flag. Only one trigger per frame is recognized.
struct vicii_lightpen_unit_t {
    bool triggered;         // Already triggered this frame (one trigger per frame)
    bool lp_pin_prev;       // Previous LP pin state (true=HIGH/released, false=LOW/active)
};

// Memory read callback type for VIC-II PHI1/PHI2 accesses.
// Decouples VIC-II from the system bus — the system injects this at init.
using vicii_mem_read_fn_t = bus_state_t (*)(void* ctx, bus_state_t bus_state, uint16_t addr);

// Bus Interface Unit - External bus communication
struct vicii_bus_unit_t {
    void* bus;
    vicii_mem_read_fn_t mem_read;  // System-provided memory read callback
    void* mem_read_ctx;            // Context for mem_read (typically the system bus)
    void (*bank_change)(void* context, uint8_t bank);
    // LP pin callback — reads the light pen input from Control Port 1.
    // Returns true if LP pin is HIGH (released), false if LOW (asserted).
    bool (*lp_pin_read)(void* context);
    void* lp_pin_context;
    uint8_t pending_phi2_access_type;  // Track which PHI2 access type was set up for vicii_tick_phi2
    vicii_sprite_unit_t* active_sprite;  // Active sprite pointer for P/S accesses (NULL if none)
    uint8_t ba_prediction_shift_reg;     // 3-bit shift register: bit0=cycle+1, bit1=cycle+2, bit2=cycle+3
    uint8_t ba_low_count;                // Consecutive cycles BA has been LOW (for AEC 3-cycle delay)
    bus_state_t bus_line_mask;           // Bitmask for bus lines to pull low (BA, AEC, etc.)
};

// Main VIC-II structure composed of units
struct vicii_t : public ChipBase {
    MOS2114* colorram = nullptr;

    // Chip configuration (set at initialization)
    const vicii_chip_config_t* config = nullptr;

    // Pre-computed display mapping constants (session-constant, set during init)
    // Eliminates per-pixel modulo and pointer dereferences in the pixel pipeline.
    uint16_t cached_pixels_per_line = 0;         // config->cycles_per_line * 8
    uint16_t cached_visible_pixels = 0;          // config->visible_pixels_per_line
    uint16_t cached_first_visible_display = 0;   // transformed first_visible_x_coord
    uint16_t cached_wrap_threshold = 0;          // (first_visible_display + visible_pixels) % pixels_per_line
    uint16_t cached_display_offset = 0;          // pixels_per_line + pipeline_delay + centering
    uint16_t cached_first_x_coord = 0;           // config->first_x_coord (X at cycle 0)

    // Topic-specific units
    vicii_registers_unit_t registers = {};
    vicii_timing_unit_t timing = {};
    vicii_video_logic_unit_t video_logic = {};
    vicii_video_data_unit_t video_data = {};
    vicii_sequencer_unit_t sequencer = {};
    vicii_border_unit_t border = {};
    vicii_memory_unit_t memory = {};
    vicii_sprites_unit_t sprites = {};
    vicii_pixel_unit_t pixel = {};
    vicii_lightpen_unit_t lightpen = {};
    vicii_bus_unit_t bus = {};

    // Destructor — cleans up dynamically allocated pixel line buffers
    ~vicii_t() override;

    // ChipBase interface
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override { return true; }
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif

    // Initialization and lifecycle
    void init(const vicii_chip_config_t* config, void (*bank_change)(void*, uint8_t));
    void reset();

    // Tick functions
    bus_state_t tick_phi1(bus_state_t bus_state);
    void tick_phi2(bus_state_t bus_state);

    // Register I/O (static — for io_page_handlers_t function pointer tables)
    static bus_state_t registers_read(void* context, bus_state_t bus_state);
    static bus_state_t registers_write(void* context, bus_state_t bus_state);

    // Memory bank change (static — used as callback from CIA2)
    static void memory_bank_change(void* chip, uint8_t bank);

    // Framebuffer management
    void set_framebuffer(uint32_t* framebuffer, int width, int height);

    // Configuration and utility
    static const vicii_chip_config_t* get_default_config(bool is_pal);
    static const uint32_t* get_default_palette();
    uint16_t get_raster_counter() const;
    uint16_t get_x_coordinate() const;

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
