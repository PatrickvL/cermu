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
 * Register map: 32 registers at $FF00-$FF1F (mirrored at $FF20-$FF3F),
 * plus banking latches at $FF3E/$FF3F.  See TED_REG_* defines below for
 * individual register descriptions and address notes.
 */

#include <cstdint>

#include "chip/video/video_chip_base.hpp"
#include "core/system_lines.hpp"
#include "utils/ring_buffer.hpp"

class IndexedFrameBuffer;

// ============================================================================
// TED REGISTER TABLE — single source of truth
// Register map: $FF00-$FF1F (mirrored at $FF20-$FF3F)
// ============================================================================

// DECL(REG, FLD, CMP) — 32 registers, 17 fields (CONTROL1/2, SOUND_CTRL)
#define TED_DECL(REG, FLD, CMP) \
    REG(0x00, TED_REG_TIMER1_LO,  "Timer 1 low byte")                           \
    REG(0x01, TED_REG_TIMER1_HI,  "Timer 1 high byte")                          \
    REG(0x02, TED_REG_TIMER2_LO,  "Timer 2 low byte")                           \
    REG(0x03, TED_REG_TIMER2_HI,  "Timer 2 high byte")                          \
    REG(0x04, TED_REG_TIMER3_LO,  "Timer 3 low byte")                           \
    REG(0x05, TED_REG_TIMER3_HI,  "Timer 3 high byte")                          \
    REG(0x06, TED_REG_CONTROL1,   "Y-scroll/DEN/BMM/ECM")                       \
      FLD(TED_REG_CONTROL1, TEST,    7:7, "Test bit",          Flag,  0, 0)      \
      FLD(TED_REG_CONTROL1, ECM,     6:6, "Extended color",    Flag,  0, 0)      \
      FLD(TED_REG_CONTROL1, BMM,     5:5, "Bitmap mode",       Flag,  0, 0)      \
      FLD(TED_REG_CONTROL1, DEN,     4:4, "Display enable",    Flag,  0, 0)      \
      FLD(TED_REG_CONTROL1, RSEL,    3:3, "25/24 rows",        Flag,  0, 0)      \
      FLD(TED_REG_CONTROL1, YSCROLL, 2:0, "Y scroll",          Value, 0, 0)      \
    REG(0x07, TED_REG_CONTROL2,   "X-scroll/CSEL/MCM")                           \
      FLD(TED_REG_CONTROL2, RVS,      7:7, "Reverse screen",   Flag,  0, 0)      \
      FLD(TED_REG_CONTROL2, PAL_NTSC, 6:6, "PAL/NTSC",         Flag,  0, 0)      \
      FLD(TED_REG_CONTROL2, FREEZE,   5:5, "Freeze display",   Flag,  0, 0)      \
      FLD(TED_REG_CONTROL2, MCM,      4:4, "Multicolor",       Flag,  0, 0)      \
      FLD(TED_REG_CONTROL2, CSEL,     3:3, "40/38 columns",    Flag,  0, 0)      \
      FLD(TED_REG_CONTROL2, XSCROLL,  2:0, "X scroll",         Value, 0, 0)      \
    REG(0x08, TED_REG_KEYBOARD,   "Keyboard col/row latch")                      \
    REG(0x09, TED_REG_IRQ_STATUS, "IRQ status/acknowledge")                      \
    REG(0x0A, TED_REG_IRQ_MASK,   "IRQ enable mask")                             \
    REG(0x0B, TED_REG_RASTER_CMP, "Raster compare lo")                           \
    REG(0x0C, TED_REG_CURSOR_HI,  "Cursor position hi")                          \
    REG(0x0D, TED_REG_CURSOR_LO,  "Cursor position lo")                          \
    REG(0x0E, TED_REG_SOUND1_LO,  "Sound 1 freq lo")                            \
    REG(0x0F, TED_REG_SOUND2_LO,  "Sound 2 freq lo")                            \
    REG(0x10, TED_REG_SOUND2_HI,  "Sound 2 freq hi")                            \
    REG(0x11, TED_REG_SOUND_CTRL, "DA/noise/enable/volume")                      \
      FLD(TED_REG_SOUND_CTRL, DA_MODE,  7:7, "D/A mode",       Flag,  0, 0)      \
      FLD(TED_REG_SOUND_CTRL, NOISE_EN, 6:6, "Noise enable",   Flag,  0, 0)      \
      FLD(TED_REG_SOUND_CTRL, CH2_EN,   5:5, "Ch 2 enable",    Flag,  0, 0)      \
      FLD(TED_REG_SOUND_CTRL, CH1_EN,   4:4, "Ch 1 enable",    Flag,  0, 0)      \
      FLD(TED_REG_SOUND_CTRL, SND_VOL,  3:0, "Volume",         Value, 0, 0)      \
    REG(0x12, TED_REG_MEM_CTRL,   "Sound1 hi + memory map")                      \
    REG(0x13, TED_REG_CHAR_HI,    "Char generator base")                         \
    REG(0x14, TED_REG_BITMAP_ADDR,"Screen/bitmap base")                          \
    REG(0x15, TED_REG_COLOR_BG0,  "Background color 0",  Color, 6:0)            \
    REG(0x16, TED_REG_COLOR_BG1,  "Background color 1",  Color, 6:0)            \
    REG(0x17, TED_REG_COLOR_BG2,  "Background color 2",  Color, 6:0)            \
    REG(0x18, TED_REG_COLOR_BG3,  "Background color 3",  Color, 6:0)            \
    REG(0x19, TED_REG_BORDER,     "Border color",         Color, 6:0)            \
    REG(0x1A, TED_REG_CHARPOS_HI, "Char counter hi")                             \
    REG(0x1B, TED_REG_CHARPOS_LO, "Char counter lo")                             \
    REG(0x1C, TED_REG_RASTER_HI,  "Raster counter hi")                           \
    REG(0x1D, TED_REG_RASTER_LO,  "Raster counter lo")                           \
    REG(0x1E, TED_REG_HPOS,       "Horizontal position")                         \
    REG(0x1F, TED_REG_FLASH_RC,   "Flash counter/row ctr")

