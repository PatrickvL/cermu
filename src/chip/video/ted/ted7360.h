#pragma once
/*
 * ted7360.h — TED 7360/8360 (Text Editing Device) cycle-accurate emulation
 *
 * The TED is the combined video, sound, and I/O controller used in the
 * Commodore 16, 116, and Plus/4 computers.  It generates the video display,
 * produces 2-channel sound, scans the keyboard matrix, manages memory
 * banking (ROM/RAM), and contains three 16-bit countdown timers.
 *
 * Unlike the VIC-II, the TED has no sprites and runs at a single-clock
 * rate — every TED cycle is one half a CPU cycle (the TED master clock
 * is 2× the CPU clock).  A TED line consists of 114 TED single-clock
 * cycles (= 57 CPU cycles of paired PHI1+PHI2 half-cycles).
 *
 * Memory access model (adapted from VIC-II phi1/phi2):
 *   PHI1 (first half of CPU cycle): TED performs its own memory reads
 *     - g-access (character generator / bitmap data)
 *     - DMA screen reads (character + attribute data on DMA lines)
 *     - Idle access ($FFFF)
 *   PHI2 (second half of CPU cycle): CPU has bus access (unless DMA steal)
 *     - c-access (screen/color matrix reads during DMA lines)
 *     - CPU memory reads/writes when not stolen
 *
 * The TED steals CPU cycles on "DMA lines" (analogous to VIC-II bad lines):
 * when the lower 3 bits of the raster counter match YSCROLL and the display
 * is enabled (DEN=1), the TED takes over the bus for 40+3 cycles to fetch
 * screen matrix and color attribute data.
 *
 * Register map: 32 registers at $FF00-$FF1F (mirrored in $FF20-$FF3F),
 * plus banking latches at $FF3E/$FF3F.
 *
 *   $FF00-$FF01  Timer 1 (low/high)
 *   $FF02-$FF03  Timer 2 (low/high)
 *   $FF04-$FF05  Timer 3 (low/high)
 *   $FF06        Control register 1 (DEN, BMM, ECM, RSEL, YSCROLL)
 *   $FF07        Control register 2 (MCM, CSEL, XSCROLL, FREEZE, PAL/NTSC, RVS)
 *   $FF08        Keyboard latch
 *   $FF09        IRQ status register
 *   $FF0A        IRQ mask register
 *   $FF0B-$FF0C  Cursor position (low/high)
 *   $FF0D-$FF0E  Sound channel 1 frequency (low/high)
 *   $FF0F-$FF10  Sound channel 2 frequency (low/high)
 *   $FF11        Sound control
 *   $FF12        Memory control (character/screen base, ROM bank)
 *   $FF13        Character base address high nibble
 *   $FF14        Screen/bitmap base address
 *   $FF15-$FF18  Background colors 0-3
 *   $FF19        Border color
 *   $FF1A        Character position high / raster bit 8
 *   $FF1B        Raster counter low 8 bits
 *   $FF1C        Cursor blink position / vertical sub-address
 *   $FF1D        Horizontal position
 *   $FF1E        Flash counter / raster compare high bits
 *   $FF1F        ROM/RAM banking + CPU clock
 *   $FF3E        Write = switch to ROM mode
 *   $FF3F        Write = switch to RAM mode
 */

#include <stdint.h>
#include <stdbool.h>
#include "../../core/chip.h"
#include "../../core/system_lines.h"

// ============================================================================
// REGISTER INDICES ($FF00-$FF1F)
// ============================================================================

