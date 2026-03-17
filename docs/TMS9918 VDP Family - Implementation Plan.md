# TMS9918 VDP Family — Implementation Plan

> Template-driven VDP family covering TMS9918, TMS9918A, TMS9928A, TMS9929/A, V9938, V9958, and Sega 315-5124/315-5246. Uses the NTTP (non-type template parameter) pattern established by `fam65xx_t<CPUTraits>`.

---

## 1. Chip Coverage

| Chip | Notes |
|------|-------|
| TMS9918 | Original NTSC, TI-99/4 |
| TMS9918A | Revised NTSC, ColecoVision, MSX1, SG-1000 |
| TMS9928A | RGB output, NTSC timing, some MSX1 boards |
| TMS9929 | PAL original |
| TMS9929A | PAL revised, Tatung Einstein, PAL MSX1 |
| V9938 (TMS9938) | MSX2 — bitmap modes, 512-colour palette, vertical scroll |
| V9958 | MSX2+ — adds YJK/YAE colour encoding, horizontal scroll |
| Sega 315-5124 | Master System NTSC — sprite/scroll extensions over 9918A base |
| Sega 315-5246 | Master System PAL / Game Gear — minor register delta on 315-5124 |

---

## 2. Trait Axes

The divergence points across the family:

| Axis | Values | Compile-time? |
|------|--------|--------------|
| NTSC / PAL | Clock frequency, line count, colorburst | Yes (NTTP) |
| Output type | Composite RF, composite video, RGB | Yes (NTTP) |
| Sprite model | Original, Sega extensions, V9938 mode 2 | Yes (NTTP) |
| Bitmap modes | Absent on base, present from V9938+ | Yes (feature flag) |
| Palette model | Fixed 15-colour, 512-colour (V9938), YJK (V9958) | Yes (NTTP) |
| Scroll model | None, Sega per-line, V9938 vertical, V9958 H+V | Yes (NTTP) |
| Register count | 8 (base), 11 (Sega), 47 (V9938), 48+ (V9958) | Yes (NTTP) |
| VRAM capacity | 16 KB (base/Sega), 128 KB (V9938+) | Yes (NTTP) |

All axes are hard-wired per physical chip — no TMS9918A ever becomes a V9938 at runtime. This makes NTTP the correct mechanism: `if constexpr` gates out V9938 command engine code (~1000+ lines) from TMS9918A instantiations, yielding smaller binaries and better icache behaviour.

---

## 3. VDPTraits Struct

```cpp
// === Enums for trait axes ===

enum class VDPRegion : uint8_t {
    NTSC,       // 262 lines, 3.58 MHz colorburst
    PAL,        // 313 lines, 4.43 MHz colorburst
};

enum class VDPOutput : uint8_t {
    COMPOSITE,  // TMS9918, TMS9918A, TMS9929A (internal DAC)
    RGB,        // TMS9928A (separate R/G/B/Y outputs)
};

enum class VDPSpriteModel : uint8_t {
    ORIGINAL,   // 4 sprites/line, 32 total, 8×8 or 16×16
    SEGA,       // SMS: per-line limit relaxed, 8×16 mode, zoom bits reused
    V9938,      // V9938+: collision/priority extensions, sprite mode 2 (16 colors/line)
};

enum class VDPPaletteModel : uint8_t {
    FIXED_15,   // TMS9918/A/28/29 — 15 fixed colors + transparent
    PALETTE_512,// V9938 — 16 entries from 512-color 9-bit RGB
    PALETTE_YJK,// V9958 — adds YJK/YAE encoding (MSX2+)
};

enum class VDPScrollModel : uint8_t {
    NONE,       // TMS9918 base — no hardware scroll
    SEGA,       // SMS/GG — per-line horizontal + column vertical
    V9938,      // V9938 — vertical scroll register
    V9958,      // V9958 — vertical + horizontal scroll registers
};

namespace VDPFeatureFlags {
    inline constexpr uint32_t BITMAP_MODES     = 1 << 0; // V9938+: Graphic 4-7, screen 5-8
    inline constexpr uint32_t VRAM_128K        = 1 << 1; // V9938+: 128KB VRAM (vs 16KB)
    inline constexpr uint32_t COMMAND_ENGINE   = 1 << 2; // V9938+: hardware blitter
    inline constexpr uint32_t INTERLACE        = 1 << 3; // V9938+: interlaced modes
    inline constexpr uint32_t MOUSE_PORT       = 1 << 4; // V9938+: built-in mouse/trackball
    inline constexpr uint32_t SEGA_MODE_EXT    = 1 << 5; // SMS: mode 4, extended patterning
    inline constexpr uint32_t SEGA_GG_MODE     = 1 << 6; // Game Gear: 12-bit CRAM, viewport
    inline constexpr uint32_t STATUS_EXT       = 1 << 7; // V9938+: extended status registers
    inline constexpr uint32_t WAIT_STATE       = 1 << 8; // V9938+: CPU wait signal
}

struct VDPTraits {
    const char*      vendor;
    const char*      chip_id;
    VDPRegion        region;
    VDPOutput        output;
    VDPSpriteModel   sprite_model;
    VDPPaletteModel  palette_model;
    VDPScrollModel   scroll_model;
    uint32_t         feature_flags;
    uint8_t          num_registers;    // 8, 11 (Sega), 47 (V9938), 48+ (V9958)
    uint16_t         vram_size_kb;     // 16 or 128
    uint16_t         total_lines;      // 262 NTSC, 313 PAL
    uint16_t         visible_lines;    // 192 base, 212 V9938 extended
    uint32_t         dot_clock_hz;     // 5.37 MHz NTSC, 5.32 MHz PAL (base)

    // Helper methods
    constexpr bool has(uint32_t flag) const { return (feature_flags & flag) != 0; }
    constexpr bool is_pal() const { return region == VDPRegion::PAL; }
    constexpr bool is_sega() const {
        return has(VDPFeatureFlags::SEGA_MODE_EXT);
    }
    constexpr bool is_v9938_class() const {
        return has(VDPFeatureFlags::BITMAP_MODES);
    }
    constexpr bool has_command_engine() const {
        return has(VDPFeatureFlags::COMMAND_ENGINE);
    }
    constexpr bool has_programmable_palette() const {
        return palette_model != VDPPaletteModel::FIXED_15;
    }
};
```

