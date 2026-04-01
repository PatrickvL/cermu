#pragma once
/*
 * crtc_common.hpp — Unified CRTC/VDC chip family implementation
 *
 * Single template-driven implementation covering the full MC6845 lineage:
 * from the base Motorola MC6845 through second sources (R6545, HD6845,
 * UM6845, EF6845) to the MOS 8563/8568 VDC used in the Commodore 128.
 *
 * Architecture:
 *   crtc_base_t              — non-template base with all CRTC + VDC state
 *   crtc_t<const CRTCTraits&> — thin NTTP wrapper for type-safe variant dispatch
 *
 * The base class carries state for ALL variants (including VDC DRAM/DMA).
 * For plain 6845 instances the VDC fields are dormant — the overhead is
 * ~200 bytes of unused state, negligible since there's one chip per system.
 *
 * I/O interface:
 *   6845 variants:  two registers at base+0 (address) and base+1 (data)
 *   8563/8568 VDC:  $D600 (RS=0: status/address) and $D601 (RS=1: data)
 *                   All 37 registers accessed indirectly through the latch.
 */

#include "chip/video/fam6845/crtc_traits.hpp"
#include "chip/video/video_chip_base.hpp"
#include "core/indexed_frame_buffer.hpp"
#include "core/signal/rgbi_video_out.hpp"
#include <cstdint>
#include <functional>

// ============================================================================
// REGISTER TABLE — superset covering both CRTC (R0–R17) and VDC (R18–R36)
// ============================================================================
//
// The base MC6845 has 18 registers (R0-R17).  The MOS 8563/8568 VDC extends
// this to 37 registers (R0-R36).  R0-R17 are shared and semantically
// identical across all variants.  R18-R36 exist only on VDC chips.
//
// All registers are stored in ChipBase::regs_[].  Plain 6845 variants
// init with 18 registers; VDC variants init with 37.

