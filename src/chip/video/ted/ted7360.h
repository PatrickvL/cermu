#pragma once
/*
 * ted7360.h — TED 7360/8360 (Text Editing Device) stub
 *
 * The TED is the combined video, sound, and I/O controller used in the
 * Commodore 16 and Plus/4 computers.  It generates the video display,
 * produces 2-channel sound, scans the keyboard matrix, manages memory
 * banking (ROM/RAM), and contains three 16-bit countdown timers.
 *
 * Register map: 64 registers at $FF00-$FF3F (active area $FF00-$FF1F,
 * remainder is mirrored or used for banking latches).
 *
 *   $FF00-$FF01  Timer 1 (low/high)
 *   $FF02-$FF03  Timer 2 (low/high)
 *   $FF04-$FF05  Timer 3 (low/high)
 *   $FF06        Control register 1 (display mode, rows)
 *   $FF07        Control register 2 (multicolor, columns)
 *   $FF08        Keyboard latch (directly scans keyboard matrix)
 *   $FF09        IRQ status register
 *   $FF0A        IRQ mask register
 *   $FF0B-$FF0C  Cursor position counter (low/high)
 *   $FF0D-$FF0E  Sound channel 1 frequency (low/high)
 *   $FF0F-$FF10  Sound channel 2 frequency (low/high)
 *   $FF11        Sound control (volume, channel enable)
 *   $FF12        Memory control (bitmap/char/screen, ROM bank select)
 *   $FF13        Character base address high
 *   $FF14        Screen/video address low
 *   $FF15-$FF19  Color registers (BG0, BG1, BG2, BG3, border)
 *   $FF1A        Character position / raster line MSB
 *   $FF1B        Current raster line (low 8 bits)
 *   $FF1C-$FF1D  Vertical/horizontal position counter
 *   $FF1E        Flash counter / raster compare high
 *   $FF1F        ROM/RAM banking / CPU clock control
 *   $FF3E        Write = switch to ROM mode
 *   $FF3F        Write = switch to RAM mode
 *
 * This is a STUB implementation providing:
 *   - Register read/write
 *   - Three countdown timers with IRQ generation
 *   - Raster counter tracking
 *   - ROM/RAM banking state
 *   - Keyboard scanning via callback
 *   - Solid-color framebuffer output (border color)
 */

#include <stdint.h>
#include <stdbool.h>
#include "../../core/chip.h"
#include "../../core/system_lines.h"

// Forward declaration
typedef struct ted7360_t ted7360_t;

// ============================================================================
// REGISTER INDICES
// ============================================================================

#define TED_REG_TIMER1_LO    0x00
#define TED_REG_TIMER1_HI    0x01
#define TED_REG_TIMER2_LO    0x02
#define TED_REG_TIMER2_HI    0x03
#define TED_REG_TIMER3_LO    0x04
#define TED_REG_TIMER3_HI    0x05
#define TED_REG_CONTROL1     0x06
#define TED_REG_CONTROL2     0x07
#define TED_REG_KEYBOARD     0x08
#define TED_REG_IRQ_STATUS   0x09
#define TED_REG_IRQ_MASK     0x0A
#define TED_REG_CURSOR_LO    0x0B
#define TED_REG_CURSOR_HI    0x0C
#define TED_REG_SOUND1_LO    0x0D
#define TED_REG_SOUND1_HI    0x0E
#define TED_REG_SOUND2_LO    0x0F
#define TED_REG_SOUND2_HI    0x10
#define TED_REG_SOUND_CTRL   0x11
#define TED_REG_MEM_CTRL     0x12
#define TED_REG_CHAR_HI      0x13
#define TED_REG_VIDEO_LO     0x14
#define TED_REG_COLOR_BG0    0x15
#define TED_REG_COLOR_BG1    0x16
#define TED_REG_COLOR_BG2    0x17
#define TED_REG_COLOR_BG3    0x18
#define TED_REG_BORDER       0x19
#define TED_REG_CHARPOS_HI   0x1A   // Also raster line bit 8
#define TED_REG_RASTER_LO    0x1B
#define TED_REG_VPOS         0x1C
#define TED_REG_HPOS         0x1D
#define TED_REG_FLASH        0x1E   // Flash counter / raster compare high
#define TED_REG_ROM_RAM      0x1F   // ROM/RAM banking + CPU clock

#define TED_NUM_REGS         0x20   // 32 writable registers

// Control register 1 bits
#define TED_CR1_DISPLAY_EN   0x10   // Display enable (DEN)
#define TED_CR1_BITMAP       0x20   // Bitmap mode
#define TED_CR1_ECM          0x40   // Extended color mode
#define TED_CR1_ROWS_25      0x08   // 25 rows (vs 24)
#define TED_CR1_YSCROLL_MASK 0x07   // Y scroll (3 bits)