---

## 4. Variant Trait Definitions

Each physical chip gets a tiny header with an `inline constexpr VDPTraits` and a `using` alias. Examples:

### TMS9918A (NTSC, composite, base features)

```cpp
// tms9918a.hpp
inline constexpr VDPTraits TMS9918ATraits = {
    "Texas Instruments",             // vendor
    "TMS9918A",                      // chip_id
    VDPRegion::NTSC,                 // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::ORIGINAL,        // sprite_model
    VDPPaletteModel::FIXED_15,       // palette_model
    VDPScrollModel::NONE,            // scroll_model
    0,                               // feature_flags
    8,                               // num_registers
    16,                              // vram_size_kb
    262,                             // total_lines
    192,                             // visible_lines
    5'370'000,                       // dot_clock_hz
};

using TMS9918A = tms9918_t<TMS9918ATraits>;
```

### TMS9928A (RGB output, NTSC)

```cpp
inline constexpr VDPTraits TMS9928ATraits = {
    "Texas Instruments", "TMS9928A",
    VDPRegion::NTSC, VDPOutput::RGB,
    VDPSpriteModel::ORIGINAL, VDPPaletteModel::FIXED_15, VDPScrollModel::NONE,
    0, 8, 16, 262, 192, 5'370'000,
};

using TMS9928A = tms9918_t<TMS9928ATraits>;
```

### TMS9929A (PAL, composite)

```cpp
inline constexpr VDPTraits TMS9929ATraits = {
    "Texas Instruments", "TMS9929A",
    VDPRegion::PAL, VDPOutput::COMPOSITE,
    VDPSpriteModel::ORIGINAL, VDPPaletteModel::FIXED_15, VDPScrollModel::NONE,
    0, 8, 16, 313, 192, 5'320'000,
};

using TMS9929A = tms9918_t<TMS9929ATraits>;
```

### V9938 (MSX2)

```cpp
inline constexpr VDPTraits V9938Traits = {
    "Yamaha", "V9938",
    VDPRegion::NTSC, VDPOutput::RGB,
    VDPSpriteModel::V9938, VDPPaletteModel::PALETTE_512, VDPScrollModel::V9938,
    VDPFeatureFlags::BITMAP_MODES | VDPFeatureFlags::VRAM_128K |
    VDPFeatureFlags::COMMAND_ENGINE | VDPFeatureFlags::INTERLACE |
    VDPFeatureFlags::MOUSE_PORT | VDPFeatureFlags::STATUS_EXT |
    VDPFeatureFlags::WAIT_STATE,
    47, 128, 262, 212, 5'370'000,
};

using V9938 = tms9918_t<V9938Traits>;
```

### V9958 (MSX2+)