// --- Extract address constants ---
TED_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)

DECL_EXTRACT(TED, TED_DECL)

// Video counter / address masks
#define TED_VC_MASK              0x3FF    // 10-bit video counter (VC/VCBASE) mask: 0..1023 (40×25 = 1000 char positions)
#define TED_TIMER_WRAP_VALUE     0xFFFF   // 16-bit initial value for timers 2/3 (no auto-reload: wrap to $FFFF on underflow)

// Register mirror/latch constants
#define TED_REG_ADDR_MASK        0x3F     // 6-bit address mask: TED occupies 64 locations $FF00-$FF3F
#define TED_REG_MIRROR_START     0x20     // Registers $FF20-$FF3D mirror $FF00-$FF1D (except $FF3E/$FF3F)
#define TED_REG_ROM_LATCH        0x3E     // $FF3E — write-only banking latch: switch to ROM mode
#define TED_REG_RAM_LATCH        0x3F     // $FF3F — write-only banking latch: switch to RAM mode
#define TED_REG_UNMIRROR_MASK    0x1F     // Mask to fold mirrored address ($20-$3D) back into primary range ($00-$1D)

// Video rendering constants
#define TED_FLASH_PHASE_BIT      0x10     // Bit 4 of flash_counter: 0=cursor/blink off, 1=on (toggles at ~1 Hz)

// ============================================================================
// CONTROL REGISTER BITS
// ============================================================================

// Control register 1 ($FF06)
#define TED_CR1_YSCROLL_MASK 0x07   // Y scroll offset bits [2:0]: fine-scroll display window down by 0-7 pixels
#define TED_CR1_RSEL         0x08   // Row select: 1 = 25-row display window (raster 4-203), 0 = 24-row (8-199)
#define TED_CR1_DEN          0x10   // Display Enable: 1 = video output active; must be set by raster line 48 to enable DMA lines
#define TED_CR1_BMM          0x20   // Bitmap Mode: 1 = bitmap graphics, 0 = character mode
#define TED_CR1_ECM          0x40   // Extended Color Mode: 1 = ECM text (4 backgrounds via screen code bits [7:6])
#define TED_CR1_TEST         0x80   // Test bit: active-low on real hardware; do not set in normal operation

