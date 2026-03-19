# Cermu Analog Signal Emulation Design Plan

## Table of Contents

1. [Core Philosophy](#core-philosophy)
2. [Analog Signal Fundamentals](#analog-signal-fundamentals)
3. [Signal Slot Model](#signal-slot-model)
4. [VideoFlags — Shared Concrete Type](#videoflags--shared-concrete-type)
5. [Video Sample Types](#video-sample-types)
6. [VideoStream — Chip-Facing Write Interface](#videostream--chip-facing-write-interface)
7. [Concrete Stream Type Aliases](#concrete-stream-type-aliases)
8. [VideoPort — Owns and Wires the Stream](#videoport--owns-and-wires-the-stream)
9. [Chip Implementation — Zero Upstream Dependency](#chip-implementation--zero-upstream-dependency)
10. [Board — Declares and Wires Everything](#board--declares-and-wires-everything)
11. [Mode Switching (C128 / Multi-CPU Boards)](#mode-switching-c128--multi-cpu-boards)
12. [Raster Reconstruction](#raster-reconstruction)
13. [Vector Graphics Signals](#vector-graphics-signals)
14. [Color Conversion](#color-conversion)
15. [GPU-Accelerated Rendering](#gpu-accelerated-rendering)
16. [Audio Signal Architecture](#audio-signal-architecture)
17. [AudioPort Implementation](#audioport-implementation)
18. [Host Performance Optimization](#host-performance-optimization)
19. [System Assembly](#system-assembly)
20. [Implementation Status](#implementation-status)

---

## Core Philosophy

- **Chips drive pins; ports observe signals.** A chip has no semantic knowledge of scanlines, frames, sample rates, or downstream consumers. It drives output pins based on internal counters and state. All structure is inferred downstream.
- **Zero-overhead abstraction.** Every interface boundary compiles away via templates and `if constexpr`. The emitted machine code is identical to hand-written flat code.
- **Hot path is the only path that matters.** The chip tick loop runs millions of times per emulated second. Everything else is amortized.
- **No virtual dispatch, no heap allocation, no locks on the hot path.**
- **Chips are concrete, boards are generic.** A chip holds a reference to a concrete stream type and includes only the header for that type. The board is the only place that knows about both chips and ports. Changing the reconstructor, GPU upload strategy, or sync classifier never recompiles the chip.

---

## Analog Signal Fundamentals

### Multiple Drivers on One Line

Different topologies govern how multiple chips can share an analog line:

| Topology | Mechanism | Behavior | Example |
|---|---|---|---|
| Wired-OR / Wired-AND | Open-collector/drain outputs, shared pull-up | Any driver pulling low wins; no contention possible | C64 serial bus, 6502 IRQ/NMI |
| Tri-state bus | Hi-Z third state; arbiter ensures one driver at a time | Bus conflict if two drivers simultaneously active — hard short, chip damage | CPU/memory data bus |
| Resistor mixing | Each source through series resistor into shared line | Weighted voltage average; sources partially load each other | NES expansion audio, SID EXT IN |
| Current summing (virtual ground) | Sources feed op-amp inverting input | True summing, no inter-source loading, gain set by R_feedback/R_input | Professional audio mixing |

**Bus conflict consequence:** Two push-pull outputs fighting produce a crowbar short — hundreds of milliamps through transistors, heat, potential permanent damage.

### Relevant Real Hardware Cases

- **C64 IRQ/NMI:** Open-drain wired-OR — CIA, VIC-II, and cartridge all share the line safely
- **NES expansion audio:** Cartridge EXP pin passively mixed into APU output via resistor; mutual loading is intentional and must be modeled
- **SID EXT IN:** Routes into SID's internal filter — external signal becomes filterable and mixes with voices before output
- **C64 paddle ports:** POTX/POTY use SID's internal ramp generator — timing-based, not voltage-based ADC; reads must be tied to SID clock cycles
- **Cassette read path:** Raw tape head signal through board-level amplifier and comparator before reaching CIA as a digital edge; analog characteristics affect loader reliability
- **Light pen / LP pin:** Photodiode pulse feeds VIC-II LP pin; falling edge latches current raster X — a hardwired analog-to-raster-coordinate conversion at cycle accuracy

---

## Signal Slot Model

Each analog output line is a **resolved value** produced once per clock step. The slot abstracts whatever the analog line carries at that moment into a domain meaningful to the downstream consumer.

### Slot Resolution Graph

```
[VIC-II color_out] ──→ [CompositeVideoStream] ──→ VideoPort ──→ GPU texture ──→ Display

[SID voice_out]    ──→ [audio slot: raw_dac]   ┐
[SID filter_out]   ──→ [audio slot: filtered]  ├─→ MixPolicy ──→ BLEP ──→ RingBuf ──→ Host audio
[cart_exp_audio]   ──→ [audio slot: expansion] ┘
```

Resolution runs **once per pixel clock** (video) or **once per chip clock** (audio). All contributing chips write to their sub-slots before the resolver runs.

---

## VideoFlags — Shared Concrete Type

All flag signals across every video system are subsets of the same small set of sync and control signals. All fit in `uint8_t` with no bit conflicts. One concrete enum covers every system — no template parameter needed for it anywhere.

```cpp
enum class VideoFlags : uint8_t {
    None     = 0x00,
    HSync    = 0x01,   // composite sync on composite systems; H-sync on RGB
    VSync    = 0x02,   // unused on composite (classified from pulse length); explicit on RGB
    Blank    = 0x04,   // blanking active — no visible output
    Burst    = 0x08,   // composite color burst gate; unused on RGB
    BeamOn   = 0x10,   // vector: beam actively drawing
    FrameEnd = 0x20,   // vector: drawing list complete; semantic equivalent of VSync
};

constexpr VideoFlags operator|(VideoFlags a, VideoFlags b) noexcept {
    return VideoFlags(uint8_t(a) | uint8_t(b));
}
constexpr bool operator&(VideoFlags a, VideoFlags b) noexcept {
    return uint8_t(a) & uint8_t(b);
}
```

`VideoFlags` is declared in its own header. Every chip, every port, every reconstructor includes the same header and uses the same type with no indirection.

---

## Video Sample Types

The sample type is the only thing that varies between signal standards. Each is a plain struct, sized and aligned for efficient store and compare.

```cpp
// Composite / S-Video — VIC-II, TED, NES PPU, VIC-20 VIC
// color_index implies luma + chroma via chip DAC network
struct alignas(2) CompositeVideoSample {
    uint8_t    color_index;
    VideoFlags flags;
};
static_assert(sizeof(CompositeVideoSample) == 2);

// RGB — Amiga Denise, Atari ST shifter, later consoles
// Three independent DAC values; separate H and V sync
struct alignas(4) RGBVideoSample {
    uint8_t    r, g, b;
    VideoFlags flags;
};
static_assert(sizeof(RGBVideoSample) == 4);

// RGBI digital — C128 VDC, EGA
// 4-bit: R, G, B, I as individual TTL lines
struct alignas(2) RGBIVideoSample {
    uint8_t    rgbi;    // bits [3:0] = I, B, G, R
    VideoFlags flags;
};
static_assert(sizeof(RGBIVideoSample) == 2);

// Vector — Vectrex, Atari Asteroids/Tempest/Star Wars
// Beam position and intensity; no scanlines
struct alignas(8) VectorVideoSample {
    int16_t    x, y;
    uint8_t    intensity;
    uint8_t    _pad;
    VideoFlags flags;
    uint8_t    _pad2;
};
static_assert(sizeof(VectorVideoSample) == 8);
```

---

## VideoStream — Chip-Facing Write Interface

`VideoStream` is the only type a chip needs to know about. It holds a write pointer, a base pointer for position calculation, the previous flags byte for sync edge detection, and a cold-path function pointer for the rare sync event.

The chip includes only `VideoStream.hpp` — it never sees `VideoPort`, traits, reconstructors, or GPU upload code.

The `drive` function receives the sample directly. The chip constructs it at the call site from its own register-resident fields. `prev_flags` stores only the flags byte — not the full sample — because its sole purpose is sync edge detection, and storing the full sample would be wasted space and a wider store.

```cpp
// VideoStream.hpp — included by chips only

#include "VideoFlags.hpp"

template<typename SampleT>
struct VideoStream {
    SampleT*   ptr;
    SampleT*   base;
    VideoFlags prev_flags = VideoFlags::None;

    void (*on_sync_change)(void* ctx, VideoFlags flags, uint32_t pos) noexcept;
    void* ctx;

    __attribute__((always_inline))
    void drive(SampleT s) noexcept {
        SampleT* pos = ptr;
        *ptr++       = s;

        if (__builtin_expect(s.flags != prev_flags, 0)) [[unlikely]]
            on_sync_change(ctx, s.flags, (uint32_t)(pos - base));

        prev_flags = s.flags;
    }
};
```

The flags comparison is a single byte load from the struct argument (already in a register) versus a byte load from `prev_flags`. No mask, no bit manipulation, no `sync_mask` member. The comparison is self-documenting and generates optimal code.

The `[[unlikely]]` annotation tells the branch predictor the fast path is the non-sync case. The cold path function is marked `[[gnu::cold, gnu::noinline]]` at the definition site in `VideoPort`, pushing its code out of the instruction cache footprint of `drive()` entirely.

### Null Stream Sink

For inactive chips that must keep ticking but whose output should be discarded (see Mode Switching), a null sink accepts drives and does nothing:

```cpp
template<typename SampleT>
struct NullVideoStream {
    SampleT    scratch;                         // single cell — permanently L1-resident
    VideoFlags prev_flags = VideoFlags::None;

    __attribute__((always_inline))
    void drive(SampleT s) noexcept {
        scratch    = s;     // always same address — no buffer advance, no sync detection
        prev_flags = s.flags;
    }
};
```

The scratch cell stays permanently in L1 because it is always the same address. The store is not eliminated — it avoids the chip needing any branch — but costs essentially nothing.

---

## Concrete Stream Type Aliases

Each alias is declared in its own header. A chip includes exactly one:

```cpp
// CompositeVideoStream.hpp — included by VIC-II, TED, NES PPU, VIC-20 VIC
using CompositeVideoStream     = VideoStream<CompositeVideoSample>;
using NullCompositeVideoStream = NullVideoStream<CompositeVideoSample>;

// RGBVideoStream.hpp — included by Amiga Denise, Atari ST shifter, etc.
using RGBVideoStream     = VideoStream<RGBVideoSample>;
using NullRGBVideoStream = NullVideoStream<RGBVideoSample>;

// RGBIVideoStream.hpp — included by C128 VDC, etc.
using RGBIVideoStream     = VideoStream<RGBIVideoSample>;
using NullRGBIVideoStream = NullVideoStream<RGBIVideoSample>;

// VectorVideoStream.hpp — included by vector display chips
using VectorVideoStream     = VideoStream<VectorVideoSample>;
using NullVectorVideoStream = NullVideoStream<VectorVideoSample>;
```

---

## VideoPort — Owns and Wires the Stream

`VideoPort` is templated on the stream type. It owns the stream, the frame buffer, and the sync event list. It is included only by boards — never by chips.

```cpp
// VideoPort.hpp — included only by boards

template<typename StreamT>
class VideoPort {
public:
    using Stream = StreamT;
    using Sample = typename StreamT::value_type;

    VideoPort() {
        stream_.ptr            = buf_;
        stream_.base           = buf_;
        stream_.prev_flags     = VideoFlags::None;
        stream_.ctx            = this;
        stream_.on_sync_change = &VideoPort::cold_path;
    }

    Stream& stream() noexcept { return stream_; }

    FrameData swap_frame() noexcept {
        FrameData fd {
            .stream      = buf_,
            .stream_len  = (uint32_t)(stream_.ptr - buf_),
            .sync_events = sync_events_,
            .sync_count  = sync_count_,
            .signal_type = SignalTraits<Sample>::type,
        };
        stream_.ptr        = buf_;
        stream_.prev_flags = VideoFlags::None;
        sync_count_        = 0;
        return fd;
    }

private:
    Stream    stream_;
    Sample    buf_[MAX_STREAM_SAMPLES];
    SyncEvent sync_events_[MAX_SYNC_EVENTS];
    uint32_t  sync_count_ = 0;
    int       sync_run_   = 0;

    [[gnu::cold, gnu::noinline]]
    static void cold_path(void* ctx, VideoFlags flags, uint32_t pos) noexcept {
        static_cast<VideoPort*>(ctx)->handle_sync(flags, pos);
    }

    void handle_sync(VideoFlags flags, uint32_t pos) noexcept {
        bool falling = (stream_.prev_flags & VideoFlags::HSync)
                    && !(flags             & VideoFlags::HSync);
        if (falling)
            sync_events_[sync_count_++] = { pos, classify_sync() };
    }

    SyncType classify_sync() const noexcept;
};

// Concrete port types — only these names appear in board headers
using CompositeVideoPort = VideoPort<CompositeVideoStream>;
using RGBVideoPort       = VideoPort<RGBVideoStream>;
using RGBIVideoPort      = VideoPort<RGBIVideoStream>;
using VectorVideoPort    = VideoPort<VectorVideoStream>;
```

`FrameData` carries a `SignalType` field so the GPU rendering layer selects the correct shader without any additional runtime query.

---

## Chip Implementation — Zero Upstream Dependency

A chip includes only its concrete stream header. It holds a pointer to the concrete stream type (a pointer rather than a reference to support stream redirection on mode switch — see Mode Switching). No templates on the chip class, no knowledge of `VideoPort` or anything downstream.

```cpp
// VIC2.hpp
#include "CompositeVideoStream.hpp"

class VIC2 {
public:
    explicit VIC2(CompositeVideoStream* stream) : stream_(stream) {}

    void set_stream(CompositeVideoStream* s) noexcept { stream_ = s; }

    __attribute__((always_inline))
    void tick() noexcept {
        advance_raster();
        stream_->drive({ .color_index = color_latch_,
                         .flags       = flags_prepack_ });
    }

private:
    CompositeVideoStream* stream_;
    uint8_t    color_latch_   = 0;
    VideoFlags flags_prepack_ = VideoFlags::None;

    // Called only on internal flag state transitions — not every tick
    void update_flags() noexcept {
        flags_prepack_ = (hsync_ ? VideoFlags::HSync  : VideoFlags::None)
                       | (blank_ ? VideoFlags::Blank  : VideoFlags::None)
                       | (burst_ ? VideoFlags::Burst  : VideoFlags::None);
    }
};
```

`VIC2.hpp` has no `#include "VideoPort.hpp"`. Recompiling or replacing the port, reconstructor, or GPU upload layer does not recompile the chip.

The C128 VDC follows the same pattern with its own stream type:

```cpp
// VDC.hpp
#include "RGBIVideoStream.hpp"

class VDC {
public:
    explicit VDC(RGBIVideoStream* stream) : stream_(stream) {}
    void set_stream(RGBIVideoStream* s) noexcept { stream_ = s; }
    void tick() noexcept;

private:
    RGBIVideoStream* stream_;
};
```

The dependency graph is strictly one-directional:

```
Chip.hpp        ──→  ConcreteVideoStream.hpp   (chip knows only its stream type)
VideoPort.hpp   ──→  ConcreteVideoStream.hpp   (port owns the stream)
XxxBoard.hpp    ──→  Chip.hpp + VideoPort.hpp  (board wires them together)
```

---

## Board — Declares and Wires Everything

The board is the only translation unit that includes both chip headers and port headers. It declares ports as plain members, constructs chips by passing stream pointers, and exposes one `active_frame()` per display output.

```cpp
// C64Board.hpp
#include "VIC2.hpp"
#include "MOS6510.hpp"
#include "SID.hpp"
#include "CIA6526.hpp"
#include "CompositeVideoPort.hpp"
#include "AudioPort.hpp"

struct C64Board {
    CompositeVideoPort video;
    AudioPort          audio;

    MOS6510  cpu  { &mem_  };
    VIC2     vic  { &video.stream() };
    SID      sid  { &audio.stream() };
    CIA6526  cia1 { };
    CIA6526  cia2 { };

    FrameData active_frame() noexcept { return video.swap_frame(); }
};
```

A board with multiple simultaneous video outputs — or outputs selected by mode — declares all ports and wires each chip to its own stream:

```cpp
// C128Board.hpp — see Mode Switching for full detail
#include "VIC2.hpp"
#include "VDC.hpp"
#include "MOS8502.hpp"
#include "Z80.hpp"
#include "CompositeVideoPort.hpp"
#include "RGBIVideoPort.hpp"

struct C128Board {
    CompositeVideoPort     video_vic;
    RGBIVideoPort          video_vdc;
    NullCompositeVideoStream null_vic;
    NullRGBIVideoStream      null_vdc;

    MOS8502  cpu_6502 { &mem_ };
    Z80      cpu_z80  { &mem_ };
    VIC2     vic      { &video_vic.stream() };
    VDC      vdc      { &video_vdc.stream() };
    // ...
};
```

An arcade board with vector and raster output simultaneously:

```cpp
struct ArcadeBoard {
    VectorVideoPort    vector_out;
    CompositeVideoPort raster_overlay;

    VectorChip vector { &vector_out.stream()     };
    RasterChip raster { &raster_overlay.stream() };
};
```

---

## Mode Switching (C128 / Multi-CPU Boards)

The C128 switches between C64 mode (8502 + VIC-II composite) and C128 mode (VDC RGBI, or VIC-II composite for 40-column). This involves switching the active CPU and which video output the display layer consumes.

### CPUs — Skip the Inactive One (Accurate)

The hardware explicitly halts the inactive CPU. The Z80 is held via `BUSRQ` — it releases the bus and freezes. The 8502 is held via `RDY` low — it pauses and waits. Neither CPU's internal state evolves while halted. **Not ticking the inactive CPU is not just a performance optimization — it is the accurate behavior.**

A function pointer on the board selects the active CPU tick. It is updated only at mode switch time and is perfectly branch-predicted for the entire duration of the current mode:

```cpp
void (*cpu_tick)(C128Board&) noexcept = &C128Board::tick_6502;

void run_for(int cycles) noexcept {
    for (int i = 0; i < cycles; i++) {
        cpu_tick(*this);    // one indirect call — well-predicted, mode rarely changes
        vic.tick();
        vdc.tick();
        cia1.tick();
        cia2.tick();
    }
}

static void tick_6502(C128Board& b) noexcept { b.cpu_6502.tick(); }
static void tick_z80 (C128Board& b) noexcept { b.cpu_z80.tick();  }
```

### Video Chips — Must Always Tick (Inaccurate to Skip)

Both video chips run from independent clocks and evolve internal state continuously regardless of which output the monitor receives:

- The VDC has its own oscillator (12.288 MHz, independent of the system clock) and its own VRAM refresh cycle
- Software in C64 mode can read and write VDC registers at `$D600/$D601` and expects the VDC's internal counters and state to be current
- The VIC-II keeps running in 80-column mode — raster interrupts fire, the raster counter advances, sprite collision registers update
- Demos and productivity software routinely program the inactive video chip

**Skipping an inactive video chip's tick is inaccurate. Both must tick every cycle regardless of mode.**

### Redirect Inactive Stream to Null Sink

The performance optimization available is redirecting the inactive chip's stream to a null sink — eliminating frame buffer writes and sync detection without affecting the chip's internal accuracy. The chip ticks and evolves state normally; only the stream destination changes.

Mode switch updates the CPU function pointer and two stream pointers — three pointer stores, done once outside the hot loop:

```cpp
void set_mode(Mode m) noexcept {
    mode_    = m;
    cpu_tick = (m == Mode::C64 || m == Mode::C128_40col)
             ? &C128Board::tick_6502
             : &C128Board::tick_z80;

    if (m == Mode::C128_80col) {
        vic.set_stream(&null_vic);            // VIC-II output discarded
        vdc.set_stream(&video_vdc.stream());  // VDC output to real port
    } else {
        vic.set_stream(&video_vic.stream());  // VIC-II output to real port
        vdc.set_stream(&null_vdc);            // VDC output discarded
    }
}
```

### active_frame() Selects the Live Output

Called once per frame by the session — a switch statement here is negligible:

```cpp
FrameData active_frame() noexcept {
    switch (mode_) {
        case Mode::C64:
        case Mode::C128_40col: return video_vic.swap_frame();
        case Mode::C128_80col: return video_vdc.swap_frame();
    }
}
```

`FrameData.signal_type` is set by `swap_frame()` so the GPU selects the correct shader without any additional query.

### Summary of Mode Switch Behavior

| Component | On mode switch | Per cycle (hot path) |
|---|---|---|
| Inactive CPU | Not ticked — accurate | Function pointer selects active CPU only |
| Active CPU | Ticked normally | One indirect call, well-predicted |
| Both video chips | Stream pointer updated | Always ticked — accuracy requirement |
| Inactive video stream | Redirected to null sink | One store to permanent L1 scratch cell |
| Active video stream | Redirected to real port | Normal drive: one store, one compare |
| Display layer | Reads active_frame() | Consumes only the active port's FrameData |

---

## Raster Reconstruction

### CPU-Side Frame Data

The CPU writes a flat stream and a tiny sync event list. No beam tracking, no 2D management. The GPU infers all structure:

```cpp
struct FrameData {
    void*      stream;
    uint32_t   stream_len;
    SyncEvent* sync_events;
    uint32_t   sync_count;     // ~313 for C64 PAL (312 H-syncs + 1 V-sync)
    SignalType signal_type;    // drives GPU shader selection
};

struct SyncEvent {
    uint32_t stream_pos;       // sample offset in stream at falling edge
    uint8_t  type;             // SyncType::HSync, VSync, Equalizing, FrameEnd, BeamOn, BeamOff
};
```

For a C64 PAL frame: ~313 events × 5 bytes = ~1.5 KB per frame.

### Sync Pulse Classification (Composite)

Composite sync carries H-sync and V-sync on the same line, classified by pulse length. Example thresholds for C64 PAL at ~7.88 MHz dot clock:

| Pulse type | Duration | Cycles (approx) |
|---|---|---|
| H-sync | ~4.7 µs | ~37 |
| Equalizing (interlace) | ~2.3 µs | ~18 |
| V-sync (broad pulse) | ~27 µs | ~213 |

RGB systems carry separate `HSync` and `VSync` flag bits — classification by pulse length is not needed.

### GPU Pass 1 — Build Scanline Map (Compute Shader)

The sync event list is pre-computed by the CPU. Pass 1 reads it and builds a scanline offset table — O(scanlines), not O(stream_length):

```glsl
layout(local_size_x = 64) in;

struct SyncEvent { uint stream_pos; uint type; };
layout(std430, binding = 0) readonly  buffer Events  { SyncEvent events[]; };
layout(std430, binding = 1) writeonly buffer ScanMap { int scanline_starts[]; };

uniform int back_porch_cycles;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (events[i].type == HSYNC)
        scanline_starts[i] = int(events[i].stream_pos) + back_porch_cycles;
}
```

### GPU Pass 2 — Reconstruct Image (Fragment Shader)

```glsl
uniform usampler2D u_raw;
layout(std430, binding = 1) readonly buffer ScanMap { int scanline_starts[]; };
uniform int scanline_count;
uniform int active_width;

void main() {
    int py = int(v_uv.y * float(scanline_count));
    int px = int(v_uv.x * float(active_width));

    int  stream_pos = scanline_starts[py] + px;
    uint packed     = texelFetch(u_raw,
        ivec2(stream_pos % RAW_STRIDE, stream_pos / RAW_STRIDE), 0).r;

    uint flags = packed >> 8;
    if ((flags & BLANK_BIT) != 0u) { frag_color = vec4(0.0); return; }

    frag_color = texture(u_palette,
        vec2(float(packed & 0xFFu) / float(PALETTE_SIZE), 0.5));
}
```

### Interlace Field Detection

Field parity is determined from the V-sync event's position relative to the last H-sync:

```glsl
int vsync_pos      = int(events[vsync_idx].stream_pos);
int last_hsync_pos = find_last_hsync_before(vsync_pos);
int line_phase     = vsync_pos - last_hsync_pos;
int field          = (line_phase > cycles_per_line / 3) ? 1 : 0;

// Even field (0): output rows 0, 2, 4, ...
// Odd field  (1): output rows 1, 3, 5, ...
int output_row = scanline_index * 2 + field;
```

---

## Vector Graphics Signals

### What Changes

A vector system (Vectrex, Atari Asteroids/Tempest/Star Wars) has no pixel clock and no scanlines. The video hardware steers an electron beam to arbitrary X/Y coordinates via two DACs; a brightness signal controls whether the beam draws. The `VideoStream` mechanism is entirely unchanged — only the sample type and the GPU rendering pass differ.

`VectorVideoSample` carries beam position and intensity rather than a color index. `VideoFlags.BeamOn` and `VideoFlags.FrameEnd` replace `HSync`/`VSync` semantically. `FrameEnd` signals completion of the drawing list — the GPU equivalent of V-sync.

### VideoPort Cold Path for Vector

```cpp
void handle_sync(VideoFlags flags, uint32_t pos) noexcept {
    bool frame_end   = (flags              & VideoFlags::FrameEnd)
                    && !(stream_.prev_flags & VideoFlags::FrameEnd);
    bool beam_change = (flags              & VideoFlags::BeamOn)
                    != (stream_.prev_flags & VideoFlags::BeamOn);

    if (frame_end)
        sync_events_[sync_count_++] = { pos, SyncType::FrameEnd };
    if (beam_change)
        sync_events_[sync_count_++] = {
            pos, (flags & VideoFlags::BeamOn) ? SyncType::BeamOn : SyncType::BeamOff
        };
}
```

### GPU — Line Rendering and Phosphor

The GPU renders line segments between consecutive `BeamOn` samples via a geometry shader, then applies phosphor persistence via framebuffer accumulation:

```glsl
// Geometry shader: emit a quad for each consecutive beam-on sample pair
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;
in VectorSample { vec2 pos; float intensity; } gs_in[];
uniform float beam_width;

void main() {
    vec2 a    = gs_in[0].pos;
    vec2 b    = gs_in[1].pos;
    vec2 perp = vec2(-(b-a).y, (b-a).x) / length(b-a) * beam_width;

    gl_Position = vec4(a - perp, 0, 1); EmitVertex();
    gl_Position = vec4(a + perp, 0, 1); EmitVertex();
    gl_Position = vec4(b - perp, 0, 1); EmitVertex();
    gl_Position = vec4(b + perp, 0, 1); EmitVertex();
    EndPrimitive();
}

// Fragment shader: gaussian beam profile, P31 phosphor tint
void main() {
    float dist   = abs(v_dist_from_center);
    float bright = exp(-dist * dist * 8.0) * v_intensity;
    frag_color   = vec4(bright, bright * 0.95, bright * 0.8, 1.0);
}
```

Phosphor persistence: render new vectors additively into a persistence buffer; multiply buffer by `(1 - decay_rate)` each frame before rendering the next frame.

### What Is Common to Both Raster and Vector

| Component | Raster | Vector |
|---|---|---|
| `VideoStream<SampleT>` | unchanged | unchanged |
| `VideoFlags` | HSync / Blank / Burst | BeamOn / FrameEnd |
| Cold path detection | falling sync edge | BeamOn transition + FrameEnd |
| Sync event list | HSync / VSync entries | BeamOn / BeamOff / FrameEnd entries |
| PBO upload | raw stream + events | raw stream + events |
| GPU Pass 1 | build scanline offset table | build segment index list |
| GPU Pass 2 | fragment — palette lookup | geometry + fragment — line + phosphor |

---

## Color Conversion

### Static Palette (GPU)

```glsl
uniform vec4 u_palette[16];   // measured hardware values, uploaded at init

frag_color = u_palette[color_index];
```

### Mid-Frame Palette Changes

**Option A — Per-scanline palette texture:** A 2D texture `(palette_size × scanline_count)`. The CPU records palette state per scanline during emulation; the shader selects the row by fragment Y.

**Option B — Palette epoch embedding:** Pack a snapshot ID into the stream sample's upper bits. A small SSBO maps epoch → palette state. At most a few hundred distinct states per frame even for heavy demo effects.

### Composite Artifacting (Apple II, CGA)

Entirely GPU-side in the fragment shader. Runs in parallel across all pixels — O(1) GPU time regardless of resolution:

```glsl
float subcarrier_phase = mod(float(pixel_x) * SUBCARRIER_CYCLES_PER_PIXEL, 1.0) * TAU;
float composite = float(color_index & 1u);

float luma   = (composite + prev_composite + next_composite) / 3.0;
float chroma = composite - luma;
float I      = chroma * cos(subcarrier_phase);
float Q      = chroma * sin(subcarrier_phase);

frag_color   = vec4(yiq_to_rgb(vec3(luma, I, Q)), 1.0);
```

---

## GPU-Accelerated Rendering

### Index Texture

The frame buffer is an 8bpp index texture — 4x smaller than RGBA. For a C64 PAL frame (~504×312) this is ~39 KB. The active scanline fits in L2; hot lines in L1. The CPU writes raw color indices; the GPU resolves them to RGBA via palette lookup.

### PBO Upload (Zero-Copy)

```cpp
// Frame start: map PBO — CPU writes directly into GPU-accessible memory
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[write_idx_]);
auto* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, STREAM_BYTES,
    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

stream_.base = stream_.ptr = static_cast<CompositeVideoSample*>(ptr);

// Frame end:
glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RAW_STRIDE, RAW_ROWS,
    GL_RED_INTEGER, GL_UNSIGNED_SHORT, 0);   // async DMA from PBO — CPU not blocked
```

Double-buffer the PBO: GPU reads last frame's PBO while CPU writes this frame's. Zero stalls, zero copies.

### Displayed Frame Double Buffering

```cpp
std::atomic<int> display_idx{0};

// Emulator thread: one atomic store per frame (~60/sec)
display_idx.store(write_idx, std::memory_order_release);

// Display thread:
int idx = display_idx.load(std::memory_order_acquire);
```

No mutex, no condition variable.

### Shader Variants per Signal Type

Signal type is known from `FrameData.signal_type`. Separate GLSL programs per variant — no runtime branching inside shaders:

```cpp
GLuint select_shader(SignalType t) noexcept {
    switch (t) {
        case SignalType::Composite:         return shader_composite_;
        case SignalType::RGB:               return shader_rgb_;
        case SignalType::RGBI:              return shader_rgbi_;
        case SignalType::CompositeArtifact: return shader_composite_artifact_;
        case SignalType::Vector:            return shader_vector_;
    }
}
```

---

## Audio Signal Architecture

### The Fundamental Mismatch

The audio chip runs at its native clock (~985 kHz SID PAL, ~1.79 MHz NES APU) but the host wants 44100 or 48000 Hz. Naive decimation aliases badly — square and sawtooth waves contain harmonics above Nyquist. The solution is BLEP.

### BLEP: Band-Limited Step Functions

Every aliasing artifact in waveform generators comes from hard transitions. Replace each hard transition with a pre-computed band-limited step kernel (windowed sinc integral):

1. Detect each waveform state transition and its **fractional timing** within the current host sample period
2. Add the BLEP residual to a correction buffer at that fractional offset
3. Output sample = decimated naive sample + accumulated corrections

```cpp
void blep_add(float delta, double fractional_offset) noexcept {
    int n = (int)(fractional_offset * BLEP_TABLE_SIZE);
    for (int i = 0; i < BLEP_WIDTH; i++)
        correction_[i] += delta * blep_table_[n + i];
}
```

### SID Filter

State-variable form, runs at chip rate or 2–4x oversampled for stability:

```cpp
lp_ += f_ * bp_;
hp_  = input - lp_ - resonance_ * bp_;
bp_ += f_ * hp_;
// f_ ≈ 2 * sin(π * fc / chip_rate)
// Output: lp_, bp_, hp_, or notch (lp_ + hp_)
```

The fc → f_ mapping is non-linear and revision-dependent:
- **6581:** approximately exponential with a kink around mid-range
- **8580:** approximately linear

Use a lookup table fitted to measured hardware data.

### Resistor Mixing (NES)

Non-linear network derived from actual PCB resistor values — not a simple sum:

```cpp
float pulse = 95.88f  / (8128.0f  / (p1 + p2)  + 100.0f);
float tnd   = 159.79f / (1.0f / (tri/8227.0f + noise/12241.0f + dmc/22638.0f) + 100.0f);
float final = pulse + tnd;
```

---

## AudioPort Implementation

```cpp
struct AudioPort {
    alignas(64) float correction_[BLEP_WIDTH] = {};   // one cache line
    float     accumulator_ = 0.f;
    int       acc_count_   = 0;
    int       period_;         // chip clocks per host sample
    float     prev_        = 0.f;
    AudioRing ring_;

    __attribute__((always_inline))
    void drive(float value) noexcept {
        float delta = value - prev_;
        if (__builtin_expect(delta != 0.f, 0)) [[unlikely]]
            blep_add(delta, fractional_phase());

        prev_          = value;
        accumulator_  += value;

        if (++acc_count_ == period_) {
            emit_sample();
            acc_count_   = 0;
            accumulator_ = 0.f;
        }
    }

private:
    void emit_sample() noexcept {
        float s = accumulator_ * (1.f / period_) + correction_[0];
        ring_.push_unchecked(s);
        __builtin_memmove(correction_, correction_ + 1,
                          (BLEP_WIDTH - 1) * sizeof(float));
        correction_[BLEP_WIDTH - 1] = 0.f;
    }

    [[gnu::noinline]]
    void blep_add(float delta, double t) noexcept {
        // AVX2: 2 FMA instructions for BLEP_WIDTH=16
        __m256 d = _mm256_set1_ps(delta);
        for (int i = 0; i < BLEP_WIDTH; i += 8) {
            __m256 c = _mm256_load_ps(correction_ + i);
            __m256 b = _mm256_load_ps(blep_table_  + i);
            _mm256_store_ps(correction_ + i, _mm256_fmadd_ps(b, d, c));
        }
    }
};
```

### Lock-Free Ring Buffer

```cpp
struct alignas(64) AudioRing {
    static constexpr int CAP = 4096;    // power of 2 — enables & mask instead of modulo
    float    buf_[CAP];
    alignas(64) std::atomic<uint32_t> write_{0};
    alignas(64) std::atomic<uint32_t> read_{0};

    // Producer (emulator thread) — never blocks
    void push_unchecked(float s) noexcept {
        buf_[write_.load(std::memory_order_relaxed) & (CAP - 1)] = s;
        write_.fetch_add(1, std::memory_order_release);
    }

    // Consumer (audio callback)
    int pop(float* dst, int n) noexcept {
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        int avail  = std::min(n, (int)(w - r));
        for (int i = 0; i < avail; i++)
            dst[i] = buf_[(r + i) & (CAP - 1)];
        read_.store(r + avail, std::memory_order_release);
        return avail;
    }
};
```

### Clock Drift Compensation

A PI controller on ring buffer fill level adjusts the cycle budget per frame:

```
error      = actual_fill - target_fill
P_term     = Kp * error
I_term    += Ki * error
adjustment = P_term + I_term      // added to cycles_per_frame
```

---

## Host Performance Optimization

### The Chip Tick Loop

```cpp
void run_for(int cycles) noexcept {
    for (int i = 0; i < cycles; i++) {
        cpu_tick(*this);   // function pointer — active CPU only, well-predicted
        vic.tick();        // inlined — one store to video stream
        sid.tick();        // inlined — accumulate + conditional emit
        cia1.tick();
        cia2.tick();
    }
}
```

No heap, no syscalls, no locks, no virtual dispatch.

### Hot State Layout

One cache line per chip's hot state eliminates cross-line false sharing and keeps the working set in L1:

```cpp
struct alignas(64) VIC2Hot {
    uint16_t   x_count;
    uint16_t   y_count;
    uint8_t    color_latch;
    uint8_t    border_color;
    uint8_t    bg_color[4];
    VideoFlags flags_prepack;   // updated only on flag transitions
    // ... fill to 64 bytes
};
```

Rarely-accessed state (register array, sprite descriptors) lives in a separate struct.

### Branch Elimination on Hot Path

- `[[unlikely]]` / `__builtin_expect` on sync edge detection — executes ~313 times per 8M-cycle frame
- `[[gnu::cold, gnu::noinline]]` on cold path pushes it out of `drive()`'s instruction cache footprint
- Null sink writes to a permanent L1 scratch cell — same address every time, no buffer advance

### Loop Unrolling

```cpp
#pragma GCC unroll 4
for (int i = 0; i < cycles; i++) { /* all chip ticks */ }
```

Lets the OOO engine overlap independent chips' ticks across iterations.

### Software Prefetch

For chips that access RAM/ROM during tick:

```cpp
void CPU::tick() noexcept {
    __builtin_prefetch(&mem_[pc_ + 8], 0, 1);   // prefetch ahead, read, L1
    execute_cycle();
}
```

### Performance Target

On a modern x86 at 4 GHz with a 985 kHz PAL clock, ~4000 host cycles are available per emulated cycle. Target: fewer than 10 host cycles per emulated cycle → 400x real-time headroom.

```cpp
uint64_t t0 = __builtin_ia32_rdtsc();
system.run_for(CYCLES_PER_FRAME);
uint64_t t1 = __builtin_ia32_rdtsc();
// (t1 - t0) / CYCLES_PER_FRAME — target < 10
```

Profile with `perf record -g` / `perf report`. Any spike on a single chip indicates a cache miss, branch misprediction, or unexpected function call.

---

## System Assembly

### Compile-Time Instantiation

The entire system is a concrete type. Every `drive()` and `tick()` is inlined. `run_for` compiles to a single monolithic loop with no indirect calls except the one CPU function pointer:

```cpp
using C64PAL = System<
    MOS6510,
    VIC2,
    SID<Revision::R8580>,
    CIA6526, CIA6526,
    C64MemoryMap,
    CompositeVideoPort,
    AudioPort<BlepDecimator<44100>, ResistorMix<SID_R, EXPANSION_R>>
>;

using NES_NTSC = System<
    RP2A03,
    RP2C02<NtscTiming>,
    APU,
    NESMemoryMap,
    CompositeVideoPort,
    AudioPort<BlepDecimator<44100>, NESResistorMix>
>;

using AmigaOCS = System<
    MC68000,
    Denise,
    Paula,
    Agnus, CIA8520, CIA8520,
    AmigaMemoryMap,
    RGBVideoPort,
    AudioPort<BlepDecimator<44100>, AmigaMix>
>;
```

### Audio Mix Policies

```cpp
struct PassthroughMix {
    template<typename S>
    S resolve(S s) const noexcept { return s; }
};

template<int R1, int R2>
struct ResistorMix {
    float resolve(float v1, float v2) const noexcept {
        constexpr float G1 = 1.0f / R1, G2 = 1.0f / R2;
        return (v1 * G1 + v2 * G2) / (G1 + G2);
    }
};

struct NESResistorMix {
    float resolve(float p1, float p2, float tri, float noise, float dmc) const noexcept {
        float pulse = 95.88f  / (8128.0f  / (p1 + p2)  + 100.0f);
        float tnd   = 159.79f / (1.0f / (tri/8227.0f + noise/12241.0f + dmc/22638.0f) + 100.0f);
        return pulse + tnd;
    }
};
```

---

## Summary: Division of Work

| Responsibility | CPU | GPU |
|---|---|---|
| Drive pixel clock, emit packed signal sample | ✓ (one store/cycle) | |
| Maintain pre-packed flag word | ✓ (on transition only) | |
| Detect sync / beam edges, append to event list | ✓ (~313/frame, `[[unlikely]]`) | |
| Upload stream + event list via PBO | ✓ (DMA, async) | |
| Build scanline offset table or segment list | | ✓ (compute, O(lines)) |
| Detect interlace field parity | | ✓ (compute) |
| Reconstruct 2D image from stream | | ✓ (fragment) |
| Palette lookup / color conversion | | ✓ (fragment) |
| Composite artifacting / NTSC decode | | ✓ (fragment, all pixels parallel) |
| Vector line rendering + phosphor | | ✓ (geometry + fragment) |
| BLEP band-limited step correction | ✓ (inline, SIMD) | |
| Audio decimation and sample emission | ✓ (inline) | |
| Lock-free ring buffer write | ✓ | |
| Ring buffer read → host audio callback | | ✓ (host audio thread) |
| Clock drift compensation | ✓ (PI controller, per-frame) | |
| Mode switch — CPU selection | ✓ (function pointer update, rare) | |
| Mode switch — stream redirection | ✓ (pointer update, rare) | |
| Inactive CPU ticking | Skipped — accurate | |
| Inactive video chip ticking | ✓ Always — accuracy requirement | |

---

## Implementation Status

Migration status of all chips and systems to the new video/audio port infrastructure.

### Video — Chip-Level (CompositeVideoStream)

All video chips with implemented rendering have been migrated. Each chip holds a `CompositeVideoStream*` member, exposes a `set_stream()` setter, and drives the stream with `HSync`, `BeamOn`, `Blank`, `VSync`, and `FrameEnd` flags at the appropriate points in their rendering pipeline.

| Chip | Pattern | Signal Source | Commit |
|---|---|---|---|
| VIC (6560/6561) | per-dot-clock | `tick()` inline | prior session |
| VIC-II (6567/6569) | per-dot-clock | `tick()` inline | prior session |
| TED 7360 | per-dot-clock | `tick()` inline | prior session |
| NES PPU (RP2C02) | per-dot-clock | `tick()` inline | prior session |
| TIA (Atari 2600) | per-scanline | end-of-scanline flush, VSYNC edge for FrameEnd | `32c9890e` |
| Amstrad Gate Array | per-frame | after `display_->flush()`, line-by-line from framebuffer | `daef0f6c` |
| BBC VIDPROC | per-frame | in `vsync()` after flush, line-by-line from framebuffer | `daef0f6c` |
| MC6847 VDG | per-frame | private `drive_stream_from_indices()` helper, both render paths | `daef0f6c` |
| Ferranti ULA | per-frame | after `display_->flush()` in `render_frame()` | `daef0f6c` |
| TMS9918 | per-scanline | `flush_scanline()` for visible, VBlank+FrameEnd in `tick()` | `daef0f6c` |

### Video — System-Level (CompositeVideoPort)

Systems that don't use a dedicated video chip but render directly create a `CompositeVideoPort` and drive the stream from their framebuffer after `display_.flush()`.

| System | Resolution | Commit |
|---|---|---|
| C64 | chip-wired (VIC-II) | prior session |
| VIC-20 | chip-wired (VIC) | prior session |
| C16/Plus4 | chip-wired (TED) | prior session |
| NES/Famicom | chip-wired (PPU) | prior session |
| Atari 2600 | chip-wired (TIA) | `32c9890e` |
| Amstrad CPC | chip-wired (Gate Array) | `daef0f6c` |
| BBC Micro | chip-wired (VIDPROC) | `daef0f6c` |
| Acorn Atom | chip-wired (MC6847) | `daef0f6c` |
| VTech VZ | chip-wired (MC6847) | `daef0f6c` |
| ZX Spectrum | chip-wired (Ferranti ULA) + audio_port_ | `daef0f6c` |
| CHIP-8 | 128×64, system-level render | `79b24bf1` |
| KC85 | 320×256, system-level render | `79b24bf1` |
| Z9001 | 320×192, system-level render | `79b24bf1` |
| Z1013 | 256×256, system-level render | `79b24bf1` |
| Bomb Jack (arcade) | 256×224, system-level render | `79b24bf1` |
| Namco Arcade | 224×288, system-level render | `79b24bf1` |

### Audio — Chip-Level (AudioPort)

Dedicated sound chips use `AudioPort::drive()` for per-clock output with BLEP decimation, alongside their legacy audio buffer for backward compatibility.

| Chip | Drive Method | Commit |
|---|---|---|
| MOS 6581 SID | `drive()` per chip clock | prior session |
| NES APU (RP2A03) | `drive()` per chip clock | prior session |
| AY-3-8910 PSG | `drive()` per chip clock | prior session |
| SN76489 | `drive()` per chip clock | prior session |
| Namco WSG | `drive()` per chip clock | prior session |
| TIA (2-channel audio) | `drive_sample()` pre-decimated | `32c9890e` |

### Audio — System-Level (AudioPort)

Systems with software-generated audio (beeper, CTC) use `AudioPort::drive_sample()` for pre-decimated output alongside their legacy ring buffer.

| System | Audio Source | Commit |
|---|---|---|
| ZX Spectrum | beeper + AY mix | `79b24bf1` |
| KC85 | CTC beeper (2 channels) | `79b24bf1` |

### Not Migrated (Intentional)

| System/Chip | Reason |
|---|---|
| Apple II | Display and audio are TODO stubs |
| Oric | Display and audio are TODO stubs |
| LC80 | LED segment display (no raster), audio stub |
| Apple 1 | Terminal-style display only |
| Z9001, Z1013 audio | Audio generation is a stub |
| CHIP-8 audio | Pull-based on-demand generation; not suitable for push-based AudioPort |
| Inactive video stream writes | Null sink — L1 scratch cell | |