```cpp
inline constexpr VDPTraits V9958Traits = {
    "Yamaha", "V9958",
    VDPRegion::NTSC, VDPOutput::RGB,
    VDPSpriteModel::V9938, VDPPaletteModel::PALETTE_YJK, VDPScrollModel::V9958,
    VDPFeatureFlags::BITMAP_MODES | VDPFeatureFlags::VRAM_128K |
    VDPFeatureFlags::COMMAND_ENGINE | VDPFeatureFlags::INTERLACE |
    VDPFeatureFlags::MOUSE_PORT | VDPFeatureFlags::STATUS_EXT |
    VDPFeatureFlags::WAIT_STATE,
    48, 128, 262, 212, 5'370'000,
};

using V9958 = tms9918_t<V9958Traits>;
```

### Sega 315-5124 (Master System NTSC)

```cpp
inline constexpr VDPTraits SEGA_315_5124Traits = {
    "Sega", "315-5124",
    VDPRegion::NTSC, VDPOutput::COMPOSITE,
    VDPSpriteModel::SEGA, VDPPaletteModel::FIXED_15, VDPScrollModel::SEGA,
    VDPFeatureFlags::SEGA_MODE_EXT,
    11, 16, 262, 192, 5'370'000,
};

using SEGA_315_5124 = tms9918_t<SEGA_315_5124Traits>;
```

### Sega 315-5246 (Master System PAL / Game Gear)

```cpp
inline constexpr VDPTraits SEGA_315_5246Traits = {
    "Sega", "315-5246",
    VDPRegion::PAL, VDPOutput::COMPOSITE,
    VDPSpriteModel::SEGA, VDPPaletteModel::FIXED_15, VDPScrollModel::SEGA,
    VDPFeatureFlags::SEGA_MODE_EXT,
    11, 16, 313, 192, 5'320'000,
};

using SEGA_315_5246 = tms9918_t<SEGA_315_5246Traits>;
```

---

## 5. Template Hierarchy

### Why Not CRTP

The coding guidelines mandate: *"No CRTP — classical virtual + template instantiation."* The V9938/V9958 extensions are handled via **conditional mixins** (the `fam65xx_t` pattern) instead.

### Inheritance Diagram

```
VideoChipBase                              (src/chip/video/video_chip_base.hpp)
  └─ tms9918_t<VDPTraits>                 (src/chip/video/tms9918/tms9918.hpp)
       ├─ vdp_palette_base_t<Traits>       programmable palette (empty for FIXED_15)
       ├─ vdp_command_base_t<Traits>       hardware blitter   (empty for non-V9938)
       ├─ vdp_scroll_base_t<Traits>        scroll registers   (empty for NONE)
       └─ vdp_sega_base_t<Traits>          CRAM + mode 4      (empty for non-Sega)
```

### Mixin Selection (std::conditional_t)

```cpp
// Empty bases for disabled features
struct vdp_empty_palette_mixin_t {};
struct vdp_empty_command_mixin_t {};
struct vdp_empty_scroll_mixin_t {};
struct vdp_empty_sega_mixin_t {};

template <const VDPTraits& Traits>
using vdp_palette_base_t = std::conditional_t<
    Traits.has_programmable_palette(),
    vdp_palette_mixin_t<Traits>,
    vdp_empty_palette_mixin_t>;

template <const VDPTraits& Traits>
using vdp_command_base_t = std::conditional_t<
    Traits.has_command_engine(),
    vdp_command_mixin_t<Traits>,
    vdp_empty_command_mixin_t>;

template <const VDPTraits& Traits>
using vdp_scroll_base_t = std::conditional_t<
    Traits.scroll_model != VDPScrollModel::NONE,
    vdp_scroll_mixin_t<Traits>,
    vdp_empty_scroll_mixin_t>;

template <const VDPTraits& Traits>
using vdp_sega_base_t = std::conditional_t<
    Traits.is_sega(),
    vdp_sega_mixin_t<Traits>,
    vdp_empty_sega_mixin_t>;
```

Zero overhead when disabled (empty base optimization). Each mixin carries only the state and methods relevant to its feature set.

---

## 6. Register Model

### I/O Port Architecture

The TMS9918 family exposes two I/O ports (directly addressed or Z80 IN/OUT mapped):

- **Port 0 (CSW/CSR)**: VRAM data read/write with auto-increment address
- **Port 1 (CSW)**: Register write / VRAM address setup (two-byte latch)

Register writes go through a **two-byte FIFO**:
1. Byte 1 = data value
2. Byte 2 = register number (bit 7 = 1) **or** VRAM address low byte (bit 7 = 0)

### Register Counts

| Variant | Write regs | Read (status) regs | Notes |
|---------|-----------|-------------------|-------|
| TMS9918/A/28/29 | 8 | 1 | R0–R7 + status |
| Sega 315-5124/5246 | 11 | 1 | R0–R10 + status |
| V9938 | 47 | 9 | R0–R46 + S0–S8 |
| V9958 | 48+ | 9 | R0–R47+ |