// Control register 2 ($FF07)
#define TED_CR2_XSCROLL_MASK 0x07   // X scroll offset bits [2:0]: fine-scroll display window right by 0-7 pixels
#define TED_CR2_CSEL         0x08   // Column select: 1 = 40-column window (pixels 24-344), 0 = 38-column (31-335)
#define TED_CR2_MCM          0x10   // Multi-Color Mode: 1 = 2 bits/pixel double-width (text or bitmap)
#define TED_CR2_FREEZE       0x20   // Freeze: 1 = stop TED video logic (single-clock CPU mode); no pixel output
#define TED_CR2_PAL_NTSC     0x40   // PAL/NTSC select (read-only on real hardware): 0 = PAL, 1 = NTSC
#define TED_CR2_RVS          0x80   // Reverse screen: 1 = bit 7 of screen code selects per-character pixel inversion

// ============================================================================
// IRQ STATUS/MASK BITS ($FF09/$FF0A)
// ============================================================================

#define TED_IRQ_RASTER       0x02   // Raster compare match (raster counter == raster compare value)
#define TED_IRQ_LIGHTPEN     0x04   // Light pen trigger (active-low input; latches H/V position on falling edge)
#define TED_IRQ_TIMER1       0x08   // Timer 1 underflow (counter reached 0; auto-reloads from latch)
#define TED_IRQ_TIMER2       0x10   // Timer 2 underflow (counter reached 0; wraps to $FFFF, no reload)
#define TED_IRQ_TIMER3       0x40   // Timer 3 underflow (counter reached 0; wraps to $FFFF, no reload)
#define TED_IRQ_ANY          0x80   // Any IRQ active: set in status read when (irq_status & irq_mask) != 0
#define TED_IRQ_CLEARABLE    0x5E   // Mask of status bits clearable by writing 1s to $FF09
                                    // (bits 1,2,3,4,6 — bit 0 unused, bit 5 unused, bit 7 = ANY is read-only)

// ============================================================================
// GRAPHICS MODES (ECM|BMM|MCM encoding, matching VIC-II convention)
// ============================================================================

#define TED_GM_STANDARD_TEXT       0  // ECM=0 BMM=0 MCM=0: standard 1bpp text; 2 colors per char from attribute
#define TED_GM_MULTICOLOR_TEXT     1  // ECM=0 BMM=0 MCM=1: 2bpp text when attr bit 3 set; double-width pixels
#define TED_GM_STANDARD_BITMAP     2  // ECM=0 BMM=1 MCM=0: hires 1bpp bitmap; per-cell hue+lum from screen block
#define TED_GM_MULTICOLOR_BITMAP   3  // ECM=0 BMM=1 MCM=1: 2bpp bitmap; double-width pixels, 4-color cells
#define TED_GM_ECM_TEXT            4  // ECM=1 BMM=0 MCM=0: extended color text; screen code [7:6] selects BG0-BG3
#define TED_GM_INVALID1            5  // ECM=1 BMM=0 MCM=1: invalid — outputs black (ECM+MCM combination)
#define TED_GM_INVALID2            6  // ECM=1 BMM=1 MCM=0: invalid — outputs black (ECM+BMM combination)
#define TED_GM_INVALID3            7  // ECM=1 BMM=1 MCM=1: invalid — outputs black (all three set)

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

// The TED line consists of 114 TED single-clock cycles = 57 CPU cycles.
// Our tick model operates per CPU cycle (calling tick_phi1 once per cycle),
// so x_cycle counts 0..56.

// PAL timing
#define TED_PAL_CLOCK_HZ            1773448 // TED master clock frequency (= 2 × CPU clock)
#define TED_PAL_CPU_CLOCK_HZ        886724  // CPU clock derived from TED master (÷2)

// NTSC timing
#define TED_NTSC_CLOCK_HZ           1789772 // TED master clock frequency (= 2 × CPU clock)
#define TED_NTSC_CPU_CLOCK_HZ       894886  // CPU clock derived from TED master (÷2)

// Display geometry
#define TED_SCREEN_TEXTCOLS         40      // Visible character columns per text row

// DMA fetch timing (CPU cycles within a line, x_cycle 0..56)
// The DMA window is 43 cycles: 3 setup (BA low, AEC still follows PHI2) + 40 data fetches.
#define TED_FETCH_CYCLE             4       // First DMA CPU cycle (BA goes LOW here)
#define TED_FETCH_END_CYCLE         46      // Last DMA CPU cycle (4 + 3 setup + 40 data - 1)
#define TED_DMA_SETUP_CYCLES        3       // BA-low warning cycles before first g-access (AEC still high)

