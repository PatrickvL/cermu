# Audio Architecture — Comprehensive Design & Future Work

This document consolidates all audio‐related design decisions, current state,
thread‐separation strategy, synchronisation solutions, and the implementation
road map for cermu's audio subsystem.  It supersedes the earlier
`AUDIO_THREAD_SEPARATION.md` and `AUDIO_INTEGRATION_SURVEY.md` as the single
source of truth; those documents remain for historical reference.

---

## Table of Contents

1. [Design goals](#1-design-goals)
2. [Existing infrastructure](#2-existing-infrastructure)
3. [Inventory of all audio sources](#3-inventory-of-all-audio-sources)
4. [Classification: offload strategy per chip](#4-classification-offload-strategy-per-chip)
5. [Synchronisation solutions](#5-synchronisation-solutions)
6. [NES APU façade design](#6-nes-apu-façade-design)
7. [C64 SID façade design](#7-c64-sid-façade-design)
8. [Spectrum beeper + AY separation](#8-spectrum-beeper--ay-separation)
9. [Embedded‐audio chips (TED, VIC, TIA)](#9-embedded-audio-chips-ted-vic-tia)
10. [Shared mixer](#10-shared-mixer)
11. [Output pipeline](#11-output-pipeline)
12. [Implementation road map](#12-implementation-road-map)
13. [Open questions](#13-open-questions)

---

## 1. Design Goals

| # | Goal |
|---|------|
| G1 | Offload all non‐trivial audio synthesis to a dedicated audio thread, freeing ~15–25 % of per‐cycle emulation work. |
| G2 | Preserve cycle‐accurate bus interactions (readback, IRQ, DMA) on the emulation thread — the emu thread must never block on the audio thread during normal operation. |
| G3 | Provide a generic, composable adapter pattern so new sound chips plug into the audio thread with minimal per‐system glue. |
| G4 | Keep latency within a 1–3 audio‐block budget (~11–35 ms at 512 samples / 44.1 kHz). |
| G5 | Scale naturally with the multi‐session architecture — one audio thread, N sound engines, one shared mixer. |

---

## 2. Existing Infrastructure

### 2.1 AudioThread (`src/core/audio_thread.hpp`)

Dedicated `std::thread` that processes registered `AudioSynthEngine*` instances.
Wakes on `signal_progress(cycle)` (atomic store + CV notify) or a 5 ms timeout
to prevent starvation.  The thread calls `engine->process_until(target_cycle)`
for every registered engine each iteration.

### 2.2 AudioSynthEngine (`src/core/audio_synth_engine.hpp`)

Abstract interface:

```cpp
virtual void process_until(uint64_t target_cycle) = 0;
virtual AudioCommandQueue& cmd_queue() = 0;
virtual void reset() = 0;
```

### 2.3 AudioCommandQueue (`src/utils/audio_cmd_queue.hpp`)

Lock‐free SPSC ring buffer of 16‐byte `AudioCommand` structs (cycle + type +
reg + value).  Command types:

| Type | Usage |
|------|-------|
| `REGISTER_WRITE` | Normal register write (reg, value) |
| `DMC_SAMPLE_LOADED` | NES DMC sample byte fetched by emu thread |
| `RESET` | Hard reset of synthesis state |
| `SYNC_READ_REQUEST` | SID OSC3/ENV3 readback (emu thread blocks) |

Convenience producers: `push_write()`, `push_dmc_sample()`, `push_reset()`,
`push_sync_read()`.

Consumer: `drain_until(target_cycle, fn)` — invokes callback for each command
whose cycle ≤ target, in FIFO order.

### 2.4 WriteOnlySynthAdapter (`src/utils/write_only_synth_adapter.hpp`)

Generic `AudioSynthEngine` implementation for write‐only sound chips.
Template parameters:

- `Chip` — sound chip type (must expose `tick()`, `reset()`, and either
  `write(uint8_t)` or `write_register(uint8_t reg, uint8_t data)`).
- `Addressed` — `false` for single‐byte chips (SN76489), `true` for
  register‐addressed chips (AY‐3‐8910, Namco WSG).

Converts CPU‐cycle timestamps into chip ticks via `cpu_cycles_per_tick`.

### 2.5 AudioRingBuffer (`src/utils/ring_buffer.hpp`)

SPSC `RingBuffer<float>` used for all sample output — both by AudioThread
engines (producer: audio thread, consumer: SDL callback) and by emu‐thread
chips that generate samples directly.

---

## 3. Inventory of All Audio Sources

### 3.1 Already on AudioThread (write‐only, completed)

| System | Chip | Adapter | `cpu_cycles_per_tick` | Commit |
|--------|------|---------|-----------------------|--------|
| BBC Micro | SN76489 | `WriteOnlySynthAdapter<sn76489_t>` | 4 | `9d2cf60e` |
| Namco Arcade (Pac‐Man, Galaga, …) | Namco WSG | `WriteOnlySynthAdapter<namco_wsg_t, true>` | 32 | `f6e1ddd8` |
| Bomb Jack (3× AY) | AY‐3‐8910 | `WriteOnlySynthAdapter<ay_3_8910_t, true>` | 2 | `a57b69b4` |
| Amstrad CPC (1× AY) | AY‐3‐8910 | `WriteOnlySynthAdapter<ay_3_8910_t, true>` | 4 | `a57b69b4` |

These chips are fire‐and‐forget: the CPU writes registers but never reads back.
The `WriteOnlySynthAdapter` pattern handles them generically.

### 3.2 On emulation thread — offload candidates

| System | Chip | Synthesis cost | Readback | IRQ/DMA | Current buffer |
|--------|------|---------------|----------|---------|---------------|
| **C64** | MOS 6581 SID | **High** — 3 voices, ZDF SVF filter, CIC‐3 decimation at CPU rate (~985 kHz) | OSC3 ($D41B), ENV3 ($D41C) | None | Internal `ring_buffer_t` via `generate_samples()` |
| **NES** | RP2A03 APU | **Medium** — 5 channels, envelope/sweep/length, lookup‐table mixer | $4015 (status) | Frame counter IRQ, DMC IRQ, DMC DMA (1–4 cycle stall) | `AudioRingBuffer` (8192) |

### 3.3 On emulation thread — embedded in video chip

| System | Chip | Synthesis cost | Coupling | Current buffer |
|--------|------|---------------|----------|---------------|
| **C16/Plus4** | TED 7360 | Low — 2 square‐wave channels | Audio generation inside video tick (`tick_phi1` / `tick_phi2`) | `AudioRingBuffer` via `audio_read()` |
| **VIC‐20** | VIC 6560/6561 | Low — 4 channels (3 square + 1 noise) | Audio inside video tick | `RingBuffer<uint8_t>` via `audio_read()` → float conversion in system |
| **Atari 2600** | TIA | Low — 2 channels (frequency divider + LFSR) | Audio inside video+collision tick | `AudioRingBuffer` via `audio_read()` |

### 3.4 Mixed sources (emu‐thread, multi‐chip)

| System | Sources | Current mixing | Notes |
|--------|---------|---------------|-------|
| **ZX Spectrum 128K** | ULA beeper + AY‐3‐8912 | Per‐sample: `beeper*0.5 + ay.get_sample()*0.5` at sample rate | AY ticked at CPU/2; beeper is 1‐bit from ULA `ear_output` |

### 3.5 Trivial / no offload value

| System | Source | Description |
|--------|--------|-------------|
| **PET** | PIA/VIA CB2 | 1‐bit speaker; toggled once per video frame at most. Already uses a 4096‐sample ring buffer. |
| **CHIP‐8** | Beeper / XO‐CHIP pattern | Square wave when sound timer > 0; XO‐CHIP: 128‐bit waveform pattern. Negligible cost. |
| **Apple 1** | — | No audio |
| **DDR systems** (Z9001, LC80, KC85, Z1013) | — | `get_audio_samples()` returns 0 |
| **Acorn Atom** | — | `get_audio_samples()` returns 0 |

---

## 4. Classification: Offload Strategy per Chip

### Tier 1 — Full offload via façade (high impact, medium complexity)

**MOS 6581 SID** and **NES APU**: Both have significant per‐cycle synthesis
cost and can be separated via a command‐queue façade.  The emu thread tracks
bus‐facing state (IRQ, DMA, readback) locally; the audio thread runs the full
synthesis pipeline.

### Tier 2 — Already done (write‐only adapter)

SN76489, Namco WSG, AY‐3‐8910: All wired via `WriteOnlySynthAdapter`.  No
further work needed.

### Tier 3 — Possible extraction from video chip (low priority)

TED, VIC, TIA: Audio is cheap (a few ALU ops per cycle) and deeply coupled to
the video tick.  Extracting audio into a separate engine requires either:

- (a) Duplicating the frequency divider / counter state into a standalone
  audio‐only chip model, or
- (b) Splitting the video chip's tick into video + audio sub‐ticks.

Given the low synthesis cost, the benefit is marginal.  **Defer until profiling
shows these are bottlenecks** — which is unlikely.  The existing
`AudioRingBuffer` output already decouples the consumer (SDL callback) from
production, which is the important part.

### Tier 4 — Mixed sources requiring restructure

**ZX Spectrum 128K**: The beeper and AY are mixed per‐sample on the emu thread.
Two approaches:

1. **Move AY to AudioThread, keep beeper on emu thread, mix in shared mixer.**
   The beeper writes `{cycle, value}` pairs into a lightweight event log; the
   mixer interpolates beeper state between events and adds the AY ring buffer
   output.  This is the cleanest separation but requires the shared mixer to
   understand two fundamentally different input types (ring buffer vs event
   stream).

2. **Keep both on emu thread.**  The AY tick is cheap (~8 ops per AY tick).
   Since the Spectrum already uses `AudioRingBuffer` for output, the consumer
   side is already decoupled.  The synthesis overhead is low enough that
   offloading provides minimal benefit.

**Recommendation**: Option 2 (keep both on emu thread) for now.  Revisit only
if AY synthesis becomes more elaborate (e.g. adding envelope shapes with higher
fidelity) or if the Spectrum host thread has performance issues.

### Tier 5 — Not worth offloading

PET CB2 speaker, CHIP‐8 beeper: Trivially cheap.  Leave on emu thread.

---

## 5. Synchronisation Solutions

The key challenge of façade‐based offloading is handling the
**audio → CPU feedback paths** that exist in SID and NES APU.  The solutions
below ensure the emu thread never blocks on the audio thread during normal
operation, except for the explicitly rare readback case.

### 5.1 Deterministic pre‐computation (IRQ, DMA timing)

Both the NES APU's frame‐counter IRQ and DMC IRQ are **fully deterministic**
from register state.  Given the current register values, the emu thread can
compute exactly which cycle the next IRQ will fire, without consulting the
audio thread.

**Pattern**: On every register write that affects IRQ timing ($4017 frame mode,
$4010–$4013 DMC config), the façade re‐computes the next IRQ cycle and stores
it locally.  Each emu‐thread tick compares the current cycle against this
pre‐computed target.

This is zero overhead beyond the initial computation on write.

### 5.2 Shadow state (NES $4015 readback)

The NES APU status register ($4015) reports:

- Bits 0–3: whether each channel's length counter > 0
- Bit 4: DMC bytes remaining > 0
- Bit 6: frame IRQ flag (cleared on read)
- Bit 7: DMC IRQ flag

All of these are deterministic from register writes and cycle counting:

- **Length counters**: Loaded on channel enable ($4015 write) with values from
  the length counter lookup table; decremented by the frame counter (whose
  ticks are deterministic from $4017).
- **DMC bytes remaining**: Loaded from $4013, decremented on each sample fetch
  (deterministic timing from $4010 period).
- **IRQ flags**: Deterministic from frame counter mode and DMC completion.

The façade maintains a lightweight shadow of these ~30–40 bytes of state,
updated on each register write and frame‐counter tick.  $4015 reads are
serviced entirely from this shadow — no audio thread involvement.

### 5.3 Synchronous readback (SID OSC3/ENV3)

$D41B (oscillator 3 output) and $D41C (envelope 3 output) return the current
internal state of voice 3.  Unlike the NES status register, these are driven
by the full synthesis pipeline (24‐bit phase accumulator, waveform generator,
ADSR envelope) and cannot be cheaply shadowed without duplicating significant
synthesis logic.

**Protocol**:

1. Emu thread detects a read to $D41B or $D41C.
2. Emu thread enqueues a `SYNC_READ_REQUEST` command with the current cycle.
3. Emu thread stores the request in a shared `SyncReadRequest` struct:
   ```cpp
   struct SyncReadRequest {
       std::atomic<uint64_t> request_cycle{0};    // emu → audio
       std::atomic<uint16_t> result{0};            // audio → emu (OSC3|ENV3 packed)
       std::atomic<bool>     ready{false};          // audio → emu
   };
   ```
4. Audio thread encounters the `SYNC_READ_REQUEST` during `drain_until()`,
   catches up synthesis to that cycle, writes result + sets `ready`.
5. Emu thread spin‐waits on `ready` (sub‐microsecond on modern x86 — the
   audio thread is typically at most a few hundred cycles behind for these
   intra‐frame reads).

**Fallback — stale readback**: For software that doesn't depend on precise
OSC3/ENV3 values (most games), the façade can return the most recently
published value without blocking.  The audio thread periodically (e.g. every
sample block) writes its latest OSC3/ENV3 to a shared atomic.  A runtime flag
controls which path is taken: `SyncReadMode::Blocking` (default for demos) or
`SyncReadMode::Stale` (fast path for games).

**Performance impact**: Real‐world SID music engines (GoatTracker, SIDWizard,
etc.) never read $D41B/$D41C.  Demoscene productions that do typically read
once per raster line at most.  Even with blocking, the sync latency is
negligible compared to the per‐cycle cost saved by offloading synthesis.

### 5.4 DMC sample fetch (NES)

The DMC channel periodically fetches a byte from CPU address space, causing a
1–4 cycle CPU stall.  This is a **CPU → memory** interaction, not an
**APU → CPU** interaction — the APU merely _requests_ the fetch; the actual
bus read is performed by the emu thread.

**Protocol**:

1. The façade tracks the DMC timer countdown (period from $4010) and sample
   address/length ($4012/$4013).
2. When the façade determines a fetch is due (timer expires, bytes remaining
   > 0), the emu thread:
   - Performs the bus read: `uint8_t byte = bus.cpu_read(dmc_address)`
   - Models the CPU stall (1–4 cycles depending on alignment)
   - Enqueues `push_dmc_sample(cycle, byte)` into the command queue
   - Advances the façade's address pointer and decrements bytes remaining
3. The audio thread, upon consuming the `DMC_SAMPLE_LOADED` command, feeds the
   byte to its DMC output unit.

The emu thread drives all timing; the audio thread is purely a consumer of
pre‐fetched sample bytes.

---

## 6. NES APU Façade Design

### 6.1 Components

```
┌── Emu Thread ─────────────────────────────────────────────┐
│                                                           │
│  apu_facade_t                                             │
│  ├─ frame_counter_state (mode, divider, step counter)     │
│  ├─ length_counter_active[5]  (for $4015 read)            │
│  ├─ dmc_state (timer, address, bytes_remaining, loop, …)  │
│  ├─ irq_state (frame_irq, dmc_irq, irq_inhibit)          │
│  ├─ audio_sample_counter (decimation counter)             │
│  └─ cmd_queue → AudioThread                               │
│                                                           │
│  On register write ($4000–$4013, $4015, $4017):           │
│    1. Update façade shadow state                          │
│    2. cmd_queue.push_write(cycle, reg, value)             │
│                                                           │
│  On register read ($4015):                                │
│    → Return shadow (length_active | dmc_active | irqs)    │
│    → Clear frame_irq flag                                 │
│                                                           │
│  On each CPU cycle:                                       │
│    1. Tick frame counter → update IRQ flags                │
│    2. Check DMC timer → if expired, bus_read + enqueue    │
│    3. Decrement audio_sample_counter (for decimation)     │
│                                                           │
└───────────────────────────────────────────────────────────┘

┌── Audio Thread ───────────────────────────────────────────┐
│                                                           │
│  NesApuSynthEngine : AudioSynthEngine                     │
│  ├─ Full APU state (pulse, triangle, noise, DMC output)   │
│  ├─ Mixer (nonlinear lookup table)                        │
│  ├─ High-pass / DC blocker                                │
│  └─ AudioRingBuffer (sample output)                       │
│                                                           │
│  process_until(target_cycle):                             │
│    drain_until(target, [](cmd) {                          │
│      advance_to(cmd.cycle);                               │
│      if REGISTER_WRITE → write(cmd.reg, cmd.value)        │
│      if DMC_SAMPLE_LOADED → dmc.load_sample(cmd.value)    │
│    });                                                    │
│    advance_to(target_cycle);                              │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

### 6.2 Façade state (~40–50 bytes)

| Field | Type | Purpose |
|-------|------|---------|
| `frame_mode` | `uint8_t` | 4‐step or 5‐step (from $4017 bit 7) |
| `frame_irq_inhibit` | `bool` | From $4017 bit 6 |
| `frame_divider` | `uint16_t` | CPU cycles until next frame step |
| `frame_step` | `uint8_t` | Current step in the frame sequence |
| `frame_irq_flag` | `bool` | Asserted by frame counter step 4 |
| `length_active[5]` | `bool[5]` | Per‐channel: length counter > 0 |
| `dmc_period` | `uint16_t` | Timer period (from $4010 lookup) |
| `dmc_timer` | `uint16_t` | Current countdown |
| `dmc_address` | `uint16_t` | Current sample address |
| `dmc_bytes_remaining` | `uint16_t` | Bytes left |
| `dmc_loop` | `bool` | Loop flag (from $4010 bit 6) |
| `dmc_irq_enable` | `bool` | IRQ enable (from $4010 bit 7) |
| `dmc_irq_flag` | `bool` | Asserted on sample end if enabled |

### 6.3 Integration changes

| Location | Change |
|----------|--------|
| `fam65xx_mixins.hpp` `clock_apu()` | Replace `APU::tick(bus_state)` with façade tick + `signal_progress(cycle)` |
| `fam65xx_mixins.hpp` `write_apu_register()` | Shadow write to façade + `cmd_queue.push_write()` |
| `fam65xx_mixins.hpp` `read_apu_register()` | Return from façade shadow (no queue involvement) |
| `nes_system.cpp` DMC handling | Façade drives `apu_needs_dma()` / `apu_dma_address()`; bus read + enqueue on fetch |
| `nes_system.cpp` IRQ handling | Read IRQ from façade, not from APU instance |
| `nes_system.cpp` audio output | Remove `generate_audio_sample()` call; audio thread writes to `AudioRingBuffer` |
| `nes_system.cpp` `get_audio_samples()` | Already reads from `AudioRingBuffer` — no change needed |

### 6.4 Length counter shadow

The façade must track whether each channel's length counter is non‐zero for
$4015 readback.  This requires tracking:

- On write to $4003/$4007/$400B/$400F (triangle/pulse/noise length load):
  if channel enabled, set `length_active[ch] = true`.
- On write to $4015 (channel enable): if channel disabled, set
  `length_active[ch] = false`.
- On frame counter half‐frame step: if length counter was loaded and would
  decrement to zero, set `length_active[ch] = false`.

The half‐frame step only fires at deterministic cycles (every 7457 or 7458 CPU
cycles in 4‐step mode).  The façade counts cycles and applies the decrement
logic using the loaded length value from the lookup table.  This is a few
comparisons per half‐frame — negligible cost.

---

## 7. C64 SID Façade Design

### 7.1 Components

```
┌── Emu Thread ─────────────────────────────────────────────┐
│                                                           │
│  sid_facade_t                                             │
│  ├─ regs_[32] (register shadow for write capture)         │
│  ├─ sync_request (SyncReadRequest struct)                 │
│  ├─ stale_osc3 / stale_env3 (atomics, updated by audio)  │
│  ├─ sync_mode (Blocking / Stale)                          │
│  └─ cmd_queue → AudioThread                               │
│                                                           │
│  On register write ($D400–$D418):                         │
│    1. Store in regs_[] (for write_capture_fn if enabled)   │
│    2. cmd_queue.push_write(cycle, reg, value)             │
│                                                           │
│  On register read ($D41B/$D41C):                          │
│    if sync_mode == Blocking:                              │
│      → post SYNC_READ_REQUEST, spin until ready           │
│    else:                                                  │
│      → return stale_osc3 / stale_env3                     │
│                                                           │
│  On register read ($D419/$D41A — paddle ADC):             │
│    → Read from port/peripheral system (stays on emu)      │
│                                                           │
│  On each CPU cycle:                                       │
│    → Increment cycle counter; signal_progress()           │
│    → (No per-cycle logic needed — SID has no IRQ/DMA)     │
│                                                           │
└───────────────────────────────────────────────────────────┘

┌── Audio Thread ───────────────────────────────────────────┐
│                                                           │
│  SidSynthEngine : AudioSynthEngine                        │
│  ├─ Full mos6581_t state (3 voices, filter, envelope)     │
│  ├─ CIC-3 decimation pipeline                             │
│  ├─ DC blocker                                            │
│  ├─ AudioRingBuffer (sample output)                       │
│  └─ Shared atomics: stale_osc3, stale_env3                │
│                                                           │
│  process_until(target_cycle):                             │
│    drain_until(target, [](cmd) {                          │
│      advance_to(cmd.cycle);                               │
│      if REGISTER_WRITE → write_register(cmd.reg, cmd.val) │
│      if SYNC_READ_REQUEST → catch up, write result        │
│    });                                                    │
│    advance_to(target_cycle);                              │
│    update stale_osc3 / stale_env3 atomics                 │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

### 7.2 Façade state (minimal)

| Field | Type | Purpose |
|-------|------|---------|
| `regs_[32]` | `uint8_t[32]` | Shadow for write‐capture callback |
| `sync_request` | `SyncReadRequest` | Shared struct for blocking readback |
| `stale_osc3` | `std::atomic<uint8_t>` | Latest OSC3 value from audio thread |
| `stale_env3` | `std::atomic<uint8_t>` | Latest ENV3 value from audio thread |
| `sync_mode` | `enum` | `Blocking` or `Stale` |

The SID façade is simpler than the APU façade because SID has no IRQ, no DMA,
and no complex status register.  The only feedback path is OSC3/ENV3 readback.

### 7.3 Integration changes

| Location | Change |
|----------|--------|
| `c64_system.cpp` `system_tick()` | Replace `sid->tick(s)` with cycle‐counter increment + periodic `signal_progress()` |
| `mos6581.cpp` `registers_write()` | Shadow in `regs_[]` + `cmd_queue.push_write()` |
| `mos6581.cpp` `registers_read()` | $D41B/$D41C → sync query or stale value; $D419/$D41A → from peripheral system; all others → from shadow `regs_[]` |
| `c64_system.cpp` `get_audio_samples()` | Replace `sid->generate_samples()` with ring buffer drain (same as NES pattern) |

### 7.4 The `write_capture_fn` concern

The SID has a callback (`write_capture_fn`) used by the test harness to log
every register write.  After the façade split, the capture can fire from the
emu thread's shadow write (which happens before enqueue), preserving the exact
cycle timestamp.  No change to user‐facing behavior.

---

## 8. Spectrum Beeper + AY Separation

The ZX Spectrum 128K mixes two audio sources on the emu thread:

```cpp
float sample = ula_.get_ear_output() ? 0.5f : 0.0f;
if constexpr (Traits::has_ay_sound) {
    sample += ay_.get_sample() * 0.5f;
}
audio_ring_buf_.write(&sample, 1);
```

The AY is ticked at CPU/2 (`frame_tstate_counter_ & 1`); the beeper is a
1‐bit ULA output (`ear_output`).

**Assessment**: The AY synthesis cost (~8–10 ops per AY tick) is low.  The
combined mixing per audio sample is ~4 floating‐point ops.  The total audio
overhead is well under 1 % of the Spectrum's per‐cycle budget.

**Decision**: **Keep both on emu thread.**  The `AudioRingBuffer` already
decouples the consumer (SDL callback) from the producer.  Offloading the AY
alone would require either a shared mixer that understands event‐based beeper
input, or duplicating the beeper on the audio thread — both add complexity for
negligible gain.

If the AY model is later upgraded (e.g. envelope shape interpolation, analog
filter modeling), this decision should be revisited.

---

## 9. Embedded‐Audio Chips (TED, VIC, TIA)

### 9.1 Current state

All three generate audio inside their video tick function:

| Chip | Audio channels | Complexity | Output buffer |
|------|---------------|------------|---------------|
| TED 7360 | 2 square waves | ~6 ops/cycle | `AudioRingBuffer` via `audio_read()` |
| VIC 6560/61 | 3 square + 1 noise | ~12 ops/cycle | `RingBuffer<uint8_t>` → float conversion |
| TIA | 2 channels (freq div + LFSR) | ~8 ops/cycle | `AudioRingBuffer` via `audio_read()` |

### 9.2 Extraction feasibility

Extracting audio from a video chip requires one of:

- **Duplicate the audio counters** into a standalone audio engine that receives
  register writes.  This means maintaining two copies of the frequency/volume
  registers and counter state.
- **Refactor the video chip** to separate audio and video sub‐ticks.  But
  TED and VIC interleave audio/video behavior in ways that make clean
  separation non‐trivial (e.g. TED's audio counters are part of the
  raster‐line timing).

### 9.3 Recommendation

**Do not extract.**  The synthesis cost is negligible relative to the video tick
work in the same chip.  The output is already buffered via `AudioRingBuffer`,
which is the important decoupling point (producer on emu thread, consumer on
SDL callback thread).  Extraction would add complexity and a second copy of
register state for no measurable benefit.

These chips should only be revisited if they are ported to a system where the
same video chip is ticked at very high frequency (e.g. a machine with an
unusually fast CPU).

---

## 10. Shared Mixer

### 10.1 Purpose

For multi‐session emulation (future), a shared mixer combines the audio output
of all active sessions into a single output stream consumed by the platform
audio callback.

For single‐session use, the mixer is a passthrough that simply drains the
single active ring buffer.

### 10.2 Design

```
┌── Audio Thread ───────────────────────────────────────┐
│                                                       │
│  SynthEngine A  →  RingBuffer A ──┐                   │
│  SynthEngine B  →  RingBuffer B ──┼─→ SharedMixer     │
│  SynthEngine C  →  RingBuffer C ──┘        │          │
│                                            ▼          │
│                                     Output RingBuffer │
│                                            │          │
└────────────────────────────────────────────┼──────────┘
                                             │
                              SDL audio callback drains
```

For systems with multiple chips (e.g. Bomb Jack's 3× AY), the current approach
of reading the minimum available from all ring buffers and mixing in
`get_audio_samples()` works well.  A shared mixer generalises this:

- Each `AudioSynthEngine` writes to its own `AudioRingBuffer`.
- The mixer reads the minimum available across all active inputs, scales by
  per‐channel volume, sums, and writes to the output ring buffer.
- Volume balancing is per‐engine (configurable via a `set_volume(float)` on
  each engine).

### 10.3 Single‐system simplification

For systems with exactly one audio chip, the mixer is unnecessary — the
engine's ring buffer IS the output buffer.  The system's `get_audio_samples()`
directly drains it, exactly as implemented today for BBC Micro, Amstrad CPC,
and Namco Arcade.

For systems with multiple chips on the audio thread (Bomb Jack), the
per‐system mixing in `get_audio_samples()` can remain as‐is, or be migrated
to the shared mixer interface when multi‐session support is built.

---

## 11. Output Pipeline

```
Synthesis → [per-chip ring buffer] → [optional mixer] → [output ring buffer] → SDL callback
```

### 11.1 Sample rate

All synthesis engines produce samples at the platform's output sample rate
(typically 44.1 kHz or 48 kHz).  Decimation (e.g. SID's CIC‐3, NES's simple
counter‐based decimation) happens inside the synthesis engine.

### 11.2 Latency budget

| Parameter | Value |
|-----------|-------|
| Audio block size | 512 samples (configurable) |
| At 44.1 kHz | 11.6 ms per block |
| Ring buffer capacity | 8192 samples (~186 ms) |
| Target latency | 1–3 blocks (11.6–34.8 ms) |
| Perceptible threshold | ~10 ms for rhythm games, ~50 ms for general play |

The 1–3 block latency is well within acceptable bounds for retro gaming.
Rhythm‐sensitive applications can reduce block size to 256 samples (~5.8 ms)
at the cost of higher CPU wake frequency.

### 11.3 Underrun / overrun handling

- **Underrun** (consumer faster than producer): Output silence.  This happens
  during emulator pause, load, or if the emu thread is too slow.  The ring
  buffer's `read()` returns fewer samples than requested.
- **Overrun** (producer faster than consumer): Drop oldest samples.  This is
  rare with proper ring buffer sizing but can occur during fast‐forward.  The
  ring buffer's `write()` overwrites stale data (or the producer can check
  `available()` and skip).

---

## 12. Implementation Road Map

### Phase 1 — NES APU façade (next)

Implement `apu_facade_t` alongside the existing `APU` class:

1. Create `src/chip/sound/nes_apu_facade.hpp` — the emu‐thread façade with
   frame counter, length counter shadow, DMC timer, IRQ state.
2. Create `NesApuSynthEngine` implementing `AudioSynthEngine` — wraps the
   existing `APU` class, processes commands, writes to `AudioRingBuffer`.
3. Modify `fam65xx_mixins.hpp` to use the façade for register I/O and cycle
   ticking, and the command queue for write forwarding.
4. Modify `nes_system.cpp` to wire the synthesis engine to the AudioThread
   and remove direct APU sample generation.
5. Validate against NES test ROMs (blargg's apu_test, apu_mixer).

### Phase 2 — C64 SID façade

1. Create `src/chip/sound/mos6581_facade.hpp` — emu‐thread façade with
   register shadow, sync readback protocol.
2. Create `SidSynthEngine` implementing `AudioSynthEngine` — wraps mos6581_t,
   handles `SYNC_READ_REQUEST`, writes to ring buffer.
3. Modify `mos6581.cpp` register handlers to use façade + command queue.
4. Modify `c64_system.cpp` to remove `sid->tick()` from `system_tick()` and
   wire the synthesis engine to the AudioThread.
5. Validate with HVSC tunes + known OSC3‐reading demos.

### Phase 3 — Shared mixer foundation

1. Implement `AudioMixer` class that combines N input ring buffers into one
   output ring buffer with per‐channel volume control.
2. Wire Bomb Jack's 3× AY through the mixer (replacing the current manual
   mixing in `get_audio_samples()`).
3. Test with multi‐chip systems to verify timing and volume balancing.

### Phase 4 — Multi‐session audio

1. Each session registers its engines with the shared AudioThread.
2. The mixer combines all active sessions.
3. Per‐session mute / solo / volume controls.

### Phase 5 — Future audio improvements

| Item | Description | Priority |
|------|-------------|----------|
| SID filter calibration (6581 vs 8580) | Different analog filter curves; currently only 6581 modeled | Medium |
| SID combined waveforms | Waveform AND logic for $x1+$x2 combinations | Medium |
| SID ring modulation accuracy | Verify against VICE reference | Medium |
| NES nonlinear mixing | Verify lookup table against measured hardware | Low |
| Audio recording / WAV export | Tap the output ring buffer and write to file | Low |
| Per‐channel mute/solo in debug UI | Toggle individual voices in SID/APU debug window | Low |
| VRC6/VRC7/N163/FDS expansion audio | NES mapper audio chips, same façade pattern | Future |
| SID stereo (dual‐SID) | Second SID at $D420/$D500/$DE00; two engines on audio thread | Future |

---

## 13. Open Questions

1. **Façade vs. full synthesis engine duplication for NES APU**: The façade
   needs the frame counter and length counter logic, which is currently
   embedded in `APU::tick()`.  Should we extract these into a shared utility
   class used by both the façade and the synthesis engine, or accept some
   code duplication for simplicity?

2. **SID sync readback latency bound**: What is the worst‐case latency for
   the audio thread to catch up when a `SYNC_READ_REQUEST` is posted?  If
   the audio thread is processing a large batch of register writes, the emu
   thread could spin for longer than expected.  Should we add a timeout and
   fall back to stale data?

3. **Signal_progress granularity**: Currently `signal_progress()` is called
   once per CPU cycle for systems with audio thread engines.  Should we batch
   this (e.g. once per scanline) to reduce atomic contention?  The CV notify
   is already coalesced by the mutex, but the atomic store has cache‐line
   bouncing overhead.

4. **DMC sample pre‐fetch buffer**: Should the façade pre‐fetch the next DMC
   sample byte before it's needed (if the address is in RAM, which doesn't
   change between fetches for most programs)?  This would eliminate the
   bus‐read latency at the cost of incorrect behavior if the sample data is
   modified between writes.  Probably not worth the risk.

5. **ring_buffer_t consolidation**: The SID uses a custom `ring_buffer_t`
   inside `mos6581.hpp`; the rest of the codebase uses `RingBuffer<T>` from
   `ring_buffer.hpp`.  Should we consolidate to one implementation during
   the SID façade work?

---

*Last updated: 2025-07-16 — covers state after AY‐3‐8910 AudioThread wiring (commit a57b69b4).*