### X-Macro Declarations

```cpp
// Base registers (all variants share R0–R7)
#define TMS9918_BASE_REGS(REG, FLD, CMP) \
    REG(0, R0, "Mode Control 0") \
      FLD(R0, M3,     1:1, "Mode bit 3",    Flag, 0, 0) \
      FLD(R0, EXTVID, 0:0, "Ext video in",  Flag, 0, 0) \
    REG(1, R1, "Mode Control 1") \
      FLD(R1, VRAM16K, 7:7, "16K VRAM",     Flag, 0, 0) \
      FLD(R1, BL,      6:6, "Blank",        Flag, 0, 0) \
      FLD(R1, IE,      5:5, "INT enable",   Flag, 0, 0) \
      FLD(R1, M1,      4:4, "Mode bit 1",   Flag, 0, 0) \
      FLD(R1, M2,      3:3, "Mode bit 2",   Flag, 0, 0) \
      FLD(R1, SI,      1:1, "Sprite 16×16", Flag, 0, 0) \
      FLD(R1, MAG,     0:0, "Sprite zoom",  Flag, 0, 0) \
    REG(2, R2, "Name Table Base") \
      FLD(R2, NT, 3:0, "NT base [13:10]",   Value, 0, 0) \
    REG(3, R3, "Color Table Base") \
    REG(4, R4, "Pattern Gen Base") \
      FLD(R4, PG, 2:0, "PG base [13:11]",   Value, 0, 0) \
    REG(5, R5, "Sprite Attr Base") \
      FLD(R5, SA, 6:0, "SA base [13:7]",    Value, 0, 0) \
    REG(6, R6, "Sprite Pattern Base") \
      FLD(R6, SG, 2:0, "SG base [13:11]",   Value, 0, 0) \
    REG(7, R7, "Color 0 / Backdrop") \
      FLD(R7, TC, 7:4, "Text color",        Value, 0, 0) \
      FLD(R7, BD, 3:0, "Backdrop color",    Value, 0, 0)

// Sega-extended registers (mode 4)
#define TMS9918_SEGA_REGS(REG, FLD, CMP) \
    REG(8,  R8,  "Sega H-Scroll") \
    REG(9,  R9,  "Sega V-Scroll") \
    REG(10, R10, "Sega Line Counter")

// V9938-extended registers (R8–R46, separate declaration block)
#define TMS9918_V9938_REGS(REG, FLD, CMP) \
    /* R8–R46: mode, palette, scroll, command, status ... */ \
    /* (detailed declaration deferred to implementation) */
```

The `init_regs()` / `init_split_regs()` call uses `Traits.num_registers` to allocate exactly the right count. V9938+ uses `init_split_regs()` for separate write (47) and read (9) register banks.

---

## 7. Palette Model

Three strategies selected by `VDPPaletteModel`:

### FIXED_15 (TMS9918/A/28/29)

Static 15-color + transparent palette. Header-only `inline constexpr` tables with multiple named community alternatives:

```cpp
// tms9918_palette.hpp

// Format: 0xAABBGGRR (GL_RGBA little-endian convention)
inline constexpr uint32_t TMS9918_PALETTE_DATASHEET[16] = {
    0x00000000,  //  0: Transparent
    0xFF000000,  //  1: Black
    0xFF47B73E,  //  2: Medium Green
    0xFF7CCF6F,  //  3: Light Green
    0xFFEF5350,  //  4: Dark Blue
    0xFFFF7D6F,  //  5: Light Blue
    0xFF4E51B8,  //  6: Dark Red
    0xFFFFD26C,  //  7: Cyan
    0xFF5254FB,  //  8: Medium Red
    0xFF7A79FF,  //  9: Light Red
    0xFF50BED2,  // 10: Dark Yellow
    0xFF6DCFE0,  // 11: Light Yellow
    0xFF3DA241,  // 12: Dark Green
    0xFFB668C4,  // 13: Magenta
    0xFFCCCCCC,  // 14: Gray
    0xFFFFFFFF,  // 15: White
};

inline constexpr uint32_t TMS9918_PALETTE_TMSEMU[16] = { /* measured */ };
inline constexpr uint32_t TMS9918_PALETTE_OPENMSX[16] = { /* openMSX */ };

inline const NamedPalette TMS9918_NAMED_PALETTES[] = {
    { "datasheet", "TI Datasheet",       TMS9918_PALETTE_DATASHEET, 16 },
    { "tmsemu",    "TMSEmu (Measured)",   TMS9918_PALETTE_TMSEMU,    16 },
    { "openmsx",   "openMSX",            TMS9918_PALETTE_OPENMSX,   16 },
};
```