// DMA line range (TED raster counter values where DMA can occur)
#define TED_FIRST_DMA_LINE          0       // First raster line where DMA is possible
#define TED_LAST_DMA_LINE           0xCB    // Last raster line where DMA is possible (203 decimal)

// Visible area for framebuffer rendering
#define TED_VISIBLE_WIDTH           384     // Framebuffer width in pixels (320 active + left/right borders)
#define TED_VISIBLE_HEIGHT_PAL      288     // PAL framebuffer height (normal border, derived from VICE timing)
#define TED_VISIBLE_HEIGHT_NTSC     242     // NTSC framebuffer height (normal border, derived from VICE timing)

// First TED raster line mapped to framebuffer row 0
#define TED_FIRST_VISIBLE_LINE_PAL  275     // PAL: TED raster 275 → fb row 0 (wraps: last frame lines first)
#define TED_FIRST_VISIBLE_LINE_NTSC 19      // NTSC: TED raster 19 → fb row 0

// ============================================================================
// LINE STATE MACHINE — driven by x_cycle position
// ============================================================================
// Instead of a per-cycle callback table (as VIC-II needs for sprites), the TED
// uses a state enum driven by x_cycle position within the line.  State
// transitions happen at specific x_cycle values.

enum ted_line_state_e {
    TED_STATE_HBLANK,           // Horizontal blanking / retrace region (no pixel output)
    TED_STATE_LEFT_BORDER,      // Left border (between retrace end and display window left edge)
    TED_STATE_DISPLAY,          // Active display area (40 character columns)
    TED_STATE_RIGHT_BORDER,     // Right border (between display window right edge and retrace)
    TED_STATE_IDLE,             // Idle cycles outside display and border (output bg color)
};

// ============================================================================
// TED MEMORY ACCESS TYPES (adapted from VIC-II model)
// ============================================================================

#define TED_ACCESS_IDLE        0   // PHI1: idle access to $FFFF (no useful data)
#define TED_ACCESS_REFRESH     1   // PHI1: DRAM refresh cycle
#define TED_ACCESS_G           2   // PHI1: g-access — fetch character generator or bitmap pixel data
#define TED_ACCESS_C           3   // PHI2: c-access — fetch screen matrix + color attribute (DMA lines only)

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

/**
 * Banking change notification bits.
 * Passed to the banking_change callback to identify which state changed.
 */
static constexpr uint8_t TED_BANK_ROM_LATCH    = 0x01;  // rom_enabled toggled ($FF3E/$FF3F)
static constexpr uint8_t TED_BANK_VIDEO_ROMSEL = 0x02;  // video ROM select changed ($FF12 bit 2)

/**
 * Banking state change callback.
 * Called when CPU-visible ROM/RAM banking ($FF3E/$FF3F) or TED's video
 * ROM select ($FF12 bit 2) changes.  The system uses this to update
 * pre-computed page tables for instant bank switching.
 *
 * @param user_data  Context pointer (typically system instance)
 * @param changes    Bitmask of TED_BANK_* indicating which state changed
 */
typedef void (*ted_banking_change_fn)(void* user_data, uint8_t changes);

// ============================================================================
// DESCRIPTOR
// ============================================================================

struct ted7360_desc_t {
    bool is_pal;                        // true = PAL (312 lines), false = NTSC (262 lines)
    ted_keyboard_scan_fn keyboard_scan; // Keyboard matrix scan callback (may be nullptr)
    void* keyboard_user_data;           // Context pointer passed to keyboard_scan
    ted_mem_read_fn mem_read;           // Memory read callback for TED's own PHI1 accesses
    void* mem_read_user_data;           // Context pointer passed to mem_read
    ted_banking_change_fn banking_change;   // Banking state notification (may be nullptr)
    void* banking_change_user_data;         // Context pointer passed to banking_change
};

// ============================================================================
// UNIT STRUCTURES (following VIC-II unit-based decomposition)
// ============================================================================

// Register storage is provided by ChipBase::regs_[] (TED_NUM_REGS bytes)

