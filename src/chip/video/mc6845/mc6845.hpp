#pragma once
/*
 * mc6845.h — Motorola MC6845 CRT Controller (CRTC)
 *
 * Generic implementation of the MC6845 (and pin-compatible variants:
 * Hitachi HD46505, UMC UM6845, Rockwell 6545, Synertek SY6545).
 *
 * The MC6845 generates timing and addressing for raster-scanned CRT displays.
 * It does NOT produce pixel data — it provides row/column addresses and sync
 * signals.  The host system combines these with character ROM lookup and
 * attribute RAM to produce the display.
 *
 * Used in: Commodore PET/CBM, BBC Micro, Amstrad CPC, many S-100 systems.
 *
 * I/O interface: two registers at base+0 (address) and base+1 (data).
 */

#include "chip/video/video_chip_base.hpp"
#include "core/indexed_frame_buffer.hpp"
#include <cstdint>
#include <functional>

// ============================================================================
// MC6845 REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 18 registers, 4 fields (R3 sync widths, R10 cursor mode)
#define MC6845_DECL(REG, FLD, CMP) \
    REG( 0, R0_HTOTAL,         "Horiz total chars-1")                     \
    REG( 1, R1_HDISPLAYED,     "Horiz displayed chars")                   \
    REG( 2, R2_HSYNC_POS,      "Horiz sync position")                     \
    REG( 3, R3_SYNC_WIDTHS,    "H/V sync widths")                         \
      FLD(R3_SYNC_WIDTHS, HSYNC_W, 3:0, "HSYNC width (chars)", Value, 0, 0) \
      FLD(R3_SYNC_WIDTHS, VSYNC_W, 7:4, "VSYNC width (rows)",  Value, 0, 0) \
    REG( 4, R4_VTOTAL,         "Vert total rows-1")                       \
    REG( 5, R5_VADJUST,        "Vert fine adjust")                        \
    REG( 6, R6_VDISPLAYED,     "Vert displayed rows")                     \
    REG( 7, R7_VSYNC_POS,      "Vert sync position")                      \
    REG( 8, R8_MODE_CTRL,      "Mode/interlace")                          \
    REG( 9, R9_MAX_SCANLINE,   "Max raster address")                      \
    REG(10, R10_CURSOR_START,  "Cursor start scan")                       \
      FLD(R10_CURSOR_START, CURSOR_MODE, 6:5, "Cursor blink mode", Value, 0, 0) \
      FLD(R10_CURSOR_START, CURSOR_SL,   4:0, "Cursor scan line",  Value, 0, 0) \
    REG(11, R11_CURSOR_END,    "Cursor end scan")                         \
    REG(12, R12_START_ADDR_HI, "Display start hi")                        \
    REG(13, R13_START_ADDR_LO, "Display start lo")                        \
    REG(14, R14_CURSOR_HI,     "Cursor position hi")                      \
    REG(15, R15_CURSOR_LO,     "Cursor position lo")                      \
    REG(16, R16_LPEN_HI,       "Light pen hi (RO)")                       \
    REG(17, R17_LPEN_LO,       "Light pen lo (RO)")

// --- Extract address constants ---
namespace mc6845 {
namespace reg {
    MC6845_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr int NUM_REGISTERS = 18;
} // namespace reg
} // namespace mc6845

DECL_EXTRACT(MC6845, MC6845_DECL)

// --- Bitfield accessors ---
namespace mc6845 { namespace fld {
#define MC6845_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
MC6845_DECL(DECL_REG_NOP, MC6845_X_FLD_NS_, DECL_CMP_NOP)
#undef MC6845_X_FLD_NS_
} } // namespace mc6845::fld

using namespace mc6845::reg;

// ============================================================================
// MC6845 CHIP STRUCTURE
// ============================================================================