#define FAM6845_DECL(REG, FLD, CMP) \
    /* ── Base CRTC registers (R0-R17, shared) ─────────────────────── */ \
    REG( 0, R0_HTOTAL,            "Horiz total chars-1")                    \
    REG( 1, R1_HDISPLAYED,        "Horiz displayed chars")                  \
    REG( 2, R2_HSYNC_POS,         "Horiz sync position")                    \
    REG( 3, R3_SYNC_WIDTHS,       "H/V sync widths")                        \
      FLD(R3_SYNC_WIDTHS, HSYNC_W, 3:0, "HSYNC width (chars)",  Value, 0, 0) \
      FLD(R3_SYNC_WIDTHS, VSYNC_W, 7:4, "VSYNC width (rows)",   Value, 0, 0) \
    REG( 4, R4_VTOTAL,            "Vert total rows-1")                      \
    REG( 5, R5_VADJUST,           "Vert fine adjust")                       \
    REG( 6, R6_VDISPLAYED,        "Vert displayed rows")                    \
    REG( 7, R7_VSYNC_POS,         "Vert sync position")                     \
    REG( 8, R8_MODE_CTRL,         "Mode/interlace")                         \
    REG( 9, R9_MAX_SCANLINE,      "Max raster address")                     \
    REG(10, R10_CURSOR_START,     "Cursor start scan")                      \
      FLD(R10_CURSOR_START, CURSOR_MODE, 6:5, "Cursor blink mode", Value, 0, 0) \
      FLD(R10_CURSOR_START, CURSOR_SL,   4:0, "Cursor scan line",  Value, 0, 0) \
    REG(11, R11_CURSOR_END,       "Cursor end scan")                        \
    REG(12, R12_START_ADDR_HI,    "Display start hi")                       \
    REG(13, R13_START_ADDR_LO,    "Display start lo")                       \
    REG(14, R14_CURSOR_HI,        "Cursor position hi")                     \
    REG(15, R15_CURSOR_LO,        "Cursor position lo")                     \
    REG(16, R16_LPEN_HI,          "Light pen hi (RO)")                      \
    REG(17, R17_LPEN_LO,          "Light pen lo (RO)")                      \
    /* ── VDC extension registers (R18-R36, 8563/8568 only) ────────── */ \
    REG(18, R18_UPDATE_ADDR_HI,   "Update address hi")                      \
    REG(19, R19_UPDATE_ADDR_LO,   "Update address lo")                      \
    REG(20, R20_ATTR_ADDR_HI,     "Attribute start hi")                     \
    REG(21, R21_ATTR_ADDR_LO,     "Attribute start lo")                     \
    REG(22, R22_CHAR_DISP_HZ,     "Char total disp Hz")                     \
      FLD(R22_CHAR_DISP_HZ, CHAR_HZ_TOTAL,     7:4, "Char horiz total",  Value, 0, 0) \
      FLD(R22_CHAR_DISP_HZ, CHAR_HZ_DISPLAYED, 3:0, "Char horiz disp",   Value, 0, 0) \
    REG(23, R23_CHAR_DISP_VT,     "Char vert displayed")                    \
    REG(24, R24_VSCROLL,          "Vert smooth scroll")                     \
      FLD(R24_VSCROLL, BLOCK_COPY,   7:7, "Block copy mode",    Flag, 0, 0) \
      FLD(R24_VSCROLL, REVERSE,      6:6, "Reverse screen",     Flag, 0, 0) \
      FLD(R24_VSCROLL, VSCROLL_VAL,  4:0, "V scroll value",     Value, 0, 0) \
    REG(25, R25_HSCROLL,          "Horiz smooth scroll")                    \
      FLD(R25_HSCROLL, HSCROLL_VAL,  7:4, "H scroll value",     Value, 0, 0) \
      FLD(R25_HSCROLL, DOUBLE_PIX,   4:4, "Double pixel mode",  Flag, 0, 0) \
      FLD(R25_HSCROLL, SEMI_GRAPH,   5:5, "Semi-graphic mode",  Flag, 0, 0) \
      FLD(R25_HSCROLL, ATTR_ENABLE,  6:6, "Attribute enable",   Flag, 0, 0) \
      FLD(R25_HSCROLL, TEXT_MODE,     7:7, "Text/bitmap mode",   Flag, 0, 0) \
    REG(26, R26_FGBG_COLOR,       "FG/BG colour")                           \
      FLD(R26_FGBG_COLOR, FG_COLOR, 7:4, "Foreground colour",   Value, 0, 0) \
      FLD(R26_FGBG_COLOR, BG_COLOR, 3:0, "Background colour",   Value, 0, 0) \
    REG(27, R27_ROW_INC,          "Address increment per row")              \
    REG(28, R28_CHARSET_BASE,     "Char set start address")                 \
      FLD(R28_CHARSET_BASE, CHARSET_ADDR, 7:4, "Charset base addr", Value, 0, 0) \
      FLD(R28_CHARSET_BASE, RAM_TYPE,     3:0, "RAM type/refresh",  Value, 0, 0) \
    REG(29, R29_UNDERLINE,        "Underline scan line")                    \
    REG(30, R30_WORD_COUNT,       "Word count (DMA)")                       \
    REG(31, R31_DATA_PORT,        "Data port (R/W)")                        \
    REG(32, R32_BLOCK_SRC_HI,     "Block copy source hi")                   \
    REG(33, R33_BLOCK_SRC_LO,     "Block copy source lo")                   \
    REG(34, R34_DISP_BEGIN,       "Display begin")                          \
    REG(35, R35_DISP_END,         "Display end")                            \
    REG(36, R36_DRAM_REFRESH,     "DRAM refresh rate")

// --- Extract address constants ---
namespace fam6845 {
namespace reg {
    FAM6845_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr int NUM_CRTC_REGISTERS = 18;  // Base MC6845
    constexpr int NUM_VDC_REGISTERS  = 37;  // MOS 8563/8568
} // namespace reg
} // namespace fam6845

DECL_EXTRACT(FAM6845, FAM6845_DECL)

// --- Bitfield accessors ---
namespace fam6845 { namespace fld {
#define FAM6845_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
FAM6845_DECL(DECL_REG_NOP, FAM6845_X_FLD_NS_, DECL_CMP_NOP)
#undef FAM6845_X_FLD_NS_
} } // namespace fam6845::fld

// Import register constants into file scope for convenient access
using namespace fam6845::reg;

// ============================================================================
// crtc_base_t — non-template base for all CRTC / VDC variants
// ============================================================================
//
// Contains ALL state for the full superset (including VDC DRAM/DMA).
// For plain 6845 instances the VDC-specific fields are dormant.
// The template wrapper crtc_t<Traits> provides variant-specific init
// and identity; all tick/render code operates on crtc_base_t*.

struct crtc_base_t : public VideoChipBase {
    crtc_base_t() { category_ = "Video"; }
    explicit crtc_base_t(ChipInfo info) : VideoChipBase(std::move(info)) {}