#define TED_REG_TIMER1_LO    0x00
#define TED_REG_TIMER1_HI    0x01
#define TED_REG_TIMER2_LO    0x02
#define TED_REG_TIMER2_HI    0x03
#define TED_REG_TIMER3_LO    0x04
#define TED_REG_TIMER3_HI    0x05
#define TED_REG_CONTROL1     0x06   // $FF06
#define TED_REG_CONTROL2     0x07   // $FF07
#define TED_REG_KEYBOARD     0x08   // $FF08
#define TED_REG_IRQ_STATUS   0x09   // $FF09
#define TED_REG_IRQ_MASK     0x0A   // $FF0A
#define TED_REG_CURSOR_LO    0x0B
#define TED_REG_CURSOR_HI    0x0C
#define TED_REG_SOUND1_LO    0x0D
#define TED_REG_SOUND1_HI    0x0E
#define TED_REG_SOUND2_LO    0x0F
#define TED_REG_SOUND2_HI    0x10
#define TED_REG_SOUND_CTRL   0x11
#define TED_REG_MEM_CTRL     0x12   // $FF12
#define TED_REG_CHAR_HI      0x13   // $FF13
#define TED_REG_BITMAP_ADDR  0x14   // $FF14
#define TED_REG_COLOR_BG0    0x15
#define TED_REG_COLOR_BG1    0x16
#define TED_REG_COLOR_BG2    0x17
#define TED_REG_COLOR_BG3    0x18
#define TED_REG_BORDER       0x19
#define TED_REG_CHARPOS_HI   0x1A   // Also raster bit 8
#define TED_REG_RASTER_LO    0x1B
#define TED_REG_VPOS         0x1C
#define TED_REG_HPOS         0x1D
#define TED_REG_FLASH        0x1E   // Flash counter / raster compare
#define TED_REG_ROM_RAM      0x1F   // ROM/RAM banking + CPU clock

#define TED_NUM_REGS         0x20   // 32 registers

// ============================================================================
// CONTROL REGISTER BITS
// ============================================================================

// Control register 1 ($FF06)
#define TED_CR1_YSCROLL_MASK 0x07   // Y scroll (3 bits)
#define TED_CR1_RSEL         0x08   // 25 rows (1) vs 24 rows (0)
#define TED_CR1_DEN          0x10   // Display Enable
#define TED_CR1_BMM          0x20   // Bitmap mode
#define TED_CR1_ECM          0x40   // Extended color mode
#define TED_CR1_TEST         0x80   // Test bit (active low on real HW)

// Control register 2 ($FF07)
#define TED_CR2_XSCROLL_MASK 0x07   // X scroll (3 bits)
#define TED_CR2_CSEL         0x08   // 40 columns (1) vs 38 columns (0)
#define TED_CR2_MCM          0x10   // Multi-color mode
#define TED_CR2_FREEZE       0x20   // Freeze TED (stop video, single-clock)
#define TED_CR2_PAL_NTSC     0x40   // 0 = PAL, 1 = NTSC (read-only on real HW)
#define TED_CR2_RVS          0x80   // Reverse screen mode

// ============================================================================
// IRQ STATUS/MASK BITS ($FF09/$FF0A)
// ============================================================================

#define TED_IRQ_RASTER       0x02   // Raster compare IRQ
#define TED_IRQ_LIGHTPEN     0x04   // Light pen IRQ (active low input)
#define TED_IRQ_TIMER1       0x08   // Timer 1 underflow IRQ
#define TED_IRQ_TIMER2       0x10   // Timer 2 underflow IRQ
#define TED_IRQ_TIMER3       0x40   // Timer 3 underflow IRQ
#define TED_IRQ_ANY          0x80   // Any IRQ active (ORed status & mask)
#define TED_IRQ_CLEARABLE    0x5E   // Bits clearable by writing 1s

// ============================================================================
// GRAPHICS MODES (ECM|BMM|MCM encoding, matching VIC-II convention)
// ============================================================================