### PALETTE_512 (V9938) — Programmable Palette Mixin

```cpp
template <const VDPTraits& Traits>
struct vdp_palette_mixin_t {
    uint16_t palette_ram_[16] = {};       // 9-bit RGB entries (3 bits per channel)
    uint32_t palette_cache_[16] = {};     // Decoded RGBA for rendering
    uint8_t  palette_latch_ = 0;          // First/second byte toggle
    uint8_t  palette_byte_ = 0;           // Latched first byte

    void write_palette_port(uint8_t val);
    void rebuild_palette_cache();
};
```

On port write, the 9-bit RGB value is decoded to RGBA and cached. The rendering pipeline reads `palette_cache_[]` directly.

### PALETTE_YJK (V9958) — Extends PALETTE_512

Same mixin, with an additional YJK→RGB conversion lookup table used during bitmap mode rendering.

---

## 8. Mixin Details

### Programmable Palette Mixin

Active when `Traits.has_programmable_palette()`. Holds 16-entry palette RAM (9-bit or 12-bit per entry) and a decoded RGBA cache. Accessed via the palette port (V9938 register indirect, Sega CRAM direct).

### Command Engine Mixin (V9938+)

Active when `Traits.has_command_engine()`. The V9938/V9958 hardware blitter supports ~30 commands (HMMC, YMMM, HMMM, HMMV, LMMC, LMCM, LMMM, LMMV, LINE, SRCH, PSET, POINT, etc.).

```cpp
template <const VDPTraits& Traits>
struct vdp_command_mixin_t {
    // Command registers (subset of R32–R46)
    uint16_t cmd_sx_, cmd_sy_;     // Source X, Y
    uint16_t cmd_dx_, cmd_dy_;     // Dest X, Y
    uint16_t cmd_nx_, cmd_ny_;     // Width, Height (or major/minor axis)
    uint8_t  cmd_clr_;             // Color argument
    uint8_t  cmd_arg_;             // Direction / logic op
    uint8_t  cmd_op_;              // Command code (R46 bits 7–4)

    // State machine
    bool     cmd_active_ = false;
    uint16_t cmd_cx_, cmd_cy_;     // Current position
    uint16_t cmd_count_;           // Remaining pixels/bytes

    void execute_command();
    void step_command();           // Called per dot-clock during active command
    bool command_ready() const;    // CE bit for status register
};
```

This is the largest single module. It gets its own `tms9918_command.inc.hpp` included from the main header.

### Scroll Mixin

Active when `Traits.scroll_model != VDPScrollModel::NONE`. Holds scroll register values and provides fine-scroll offset calculation. The Sega model supports per-line horizontal scroll tables in VRAM.

```cpp
template <const VDPTraits& Traits>
struct vdp_scroll_mixin_t {
    uint8_t  scroll_x_ = 0;       // Horizontal scroll (Sega, V9958)
    uint8_t  scroll_y_ = 0;       // Vertical scroll (all scroll models)
    // Sega: line scroll table base address
    // V9958: additional scroll registers

    uint16_t effective_scroll_x(int line) const;
    uint16_t effective_scroll_y() const;
};
```

### Sega Mode Extension Mixin

Active when `Traits.is_sega()`. Holds CRAM (Color RAM, 32 or 64 entries), mode 4 tile decoding state, and the extended backdrop / priority logic.

```cpp
template <const VDPTraits& Traits>
struct vdp_sega_mixin_t {
    uint8_t  cram_[64] = {};           // Color RAM (SMS: 32 entries, GG: 64)
    uint32_t cram_cache_[32] = {};     // Decoded RGBA

    void write_cram(uint8_t addr, uint8_t val);
    void rebuild_cram_cache();
};
```

---

## 9. Core Template Structure