// Timing Unit — horizontal/vertical counters
struct ted_timing_unit_t {
    uint16_t raster_counter;            // 9-bit vertical raster counter (0-311 PAL / 0-261 NTSC)
    uint16_t raster_compare;            // Cached 9-bit raster IRQ trigger value (from $FF1A bit 0 + $FF1B)
    uint8_t  x_cycle;                   // CPU-cycle counter within current line (0-56)
    uint16_t x_pixel;                   // Pixel X position = x_cycle × 8 (0-456)
    uint32_t frame_count;               // Monotonically increasing frame counter
    uint16_t lines_per_frame;           // Total raster lines per frame: PAL=312, NTSC=262
    uint16_t first_visible_line;        // First TED raster line mapped to framebuffer row 0
    uint8_t  cpu_cycles_per_line;       // Always 57 (114 TED clocks ÷ 2)
    bool     is_pal;                    // true = PAL timing; false = NTSC timing
};

// Video Logic Unit — display state and DMA line detection
struct ted_video_logic_unit_t {
    bool     display_state;             // true = display window active; false = idle (background color)
    bool     is_dma_line;               // Current line is a DMA line (YSCROLL match + DEN asserted)
    bool     dma_line_occurred;         // Latch: DMA was triggered this line (cleared at end of line)
    bool     den_latched;               // DEN was seen set; gates DMA line detection for the frame
    uint16_t vcbase;                    // VCBASE — video counter base latched at end of each character row
    uint16_t vc;                        // VC — video counter, increments once per g-access (10-bit)
    uint8_t  rc;                        // RC — row counter within character cell (3-bit, 0-7)
    uint8_t  vmli;                      // VMLI — video matrix line index, increments during DMA (0-39)
    uint8_t  refresh_counter;           // 8-bit DRAM refresh counter (decrements each refresh cycle)
};

// Video Data Unit — character and color line buffers (filled during DMA)
struct ted_video_data_unit_t {
    uint8_t screen_line[TED_SCREEN_TEXTCOLS];   // Screen codes (char indices or bitmap hue data)
    uint8_t color_line[TED_SCREEN_TEXTCOLS];    // Color attributes (hue + lum + blink flag)
};

// Graphics Sequencer Unit — shift register and mode-dependent pixel production
struct ted_sequencer_unit_t {
    uint8_t graphics_mode;                      // Cached mode bits (ECM|BMM|MCM) from CR1/CR2; see TED_GM_*
    uint8_t shift_reg;                          // 8-bit graphics shift register (loaded per character cell)
    uint8_t xscroll;                            // XSCROLL delay countdown (pixels remaining before display opens)
    uint8_t char_data[TED_SCREEN_TEXTCOLS];     // g-access pattern data for current character row (chargen or bitmap)
    uint8_t pixel_in_char;                      // Pixel position within current character cell (0-7)
    uint8_t display_vmli;                       // Display-side column counter: which char_data[] slot to show (0-39)
    uint8_t active_display_column;              // Column index whose data is currently in shift_reg
};

// Border Unit — border flip-flops and comparison limits
struct ted_border_unit_t {
    uint16_t top;                       // First display raster line (RSEL=1: 4, RSEL=0: 8)
    uint16_t bottom;                    // Last display raster line (RSEL=1: 0xCB, RSEL=0: 0xC7)
    uint16_t left;                      // Left display pixel (CSEL=1: 24, CSEL=0: 31)
    uint16_t right;                     // Right border open pixel (CSEL=1: 344, CSEL=0: 335)
    bool     main_ff;                   // Main (horizontal) border flip-flop: true = border active
    bool     vert_ff;                   // Vertical border flip-flop: true = border active for entire line
};

// Memory Mapping Unit — address calculation for screen/char/bitmap
struct ted_memory_unit_t {
    uint16_t screen_base;               // Screen matrix base address (derived from $FF14 bits [7:3])
    uint16_t char_base;                 // Character generator base address (derived from $FF13 bits [7:2])
    uint16_t bitmap_base;               // Bitmap base: 0x0000 ($FF14 bit 3 = 0) or 0x2000 (bit 3 = 1)
};

// Timer Unit — single 16-bit countdown timer with latch
struct ted_timer_unit_t {               // Timer counts down once per CPU cycle
    uint16_t counter;                   // Current 16-bit counter (decrements each CPU cycle)
    uint16_t latch;                     // Reload value written by software (timer 1 auto-reloads; 2/3 do not)
};
// Note: typedef TedTimer used by tick_one_timer helper in .cpp