#define TED_GM_STANDARD_TEXT       0  // ECM/BMM/MCM=0/0/0
#define TED_GM_MULTICOLOR_TEXT     1  // ECM/BMM/MCM=0/0/1
#define TED_GM_STANDARD_BITMAP     2  // ECM/BMM/MCM=0/1/0
#define TED_GM_MULTICOLOR_BITMAP   3  // ECM/BMM/MCM=0/1/1
#define TED_GM_ECM_TEXT            4  // ECM/BMM/MCM=1/0/0
#define TED_GM_INVALID1            5  // ECM/BMM/MCM=1/0/1
#define TED_GM_INVALID2            6  // ECM/BMM/MCM=1/1/0
#define TED_GM_INVALID3            7  // ECM/BMM/MCM=1/1/1

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

// The TED line consists of 114 TED single-clock cycles = 57 CPU cycles.
// Our tick model operates per CPU cycle (calling ted7360_tick_phi1 once per
// CPU cycle), so x_cycle counts 0..56.

// PAL timing
#define TED_PAL_LINES_PER_FRAME     312
#define TED_PAL_CPU_CYCLES_PER_LINE 57      // 114 TED clocks / 2
#define TED_PAL_CLOCK_HZ            1773448 // TED master clock (2× CPU clock)
#define TED_PAL_CPU_CLOCK_HZ        886724  // CPU clock

// NTSC timing
#define TED_NTSC_LINES_PER_FRAME    262
#define TED_NTSC_CPU_CYCLES_PER_LINE 57     // Same cycles per line as PAL
#define TED_NTSC_CLOCK_HZ           1789772 // TED master clock
#define TED_NTSC_CPU_CLOCK_HZ       894886  // CPU clock

// Display geometry
#define TED_SCREEN_TEXTCOLS         40
#define TED_SCREEN_TEXTLINES        25
#define TED_SCREEN_XPIX             320
#define TED_SCREEN_YPIX             200

// Border limits (in TED raster counter coordinates)
#define TED_25ROW_START_LINE        4
#define TED_25ROW_STOP_LINE         0xCB    // 203
#define TED_24ROW_START_LINE        8
#define TED_24ROW_STOP_LINE         0xC7    // 199

// Horizontal border limits (in pixel coordinates within a line)
// The 320-pixel display window is centered in the 456-pixel visible area.
// x_cycle 0 corresponds to the start of the line. Characters are displayed
// starting at around CPU cycle 4..5 depending on horizontal scroll.
// These values represent the pixel at which border opens/closes.
#define TED_40COL_LEFT_BORDER_PX    24      // Pixel offset where 40-col display starts
#define TED_40COL_RIGHT_BORDER_PX   344     // Pixel offset where 40-col display ends
#define TED_38COL_LEFT_BORDER_PX    31      // 40-col + 7
#define TED_38COL_RIGHT_BORDER_PX   335     // 40-col - 9

// DMA fetch timing (in CPU cycles within a line)
// DMA starts at CPU cycle 4 (TED_FETCH_CYCLE) and runs for 40+3=43 cycles
#define TED_FETCH_CYCLE             4       // CPU cycle at which DMA begins
#define TED_FETCH_END_CYCLE         46      // Last DMA CPU cycle (4 + 3 setup + 40 data - 1)
#define TED_DMA_SETUP_CYCLES        3       // Cycles to take over bus before char fetch

// DMA line range (TED raster counter values where DMA can occur)
#define TED_FIRST_DMA_LINE          0
#define TED_LAST_DMA_LINE           0xCB

// Visible area for framebuffer rendering
#define TED_VISIBLE_WIDTH           384     // 320 + borders
#define TED_VISIBLE_HEIGHT          288     // 200 + borders (PAL)

// Number of colors: 16 hues × 8 luminances = 128 possible values
// (hue 0 is luminance-independent black, so 121 unique colors)
#define TED_NUM_COLORS              128

// ============================================================================
// LINE STATE MACHINE — driven by x_cycle position
// ============================================================================
// Instead of a per-cycle callback table (as VIC-II needs for sprites), the TED
// uses a state enum driven by x_cycle position within the line.  State
// transitions happen at specific x_cycle values.