// Control register 2 bits
#define TED_CR2_MULTICOLOR   0x10   // Multi-color mode
#define TED_CR2_COLS_40      0x08   // 40 columns (vs 38)
#define TED_CR2_XSCROLL_MASK 0x07   // X scroll (3 bits)
#define TED_CR2_FREEZE       0x20   // Freeze TED (stop video)
#define TED_CR2_NTSC         0x40   // NTSC mode
#define TED_CR2_REVERSE      0x80   // Reverse screen mode

// IRQ status/mask bits
#define TED_IRQ_RASTER       0x02   // Raster compare IRQ
#define TED_IRQ_TIMER1       0x08   // Timer 1 underflow IRQ
#define TED_IRQ_TIMER2       0x10   // Timer 2 underflow IRQ
#define TED_IRQ_TIMER3       0x40   // Timer 3 underflow IRQ
#define TED_IRQ_ANY          0x80   // Any IRQ active (status bit 7)

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

// PAL timing
#define TED_PAL_LINES_PER_FRAME     312
#define TED_PAL_CYCLES_PER_LINE     114  // TED cycles (= 2× CPU cycles per line -> 57 CPU cycles)
#define TED_PAL_CLOCK_HZ            1773448  // TED master clock (2× CPU clock)
#define TED_PAL_CPU_CLOCK_HZ        886724   // CPU clock

// NTSC timing
#define TED_NTSC_LINES_PER_FRAME    262
#define TED_NTSC_CYCLES_PER_LINE    114
#define TED_NTSC_CLOCK_HZ           1789772  // TED master clock (2× CPU clock)
#define TED_NTSC_CPU_CLOCK_HZ       894886   // CPU clock

// ============================================================================
// KEYBOARD SCAN CALLBACK
// ============================================================================

/**
 * Keyboard scan callback.
 * Called by TED when the keyboard register ($FF08) is read.
 * The callback should return the keyboard row state for the column pattern
 * written to the keyboard latch.
 *
 * @param user_data  Opaque pointer (typically points to keyboard matrix)
 * @param column     Column select pattern (active LOW bits select columns)
 * @return           Row state (active LOW bits indicate pressed keys)
 */
typedef uint8_t (*ted_keyboard_scan_fn)(void* user_data, uint8_t column);

// ============================================================================
// DESCRIPTOR
// ============================================================================

typedef struct {
    bool is_pal;                        // true = PAL, false = NTSC
    ted_keyboard_scan_fn keyboard_scan; // Keyboard scanning callback
    void* keyboard_user_data;           // Context for keyboard callback
} ted7360_desc_t;

// ============================================================================
// TED 7360 STRUCTURE
// ============================================================================

struct ted7360_t {
    // Register file
    uint8_t regs[TED_NUM_REGS];

    // Timers (16-bit countdown, run at CPU clock rate)
    uint16_t timer1;
    uint16_t timer2;
    uint16_t timer3;
    uint16_t timer1_latch;   // Reload value
    uint16_t timer2_latch;
    uint16_t timer3_latch;

    // Raster tracking
    uint16_t raster_line;    // Current raster line (0-311 PAL, 0-261 NTSC)
    uint16_t raster_compare; // Raster line IRQ trigger value
    uint8_t  h_counter;      // Horizontal position counter (0-113)

    // IRQ state
    uint8_t irq_status;      // Pending IRQ sources
    uint8_t irq_mask;        // Enabled IRQ sources

    // Memory banking
    bool rom_enabled;         // true = ROM visible, false = RAM visible
    uint8_t mem_config;       // Memory configuration ($FF12 value)

    // Timing configuration
    bool is_pal;
    uint16_t lines_per_frame;
    uint8_t  cycles_per_line;
    uint32_t frame_cycle;     // Current cycle within frame

    // Keyboard
    ted_keyboard_scan_fn keyboard_scan;
    void* keyboard_user_data;
    uint8_t keyboard_latch;   // Last value written to $FF08

    // Framebuffer
    uint32_t* framebuffer;
    int fb_width;
    int fb_height;

    // Frame counter (for flash timing)
    uint8_t flash_counter;
};

// ============================================================================
// API
// ============================================================================

ted7360_t* ted7360_create(const ted7360_desc_t* desc);
void       ted7360_destroy(ted7360_t* ted);
void       ted7360_reset(ted7360_t* ted);

/** Tick one TED cycle (runs at 2× CPU clock). */
void       ted7360_tick(ted7360_t* ted);

/** Read a TED register (addr = $FF00-$FF3F offset, i.e. 0x00-0x3F). */
uint8_t    ted7360_read_register(ted7360_t* ted, uint8_t reg);

/** Write a TED register (addr = $FF00-$FF3F offset, i.e. 0x00-0x3F). */
void       ted7360_write_register(ted7360_t* ted, uint8_t reg, uint8_t data);

/** Check if TED has a pending IRQ (true = IRQ line asserted). */
bool       ted7360_irq_pending(const ted7360_t* ted);

/** Set the output framebuffer. */
void       ted7360_set_framebuffer(ted7360_t* ted, uint32_t* buffer, int width, int height);

// Chip descriptor for system registration
extern chip_descriptor_t ted7360_descriptor;