```cpp
template <const VDPTraits& Traits>
class tms9918_t : public VideoChipBase,
                  public vdp_palette_base_t<Traits>,
                  public vdp_command_base_t<Traits>,
                  public vdp_scroll_base_t<Traits>,
                  public vdp_sega_base_t<Traits> {
public:
    tms9918_t();

    // --- Bus interface ---
    bus_state_t io_read(bus_state_t bus);     // Port 0/1 read
    bus_state_t io_write(bus_state_t bus);    // Port 0/1 write

    // --- Tick (dot clock) ---
    bus_state_t tick(bus_state_t bus);

    // --- Display output ---
    void set_display(IndexedFrameBuffer* d) { display_ = d; }

    // --- Static I/O dispatch helpers ---
    static bus_state_t port_read(void* ctx, bus_state_t bus);
    static bus_state_t port_write(void* ctx, bus_state_t bus);

private:
    // --- VRAM (compile-time sized) ---
    uint8_t vram_[Traits.vram_size_kb * 1024] = {};

    // --- Register latch (two-byte FIFO) ---
    uint8_t  latch_byte_ = 0;
    bool     latch_first_ = true;   // true = next write is first byte

    // --- VRAM address pointer ---
    uint16_t vram_addr_ = 0;
    uint8_t  read_ahead_ = 0;       // Read-ahead buffer (TMS9918 quirk)

    // --- Timing state ---
    uint16_t dot_ = 0;              // Horizontal dot counter (0–341 NTSC)
    uint16_t line_ = 0;             // Vertical line counter
    uint32_t frame_ = 0;            // Frame counter

    // --- Status register ---
    uint8_t  status_ = 0;           // Bit 7=vblank IRQ, 6=5th sprite, 5=collision

    // --- Scanline rendering ---
    uint8_t  color_line_[256] = {}; // Palette index per pixel (current scanline)
    uint8_t  sprite_line_[256] = {};// Sprite overlay per pixel

    // --- Display output ---
    IndexedFrameBuffer* display_ = nullptr;

    // --- Rendering dispatch ---
    void render_scanline(int line);
    void render_mode0_line(int line);    // Graphics I
    void render_mode1_line(int line);    // Text
    void render_mode2_line(int line);    // Graphics II
    void render_mode3_line(int line);    // Multicolor
    void render_sprites(int line);

    // --- if constexpr-gated rendering ---
    // These compile to nothing for variants that don't support them:
    void render_mode4_line(int line);    // Sega mode 4
    void render_bitmap_line(int line);   // V9938+ Graphic 4-7

    // --- Screen mode detection ---
    uint8_t current_screen_mode() const;
    bool in_active_display(int line) const;
};
```

### Key Design Decisions

- **VRAM is template-sized**: `uint8_t vram_[Traits.vram_size_kb * 1024]`. This creates two binary instantiations (16 KB and 128 KB) but avoids runtime allocation and is consistent with flat-memory performance priorities.
- **`tick()` is non-virtual**: Called in the system's hot loop per dot-clock cycle. No virtual dispatch overhead.
- **Static I/O helpers**: `port_read()` / `port_write()` are registered in the system's I/O handler table (function pointer dispatch, same pattern as VIC-II).

---

## 10. Rendering Pipeline

### Screen Modes

| Mode | Name | Resolution | Color Model | Chips |
|------|------|-----------|-------------|-------|
| 0 | Graphics I | 256×192, 8×8 tiles, 1 of 16 fg/bg per 8 tiles | Pattern + color table | All |
| 1 | Text | 240×192, 6×8 font, 1 fg/1 bg global | Pattern only | All |
| 2 | Graphics II | 256×192, 8×8 tiles, per-row fg/bg | Full pattern + color | All |
| 3 | Multicolor | 64×48 blocks (4×4 pixels each) | 2-color per block | All |
| 4 | Sega Mode | 256×192/224/240, 8×8 tiles, 4bpp | Sega CRAM palette | Sega only |
| G4–G7 | Bitmap | 256×212, 4/8 bpp, 512-color | V9938 palette | V9938+ only |

### Rendering Flow

```cpp
template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_scanline(int line) {
    if (!in_active_display(line)) return;

    switch (current_screen_mode()) {
    case 0: render_mode0_line(line); break;
    case 1: render_mode1_line(line); break;
    case 2: render_mode2_line(line); break;
    case 3: render_mode3_line(line); break;
    default:
        if constexpr (Traits.is_sega()) {
            render_mode4_line(line);
        }
        if constexpr (Traits.is_v9938_class()) {
            render_bitmap_line(line);
        }
        break;
    }

    render_sprites(line);

    // Output to display via IndexedFrameBuffer
    if (display_) {
        display_->flush_line(line, color_line_, system_palette(), 256);
    }
}
```

### Sprite Rendering Dispatch

```cpp
template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_sprites(int line) {
    if constexpr (Traits.sprite_model == VDPSpriteModel::ORIGINAL) {
        // 32 sprites, 4 per line, 8×8 or 16×16, zoom
        render_original_sprites(line);
    } else if constexpr (Traits.sprite_model == VDPSpriteModel::SEGA) {
        // 64 sprites, 8 per line, 8×8 or 8×16
        render_sega_sprites(line);
    } else if constexpr (Traits.sprite_model == VDPSpriteModel::V9938) {
        // Mode 2 sprites: 32 sprites, 8 per line, 16 colors per line
        render_v9938_sprites(line);
    }
}
```

---

## 11. System Integration

### Target Systems