enum ted_line_state_e {
    TED_STATE_HBLANK,           // Horizontal blanking / retrace region
    TED_STATE_LEFT_BORDER,      // Left border (before display window)
    TED_STATE_DISPLAY,          // Active display area (40 characters)
    TED_STATE_RIGHT_BORDER,     // Right border (after display window)
    TED_STATE_IDLE,             // Idle cycles (outside display + border)
};

// ============================================================================
// TED MEMORY ACCESS TYPES (adapted from VIC-II model)
// ============================================================================

#define TED_ACCESS_IDLE        0   // PHI1: idle access to $FFFF
#define TED_ACCESS_REFRESH     1   // PHI1: DRAM refresh
#define TED_ACCESS_G           2   // PHI1: g-access — character generator / bitmap data
#define TED_ACCESS_C           3   // PHI2: c-access — screen matrix + color (DMA lines)

// ============================================================================
// CALLBACK TYPES
// ============================================================================

/**
 * Keyboard scan callback.
 * Called by TED when $FF08 is read.
 * @param user_data  Context pointer
 * @param column     Column select pattern (active LOW bits select columns)
 * @return           Row state (active LOW bits indicate pressed keys)
 */
typedef uint8_t (*ted_keyboard_scan_fn)(void* user_data, uint8_t column);

/**
 * Memory read callback.
 * Called by TED during PHI1 for its own accesses (character data, bitmap data,
 * screen reads).  The system provides this so TED can access the full address
 * bus including ROM/RAM banking.
 *
 * @param user_data  Context pointer (typically system instance)
 * @param address    16-bit address to read
 * @return           Data byte at that address
 */
typedef uint8_t (*ted_mem_read_fn)(void* user_data, uint16_t address);

// ============================================================================
// DESCRIPTOR
// ============================================================================

typedef struct {
    bool is_pal;                        // true = PAL, false = NTSC
    ted_keyboard_scan_fn keyboard_scan; // Keyboard scanning callback
    void* keyboard_user_data;           // Context for keyboard callback
    ted_mem_read_fn mem_read;           // Memory read callback for TED's own accesses
    void* mem_read_user_data;           // Context for memory read
} ted7360_desc_t;

// ============================================================================
// UNIT STRUCTURES (following VIC-II unit-based decomposition)
// ============================================================================

// Register Unit — raw register file
struct ted_registers_unit_t {
    uint8_t data[TED_NUM_REGS];
};

// Timing Unit — horizontal/vertical counters
struct ted_timing_unit_t {
    uint16_t raster_counter;        // 9-bit vertical raster counter (0-311 PAL / 0-261 NTSC)
    uint16_t raster_compare;        // Raster line IRQ trigger value (9-bit)
    uint8_t  x_cycle;               // CPU-cycle counter within line (0-56)
    uint16_t x_pixel;               // Pixel X position within line (= x_cycle * 8)
    uint32_t frame_count;           // Frame counter (for flash timing)
    uint16_t lines_per_frame;       // PAL=312, NTSC=262
    uint8_t  cpu_cycles_per_line;   // Always 57
    bool     is_pal;
};

// Video Logic Unit — display state and DMA line detection
struct ted_video_logic_unit_t {
    bool     display_state;         // true = display mode, false = idle
    bool     is_dma_line;           // Current line is a DMA line (YSCROLL match + DEN)
    bool     dma_line_occurred;     // Latch: DMA triggered at some point this line
    bool     den_latched;           // DEN was set, enabling DMA line detection
    uint16_t vcbase;                // VCBASE — video counter base (10-bit)
    uint16_t vc;                    // VC — video counter (10-bit)
    uint8_t  rc;                    // RC — row counter (3-bit)
    uint8_t  vmli;                  // VMLI — video matrix line index (0-39)
    uint8_t  refresh_counter;       // 8-bit DRAM refresh counter
};

