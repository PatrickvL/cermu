# Audio Integration Survey — Seam Map

Survey of current audio integration in cermu's NES APU and C64 SID, identifying
the exact code locations where the audio thread separation (described in
`AUDIO_THREAD_SEPARATION.md`) would be applied.

---

## Current Architecture Summary

### NES APU

| Aspect | Current State |
|---|---|
| **Chip class** | `nes6502_apu::APU` in `src/chip/sound/nes_apu.hpp` |
| **Base class** | None (standalone struct inside `nes6502_apu` namespace) |
| **Tick entry** | `APU::tick(bus_state)` — clocked every CPU cycle via `apu_mixin_t::clock_apu()` |
| **Mixin** | `apu_mixin_t<Traits>` in `src/chip/cpu/fam65xx/fam65xx_mixins.hpp:91` |
| **Sample output** | `APU::sample()` returns float; `NintendoSystem` calls it every N cycles |
| **Buffer** | `std::vector<float> audio_buffer_` (unbounded, erase-from-front drain) |
| **Threading** | Single-threaded — all synthesis on the emu thread |
| **Resampling** | Decimation: sample every 37 (NTSC) / 33 (PAL) CPU cycles |
| **Filter** | 1st-order DC blocker (~37 Hz high-pass) |

### C64 SID

| Aspect | Current State |
|---|---|
| **Chip class** | `mos6581_t` in `src/chip/sound/mos6581.hpp` |
| **Base class** | `SoundChipBase : ChipBase` (via `src/chip/sound/sound_chip_base.hpp`) |
| **Tick entry** | `mos6581_t::tick(bus_state)` → `advance_cycle(bus_state)` |
| **Register I/O** | Static handlers `registers_read()` / `registers_write()` registered with C64 bus |
| **Sample output** | CIC-3 decimated samples written to internal SPSC ring buffer |
| **Buffer** | Custom `ring_buffer_t` (8192 float, atomic SPSC) inside `mos6581_t` |
| **Threading** | Producer on emu thread, consumer on host audio callback thread |
| **Resampling** | 3rd-order CIC decimation (~985 kHz → 44.1 kHz) |
| **Filter** | ZDF state-variable filter (clocked at CPU rate) + DC blocker |

---

## Seam Map — Where to Separate

### NES APU: 7 Integration Points

#### SEAM 1: APU tick in main loop
- **File**: `src/systems/nes/nes_system.cpp:1201`
- **Code**: `pins_ = cpu_->tick<RICOH_2A03::Phase::PHI1>(pins_);`
- **Inside**: `fam65xx_mixins.hpp:173` → `clock_apu(bus_state)` → `APU::tick(bus_state)`
- **Change**: Replace `APU::tick()` with enqueue to `AudioCommandQueue`. The mixin's `clock_apu()` becomes a no-op or enqueues a "cycle advance" marker.
- **Difficulty**: Medium — the APU tick currently returns modified `bus_state` but the only bus modification is IRQ pin assertion, which moves to the façade.

#### SEAM 2: Register writes via mixin
- **File**: `src/chip/cpu/fam65xx/fam65xx_mixins.hpp:131`
- **Code**: `apu_state.apu_instance->write(addr, value, bus_state)`
- **Change**: Instead of direct `write()`, call `queue.push_write(current_cycle, reg, value)`.
- **Difficulty**: Easy — write is fire-and-forget.

#### SEAM 3: Register read ($4015)
- **File**: `src/chip/cpu/fam65xx/fam65xx_mixins.hpp:143`
- **Code**: `bus_state = apu_state.apu_instance->read(addr, bus_state)`
- **Change**: $4015 returns channel length counter status + IRQ flags. Length counter status can be tracked by the façade (it only depends on register writes and frame counter ticks, both deterministic). IRQ flags are already tracked for the IRQ wire. This read can stay on the emu thread with a shadow state.
- **Difficulty**: Medium — need to maintain shadow length-counter-active bits.

#### SEAM 4: Audio sample generation
- **File**: `src/systems/nes/nes_system.cpp:1206–1210`
- **Code**: `float sample = cpu_->generate_audio_sample(); audio_buffer_.push_back(sample);`
- **Change**: Eliminate entirely. The audio thread produces samples from enqueued commands. Replace `audio_buffer_` with a shared `RingBuffer<float>` (already available in `src/utils/ring_buffer.hpp`).
- **Difficulty**: Easy — this is pure removal.

#### SEAM 5: DMC DMA fetch
- **File**: `src/systems/nes/nes_system.cpp:1218–1221`
- **Code**: `uint8_t sample = bus_.cpu_read(dmc_addr); cpu_->apu_load_dma_sample(sample);`
- **Change**: The emu thread still performs the bus read and models the cycle stall. Instead of calling `dmc_load_sample(data)` directly, it enqueues `push_dmc_sample(cycle, data)`. The `apu_needs_dma()` check stays on the emu thread (deterministic from DMC timer state, which the façade tracks).
- **Difficulty**: Medium — façade needs to track DMC timer countdown and sample address.

#### SEAM 6: IRQ wire update
- **File**: `src/systems/nes/nes_system.cpp:1185–1197`
- **Code**: `if (cpu_->apu_irq()) irq_asserted = true;`
- **Change**: Frame counter IRQ is deterministic from $4017 mode register. DMC IRQ is deterministic from sample length and loop flag. The façade pre-computes IRQ assertion cycles on register writes and updates a local flag. No audio thread involvement.
- **Difficulty**: Easy — deterministic schedule.