| System | VDP Variant | VRAM | I/O Mapping | CPU |
|--------|-------------|------|-------------|-----|
| TI-99/4A | TMS9918A | 16 KB | Memory-mapped | TMS9900 |
| ColecoVision | TMS9918A | 16 KB | Z80 I/O ports | Z80 |
| MSX1 | TMS9918A / TMS9928A / TMS9929A | 16 KB | Z80 I/O ports $98–$9B | Z80 |
| SG-1000 | TMS9918A | 16 KB | Z80 I/O ports | Z80 |
| MSX2 | V9938 | 128 KB | Z80 I/O ports $98–$9B | Z80 |
| MSX2+ | V9958 | 128 KB | Z80 I/O ports $98–$9B | Z80 |
| Master System | 315-5124 / 315-5246 | 16 KB | Z80 I/O ports $7E–$7F | Z80 |

### Manifest Integration

Systems declare the VDP in their chip manifest:

```cpp
// colecovision_system.hpp
inline constexpr auto kColecoChips = make_chip_manifest(
    Slot<RAMChip>{0x6000, 1024, 0, "RAM"},
    Slot<Z80>    {0, 0, 0, "Z80"},
    Slot<TMS9918A>{0, 0, 0, "TMS9918A"},
    // ...
);
```

The VDP is typically I/O-mapped (Z80 IN/OUT), not memory-mapped. The system's I/O dispatch table registers the VDP's static read/write handlers.

---

## 12. File & Directory Layout

```
src/chip/video/tms9918/
├── tms9918_traits.hpp           # VDPTraits struct, enums, feature flags
├── tms9918_registers.hpp        # X-macro REG/FLD/CMP declarations (all variants)
├── tms9918_palette.hpp          # Fixed palette tables + NamedPalette registry
├── tms9918_mixins.hpp           # Conditional mixins (command, scroll, sega, palette)
├── tms9918.hpp                  # Main template: tms9918_t<VDPTraits>
├── tms9918_render.inc.hpp       # Rendering pipeline (included by tms9918.hpp)
├── tms9918_sprites.inc.hpp      # Sprite evaluation/rendering (included)
├── tms9918_command.inc.hpp      # V9938+ command engine (included, gated by if constexpr)
├── tms9918_gui.cpp              # Debug/settings UI (#ifdef CERMU_HAS_GUI)
├── tms9918_registry.cpp         # Explicit instantiation + REGISTER_CHIP_TYPE
│
├── tms9918a.hpp                 # Variant: NTSC composite (ColecoVision, MSX1, SG-1000)
├── tms9928a.hpp                 # Variant: NTSC RGB
├── tms9929.hpp                  # Variant: PAL original
├── tms9929a.hpp                 # Variant: PAL revised
├── v9938.hpp                    # Variant: MSX2
├── v9958.hpp                    # Variant: MSX2+
├── sega_315_5124.hpp            # Variant: Master System NTSC
├── sega_315_5246.hpp            # Variant: Master System PAL / Game Gear
└── README.md                    # Family overview
```

Each variant header is minimal — just the `inline constexpr VDPTraits` definition and a `using` alias.

---

## 13. Implementation Phases

### Phase 1 — Foundation

**Files**: `tms9918_traits.hpp`, `tms9918_registers.hpp`, `tms9918_palette.hpp`

- Define `VDPTraits`, all enums, all feature flags.
- All 8 variant trait constant definitions.
- X-macro register declarations for 8-register base set.
- Fixed 15-color palette table with 2–3 named community alternatives.

**Deliverable**: Compiles, no rendering. All types resolvable.

### Phase 2 — Core Template

**Files**: `tms9918.hpp`, `tms9918_mixins.hpp`

- `tms9918_t<VDPTraits>` inheriting `VideoChipBase` + 4 conditional mixins.
- VRAM storage, register latch I/O, VRAM address auto-increment, read-ahead buffer.
- `tick()` skeleton: dot/line/frame counters, vblank IRQ generation, blanking state.
- I/O port handlers (static dispatch).

**Deliverable**: Chip ticks, accepts register writes, IRQ fires at vblank. No pixels.

### Phase 3 — Base Rendering

**Files**: `tms9918_render.inc.hpp`, `tms9918_sprites.inc.hpp`

- Mode 0 (Graphics I) — simplest and most universal.
- Mode 1 (Text) — straightforward.
- Mode 2 (Graphics II) — per-row color, most commonly used.
- Mode 3 (Multicolor) — rarely used but trivial.
- Original sprite model (32 sprites, 4/line, 8×8 or 16×16, zoom).
- `IndexedFrameBuffer` integration for display output.

**Deliverable**: TMS9918A renders all 4 original modes. ColecoVision / MSX1 games display correctly.