// Bus Interface Unit — pending DMA access and BA/AEC state
struct ted_bus_unit_t {
    ted_mem_read_fn mem_read;           // Memory read callback for TED PHI1 accesses
    void* mem_read_user_data;           // Context pointer for mem_read
    uint8_t pending_access;             // TED_ACCESS_* type set in PHI1 for delivery in PHI2
    uint16_t pending_address;           // Address driven onto bus for PHI2 c-access
    bool ba_low;                        // BA signal state: true = TED asserting BA (CPU warning)
    uint8_t ba_low_count;               // Consecutive CPU cycles BA has been LOW (AEC stolen after 3)
};

// ============================================================================
// SOUND UNIT — TED 2-channel audio (square wave + noise)
// ============================================================================
//
// The TED Sound Unit contains two voices:
//   Channel 1: 10-bit frequency counter, square wave output.
//              Freq low byte from $FF0E, high bits [9:8] from $FF12 bits [1:0].
//   Channel 2: 10-bit frequency counter, square wave OR noise output.
//              Freq low byte from $FF0F, high bits [9:8] from $FF10 bits [1:0].
//
// Counters decrement at the TED master clock rate (= 2 × CPU clock).
// On underflow the counter reloads from (1024 - freq_value) and the output
// toggles (square wave) or the 8-bit LFSR is clocked (noise mode).
//
// Control register ($FF11):
//   bits [3:0] — volume (0-8; values 9-15 clamp to 8)
//   bit 4      — channel 1 enable
//   bit 5      — channel 2 enable
//   bit 6      — noise mode: channel 2 outputs LFSR instead of square wave
//                (noise is active when bit 6 set AND bit 5 clear; if both
//                 bit 5 and bit 6 are set, channel 2 outputs square wave)
//   bit 7      — DA converter mode (voices hold high, volume acts as DAC)
//
// LFSR: 8-bit, left-shifting, taps at bits 7,5,4,1 (matching VICE/YAPE
//       analysis).  Clocked by channel 2's counter underflow.  When noise
//       is active, bit 0 of the shift register drives the output.

static constexpr uint32_t TED_AUDIO_BUFFER_SIZE = 2048;

// Sound control register bit masks ($FF11)
#define TED_SND_VOLUME_MASK    0x0F   // bits [3:0]: volume (0-8, 9-15 same as 8)
#define TED_SND_CH1_ENABLE     0x10   // bit 4: channel 1 (square wave) enable
#define TED_SND_CH2_ENABLE     0x20   // bit 5: channel 2 enable
#define TED_SND_NOISE_ENABLE   0x40   // bit 6: noise mode for channel 2
#define TED_SND_DA_MODE        0x80   // bit 7: DA converter mode

struct ted_sound_unit_t {
    // --- Decoded register state (updated on register write) ---
    uint16_t freq1;                     // Channel 1 frequency (10-bit: $FF0E + $FF12 bits [1:0])
    uint16_t freq2;                     // Channel 2 frequency (10-bit: $FF0F + $FF10 bits [1:0])
    uint8_t  volume;                    // Output volume (4-bit, from $FF11 bits [3:0])
    bool     ch1_enabled;               // Channel 1 output enable ($FF11 bit 4)
    bool     ch2_enabled;               // Channel 2 output enable ($FF11 bit 5)
    bool     noise_enabled;             // Channel 2 noise mode ($FF11 bit 6 set, bit 5 clear)
    bool     da_mode;                   // DA converter mode ($FF11 bit 7)

    // --- Runtime oscillator state ---
    uint16_t ch1_counter;               // Channel 1 period counter (10-bit, counts down at TED clock)
    uint16_t ch2_counter;               // Channel 2 period counter (10-bit, counts down at TED clock)
    bool     ch1_output;                // Channel 1 current square wave level (toggled on underflow)
    bool     ch2_output;                // Channel 2 current square wave level (toggled on underflow)
    uint8_t  noise_shift_reg;           // 8-bit LFSR for noise generation

    // --- Downsampling state ---
    uint32_t sample_accum;              // Accumulated mix table values between output samples
    uint32_t sample_tick_count;         // TED clock ticks accumulated
    uint32_t cycles_per_sample_fp;      // Fixed-point 16.16: TED clocks per output sample
    uint32_t sample_frac;               // Fractional accumulator for sample timing (16.16)

