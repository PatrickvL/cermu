# Audio Thread Separation — Design Document

## Summary

Offload audio **synthesis** to a dedicated thread while keeping register I/O
timing and IRQ/DMA side-effects on the emulation thread.  The emulation thread
enqueues timestamped register writes; the audio thread drains the queue, applies
writes at the correct sample offset, and pushes completed sample blocks to an
output ring buffer.  One audio thread services all active sessions through a
shared mixer.

```
Emulation thread:  CPU + PPU (lockstepped) + audio register façade + IRQ/DMA timing
Audio thread:      synthesis engine per session + resampler + shared mixer → output
```

---

## Motivation

Audio synthesis (waveform generation, envelope clocking, filter evaluation,
CIC decimation, DC blocking) is the largest chunk of per-cycle work that
doesn't participate in cycle-exact CPU read-back.  The coupling is almost
entirely one-directional: CPU writes registers, sound chip produces samples.

Moving synthesis off the emulation thread:

1. Frees ~15–25 % of per-cycle work from the hot loop (measured on SID).
2. Lets the audio thread run at its own cadence without frame-rate jitter.
3. Keeps the emulation thread simpler — it only manages bus timing effects.
4. Scales naturally with the multi-session architecture (one mixer thread,
   N session queues).

---

## Coupling Analysis

### NES APU

| Interaction | Direction | Frequency | Offloadable? |
|---|---|---|---|
| Register writes ($4000–$4017) | CPU → APU | Every frame, bursty | Yes (queue) |
| Status read ($4015) | CPU ← APU | Uncommon | IRQ flags stay on emu thread |
| Frame counter IRQ | APU → CPU | Deterministic from $4017 | Pre-compute on emu thread |
| DMC IRQ | APU → CPU | End-of-sample | Pre-compute on emu thread |
| DMC sample fetch | APU → CPU bus | 1–4 cycle stall | Timing penalty on emu thread; data fetch is a normal bus read |

**DMC stall**: The emulation thread models the 1–4 cycle halt penalty.  The
actual sample byte is read from CPU address space by the emulation thread and
enqueued as a `DMC_SAMPLE_LOADED` event carrying the byte value.  The audio
thread consumes it when processing that timestamp.

**Frame counter / DMC IRQ**: Both are deterministic from register state.  On
every write to $4017 (frame counter mode) or $4010–$4013 (DMC config), the
emulation thread computes the next IRQ cycle and schedules it locally.  No
back-channel from the audio thread needed.

### C64 SID (MOS 6581 / 8580)

| Interaction | Direction | Frequency | Offloadable? |
|---|---|---|---|
| Register writes ($D400–$D418) | CPU → SID | Every frame, bursty | Yes (queue) |
| OSC3 readback ($D41B) | CPU ← SID | Rare (demos, RNG) | Sync query fallback |
| ENV3 readback ($D41C) | CPU ← SID | Rare (demos) | Sync query fallback |
| Paddle ADC ($D419–$D41A) | CPU ← SID | Rare | Can stay on emu thread |

**Oscillator/envelope readback**: $D41B and $D41C are cold-path reads used by
some demoscene productions for RNG or visual sync.  When a read to $D41B/$D41C
occurs, the emulation thread issues a synchronous query:

1. Emu thread posts a `SYNC_READ_REQUEST` with the current cycle timestamp.
2. Audio thread catches up to that timestamp, writes the result into a shared
   `std::atomic<uint16_t>` (OSC3 | ENV3 packed).
3. Emu thread spins briefly (sub-microsecond on x86) until the result is ready.

This reintroduces blocking but only on the actual read — 99 % of software never
hits it.  A compile-time or runtime flag can disable the query path entirely for
known-safe titles.

---

## Architecture

### Components

```
┌─────────────────────────────────────┐
│         Emulation Thread            │
│                                     │
│  CPU ←→ PPU (lockstepped)           │
│   │                                 │
│   ├─ Register writes → AudioCmdQueue│──────┐
│   ├─ DMC stall timing (local)       │      │
│   ├─ IRQ scheduling (local)         │      │
│   └─ $D41B/$D41C sync query ←───────│──┐   │
│                                     │  │   │
└─────────────────────────────────────┘  │   │
                                         │   │
┌────────────────────────────────────────┼───┼┐
│           Audio Thread                 │   ││
│                                        │   ││
│  ┌──────────────┐  drain  ┌────────────┤   ││
│  │ AudioCmdQueue│◄────────│ Synthesis  │   ││
│  └──────────────┘         │ Engine     │   ││
│                           │ (per chip) │   ││
│  sync readback ──────────►│            │   ││
│                           └────┬───────┘   ││
│                                │            ││
│                           ┌────▼───────┐   ││
│                           │   Mixer    │   ││
│                           │ (all sess) │   ││
│                           └────┬───────┘   ││
│                                │            ││
│                           ┌────▼───────┐   ││
│                           │ Output Ring│   ││
│                           │   Buffer   │   ││
│                           └────────────┘   ││
│                                            ││
└────────────────────────────────────────────┘│
                                              │
┌─────────────────────────────────────────────┘
│  SDL / platform audio callback
│  → drains Output Ring Buffer
└──────────────────────────────────────────────
```