#### SEAM 7: Audio drain callback
- **File**: `src/systems/nes/nes_system.cpp:860–871`
- **Code**: `get_audio_samples()` drains `audio_buffer_`
- **Change**: Drain from `RingBuffer<float>` instead of `std::vector`. The audio thread is the producer; the platform callback is the consumer.
- **Difficulty**: Easy — drop-in replacement.

### C64 SID: 5 Integration Points

#### SEAM A: SID tick in system_tick
- **File**: `src/systems/commodore/c64/c64_system.cpp:926`
- **Code**: `s = sid->tick(s);`
- **Change**: Replace with a no-op or cycle counter increment. All synthesis moves to the audio thread.
- **Difficulty**: Easy — SID tick already doesn't modify bus state meaningfully (it only reads address/data that were already serviced by the register handlers).

#### SEAM B: Register write handler
- **File**: `src/chip/sound/mos6581.cpp:1097` (static `registers_write`)
- **Code**: Dispatches to voice/filter/volume handlers, stores in `regs_[]`
- **Change**: Instead of applying immediately, enqueue `push_write(cycle, reg, value)`. The bus handler still acknowledges the write (returns updated bus_state) but the SID state update happens on the audio thread.
- **Difficulty**: Easy — the write handler already extracts register index and value.

#### SEAM C: Oscillator/envelope readback
- **File**: `src/chip/sound/mos6581.cpp:1190` (static `registers_read`)
- **Code**: `$D41B` → `voice3.oscillator_waveform >> 4`, `$D41C` → `voice3.envelope_output`
- **Change**: Sync query path. On read to $D41B/$D41C, emu thread posts `push_sync_read(cycle, reg)` and spins until the audio thread catches up and writes the result.
- **Difficulty**: Hard — this is the one blocking path. Needs a shared atomic for the response and a "caught up" signal.
- **Optimization**: For non-demo software, the emu thread can return stale data (last known value) without blocking, controlled by a config flag.

#### SEAM D: Sample output ring buffer
- **File**: `src/chip/sound/mos6581.cpp` (internal `ring_buffer_t`)
- **Current**: SID already writes samples to an SPSC ring buffer from `advance_cycle()`.
- **Change**: The audio thread's synthesis engine writes to the same ring buffer (or to the shared `RingBuffer<float>` from `src/utils/`). The existing consumer path in `get_audio_samples()` is unchanged.
- **Difficulty**: Easy — the buffer boundary already exists; only the producer side moves threads.

#### SEAM E: Sample rate / region configuration
- **File**: `src/chip/sound/mos6581.hpp` / `.cpp` — `set_sample_rate()`, constructor
- **Change**: These are called during init/config, not on the hot path. Can be applied to the synthesis engine directly (protected by the queue being empty during init).
- **Difficulty**: Trivial.

---

## Difficulty Summary

| Seam | System | Difficulty | Notes |
|---|---|---|---|
| 1 | NES | Medium | APU tick → enqueue; façade tracks IRQ/DMC timing |
| 2 | NES | Easy | Write → enqueue |
| 3 | NES | Medium | $4015 read needs shadow length counter bits |
| 4 | NES | Easy | Remove; audio thread produces samples |
| 5 | NES | Medium | DMC fetch: bus read stays, enqueue sample byte |
| 6 | NES | Easy | Deterministic IRQ schedule |
| 7 | NES | Easy | Swap vector for ring buffer |
| A | SID | Easy | Tick → no-op |
| B | SID | Easy | Write → enqueue |
| C | SID | Hard | Sync readback for $D41B/$D41C |
| D | SID | Easy | Ring buffer producer moves to audio thread |
| E | SID | Trivial | Init-time config |

---

## Façade State Requirements

### NES APU Façade (stays on emu thread)

The façade needs enough state to answer emu-thread queries without the audio thread:

| State | Purpose | Maintained by |
|---|---|---|
| Frame counter mode (4/5 step) | IRQ scheduling | Write to $4017 |
| Frame counter IRQ inhibit | IRQ scheduling | Write to $4017 |
| Frame counter cycle position | IRQ timing | Cycle counting (simple counter) |
| DMC timer period | DMA timing | Write to $4010 |
| DMC timer countdown | When to request sample | Cycle counting |
| DMC sample address & length | DMA address generation | Write to $4012/$4013 |
| DMC bytes remaining | DMA completion / IRQ | Decrement on each fetch |
| DMC loop flag | IRQ / restart | Write to $4010 |
| DMC IRQ enable | IRQ assertion | Write to $4010 |
| Channel length counter > 0 bits | $4015 read response | Track enable/disable writes |
| Frame IRQ flag | IRQ assertion + $4015 read | Frame counter schedule |
| DMC IRQ flag | IRQ assertion + $4015 read | DMC completion |

This is about 30–40 bytes of state — trivial.

### SID Façade (stays on emu thread)

| State | Purpose | Maintained by |
|---|---|---|
| Last OSC3 value | Stale readback fallback | Updated by audio thread periodically |
| Last ENV3 value | Stale readback fallback | Updated by audio thread periodically |
| Paddle X/Y values | $D419/$D41A reads | Updated by port/peripheral system |

Even less state needed. The sync readback path handles the accurate case.

---

## Recommended Implementation Order

1. **NES APU first** — simpler (no readback problem), more clearly one-directional
2. **SID second** — leverages existing ring buffer, adds sync readback
3. **Shared mixer** — once both systems produce to ring buffers

This order lets each phase be independently testable against reference output.