    // --- Analog output stage (first-order IIR filters) ---
    float    lowpass_buf;               // Lowpass filter state
    float    highpass_buf;              // Highpass filter state
    float    lowpass_alpha;             // Lowpass coefficient
    float    highpass_alpha;            // Highpass coefficient
    float    output_gain;               // Maps filtered output to float range

    // --- Output ring buffer (mono float, -1.0..+1.0) ---
    AudioRingBuffer audio_buffer{TED_AUDIO_BUFFER_SIZE};
};

// Alias used by tick_one_timer() in ted7360.cpp
using TedTimer = ted_timer_unit_t;

// ============================================================================
// TED 7360 MAIN STRUCTURE
// ============================================================================

struct ted7360_t : public VideoChipBase {
    // ========================================================================
    // Public API — Lifecycle
    // ========================================================================

    /** Construct and initialize a TED 7360 instance. */
    explicit ted7360_t(const ted7360_desc_t& desc);

    /** Destructor — frees internal color index line buffer. */
    ~ted7360_t();

    // Non-copyable, non-movable (owns color_line_ allocation)
    ted7360_t(const ted7360_t&) = delete;
    ted7360_t& operator=(const ted7360_t&) = delete;

    /** Reset TED to power-on state (preserves callbacks and framebuffer). */
    void reset();

    // ========================================================================
    // Public API — Cycle-accurate tick (phi1/phi2 model)
    // ========================================================================

    /**
     * PHI1 phase — one full CPU cycle of TED processing.
     *
     * Handles: DMA window detection, BA/AEC/RDY signaling, g-access memory read,
     * c-access address setup, pixel sequencing (8 pixels per call), border
     * flip-flop update, timer countdown, IRQ generation, timing advance,
     * RC/VCBASE update, and PHI2 bus address drive for c-access.
     *
     * @param bus_state  System bus state entering PHI1
     * @return           Updated bus state with TED signals (IRQ, BA, AEC, RDY, PHI2 addr)
     */
    bus_state_t tick_phi1(bus_state_t bus_state);

    /**
     * PHI2 delivery — receives memory data fetched for c-access.
     *
     * On DMA lines: reads bus data (screen matrix code delivered by memory system)
     * and stores it in video_data.screen_line[].  No-op on non-DMA lines.
     *
     * @param bus_state  Bus state after memory service (DATA field contains fetched byte)
     */
    void tick_phi2(bus_state_t bus_state);

    // ========================================================================
    // Public API — Register I/O
    // ========================================================================

    /** Read a TED register; address encodes offset within $FF00-$FF3F. */
    bus_state_t registers_read(bus_state_t bus_state);

    /** Write a TED register; address encodes offset within $FF00-$FF3F. */
    bus_state_t registers_write(bus_state_t bus_state);

    // ========================================================================
    // ChipBase MMIO interface — enables Board bus dispatch for TED registers
    // ========================================================================

    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read (bus_state_t bus) noexcept override { return registers_read(bus); }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override { return registers_write(bus); }

    // ========================================================================
    // Public API — IRQ query
    // ========================================================================

    /** Returns true if any enabled IRQ source is currently pending. */
    [[nodiscard]] bool irq_pending() const;

    // ========================================================================
    // Public API — Framebuffer
    // ========================================================================

    /** Set the display output target (called by system during init). */
    void set_display(IndexedFrameBuffer* d) { display_ = d; }

    // ========================================================================
    // Public API — Color palette (compile-time computed, rodata)
    // ========================================================================

    /** Returns pointer to the 128-entry TED RGBA palette (16 hues × 8 luminances). */
    [[nodiscard]] static const uint32_t* get_palette();

    // ========================================================================
    // Public API — Audio
    // ========================================================================

    /** Initialize audio subsystem.  Call once after construction or when sample rate changes. */
    void audio_reset(uint32_t ted_clock_hz, uint32_t sample_rate_hz);

    /** Returns number of audio samples available in the ring buffer. */
    [[nodiscard]] uint32_t audio_available() const;

    /** Read up to max_samples float samples from the ring buffer.  Returns actual count. */
    uint32_t audio_read(float* dest, uint32_t max_samples);

    // ========================================================================
    // Public data — Unit structures
    // ========================================================================