// Video Data Unit — character and color line buffers (filled during DMA)
struct ted_video_data_unit_t {
    uint8_t screen_line[TED_SCREEN_TEXTCOLS];   // Screen codes (character indices)
    uint8_t color_line[TED_SCREEN_TEXTCOLS];    // Color attributes
};

// Graphics Sequencer Unit — shift register and mode-dependent pixel production
struct ted_sequencer_unit_t {
    uint8_t graphics_mode;                       // Current mode (ECM|BMM|MCM)
    uint8_t shift_reg;                           // 8-bit graphics shift register
    uint8_t xscroll;                             // XSCROLL delay counter
    uint8_t char_data[TED_SCREEN_TEXTCOLS];     // g-access data for current line (chargen/bitmap)
    uint8_t pixel_in_char;                       // Pixel within current character (0-7)
    uint8_t display_vmli;                        // Display-side column counter (0-39)
    uint8_t active_display_column;               // Column whose data is in the shift register
};

// Border Unit — border flip-flops and comparison limits
struct ted_border_unit_t {
    uint16_t top;                    // First display row (raster counter)
    uint16_t bottom;                 // Last display row + 1
    uint16_t left;                   // Left border pixel position
    uint16_t right;                  // Right border pixel position
    bool     main_ff;                // Main (horizontal) border flip-flop
    bool     vert_ff;                // Vertical border flip-flop
};

// Memory Mapping Unit — address calculation for screen/char/bitmap
struct ted_memory_unit_t {
    uint16_t screen_base;            // Screen matrix base address ($FF14 derived)
    uint16_t char_base;              // Character generator base address ($FF13 derived)
    uint16_t bitmap_base;            // Bitmap base address ($FF12/$FF14 derived)
};

// Timer Unit — single 16-bit countdown timer with latch
struct ted_timer_unit_t {
    uint16_t counter;                // Current 16-bit counter value
    uint16_t latch;                  // Reload latch value (written via register)
};

// Pixel Output Unit — line buffer and framebuffer target
struct ted_pixel_unit_t {
    uint8_t*  color_line;            // Per-pixel color index line buffer
    uint32_t* framebuffer;           // Output framebuffer (RGBA)
    int       fb_width;
    int       fb_height;
};

// Bus Interface Unit — pending DMA access and BA/AEC state
struct ted_bus_unit_t {
    ted_mem_read_fn mem_read;        // Memory read callback
    void* mem_read_user_data;        // Context for memory read
    uint8_t pending_access;          // TED_ACCESS_* for current PHI2 delivery
    uint16_t pending_address;        // Address set up for PHI2 read
    bool ba_low;                     // BA signal state (true = TED has bus control)
    uint8_t ba_low_count;            // Consecutive CPU cycles BA has been LOW
};

// Sound Unit (placeholder — sound not yet implemented)
struct ted_sound_unit_t {
    uint16_t freq1;                  // Channel 1 frequency (10-bit)
    uint16_t freq2;                  // Channel 2 frequency (10-bit)
    uint8_t  volume;                 // Volume (4 bits)
    bool     ch1_enabled;
    bool     ch2_enabled;
    bool     noise_enabled;          // Channel 2 noise mode
};

// ============================================================================
// TED 7360 MAIN STRUCTURE
// ============================================================================

struct ted7360_t : public ChipBase {
    // ========================================================================
    // Public API — Lifecycle
    // ========================================================================

    /** Construct and initialize a TED 7360 instance. */
    explicit ted7360_t(const ted7360_desc_t& desc);

    /** Destructor — frees internal line buffer. */
    ~ted7360_t();

    // Non-copyable, non-movable
    ted7360_t(const ted7360_t&) = delete;
    ted7360_t& operator=(const ted7360_t&) = delete;

    /** Reset TED to power-on state (preserving configuration). */
    void reset();