struct mc6845_t : public VideoChipBase {
    mc6845_t() : VideoChipBase(ChipInfo{"MC6845", "Motorola", "Motorola 6845"}) {
        init_regs(mc6845::reg::NUM_REGISTERS);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // --- ChipBase bus interface (MMIO) ---
    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        BUS_SET_DATA(bus, read(BUS_GET_ADDR(bus)));
        return bus;
    }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        write(BUS_GET_ADDR(bus), BUS_GET_DATA(bus));
        return bus;
    }

    // --- CS-tick: character clock + MMIO self-dispatch ---
    // Combines the character-clock tick with CS-conditional register access.
    bus_state_t tick(bus_state_t bus) noexcept {
        tick();  // character clock (existing void tick)
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // REGISTERS
    // ========================================================================

    uint8_t address_register = 0;               // Currently selected register
    // R0–R17 register file stored in ChipBase::regs_[]

    // ========================================================================
    // COUNTERS (runtime state)
    // ========================================================================

    // Horizontal timing
    uint8_t h_char_counter = 0;         // Character counter within line (0 to R0)
    uint8_t h_sync_counter = 0;         // HSYNC duration counter
    bool    h_sync_active = false;      // HSYNC output
    bool    h_display_active = false;   // Horizontal display enable

    // Vertical timing
    uint8_t v_row_counter = 0;          // Character row counter (0 to R4)
    uint8_t v_scanline_counter = 0;     // Scan line counter within row (0 to R9)
    uint8_t v_adjust_counter = 0;       // Vertical adjust scan line counter (0 to R5)
    uint8_t v_sync_counter = 0;         // VSYNC duration counter
    bool    v_sync_active = false;      // VSYNC output
    bool    v_display_active = false;   // Vertical display enable
    bool    in_adjust = false;          // Currently in vertical adjust phase

    // Display address
    uint16_t linear_address = 0;        // Current display memory address (MA0–MA13)
    uint16_t row_start_address = 0;     // Address at start of current character row

    // Cursor
    uint8_t cursor_blink_counter = 0;   // Frame counter for cursor blink
    bool    cursor_visible = true;      // Current cursor visibility (after blink logic)

    // Light pen
    bool    light_pen_latched = false;  // Light pen position has been captured
    uint16_t light_pen_address = 0;     // Captured light pen address

    // Frame counter (for blink timing)
    uint32_t frame_count = 0;

    // ========================================================================
    // CALLBACKS — set by the host system
    // ========================================================================

    // Called once per character clock during the active display area.
    // Provides: character address (MA), scan line within character (RA),
    // cursor active flag.
    // The host system uses this to look up screen RAM + character ROM and
    // render pixels.
    using display_callback_t = std::function<void(uint16_t ma, uint8_t ra, bool cursor)>;
    display_callback_t on_display_char = nullptr;

    // Called at the start of each new frame (VSYNC rising edge).
    using vsync_callback_t = std::function<void()>;
    vsync_callback_t on_vsync = nullptr;

    // Called at the start of each new scan line (including blanked lines).
    using hsync_callback_t = std::function<void()>;
    hsync_callback_t on_hsync = nullptr;

    // ========================================================================
    // INDEXED CHARACTER RENDERING (optional)
    // ========================================================================
    //
    // When configured, the CRTC renders character pixels as palette indices
    // directly during tick(), eliminating the need for a per-character
    // display callback in the system.  The system supplies:
    //
    //   - char_render_display_ IndexedFrameBuffer for flush + bounds
    //   - char_render_indices_ Pointer to frame index buffer (W×H bytes)
    //   - char_render_rom_     Character ROM pointer (glyph bitmaps)
    //   - char_render_vram_    Screen RAM pointer (character codes)
    //   - char_render_cols_    Display width in characters (e.g. 40)
    //   - char_render_char_h_  Character cell height in pixels (e.g. 8)
    //   - char_render_fb_w_    Framebuffer width in pixels
    //   - char_render_fg_      Foreground palette index
    //   - char_render_bg_      Background palette index
    //   - char_render_palette_ RGBA palette for flush
    //   - char_render_palette_size_  palette entry count
    //   - char_render_vram_mask_     AND mask for screen_offset → VRAM index
    //   - char_render_invert_bit_    Bit in char code that means "invert" (0x80 for PET, 0 to disable)
    //
    // When char_render_rom_ is non-null, the built-in character renderer
    // fires instead of (and in addition to) on_display_char.  At VSYNC
    // the frame is flushed through the IndexedFrameBuffer (if set),
    // or left in char_render_indices_ for the system to stream.
    //
    // This is the pattern for PET, BBC Micro (text modes), and any future
    // MC6845-based system with a fixed character ROM and monochrome or
    // color-attribute display.

    IndexedFrameBuffer* char_render_display_ = nullptr;
    uint8_t*        char_render_indices_ = nullptr;
    const uint8_t*  char_render_rom_     = nullptr;
    const uint8_t*  char_render_vram_    = nullptr;
    int             char_render_cols_    = 40;
    int             char_render_char_h_  = 8;
    int             char_render_fb_w_    = 320;
    int             char_render_fb_h_    = 200;
    uint8_t         char_render_fg_      = 1;
    uint8_t         char_render_bg_      = 0;
    const uint32_t* char_render_palette_ = nullptr;
    int             char_render_palette_size_ = 0;
    uint16_t        char_render_vram_mask_   = 0x03FF;
    uint8_t         char_render_invert_bit_  = 0x80;

    /// Configure indexed character rendering.  Call once after init().
    /// Pass nullptr for display to render into indices without IFB flush.
    /// Pass nullptr for rom to disable.
    void configure_char_render(IndexedFrameBuffer* display, uint8_t* indices,
                               const uint8_t* rom, const uint8_t* vram,
                               int cols, int char_h, int fb_w,
                               uint8_t fg, uint8_t bg,
                               const uint32_t* palette, int palette_size,
                               uint16_t vram_mask = 0x03FF,
                               uint8_t invert_bit = 0x80) {
        char_render_display_ = display;
        char_render_indices_ = indices;
        char_render_rom_     = rom;
        char_render_vram_    = vram;
        char_render_cols_    = cols;
        char_render_char_h_  = char_h;
        char_render_fb_w_    = fb_w;
        char_render_fb_h_    = display ? display->height() : (char_h * 25);
        char_render_fg_      = fg;
        char_render_bg_      = bg;
        char_render_palette_ = palette;
        char_render_palette_size_ = palette_size;
        char_render_vram_mask_   = vram_mask;
        char_render_invert_bit_  = invert_bit;
    }

    // ========================================================================
    // INTERFACE
    // ========================================================================

    /// Initialize/reset the CRTC to power-on state.
    void init();
    void reset();

    /// Register access (memory-mapped: addr bit 0 selects address vs data).
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);

    /// Advance the CRTC by one character clock.
    /// Call this once per character clock (typically 1 MHz on PET).
    void tick();

    // --- Derived state queries ------------------------------------------

    /// Total characters per line (R0 + 1).
    inline uint8_t chars_per_line() const { return regs_[R0_HTOTAL] + 1; }

    /// Visible characters per line (R1).
    inline uint8_t visible_chars() const { return regs_[R1_HDISPLAYED]; }

    /// Total character rows per frame (R4 + 1).
    inline uint8_t rows_per_frame() const { return regs_[R4_VTOTAL] + 1; }

    /// Visible character rows (R6).
    inline uint8_t visible_rows() const { return regs_[R6_VDISPLAYED]; }

    /// Scan lines per character row (R9 + 1).
    inline uint8_t scanlines_per_row() const { return regs_[R9_MAX_SCANLINE] + 1; }

    /// Display start address from R12:R13.
    inline uint16_t start_address() const {
        return (static_cast<uint16_t>(regs_[R12_START_ADDR_HI]) << 8) |
               regs_[R13_START_ADDR_LO];
    }

    /// Cursor address from R14:R15.
    inline uint16_t cursor_address() const {
        return (static_cast<uint16_t>(regs_[R14_CURSOR_HI]) << 8) |
               regs_[R15_CURSOR_LO];
    }

    /// Is the display currently in the active (visible) area?
    inline bool is_display_active() const {
        return h_display_active && v_display_active;
    }

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using M = const mc6845_t;

        wire_debug_registers(MC6845_REG_INFO);
        debug_registry_.set_decl_entries(MC6845_DECL_ENTRIES.data(), MC6845_DECL_ENTRIES.size());

        // All 18 register values and R3/R10 bitfields are in the DECL walk.
        // Selected register index, counters, sync state, and addresses remain.

        debug_registry_.category("Registers")
            .value("Addr Reg", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->address_register; });

        // --- Counters ---
        debug_registry_.category("Counters", false)
            .counter("H Char Counter",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_char_counter; },
                     +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->chars_per_line() - 1); })
            .counter("V Row Counter",    +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_row_counter; },
                     +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->rows_per_frame() - 1); })
            .counter("V Scanline",       +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_scanline_counter; },
                     +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->scanlines_per_row() - 1); })
            .value("V Adjust Counter",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_adjust_counter; });

        // --- Sync & Display ---
        debug_registry_.category("Sync & Display", false)
            .flag("HSYNC",       +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_sync_active; })
            .flag("VSYNC",       +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_sync_active; })
            .flag("H Display",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->h_display_active; })
            .flag("V Display",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->v_display_active; })
            .flag("In Adjust",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->in_adjust; });

        // --- Address ---
        debug_registry_.category("Address", false)
            .address("Linear Addr", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->linear_address; })
            .address("Row Start",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->row_start_address; })
            .address("Start Addr",  +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->start_address(); })
            .address("Cursor Addr", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->cursor_address(); })
            .flag("Cursor Vis",     +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->cursor_visible; });

        // --- Light Pen ---
        debug_registry_.category("Light Pen", false)
            .flag("Latched",        +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->light_pen_latched; })
            .address("Address",     +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->light_pen_address; });

        // --- Frame ---
        debug_registry_.category("Frame", false)
            .value("Frame Count",   +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->frame_count; }, 32);
    }
#endif
};