### AudioCommandQueue

Lock-free SPSC queue of timestamped commands.  One queue per sound chip
instance.  Lives in `src/utils/audio_cmd_queue.hpp`.

```cpp
struct AudioCommand {
    uint64_t cycle;        // absolute cycle stamp (monotonic)
    uint8_t  type;         // REGISTER_WRITE, DMC_SAMPLE_LOADED, RESET, ...
    uint8_t  reg;          // register index (0–31)
    uint8_t  value;        // data byte
    uint8_t  pad;          // alignment
};
static_assert(sizeof(AudioCommand) == 12);
```

The queue is a fixed-capacity ring buffer of `AudioCommand`.  Capacity should be
large enough to absorb a full frame of writes (~512 entries covers worst-case
demoscene register dump rates).

### Synthesis Engine

Each sound chip type implements a synthesis engine that:

1. Drains its `AudioCommandQueue` up to the target cycle.
2. Between commands, clocks the chip forward cycle-by-cycle.
3. Produces samples into a per-chip float buffer.
4. The mixer collects per-chip buffers and writes the mixed result to the output
   ring buffer.

The synthesis engine is the existing `advance_cycle()` / `tick()` logic,
extracted to run independently of the bus.

### Timing Model

At 44100 Hz with ~1,789,773 NES CPU cycles/sec:

| Parameter | Value |
|---|---|
| 1 audio sample | ~40.6 CPU cycles |
| 512-sample block | ~20,786 CPU cycles ≈ 180 scanlines |
| Queue drain latency | ≤ 1 audio block (11.6 ms at 512 samples) |
| Audible latency budget | 1–3 blocks (11.6–34.8 ms) |

The audio thread wakes on a semaphore or condition variable whenever the
emulation thread has pushed enough commands for a full audio block, or on a
timeout to prevent starvation.

### Output Ring Buffer

Reuse the existing `RingBuffer<float>` from `src/utils/ring_buffer.hpp`.  The
SDL audio callback (or platform equivalent) drains this buffer.

### Shared Mixer

One mixer thread owns N input ring buffers (one per active session's sound
chip).  It interleaves, applies per-session volume, and writes to a single
output ring buffer consumed by the platform audio callback.

For single-session use, the mixer is a passthrough.

---

## Migration Strategy

### Phase 1 — AudioCommandQueue utility (this PR)

Build and test the timestamped command queue as a standalone utility in
`src/utils/`.  No integration yet.

### Phase 2 — SID extraction

The C64 SID already has a lock-free ring buffer for sample output.  The seam is
clean:

1. Replace direct `tick()` calls with command enqueue.
2. Move `mos6581_t::advance_cycle()` into a synthesis thread entry point.
3. Add sync-read path for $D41B/$D41C.
4. Thread SID's existing ring buffer through the mixer.

### Phase 3 — NES APU extraction

1. Split `nes6502_apu::APU` into a timing façade (emu thread) and synthesis
   engine (audio thread).
2. Move `APU::tick()` synthesis into the audio thread.
3. Keep DMC stall + IRQ scheduling on the emu thread.
4. Replace `std::vector<float> audio_buffer_` with the shared ring buffer path.

### Phase 4 — Shared mixer

Wire up the per-session output buffers into a single mixer that feeds the
platform audio callback.

---

## Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Queue overflow under heavy register writes | 4096-entry queue; monitor high-water mark |
| Sync readback ($D41B/$D41C) adds latency | Only blocks on actual read; bypass flag for known-safe titles |
| DMC IRQ timing drift | Pre-compute deterministically; no audio-thread feedback needed |
| Thread startup/shutdown ordering | Audio thread is long-lived; outlives any single session |
| Platform audio callback starvation | Output ring buffer sized for 3× worst-case latency |

---

## Files Affected (Eventual)

| File | Change |
|---|---|
| `src/utils/audio_cmd_queue.hpp` | **New** — timestamped command queue |
| `src/utils/ring_buffer.hpp` | No change (already suitable) |
| `src/chip/sound/mos6581.hpp` | Extract synthesis loop; add command consumer |
| `src/chip/sound/mos6581.cpp` | Split advance_cycle into façade + engine |
| `src/chip/sound/nes_apu.hpp` | Extract synthesis loop; add command consumer |
| `src/chip/cpu/fam65xx/fam65xx_mixins.hpp` | Façade: enqueue instead of direct tick |
| `src/systems/nes/nes_system.cpp` | Remove direct audio_buffer_; wire queue |
| `src/systems/commodore/c64/c64_system.cpp` | Wire SID through queue |
| `src/core/emulated_system.h` | Optional: shared mixer hookpoint |