    // ========================================================================
    // Public API — Cycle-accurate tick (phi1/phi2 model)
    // ========================================================================

    /**
     * PHI1 phase — performs one full CPU cycle of TED processing.
     *
     * Handles: timing advance, raster compare, DMA detection, g-access,
     * pixel sequencing (8 pixels per cycle), border logic, timer countdown,
     * IRQ generation, and sets up PHI2 address for DMA lines.
     *
     * @param bus_state  Current bus state (from system default state)
     * @return Updated bus state with TED's IRQ/BA signals and PHI2 address
     */
    bus_state_t tick_phi1(bus_state_t bus_state);

    /**
     * PHI2 delivery — processes data from memory tick.
     *
     * On DMA lines, reads the memory tick result (screen matrix / color data)
     * and stores it in the video line buffer.  On non-DMA lines, this is a no-op.
     *
     * @param bus_state  Bus state after memory service (contains read data)
     */
    void tick_phi2(bus_state_t bus_state);

    /** Tick one TED cycle — legacy wrapper, calls phi1+phi2 internally. */
    void tick();

    // ========================================================================
    // Public API — Register I/O
    // ========================================================================

    /** Read a TED register (addr encodes $FF00-$FF3F offset). */
    bus_state_t registers_read(bus_state_t bus_state);

    /** Write a TED register (addr encodes $FF00-$FF3F offset). */
    bus_state_t registers_write(bus_state_t bus_state);

    // ========================================================================
    // Public API — IRQ query
    // ========================================================================

    /** Check if TED has a pending IRQ (true = IRQ line asserted). */
    bool irq_pending() const;

    // ========================================================================
    // Public API — Framebuffer
    // ========================================================================

    /** Set the output framebuffer for pixel rendering. */
    void set_framebuffer(uint32_t* buffer, int width, int height);

    // ========================================================================
    // Public API — Color palette (static)
    // ========================================================================

    /** Get the 128-entry TED color palette (RGBA format). */
    static const uint32_t* get_palette();

    // ========================================================================
    // Public data — Unit structures (following VIC-II decomposition)
    // ========================================================================

    ted_registers_unit_t   registers;
    ted_timing_unit_t      timing;
    ted_video_logic_unit_t video_logic;
    ted_video_data_unit_t  video_data;
    ted_sequencer_unit_t   sequencer;
    ted_border_unit_t      border;
    ted_memory_unit_t      memory;
    ted_timer_unit_t       timer1;
    ted_timer_unit_t       timer2;
    ted_timer_unit_t       timer3;
    ted_sound_unit_t       sound;
    ted_pixel_unit_t       pixel;
    ted_bus_unit_t         bus;

    // IRQ state
    uint8_t irq_status = 0;              // Pending IRQ sources (latched)
    uint8_t irq_mask = 0;                // Enabled IRQ sources

    // Memory banking
    bool rom_enabled = false;            // true = ROM visible, false = RAM visible

    // Keyboard
    ted_keyboard_scan_fn keyboard_scan = nullptr;
    void* keyboard_user_data = nullptr;
    uint8_t keyboard_latch = 0;          // Last value written to $FF08

    // Flash / cursor blink
    uint8_t flash_counter = 0;           // 6-bit flash counter (incremented each frame)
    bool    cursor_visible = false;      // Current cursor blink phase

    // Reverse mode
    bool reverse_mode = false;           // RVS bit from $FF07

private:
    // ========================================================================
    // Internal helpers
    // ========================================================================

    void update_memory_addresses();
    void update_border_limits();
    void update_dma_condition();
    void check_raster_interrupt();
    void tick_timers();
    void pixel_sequencer();
    void flush_line(uint16_t raster_line);
    void timing_advance();

    uint8_t get_graphics_mode() const;
    uint16_t get_raster_compare() const;

    // Legacy tick subcycle tracker
    int legacy_subcycle_ = 0;

    // --- ChipBase interface ---
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;
};