### Phase 4 — Sega Extensions

**Files**: `vdp_sega_mixin_t` in `tms9918_mixins.hpp`, mode 4 in `tms9918_render.inc.hpp`

- Mode 4 tile engine (4bpp tiles, CRAM palette, 32 entries).
- Sega sprite model (relaxed limits, 8×16).
- Per-line H-scroll + column V-scroll via scroll mixin.
- Extended backdrop/priority logic.

**Deliverable**: Master System games render in mode 4.

### Phase 5 — V9938/V9958 Extensions

**Files**: `tms9918_command.inc.hpp`, V9938 register DECL block, palette mixin

- 47-register extended X-macro DECL.
- Programmable palette mixin (512-color V9938, YJK V9958).
- Bitmap modes (Graphic 4–7).
- V9938 sprite mode 2 (16 colors per line).
- Vertical + horizontal scroll.
- **Command engine** — hardware blitter with ~30 commands. Largest single module.

**Deliverable**: MSX2/MSX2+ software renders correctly.

### Phase 6 — GUI & Registry

**Files**: `tms9918_gui.cpp`, `tms9918_registry.cpp`

- Chip layout diagrams (40-pin DIP for TMS9918A, 64-pin QFP for V9938).
- Debug content: VRAM hex viewer, pattern/name/color table visualizer, sprite list.
- Settings content: palette selection (for fixed-palette variants).
- `REGISTER_CHIP_TYPE` for all 8 variants.

**Deliverable**: Full debug/visualization UI for all variants.

---

## 14. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| Sega Mode 4 divergence too large for `if constexpr` | Medium | Mode 4 rendering is a self-contained function. If divergence grows, it stays inside `vdp_sega_mixin_t` — no architectural change needed. |
| V9938 command engine complexity | High | Isolated in its own `.inc.hpp` with its own state machine. Can be developed and tested independently. ~30 commands but most share a common iteration pattern. |
| VRAM sizing creates binary bloat | Low | Only two sizes exist (16 KB, 128 KB). Two template instantiation families is acceptable. |
| Register count spanning 8–48 | Low | `init_regs(Traits.num_registers)` handles this. X-macro DECL blocks are partitioned (base, Sega, V9938). |
| TI-99/4A memory-mapped VDP (vs Z80 I/O) | Low | The VDP I/O interface is the same two-port model regardless of bus mapping. The system layer handles address decode. |

---

## 15. Reference Summary

### Existing Patterns Followed

| Pattern | Reference in cermu | Applied to TMS9918 |
|---------|-------------------|-------------------|
| NTTP traits struct | `CPUTraits` in `fam65xx_processor_traits.hpp` | `VDPTraits` with per-chip constants |
| Conditional mixins | `io_port_base_t`, `apu_base_t` in `fam65xx_mixins.hpp` | `vdp_palette_base_t`, `vdp_command_base_t`, etc. |
| Variant headers | `mos6510.hpp`, `ricoh_2a03.hpp` | `tms9918a.hpp`, `v9938.hpp`, etc. |
| Named palettes | NES PPU `nes_palette.hpp` | `tms9918_palette.hpp` |
| X-macro registers | VIC-II `VICII_DECL`, NES PPU `NES_PPU_DECL` | `TMS9918_BASE_REGS`, `TMS9918_SEGA_REGS`, etc. |
| VideoChipBase | All video chips inherit from it | `tms9918_t` inherits from it |
| IndexedFrameBuffer | VIC-II, TED, NES PPU | Scanline flush via `display_->flush_line()` |
| Static I/O dispatch | VIC-II `registers_read/write` | `tms9918_t::port_read/port_write` |
| Explicit instantiation + registry | `fam65xx_registry.cpp` | `tms9918_registry.cpp` |

Key uncertainties:

Sega Mode 4 divergence depth — may need its own rendering unit rather than if constexpr inline checks, depending on how different the tile format truly is from TMS9918 base modes.
V9938 command engine complexity — this is essentially a mini-GPU with ~30 blitter commands. It's well-isolated as a mixin, but its state machine is substantial and may warrant its own .inc.hpp.
Whether VRAM should be std::array<uint8_t, N> sized by a template constant, or a runtime allocation. Template-sized is cleaner but creates two binary instantiations (16KB vs 128KB).
How to improve this plan:

Review actual V9938 command engine documentation to estimate mixin size.
Prototype Mode 0 rendering for TMS9918A in isolation to validate the trait/mixin decomposition before committing to the full 8-variant build.
Decide on VRAM sizing strategy (I lean toward uint8_t vram_[Traits.vram_size_kb * 1024] — simple, no allocation, two instantiations is fine).