    // --- ChipBase bus interface (MMIO) ---
    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override;
    bus_state_t on_bus_write(bus_state_t bus) noexcept override;

    // --- CS-tick: character clock + MMIO self-dispatch ---
    bus_state_t tick_mmio(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    /// Combined character-clock tick + CS-conditional register access.
    /// Convenience for systems that run the CRTC at character-clock rate
    /// and want MMIO dispatch in the same call (e.g. PET).
    bus_state_t tick(bus_state_t bus) noexcept {
        tick();
        return tick_mmio(bus);
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // TRAITS (runtime copy — set by crtc_t<> constructor)
    // ========================================================================

    const CRTCTraits* traits_ = nullptr;

    // ========================================================================
    // REGISTERS — two-step access model
    // ========================================================================

    uint8_t address_register = 0;   // Currently selected register index
    uint8_t status_register  = 0;   // Status (VDC: bit 7=ready, 6=lpen, 5=vsync)

    // ========================================================================
    // CRTC CORE COUNTERS (shared by all variants)
    // ========================================================================

    // Horizontal timing
    uint8_t h_char_counter    = 0;     // Character counter within line (0 to R0)
    uint8_t h_sync_counter    = 0;     // HSYNC duration counter
    bool    h_sync_active     = false; // HSYNC output
    bool    h_display_active  = false; // Horizontal display enable

    // Vertical timing
    uint8_t v_row_counter     = 0;     // Character row counter (0 to R4)
    uint8_t v_scanline_counter = 0;    // Scan line counter within row (0 to R9)
    uint8_t v_adjust_counter  = 0;     // Vertical adjust scan line counter
    uint8_t v_sync_counter    = 0;     // VSYNC duration counter
    bool    v_sync_active     = false; // VSYNC output
    bool    v_display_active  = false; // Vertical display enable
    bool    in_adjust         = false; // Currently in vertical adjust phase

    // Display address
    uint16_t linear_address    = 0;    // Current display memory address (MA0–MA13)
    uint16_t row_start_address = 0;    // Address at start of current character row

    // Cursor
    uint8_t cursor_blink_counter = 0;  // Frame counter for cursor blink
    bool    cursor_visible    = true;  // Current cursor visibility

    // Light pen
    bool     light_pen_latched = false;
    uint16_t light_pen_address = 0;

    // Frame counter
    uint32_t frame_count = 0;

    // ========================================================================
    // VDC-SPECIFIC STATE (dormant on plain 6845 variants)
    // ========================================================================

    // ── Private DRAM subsystem ───────────────────────────────────────
    uint8_t* vram_           = nullptr;  // Pointer to private VRAM (system-supplied)
    uint32_t vram_size_      = 0;        // Actual VRAM size (16384 or 65536)
    uint16_t vram_addr_mask_ = 0x3FFF;   // Address mask (0x3FFF for 16K, 0xFFFF for 64K)

    // ── DRAM access arbitration ──────────────────────────────────────
    // The VDC arbitrates between display fetches and CPU register access.
    // Display always wins.  CPU accesses wait for a DRAM slot.

    uint16_t update_addr_     = 0;     // Internal address register (from R18:R19)
    uint8_t  read_latch_      = 0;     // Prefetched byte (from VRAM[update_addr_])
    bool     ready_           = true;  // Status bit 7: ready for CPU access
    uint8_t  dram_wait_       = 0;     // Clock cycles remaining for current DRAM op

    // ── Block copy / fill DMA engine ─────────────────────────────────
    bool     block_copy_armed_ = false;  // R24 bit 7 sets this
    uint16_t block_src_addr_   = 0;      // From R32:R33
    uint16_t block_count_      = 0;      // From R30 (byte count - 1)
    bool     dma_active_       = false;  // DMA transfer in progress
    uint16_t dma_remaining_    = 0;      // Bytes remaining in current DMA
    uint8_t  dma_latch_        = 0;      // Current byte being transferred

    // ── Smooth scroll (latched at appropriate boundaries) ────────────
    uint8_t  hscroll_latched_  = 0;      // H scroll value (latched at HBLANK)
    uint8_t  vscroll_latched_  = 0;      // V scroll value (latched at frame start)

    // ── RGBI video output (VDC only) ─────────────────────────────────
    RGBIVideoOut*   video_out_ = nullptr;  // RGBI signal output (set by system)
    NoRGBIVideoOut  no_video_out_;         // Stub for disconnected state

    // ── Pixel rendering state (VDC only) ─────────────────────────────
    uint16_t pixel_x_          = 0;      // Current pixel X within scan line
    uint16_t pixels_per_line_  = 0;      // Total pixels per line (from timing)

    // ========================================================================
    // CALLBACKS — set by the host system (for non-VDC variants)
    // ========================================================================
    //
    // Plain 6845 variants do not produce video themselves — the host system
    // reads character addresses and renders pixels.  VDC variants drive the
    // RGBI output directly from private VRAM.

    using display_callback_t = std::function<void(uint16_t ma, uint8_t ra, bool cursor)>;
    display_callback_t on_display_char = nullptr;

    using vsync_callback_t = std::function<void()>;
    vsync_callback_t on_vsync = nullptr;

    using hsync_callback_t = std::function<void()>;
    hsync_callback_t on_hsync = nullptr;

    // ========================================================================
    // INDEXED CHARACTER RENDERING (optional, for non-VDC variants)
    // ========================================================================
    //
    // When configured, the CRTC renders character pixels as palette indices
    // directly during tick(), eliminating the need for a per-character
    // display callback.  Used by PET, BBC Micro (text modes), etc.
    //
    // VDC variants ignore this — they render directly to the RGBI output.

    IndexedFrameBuffer* char_render_display_       = nullptr;
    uint8_t*            char_render_indices_        = nullptr;
    const uint8_t*      char_render_rom_            = nullptr;
    const uint8_t*      char_render_vram_           = nullptr;
    int                 char_render_cols_            = 40;
    int                 char_render_char_h_          = 8;
    int                 char_render_fb_w_            = 320;
    int                 char_render_fb_h_            = 200;
    uint8_t             char_render_fg_              = 1;
    uint8_t             char_render_bg_              = 0;
    const uint32_t*     char_render_palette_         = nullptr;
    int                 char_render_palette_size_     = 0;
    uint16_t            char_render_vram_mask_       = 0x03FF;
    uint8_t             char_render_invert_bit_      = 0x80;

    /// Configure indexed character rendering.  Call once after init().
    void configure_char_render(IndexedFrameBuffer* display, uint8_t* indices,
                               const uint8_t* rom, const uint8_t* vram,
                               int cols, int char_h, int fb_w,
                               uint8_t fg, uint8_t bg,
                               const uint32_t* palette, int palette_size,
                               uint16_t vram_mask = 0x03FF,
                               uint8_t invert_bit = 0x80) {
        char_render_display_       = display;
        char_render_indices_       = indices;
        char_render_rom_           = rom;
        char_render_vram_          = vram;
        char_render_cols_          = cols;
        char_render_char_h_        = char_h;
        char_render_fb_w_          = fb_w;
        char_render_fb_h_          = display ? display->height() : (char_h * 25);
        char_render_fg_            = fg;
        char_render_bg_            = bg;
        char_render_palette_       = palette;
        char_render_palette_size_  = palette_size;
        char_render_vram_mask_     = vram_mask;
        char_render_invert_bit_    = invert_bit;
    }

    // ========================================================================
    // VDC-specific interface
    // ========================================================================

    /// Bind private VRAM buffer (call from system init for VDC variants).
    void bind_vram(uint8_t* vram, uint32_t size) {
        vram_      = vram;
        vram_size_ = size;
        vram_addr_mask_ = static_cast<uint16_t>(size - 1);
    }

    /// Bind RGBI video output (call from system init for VDC variants).
    void set_video_out(RGBIVideoOut* out) { video_out_ = out; }

    // ========================================================================
    // CORE INTERFACE (implemented in crtc_common.cpp)
    // ========================================================================

    /// Initialize to power-on state.  Must be called after traits_ is set.
    void init();

    /// Reset to power-on state.
    void reset() override;

    /// Register read (address bit 0 selects address vs data register).
    /// For VDC: RS=0 reads status, RS=1 reads data.
    uint8_t read(uint16_t addr);

    /// Register write.
    /// For VDC: RS=0 writes address register, RS=1 writes data register.
    void write(uint16_t addr, uint8_t data);

    /// Advance the CRTC by one character clock.
    /// For plain 6845: call once per character clock (typically 1 MHz).
    /// For VDC: call once per character clock (1 MHz, derived from 16 MHz).
    void tick();

    // ── VDC DRAM operations (called internally) ──────────────────────

    /// Service one DRAM cycle: display fetch, CPU access, or DMA step.
    void dram_cycle();

    /// Start a prefetch into read_latch_ from VRAM[update_addr_].
    void prefetch();

    /// Execute a block copy/fill DMA step.
    void dma_step();

    // ── VDC pixel rendering ──────────────────────────────────────────

    /// Render one character cell (8 or 16 pixels) to the RGBI output.
    void render_char(uint16_t screen_addr, uint8_t scanline);

    // ── Derived state queries ────────────────────────────────────────

    inline uint8_t  chars_per_line()    const { return regs_[R0_HTOTAL] + 1; }
    inline uint8_t  visible_chars()     const { return regs_[R1_HDISPLAYED]; }
    inline uint8_t  rows_per_frame()    const { return regs_[R4_VTOTAL] + 1; }
    inline uint8_t  visible_rows()      const { return regs_[R6_VDISPLAYED]; }
    inline uint8_t  scanlines_per_row() const { return regs_[R9_MAX_SCANLINE] + 1; }

    inline uint16_t start_address() const {
        return (static_cast<uint16_t>(regs_[R12_START_ADDR_HI]) << 8) |
               regs_[R13_START_ADDR_LO];
    }
    inline uint16_t cursor_address() const {
        return (static_cast<uint16_t>(regs_[R14_CURSOR_HI]) << 8) |
               regs_[R15_CURSOR_LO];
    }
    inline uint16_t update_address() const {
        return (static_cast<uint16_t>(regs_[R18_UPDATE_ADDR_HI]) << 8) |
               regs_[R19_UPDATE_ADDR_LO];
    }
    inline uint16_t attribute_address() const {
        return (static_cast<uint16_t>(regs_[R20_ATTR_ADDR_HI]) << 8) |
               regs_[R21_ATTR_ADDR_LO];
    }
    inline uint16_t block_source_address() const {
        return (static_cast<uint16_t>(regs_[R32_BLOCK_SRC_HI]) << 8) |
               regs_[R33_BLOCK_SRC_LO];
    }
    inline bool is_display_active() const {
        return h_display_active && v_display_active;
    }
    inline uint16_t charset_base() const {
        return static_cast<uint16_t>((regs_[R28_CHARSET_BASE] >> 4) & 0x0F) << 12;
    }

private:
    // ── VDC-specific register write handlers ─────────────────────────
    void vdc_write_register(uint8_t reg, uint8_t data);
    uint8_t vdc_read_register(uint8_t reg);

    // ── CRTC core register access ────────────────────────────────────
    void crtc_write_register(uint8_t reg, uint8_t data);
    uint8_t crtc_read_register(uint8_t reg);

    // ── Indexed character rendering (internal) ───────────────────────
    void render_indexed_char(uint16_t screen_offset, uint8_t scanline, bool at_cursor);

#ifdef CERMU_HAS_CHIP_DEBUG
protected:
    void register_debug_fields();
private:
#endif
};

// ============================================================================
// crtc_t<CRTCTraits> — NTTP-driven template wrapper
// ============================================================================
//
// Thin wrapper that inherits all state and implementation from crtc_base_t.
// The template parameter bakes variant-specific constants into the type,
// enabling type-safe MC6845 / HD6845 / MOS8563 / etc. distinction.
//
// Usage:
//   inline constexpr CRTCTraits MC6845_traits = { ... };
//   using mc6845_t = crtc_t<MC6845_traits>;
//
//   inline constexpr CRTCTraits MOS8563_traits = { ... };
//   using mos8563_t = crtc_t<MOS8563_traits>;

template<const CRTCTraits& Traits>
struct crtc_t : public crtc_base_t {
    static constexpr const CRTCTraits& traits = Traits;

    crtc_t() : crtc_base_t(ChipInfo{Traits.chip_id, Traits.vendor, Traits.chip_name}) {
        traits_ = &Traits;
        init_regs(Traits.num_registers);

        // VDC variants: set initial status with version bits
        if constexpr (Traits.has_private_dram) {
            if constexpr (Traits.max_vram_size > 16384)
                status_register = fam6845::STATUS_READY | fam6845::VDC_VERSION_8568;
            else
                status_register = fam6845::STATUS_READY | fam6845::VDC_VERSION_8563;
        }

#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    /// Initialize with variant-specific configuration.
    void init() {
        crtc_base_t::init();
    }
};
