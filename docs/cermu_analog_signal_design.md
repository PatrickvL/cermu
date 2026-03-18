# Cermu Analog Signal Emulation Design Plan

## Table of Contents

1. [Core Philosophy](#core-philosophy)
2. [Analog Signal Fundamentals](#analog-signal-fundamentals)
3. [Signal Slot Model](#signal-slot-model)
4. [Video Signal Architecture](#video-signal-architecture)
5. [VideoPort Implementation](#videoport-implementation)
6. [Raster Reconstruction](#raster-reconstruction)
7. [Color Conversion](#color-conversion)
8. [GPU-Accelerated Rendering](#gpu-accelerated-rendering)
9. [Audio Signal Architecture](#audio-signal-architecture)
10. [AudioPort Implementation](#audioport-implementation)
11. [Host Performance Optimization](#host-performance-optimization)
12. [System Assembly](#system-assembly)

---

## Core Philosophy

- **Chips drive pins; ports observe signals.** A chip has no semantic knowledge of scanlines, frames, or sample rates. It drives output pins based on internal counters and state. All structure is inferred downstream.
- **Zero-overhead abstraction.** Every interface boundary compiles away via templates and `if constexpr`. The emitted machine code is identical to hand-written flat code.
- **Hot path is the only path that matters.** The chip tick loop runs millions of times per emulated second. Everything else is amortized.
- **No virtual dispatch, no heap allocation, no locks on the hot path.**

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

**Bus conflict consequence:** Two push-pull outputs fighting produce a crowbar short — hundreds of milliamps through transistors, heat, potential permanent damage. Bus transceivers with controlled enable timing prevent this.

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
[VIC-II color_out] ──→ [video slot] ──→ RasterReconstructor ──→ GPU texture ──→ Display

[SID voice_out]    ──→ [audio slot: raw_dac]   ┐
[SID filter_out]   ──→ [audio slot: filtered]  ├─→ MixPolicy ──→ BLEP ──→ RingBuf ──→ Host audio
[cart_exp_audio]   ──→ [audio slot: expansion] ┘
```

Resolution runs **once per pixel clock** (video) or **once per chip clock** (audio). All contributing chips write to their sub-slots before the resolver runs — resolution happens at the end of each clock's update pass.

---

## Video Signal Architecture

### Signal Types by System

Different output standards require different packed sample types:

```cpp
// Composite / S-Video (VIC-II, TED, NES PPU, VIC-20)
// color_index implies both luma and chroma via chip DAC network
struct CompositeSignalTraits {
    using Sample = uint16_t;        // bits[7:0]=color_index, bits[15:8]=flags
    static constexpr bool separate_sync = false;
    static constexpr bool has_rgb       = false;
};

// RGB (Amiga, Atari ST, later consoles)
// Three independent DAC values per channel
struct RGBSignalTraits {
    using Sample = uint32_t;        // r,g,b,flags each one byte
    static constexpr bool separate_sync = true;
    static constexpr bool has_rgb       = true;
};

// RGBI digital (C128, EGA)
// 4-bit digital: R, G, B, I as individual TTL lines
struct RGBISignalTraits {
    using Sample = uint8_t;         // 4-bit RGBI packed
    static constexpr bool separate_sync = true;
    static constexpr bool has_rgb       = true;
};
```

### Flag Layout (Composite, uint16_t)

```
bits [7:0]   color_index   — chip palette index
bit  8       csync         — composite sync active
bit  9       vsync         — (unused for composite; vsync is classified from pulse length)
bit  10      blank         — blanking active
bit  11      burst         — color burst gate active
bits [15:12] reserved
```

The high byte (`bits[15:8]`) contains all sync/control flags so a single byte comparison detects any flag change without masking.

### Sync Pulse Classification (Composite Systems)

Sync pulse length distinguishes pulse type. Example thresholds for C64 PAL at ~7.88 MHz dot clock:

| Pulse type | Duration | Cycles (approx) |
|---|---|---|
| H-sync | ~4.7 µs | ~37 |
| Equalizing (interlace) | ~2.3 µs | ~18 |
| V-sync (broad pulse) | ~27 µs | ~213 |

### Pre-Packed Flags

The chip maintains a `flags_prepack_` field that is updated **only on transitions** (far less frequent than tick). The hot path is a single OR and store:

```cpp
// Updated only when internal sync/blank/burst state changes — not every tick
void VIC2::update_flags() noexcept {
    flags_prepack_ = (hsync_  ? 0x0100 : 0)
                   | (blank_  ? 0x0400 : 0)
                   | (burst_  ? 0x0800 : 0);
}

// Hot path: one OR, one 16-bit store
void VIC2::tick() noexcept {
    advance_raster();
    *stream_ptr_++ = flags_prepack_ | color_latch_;
}
```

The chip emits directly into the video port's stream buffer. No intermediate struct, no function call in the common case.

---

## VideoPort Implementation

### Design Goals

- Chip sees only `drive(sample)` — one call per pixel clock
- Host sees only `swap_frame()` — one call per frame
- All implementation details hidden: stream pointer, sync event list, base pointer, previous sample, classify logic
- Cold path (`on_sync_change`) pushed out of hot path's instruction cache footprint

```cpp
template<typename Traits>
class VideoPort {
public:
    using Sample = typename Traits::Sample;

    __attribute__((always_inline))
    void drive(Sample s) noexcept {
        Sample* pos = stream_ptr_;
        *stream_ptr_++ = s;

        if (__builtin_expect(sync_changed(s), 0)) [[unlikely]]
            on_sync_change(s, pos);

        prev_ = s;
    }

    // Atomically hand off completed frame data, reset for next frame
    FrameData swap_frame() noexcept {
        FrameData fd {
            .stream      = stream_base_,
            .stream_len  = (uint32_t)(stream_ptr_ - stream_base_),
            .sync_events = sync_events_,
            .sync_count  = sync_count_,
        };
        stream_ptr_ = stream_base_;
        sync_count_ = 0;
        prev_       = 0;
        return fd;
    }

private:
    static constexpr Sample SYNC_BITS  = /* system-specific flag mask */;
    static constexpr Sample CSYNC_BIT  = 0x0100;
    static constexpr Sample HSYNC_BIT  = 0x0100;
    static constexpr Sample VSYNC_BIT  = 0x0200;

    Sample*   stream_base_;
    Sample*   stream_ptr_;
    Sample    prev_ = 0;

    SyncEvent sync_events_[MAX_SYNC_EVENTS];  // ~400 entries sufficient for any system
    uint32_t  sync_count_ = 0;

    __attribute__((always_inline))
    bool sync_changed(Sample s) const noexcept {
        if constexpr (Traits::separate_sync)
            return (s ^ prev_) & (HSYNC_BIT | VSYNC_BIT);
        else
            return (s ^ prev_) & CSYNC_BIT;
    }

    [[gnu::cold, gnu::noinline]]
    void on_sync_change(Sample s, Sample* pos) noexcept {
        if constexpr (Traits::separate_sync) {
            if ((prev_ & HSYNC_BIT) && !(s & HSYNC_BIT))
                sync_events_[sync_count_++] = { stream_pos(pos), SyncType::HSync };
            if ((prev_ & VSYNC_BIT) && !(s & VSYNC_BIT))
                sync_events_[sync_count_++] = { stream_pos(pos), SyncType::VSync };
        } else {
            bool falling = (prev_ & CSYNC_BIT) && !(s & CSYNC_BIT);
            if (falling)
                sync_events_[sync_count_++] = { stream_pos(pos), classify_sync() };
        }
    }

    uint32_t stream_pos(Sample* pos) const noexcept {
        return (uint32_t)(pos - stream_base_);
    }

    SyncType classify_sync() const noexcept;  // examine recent sync run length
};

// Concrete types
using C64VideoPort   = VideoPort<CompositeSignalTraits>;
using AmigaVideoPort = VideoPort<RGBSignalTraits>;
using C128VideoPort  = VideoPort<RGBISignalTraits>;
```

### SyncEvent Structure

```cpp
struct SyncEvent {
    uint32_t stream_pos;    // byte offset in raw stream at falling edge
    uint8_t  type;          // SyncType::HSync, VSync, Equalizing
};
```

For a C64 PAL frame: ~312 H-syncs + 1 V-sync = ~313 events × 5 bytes = ~1.5 KB per frame.

### What the CPU Inner Loop Looks Like

After optimization the video contribution to the chip tick loop is:

```cpp
// One 16-bit store per pixel clock
// Sync change detection: one XOR, one branch (predicted not-taken ~313/8M times per second)
video_port_.drive(flags_prepack_ | color_latch_);
```

---

## Raster Reconstruction

### The Key Insight

Reconstruction is entirely GPU-side. The CPU does not track beam X/Y, does not compute scanline boundaries, does not manage frame buffers. It writes a flat stream and a tiny sync event list. The GPU infers all 2D structure.

### CPU-Side Frame Data

```cpp
struct FrameData {
    uint16_t* stream;        // raw signal samples, linear, one per pixel clock
    uint32_t  stream_len;    // total samples this frame
    SyncEvent* sync_events;  // falling-edge sync events
    uint32_t   sync_count;   // entries used (~313 for C64 PAL)
};
```

### GPU Pass 1 — Build Scanline Map (Compute Shader)

The sync event list is already pre-computed by the CPU. Pass 1 is trivial:

```glsl
layout(local_size_x = 64) in;

struct SyncEvent { uint stream_pos; uint type; };
layout(std430, binding = 0) readonly buffer Events  { SyncEvent events[]; };
layout(std430, binding = 1) writeonly buffer ScanMap { int scanline_starts[]; };

uniform int back_porch_cycles;   // system-specific, e.g. 44 for C64 PAL

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (events[i].type == HSYNC)
        scanline_starts[i] = int(events[i].stream_pos) + back_porch_cycles;
}
```

### GPU Pass 2 — Reconstruct Image (Fragment Shader)

```glsl
uniform usampler2D u_raw;          // stream as R16UI texture
uniform sampler2D  u_palette;      // 1D palette texture, or uniform vec4[N]
layout(std430, binding = 1) readonly buffer ScanMap { int scanline_starts[]; };
uniform int scanline_count;
uniform int active_width;

void main() {
    int py = int(v_uv.y * float(scanline_count));
    int px = int(v_uv.x * float(active_width));

    int stream_pos = scanline_starts[py] + px;
    uint packed    = texelFetch(u_raw,
        ivec2(stream_pos % RAW_STRIDE, stream_pos / RAW_STRIDE), 0).r;

    uint flags = packed >> 8;
    if ((flags & BLANK_BIT) != 0u) {
        frag_color = vec4(0.0);
        return;
    }
    uint idx   = packed & 0xFFu;
    frag_color = texture(u_palette, vec2(float(idx) / float(PALETTE_SIZE), 0.5));
}
```

### Interlace Field Detection

Field parity is determined from the V-sync event's position relative to the last H-sync:

```glsl
// In compute shader, when V-sync event is found:
int vsync_pos      = int(events[vsync_idx].stream_pos);
int last_hsync_pos = find_last_hsync_before(vsync_pos);
int line_phase     = vsync_pos - last_hsync_pos;
int field          = (line_phase > cycles_per_line / 3) ? 1 : 0;

// Output row mapping:
// Even field (0): rows 0, 2, 4, ...
// Odd field  (1): rows 1, 3, 5, ...
int output_row = scanline_index * 2 + field;
```

For non-interlaced systems (the common case), field is always 0 and output_row = scanline_index.

---

## Color Conversion

### Static Palette (GPU)

The palette is a uniform array or 1D texture uploaded at system init (and re-uploaded if palette registers change mid-frame):

```glsl
// Fragment shader — composite system
uniform vec4 u_palette[16];        // measured hardware values

// ...
frag_color = u_palette[color_index];
```

### Mid-Frame Palette Changes

For systems that change palette registers mid-frame (C64 border tricks, NES palette cycling):

**Option A — Per-scanline palette texture**

A 2D texture of dimensions `(palette_size × scanline_count)`. The CPU records palette state at each scanline into a staging buffer; the shader selects the row by fragment Y.

**Option B — Palette epoch embedding**

Pack a palette snapshot ID into the upper bits of each stream sample:

```
bits [7:0]   color_index
bits [15:8]  palette_epoch   (increments each time any palette register changes)
```

A small SSBO maps epoch → palette state. At most a few hundred distinct states per frame even for heavy demo effects.

### Composite Artifacting (Apple II, CGA)

Entirely GPU-side in the fragment shader. No CPU work:

```glsl
// Reconstruct NTSC composite signal from raw 1-bit luma stream
// then demodulate to RGB

const float TAU = 6.2831853;
const float SUBCARRIER_CYCLES_PER_PIXEL = 0.25;   // NTSC: subcarrier / pixel clock

float subcarrier_phase = mod(float(pixel_x) * SUBCARRIER_CYCLES_PER_PIXEL, 1.0) * TAU;
float composite = float(color_index & 1u);         // 1-bit luma

// Approximate luma/chroma separation (box filter — widen kernel for more accuracy)
float luma   = (composite + prev_composite + next_composite) / 3.0;
float chroma = composite - luma;
float I      = chroma * cos(subcarrier_phase);
float Q      = chroma * sin(subcarrier_phase);

frag_color   = vec4(yiq_to_rgb(vec3(luma, I, Q)), 1.0);
```

Runs in parallel across all pixels — O(1) GPU time regardless of resolution.

### PAL Phase Alternation

PAL rotates chroma subcarrier phase by 90° per line. The sync event list includes per-line phase flags (or the shader derives phase from line number). The demodulation matrix is adjusted accordingly.

---

## GPU-Accelerated Rendering

### Framebuffer: 8bpp Index Texture

```cpp
// Allocate as 8bpp — 4x smaller than RGBA
// For C64 PAL ~504×312: ~157 KB → ~39 KB
// Active scanline fits in L2; hot lines fit in L1
alignas(4096) uint8_t stream_buf_[CYCLES_PER_FRAME];
```

### PBO Upload (Zero-Copy)

```cpp
// Frame start: map PBO — CPU writes directly into GPU-accessible memory
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[write_idx_]);
uint8_t* ptr = (uint8_t*)glMapBufferRange(
    GL_PIXEL_UNPACK_BUFFER, 0, STREAM_BYTES,
    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

stream_base_ = ptr;    // chip ticks write here directly

// Frame end:
glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RAW_STRIDE, RAW_ROWS,
    GL_RED_INTEGER, GL_UNSIGNED_SHORT, 0);  // 0 = read from bound PBO, async DMA
```

Double-buffer the PBO: GPU reads last frame's PBO while CPU writes this frame's. Zero stalls, zero copies.

### Double Buffering the Displayed Frame

```cpp
struct FrameBuffers {
    uint32_t pbo[2];
    std::atomic<int> display_idx{0};   // which PBO the display thread reads
};

// Emulator thread: swap on frame complete — one atomic store per frame (~60/sec)
fb_.display_idx.store(write_idx, std::memory_order_release);

// Display thread: acquire, render from that PBO
int idx = fb_.display_idx.load(std::memory_order_acquire);
```

No mutex, no condition variable.

---

## Audio Signal Architecture

### The Fundamental Mismatch

The audio chip runs at its native clock (~985 kHz SID PAL, ~1.79 MHz NES APU) but the host wants 44100 or 48000 Hz. Naive decimation aliases badly — square/sawtooth waves contain harmonics above Nyquist.

### BLEP: Band-Limited Step Functions

Every aliasing artifact in waveform generators comes from hard transitions. Replace each hard transition with a **pre-computed band-limited step kernel** (windowed sinc integral).

At each waveform transition:
1. Compute the **fractional timing** of the transition within the current host sample period
2. Add the BLEP residual (difference between ideal band-limited step and naive step) to a correction buffer
3. Output = decimated naive sample + accumulated corrections

```cpp
void blep_add(float delta, double fractional_offset) noexcept {
    // fractional_offset in [0, 1): where within current output sample the transition fell
    // delta: step amplitude
    // blep_table_: precomputed windowed sinc integral, high-resolution
    int n = (int)(fractional_offset * BLEP_TABLE_SIZE);
    for (int i = 0; i < BLEP_WIDTH; i++)
        correction_[i] += delta * blep_table_[n + i];
}
```

BLEP table: precomputed once as the integral of a windowed sinc. Kernel width 16–32 floats gives excellent quality.

### SID Filter

The SID's state-variable filter runs at chip rate (or 2–4x oversampled for numerical stability):

```cpp
// Per chip-clock update — state-variable form
lp_ += f_ * bp_;
hp_  = input - lp_ - resonance_ * bp_;
bp_ += f_ * hp_;
// f_ ≈ 2 * sin(π * fc / chip_rate)
// Output selection: lp_, bp_, hp_, or notch = lp_ + hp_
```

The cutoff frequency mapping is non-linear and revision-dependent:
- **6581:** approximately exponential with a kink around mid-range
- **8580:** approximately linear

Use a lookup table fitted to measured hardware data for the fc → f_ mapping.

### Resistor Mixing (NES)

The NES APU uses a non-linear resistor DAC network. The mix is not a simple sum:

```cpp
float pulse_out = 95.88f / (8128.0f / (pulse1 + pulse2) + 100.0f);
float tnd_out   = 159.79f / (
    1.0f / (triangle / 8227.0f + noise / 12241.0f + dmc / 22638.0f)
    + 100.0f);
float final     = pulse_out + tnd_out;
```

These constants are derived from actual PCB resistor values. The MixPolicy for the audio port encodes this formula. Each chip writes its raw DAC value; the port resolver applies the network equation.

---

## AudioPort Implementation

### Drive Path

The chip calls `audio_port_.drive(value)` every phi2 clock. The port handles BLEP, decimation, and ring buffer write — all inline:

```cpp
struct AudioPort {
    alignas(64) float correction_[BLEP_WIDTH] = {};  // one cache line
    float     accumulator_ = 0.f;
    int       acc_count_   = 0;
    int       period_;         // chip clocks per host sample (e.g. 22 for 44100 from 985248 Hz)
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
            acc_count_    = 0;
            accumulator_  = 0.f;
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
        // SIMD (AVX2): 2 FMA operations for BLEP_WIDTH=16
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
    static constexpr int CAP = 4096;    // power of 2 — enables & mask
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
        uint32_t r     = read_.load(std::memory_order_relaxed);
        uint32_t w     = write_.load(std::memory_order_acquire);
        int      avail = std::min(n, (int)(w - r));
        for (int i = 0; i < avail; i++)
            dst[i] = buf_[(r + i) & (CAP - 1)];
        read_.store(r + avail, std::memory_order_release);
        return avail;
    }
};
```

### Clock Drift Compensation

Emulator and host audio clock drift relative to each other. A PI controller on ring buffer fill level slightly adjusts emulation speed (or resampling ratio) to maintain target fill:

```
error  = actual_fill - target_fill          // e.g. target = CAP / 2
P_term = Kp * error
I_term += Ki * error
adjustment = P_term + I_term               // applied to cycles_per_frame budget
```

---

## Host Performance Optimization

### The Chip Tick Loop

The master loop runs every cycle. It must have no heap, no syscalls, no locks, no virtual dispatch:

```cpp
// In Session::run_for(int cycles):
for (int i = 0; i < cycles; i++) {
    cpu_.tick();
    vic_.tick();    // calls video_port_.drive() — one store
    sid_.tick();    // calls audio_port_.drive() — accumulate + conditional emit
    cia1_.tick();
    cia2_.tick();
}
```

### Hot State Layout

Pack the chip's hot state into a single cache line:

```cpp
struct alignas(64) VIC2Hot {   // exactly 64 bytes — one cache line
    uint16_t x_count;
    uint16_t y_count;
    uint8_t  color_latch;
    uint8_t  border_color;
    uint8_t  bg_color[4];
    uint16_t flags_prepack;    // updated only on transitions
    // ... fill to 64 bytes
};
```

Rarely-accessed state (register array, sprite descriptors) lives in a separate struct. The CPU's L1 prefetcher keeps the hot struct resident across consecutive ticks.

### Branch Elimination

- Sync edge detection: `[[unlikely]]` / `__builtin_expect` — executes ~313/8M times per second; branch predictor never misses the fast path
- `[[gnu::cold, gnu::noinline]]` on `on_sync_change` pushes cold code out of the instruction cache footprint of `drive()`
- Blanking: write unconditionally into stream; the GPU shader discards blank samples — zero branches in CPU hot path

### Loop Unrolling

Manual 4x unroll lets the OOO engine overlap independent chips' ticks:

```cpp
for (int i = 0; i < cycles; i += 4) {
    cpu_.tick(); vic_.tick(); sid_.tick(); cia1_.tick(); cia2_.tick();
    cpu_.tick(); vic_.tick(); sid_.tick(); cia1_.tick(); cia2_.tick();
    cpu_.tick(); vic_.tick(); sid_.tick(); cia1_.tick(); cia2_.tick();
    cpu_.tick(); vic_.tick(); sid_.tick(); cia1_.tick(); cia2_.tick();
}
```

Equivalent: `#pragma GCC unroll 4`.

### Software Prefetch

For chips that access RAM/ROM during tick:

```cpp
void CPU::tick() noexcept {
    __builtin_prefetch(&mem_[pc_ + 8], 0, 1);   // prefetch ahead, read, L1
    execute_cycle();
}
```

Hides ~4 cycle L1 miss latency for sequential program execution.

### SIMD for BLEP

AVX2 FMA for the BLEP correction buffer accumulation — 2 instructions for `BLEP_WIDTH=16`:

```cpp
// Aligned arrays: correction_[16], blep_table_[16]
__m256 d = _mm256_set1_ps(delta);
for (int i = 0; i < BLEP_WIDTH; i += 8) {
    __m256 c = _mm256_load_ps(correction_ + i);
    __m256 b = _mm256_load_ps(blep_table_  + i);
    _mm256_store_ps(correction_ + i, _mm256_fmadd_ps(b, d, c));
}
```

### Performance Target

On a modern x86 at 4 GHz with a 985 kHz PAL clock, there are ~4000 host cycles available per emulated cycle before hitting 1x real-time. Target: <10 host cycles per emulated cycle → 400x real-time headroom.

**Verify with:**

```cpp
uint64_t t0 = __builtin_ia32_rdtsc();
system.run_for(CYCLES_PER_FRAME);
uint64_t t1 = __builtin_ia32_rdtsc();
// cycles_per_emulated_cycle = (t1 - t0) / CYCLES_PER_FRAME
// Target: < 10
```

Profile with `perf record -g` / `perf report` — any spike on a single chip indicates a cache miss, branch misprediction, or unexpected function call.

---

## System Assembly

### Compile-Time Instantiation

The entire system is a concrete type. The compiler sees all types statically — every `drive()` and `tick()` is inlined. The `run_for` loop becomes a single monolithic function:

```cpp
template<
    typename CPU,
    typename VideoChip,
    typename AudioChip,
    typename... SupportChips,
    typename MemoryMap,
    typename VideoPortT,
    typename AudioPortT
>
class System {
    CPU         cpu_;
    VideoChip   vid_;
    AudioChip   aud_;
    std::tuple<SupportChips...> support_;
    MemoryMap   mem_;
    VideoPortT  video_port_;
    AudioPortT  audio_port_;

public:
    void run_for(int cycles) noexcept {
        for (int i = 0; i < cycles; i++) {
            cpu_.tick();
            vid_.tick();
            aud_.tick();
            std::apply([](auto&... c) { (c.tick(), ...); }, support_);
        }
    }
};

// Concrete system types
using C64PAL = System<
    MOS6510,
    VIC2<PalTiming>,
    SID<Revision::R8580>,
    CIA6526, CIA6526,
    C64MemoryMap,
    VideoPort<CompositeSignalTraits>,
    AudioPort<BlepDecimator<44100>, ResistorMix<SID_R, EXPANSION_R>>
>;

using NES_NTSC = System<
    RP2A03,
    RP2C02<NtscTiming>,
    APU,
    NESMemoryMap,
    VideoPort<CompositeSignalTraits>,
    AudioPort<BlepDecimator<44100>, NESResistorMix>
>;

using AmigaOCS = System<
    MC68000,
    Denise,
    Paula,
    Agnus, CIA8520, CIA8520,
    AmigaMemoryMap,
    VideoPort<RGBSignalTraits>,
    AudioPort<BlepDecimator<44100>, AmigaMix>
>;
```

### Port Mix Policies

```cpp
// Passthrough: single source, no mixing
struct PassthroughMix {
    template<typename S>
    S resolve(S signal) const noexcept { return signal; }
};

// Resistor network: weighted sum by conductance (1/R)
template<int R1, int R2>
struct ResistorMix {
    float resolve(float v1, float v2) const noexcept {
        constexpr float G1 = 1.0f / R1, G2 = 1.0f / R2;
        return (v1 * G1 + v2 * G2) / (G1 + G2);
    }
};

// NES non-linear mix: derived from PCB resistor values
struct NESResistorMix {
    float resolve(float p1, float p2, float tri, float noise, float dmc) const noexcept {
        float pulse = 95.88f / (8128.0f / (p1 + p2) + 100.0f);
        float tnd   = 159.79f / (1.0f / (tri/8227.0f + noise/12241.0f + dmc/22638.0f) + 100.0f);
        return pulse + tnd;
    }
};
```

### Shader Variants per Signal Type

Since signal type is a compile-time distinction, select shader programs at system init:

```cpp
enum class ShaderVariant { Composite, RGB, RGBI, CompositeArtifact };

// Selected once at system construction
GLuint select_shader(ShaderVariant v) {
    switch (v) {
        case ShaderVariant::Composite:         return shader_composite_;
        case ShaderVariant::RGB:               return shader_rgb_;
        case ShaderVariant::RGBI:              return shader_rgbi_;
        case ShaderVariant::CompositeArtifact: return shader_composite_artifact_;
    }
}
```

No runtime branching inside shaders — separate GLSL programs per variant.

---

## Summary: Division of Work

| Responsibility | CPU | GPU |
|---|---|---|
| Drive pixel clock, emit packed signal sample | ✓ (one store/cycle) | |
| Maintain pre-packed flag word | ✓ (on transition only) | |
| Detect sync edges, append to event list | ✓ (~313 times/frame, `[[unlikely]]`) | |
| Upload stream + sync event list via PBO | ✓ (DMA, async) | |
| Build scanline offset table from event list | | ✓ (compute, O(lines)) |
| Detect interlace field parity | | ✓ (compute) |
| Reconstruct 2D image from stream | | ✓ (fragment) |
| Palette lookup / color conversion | | ✓ (fragment) |
| Composite artifacting / NTSC decode | | ✓ (fragment, parallel across all pixels) |
| BLEP band-limited step correction | ✓ (inline, SIMD) | |
| Audio decimation and sample emission | ✓ (inline) | |
| Lock-free ring buffer write | ✓ | |
| Ring buffer read → host audio callback | | ✓ (host audio thread) |
| Clock drift compensation | ✓ (PI controller, per-frame) | |