    ted_timing_unit_t      timing;
    ted_video_logic_unit_t video_logic;
    ted_video_data_unit_t  video_data;
    ted_sequencer_unit_t   sequencer;
    ted_border_unit_t      border;
    ted_memory_unit_t      memory;
    TedTimer               timer1;      // Auto-reload on underflow from latch
    TedTimer               timer2;      // No auto-reload: wraps to $FFFF on underflow
    TedTimer               timer3;      // No auto-reload: wraps to $FFFF on underflow
    ted_sound_unit_t       sound;
    ted_bus_unit_t         bus;

    // Display output (non-owning pointer set by system)
    IndexedFrameBuffer*    display_ = nullptr;
    uint8_t*               color_line_ = nullptr;  // Per-pixel palette index buffer (owned)

    // IRQ state (mirrors register file but kept separate for quick access)
    uint8_t irq_status = 0;             // Latched IRQ source bits (see TED_IRQ_*)
    uint8_t irq_mask   = 0;             // Enabled IRQ source bits (see TED_IRQ_*)

    // Memory banking
    bool rom_enabled = false;           // true = ROM bank visible; false = RAM visible

    // Banking change notification
    ted_banking_change_fn banking_change      = nullptr;
    void*                 banking_change_user_data = nullptr;

    // Keyboard
    ted_keyboard_scan_fn keyboard_scan      = nullptr; // Keyboard matrix scan callback
    void*                keyboard_user_data = nullptr; // Context for keyboard_scan
    uint8_t              keyboard_latch     = 0;       // Last value written to $FF08 (column select)

    // Flash / cursor blink (5-bit cursor_phase, same as VICE)
    uint8_t flash_counter  = 0;         // Bits [3:0] = 4-bit counter (increments per frame); bit 4 = visibility toggle
    bool    cursor_visible = false;     // true when flash phase bit 4 is set (cursor and blink-text visible)

    /**
     * Compute the 10-bit hardware cursor position from register state.
     * Bits [9:8] from $FF0C bits [1:0], bits [7:0] from $FF0D.
     * Valid range: 0..999 (40 columns × 25 rows).
     */
    [[nodiscard]] uint16_t get_cursor_position() const noexcept {
        return static_cast<uint16_t>(
             regs_[TED_REG_CURSOR_LO]
           | ((regs_[TED_REG_CURSOR_HI] & 0x03u) << 8));
    }

    // Reverse mode
    bool reverse_mode = false;          // RVS bit ($FF07 bit 7): bit 7 of screen code inverts per-char pixels

    // Timer phase — TED timers count at TED single-clock rate (2× CPU clock).
    // We toggle a phase flag each CPU cycle and only decrement on one phase,
    // effectively halving the decrement rate to match real hardware.
    bool timer_tick_phase = false;

private:
    // ========================================================================
    // Internal helpers
    // ========================================================================

    void    update_memory_addresses();  // Recompute screen_base/char_base/bitmap_base from $FF12-$FF14
    void    update_border_limits();     // Recompute border top/bottom/left/right from CR1/CR2 RSEL/CSEL bits
    void    update_dma_condition();     // Evaluate whether current raster line is a DMA line
    void    check_raster_interrupt();   // Fire TED_IRQ_RASTER if raster_counter == raster_compare
    void    tick_timers();              // Decrement all three timers; fire IRQs on underflow
    void    audio_tick();               // Clock sound oscillators (2 ticks per CPU cycle) and downsample
    void    pixel_sequencer();          // Produce 8 pixels for current x_cycle; update border flip-flops
    void    flush_line(uint16_t raster_line); // Resolve color_line[] indices to RGBA in framebuffer
    void    timing_advance();           // Advance x_cycle; handle end-of-line and end-of-frame

    uint8_t  get_graphics_mode() const; // Extract current ECM|BMM|MCM mode from CR1/CR2
    uint16_t get_raster_compare() const;// Read 9-bit raster compare from $FF1A bit 0 + $FF1B

    // Legacy single-tick wrapper (alternates PHI1/PHI2 for callers using 2× clock rate)
    void tick();
    uint8_t legacy_subcycle_ = 0;       // 0 = PHI1, 1 = PHI2

    // --- ChipBase interface ---
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override;
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

#ifdef CERMU_HAS_GUI
    bus_state_t bus_snapshot_ = {};     // Last bus state captured at end of PHI2 (for debugger)
#endif
};