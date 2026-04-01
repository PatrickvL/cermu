# CS-Tick Architecture Migration Plan

## Executive Summary

Replace per-system hardcoded address decoding with a unified **chip-select
(CS) tick model** where the `MemoryBus` acts as the address decoder and each
chip's `tick()` checks its own CS line in `bus_state_t`.

---

## End Goal

```
CPU sets address on bus
        │
        ▼
  MemoryBus::resolve()         ← page-table + sub-table lookup
  embeds chip ID in CS field   ← one integer compare per chip, per tick
        │
        ▼
  Each chip tick():
    1. Advance internal counters (always, regardless of CS)
    2. if get_cs(bus) != my_chip_id  →  return
    3. MMIO chips: handle register access in-place
       Buffer chips: call service_read/service_write
        │
        ▼
  No per-system I/O dispatch code remains.
```

**Success criteria:**
- Zero per-system address-decode code for MMIO routing.
- `MemoryBus::resolve()` is the **sole** CS decoder for every system.
- Every MMIO chip self-selects from `bus_state_t`.
- Buffer-backed chips (RAM, ROM) use `service_read/service_write` after CS
  check, or remain on the flat-mem fast path as an optimisation.
- No runtime overhead increase for buffer accesses.

---

## Current State

### What already exists

| Component | File | Status |
|-----------|------|--------|
| `resolve()` — address → CS field | `bus.hpp` L84–122 | **Implemented**, gated by `requires(kCsLines)` |
| `get_cs()` / `set_cs()` | `bus.hpp` L716–723 | **Implemented** |
| `service_read()` / `service_write()` | `bus.hpp` L139–157 | **Implemented** |
| `PackingTraits` CS fields | `packing.hpp` L100–135 | `kCsLineBits`, `kCsBitShift`, `kCsMask` with `static_assert` |
| `ChipBase::bus_chip_id_` | `chip.hpp` L120 | Assigned during `Board::bind_chip()` |
| CS workflow docs | `configs.hpp` L277–340 | Two-phase model fully documented |
| `NesCpuBusSpec` with CS | `configs.hpp` L64–75 | `CsLineBits=10, CsBitShift=54` — **only adopter** |

### What's missing

| Gap | Details |
|-----|---------|
| **No system calls `resolve()` in production** | NES uses a custom `nes_bus_t` page-pointer bus, not `MemoryBus`. |
| **`ManifestBusSpec` has no CS parameters** | `chip_manifest.hpp` L720–758 — auto-derives everything except `CsLineBits` / `CsBitShift`. |
| **Most MMIO chips lack `has_mmio()` override** | Only 9 chips return `true`: TIA, MC6845, TED7360, MOS6529, MOS6522, PIA6532, PIA6820, i8255, POKEY. Missing from: VIC-II, SID, CIA, SN76489, AY-3-8910, ULA, TMS9918A, VDP, bbc_vidproc, namco_wsg, z80_pio, z80_ctc. |
| **`has_mmio()` guards auto-wiring** | `BusMap::apply()` Phase 2 skips chips where `has_mmio()` returns `false`. |
| **C64 uses manual `init_io_dispatch()`** | Hybrid: manifest + register_handler + indexed sub-table, hand-wired. |
| **VIC-20 has manual `io_tick()`** | Man-in-the-middle I/O dispatch for mirror handling. |
| **Manifest MMIO addresses** | Committed `d10fa27b` — all systems now have hardware-accurate `base_addr` / `addr_mask`. |

### System dispatch survey

| Category | Count | Systems |
|----------|-------|---------|
| ManifestBusSpec + auto-wired BusMap | ~20 | All DDR, Z80, 6502, BBC, Arcade systems |
| Manual `init_io_dispatch` | 1 | C64 (hybrid: manifest + hand-wired sub-table) |
| Manual `io_tick` dispatch | 1 | VIC-20 (partial: mirrors handled outside BusMap) |
| Custom bus implementation | 1 | NES (`nes_bus_t` page-pointer; ManifestBusSpec defined but unused) |
| No bus (interpreter) | 1 | CHIP-8 (flat memory vector) |

---

## CS Bitfield Budget

### bus_state_t layout (64 bits)

```
 Bit  63                                               48 47             32
      ├────────────────── OUTPUT PINS ──────────────────┤ ├─ INPUT PINS ─┤
 Bit  31            24 23                               8 7              0
      ├──── BANK ────┤ ├───────────── ADDR ─────────────┤ ├──── DATA ────┤
```

**Output pins (bits 48–63):**

| Bit | Pin | Used by |
|-----|-----|---------|
| 48 | RW | All |
| 49 | SYNC | 6502 family |
| 50 | PHI2 | Commodore systems |
| 51 | SP | CIA |
| 52 | BA | C64 (VIC-II) |
| 53 | VP | 6502 |
| 54 | ML | 65C816 |
| 55–63 | *reserved* | — |

### CS field placement

The CS field overlays the upper output-pin bits. Which bits are available
depends on which pins the system actually uses:

| System family | Pins used above bit 52 | Available CS bits | Max CsBitShift |
|---------------|----------------------|-------------------|----------------|
| Simple 6502 (VIC-20, PET, Atom, Apple) | VP(53) | 55–63 = 9 bits | 55 |
| C64/C128 | BA(52), VP(53), ML(54) | 55–63 = 9 bits | 55 |
| Z80 systems | none above 50 | 51–63 = 13 bits | 51 |
| BBC Micro (65C02) | VP(53) | 55–63 = 9 bits | 55 |
| Arcade (6502/Z80) | varies | 9–13 bits | varies |

### Chip ID pressure per system

The number of chip IDs (and thus CsLineBits) depends on `PageBits`:

| System | PageBits | Buffer IDs | MMIO IDs | Total | CsLineBits needed |
|--------|----------|-----------|---------|-------|-------------------|
| C64 | 12 | ~25 | ~7 | ~32 | 6 |
| C16/Plus4 | 8 | ~384 | ~3 | ~390 | 9 |
| VIC-20 | 8 | ~320 | ~4 | ~326 | 9 |
| BBC Micro | 8 | ~1216 | ~4 | ~1222 | **11** |
| Apple IIe | 8 | ~576 | ~3 | ~580 | 10 |
| MSX2 | 8 | ~640 | ~3 | ~644 | 10 |
| NES | 10 | ~512 | ~4 | ~518 | 10 |
| Pac-Man | 8 | ~76 | ~3 | ~80 | 7 |

**Observation:** PageBits has huge leverage. BBC Micro with PageBits=8 needs
11 bits; with PageBits=12 it would need only ~7. The CS field is per-BusSpec,
so each system independently chooses its shift and width.

**Constraint:** `CsBitShift + CsLineBits ≤ 64`. With `CsBitShift=55` (safe
for 6502 family) → max 9 CS bits → covers up to 511 chip IDs. Systems needing
more can lower the shift (overlapping unused pins), or increase `PageBits` to
reduce chip ID count.

### Recommendation

Add `CsLineBits` auto-derivation to `ManifestBusSpec`, computed from
`Manifest.max_chip_id(PageBits)` + sentinel headroom. Let each system control
`CsBitShift` via a defaulted template parameter. Add a `static_assert` that
the CS field fits within `bus_state_t` and doesn't overlap pins the system
actually uses.

---

## Zero-Sized Slot Taxonomy

Every manifest slot with `size_bytes == 0` falls into exactly one category:

| Category | Criteria | Bus participation | Example |
|----------|----------|-------------------|---------|
| **CPU** | Drives the bus | Master — no chip ID needed | MOS6502, Z80A |
| **MMIO** | `base_addr != 0` | Responds to address decode — needs real chip ID | VIC-II, SID, CIA, VIA, PIO |
| **NON-BUS** | `base_addr == 0 && mask == 0`, not a CPU | No bus participation | PSG via VIA, MC6847, peripherals |

No ambiguous cases exist. This classification drives chip ID assignment:
- **CPU** and **NON-BUS** slots are skipped by `BusMap`.
- **MMIO** slots get real chip IDs allocated during `BusMap::apply()`, so
  `resolve()` can emit them into the CS field and chip ticks can self-select.

---

## Prerequisites

### P1. Accurate manifest addresses (mostly done)

All systems must have hardware-accurate `base_addr` and `addr_mask` in their
chip manifests. Committed `d10fa27b` + Atari Vector mask fix. Remaining
issues documented in §Manifest Audit below.

### P2. `ManifestBusSpec` must derive CS parameters

Add defaulted `CsBitShift` template parameter. Auto-compute `CsLineBits`
from `max_chip_id()` + sentinel count + `std::bit_width()`.

### P3. `BusMap::apply()` must assign real chip IDs to MMIO slots

Currently `BusMap::apply()` only assigns chip IDs to buffer slots
(size_bytes > 0). For CS-tick, MMIO-only slots (size_bytes == 0,
base_addr != 0) must also receive real chip IDs so `resolve()` maps their
address range to a chip ID that the chip's tick() can compare against.

This **replaces** the need for `has_mmio()` — the manifest already contains
the decode parameters. `BusMap` can identify MMIO slots purely from
manifest metadata without querying the chip instance.

### Why `has_mmio()` is NOT a prerequisite

`has_mmio()` currently gates `BusMap::apply()` Phase 2 (handler-based MMIO
callback registration). Under CS-tick, that callback path is bypassed
entirely — chips handle their own register access in `tick()`. The wiring
path becomes:

1. `BusMap` assigns chip ID to MMIO slot (from manifest, not from chip).
2. `resolve()` maps address → chip ID into CS field.
3. Chip's `tick()` checks CS field against `bus_chip_id_`.

No handler registration. No `has_mmio()` query. The chip just needs to
know its own ID (already set by `Board::bind_chip()`).

---

## Migration Phases

### Phase 1: Infrastructure

**Goal:** `ManifestBusSpec` gains CS support; `BusMap::apply()` assigns real
chip IDs to MMIO-only slots.

**Steps:**

1. Add `CsBitShift` as a defaulted template parameter to `ManifestBusSpec`
   (default 0 = CS disabled).
2. When `CsBitShift > 0`, auto-compute `CsLineBits` from
   `std::bit_width(max_chip_id + mmio_count + sentinel_count)`.
3. Add `static_assert` that `CsBitShift + CsLineBits ≤ 64`.
4. Extend `BusMap::apply()` to assign real chip IDs to MMIO-only slots
   (size_bytes == 0, base_addr != 0). These IDs go above the buffer chip
   ID range. Map the corresponding pages to these IDs so `resolve()`
   routes addresses to them.
5. Add a convenience `get_cs_from_bus()` static method to `MemoryBus` (or
   expose the existing one as a free function) so chip code doesn't need to
   know the BusSpec type.

**Deliverable:** Any `ManifestBusSpec` system can opt into CS. MMIO chips
get real chip IDs. No system tick changes yet.

**Test:** Compile existing systems unchanged. Verify `static_assert` fires
when CS field would overflow. Verify MMIO slots receive chip IDs.

### Phase 2: Pilot system — enable CS-tick on a simple system

**Goal:** One system runs entirely on the CS-tick model, proving the
architecture end-to-end.

**Candidate:** Acorn Atom, Apple 1, or VIC-20.

**Steps:**

1. Set `CsBitShift=55` in the system's `ManifestBusSpec`.
2. In the system tick loop, call `resolve()` after address-driving chip
   ticks (CPU, DMA) set the address on the bus:
   ```cpp
   cpu.tick(bus);                     // CPU drives address lines
   bus = mem_bus.resolve(bus);        // decode address → CS field
   via.tick(bus);                     // VIA checks CS, handles regs
   // ... other chip ticks ...
   ```
3. In each MMIO chip's `tick()`, add CS gating:
   ```cpp
   if (MemBus::get_cs_from_bus(bus) == bus_chip_id()) {
       // handle read/write
   }
   ```
4. Buffer chips call `bus = mem_bus.service(bus)` after CS check.
5. Remove any per-system address decode code for the pilot.

**Deliverable:** One system with zero per-system I/O dispatch code.

**Test:** Boot the system. Run any available test suites. Compare output
frame-by-frame against pre-migration baseline.

### Phase 3: Roll out to simple 6502 systems

**Goal:** All simple 6502-based systems use CS-tick.

**Systems:** Apple 1, Acorn Atom, Oric-1/Atmos, VIC-20, PET,
ColecoVision (6502-adjacent; uses TMS9918A).

**Steps per system:**
1. Enable `CsBitShift` in ManifestBusSpec.
2. Add CS gating to chip ticks.
3. Remove per-system I/O dispatch code.
4. Regression test.

### Phase 4: Roll out to Z80 systems

**Goal:** All Z80-based systems use CS-tick.

**Systems:** DDR systems (LC80, KC85, Z9001, Z1013), ZX Spectrum,
Amstrad CPC, MSX variants.

**Note:** Z80 systems have ample CS bits (51–63 = 13 bits). Some use I/O
port space in addition to memory-mapped I/O — the CS model must handle
both. Z80 `IN`/`OUT` instructions place the port address on the address bus,
so `resolve()` works identically; the I/O vs. memory distinction is a
separate signal (`IORQ` vs `MREQ`), not an address-space issue.

### Phase 5: Roll out to complex systems

**Goal:** C64, C128, C16/Plus4, BBC Micro.

**Challenges:**
- **C64:** PLA banking makes the page table viewer-dependent. The indexed
  sub-table for $D000–$DFFF already exists. Transition from manual
  `init_io_dispatch()` to `BusMap::apply()` auto-wiring, then enable CS.
- **C128:** Two CPUs (8502 + Z80), MMU-controlled banking.
- **C16/Plus4:** TED-controlled banking, masked sub-table for $FF00 regs.
- **BBC Micro:** Paged ROM (16 banks × 16KB) creates high chip ID count.
  May need `PageBits=12` to keep CsLineBits under 9.

**Strategy:** Migrate each system incrementally:
1. First: enable CS in BusSpec (MMIO slots get real IDs from Phase 1).
2. Then: add CS gating to chip ticks.
3. Finally: remove per-system dispatch code.

### Phase 6: NES migration (optional / deferred)

The NES already has `CsLineBits=10` in `NesCpuBusSpec` but uses a custom
`nes_bus_t` for performance. Migration means replacing `nes_bus_t` with
`MemoryBus<NesCpuBusSpec>` and calling `resolve()` in the tick loop.

This is high-risk / high-effort due to mapper complexity and may be deferred
until the CS model is proven on all other systems.

### Phase 7: Cleanup

1. Remove `has_mmio()` / `on_bus_read()` / `on_bus_write()` from `ChipBase`
   — fully superseded by CS-tick. No chip needs these virtuals anymore.
2. Remove per-system I/O dispatch remnants (dead code).
3. Remove the non-CS `tick()` path from `MemoryBus` if all systems use CS.
4. Remove MMIO handler callback infrastructure from `BusMap` (register_handler,
   mmio_read/write_trampoline) — no longer needed.
5. Document the CS-tick model as the canonical architecture.

---

## Chip Tick Pattern

### MMIO chip (registers only)

```cpp
void my_io_chip_tick(bus_state_t& bus) {
    advance_internal_counters();  // always, regardless of CS

    if (MemBus::get_cs_from_bus(bus) != bus_chip_id())
        return;

    const uint8_t reg = BUS_GET_ADDR(bus) & kRegMask;
    if (BUS_GET_BIT(bus, BUS_RW_BIT))
        bus = read_register(bus, reg);   // receives bus so undriven bits float
    else
        write_register(reg, BUS_GET_DATA(bus));
}
```

**Why `read_register` receives `bus_state_t`:** A chip may not drive all 8
data lines. Bits the chip doesn't own must retain whatever value is already
on the bus (open-bus / floating-line behaviour). The read method uses
`BUS_SET_DATA` or `BUS_BITMIX_DATA` to drive only the bits it owns:

```cpp
// MOS 2114 (4-bit SRAM): drives D0–D3, D4–D7 float
bus_state_t read_register(bus_state_t bus, uint8_t reg) const noexcept {
    BUS_BITMIX_DATA(bus, registers_[reg], uint8_t(0x0F));
    return bus;
}
```

### Buffer chip (RAM/ROM)

```cpp
void ram_tick(MemBus& mem_bus, bus_state_t& bus) {
    if (MemBus::get_cs_from_bus(bus) != bus_chip_id())
        return;

    bus = mem_bus.service(bus);  // flat-mem transfer via CS field
}
```

### Chip with both MMIO registers and buffer (rare)

```cpp
void complex_chip_tick(MemBus& mem_bus, bus_state_t& bus) {
    advance_counters();

    if (MemBus::get_cs_from_bus(bus) != bus_chip_id())
        return;

    const uint16_t addr = BUS_GET_ADDR(bus);
    if (is_register_addr(addr)) {
        handle_register_access(bus, addr);
    } else {
        bus = mem_bus.service(bus);
    }
}
```

---

## Design Decisions

### D1: CS field is per-BusSpec

Each system's BusSpec independently sets `CsBitShift` and `CsLineBits`.
Simple systems (Apple 1: 4 chips) use 3 CS bits. Complex systems (BBC
Micro) use up to 11. The CS field overlaps unused output-pin bits, which is
safe because those pins are per-system.

### D2: `resolve()` is called after address-driving chip ticks

The CPU (or DMA master) ticks first and drives address lines via
`BUS_SET_ADDR()`. Then the system calls `resolve()` once. All subsequent
chip ticks read the same CS field. This matches real hardware where the
address decoder is combinational logic that settles once per cycle.

`BUS_SET_ADDR` remains a pure bit-manipulation macro — it does NOT trigger
resolve. The bus map and the bus_state_t are separate concerns:
- `BUS_SET_ADDR` writes the address field into bus_state_t (fast, no side effects).
- `resolve()` is a separate call that reads the address from bus_state_t,
  performs the page-table lookup, and writes the chip ID into the CS field.
- The bus map is already a member of the system class and is readily
  accessible in the tick loop where `resolve()` is called.

This two-step design keeps the hot macro zero-cost and avoids coupling
address writes to bus map access.

### D3: Buffer chips remain an optimisation

In theory, RAM and ROM could also self-select via CS and perform their own
array access. In practice, `service_read/write()` through flat-mem is faster
because it avoids virtual dispatch and keeps data in cache-friendly layout.
Buffer chips check CS to decide whether to call `service()`, but the actual
memory access goes through `MemoryBus`.

### D4: `has_mmio()` is skipped, not needed

The CS-tick model bypasses the `has_mmio()` → handler-callback path entirely.
Chips don't need to advertise MMIO capability through a virtual — the manifest
already declares decode parameters (base_addr, addr_mask). `BusMap` assigns
real chip IDs to MMIO slots from manifest metadata alone. Chips handle their
own register access in `tick()` by checking the CS field.

Existing `has_mmio()` overrides continue to work for systems still on the
callback path during migration, but no new overrides need to be added.

### D5: Real chip IDs for MMIO slots

MMIO-only slots (size_bytes == 0, base_addr != 0) receive **real chip IDs**
allocated above the buffer chip ID range. `resolve()` maps their address range
to these IDs in the page table, and each chip's `tick()` compares
`get_cs(bus) == bus_chip_id_`.

Sentinel-range IDs (kRegChipBase + handler_idx) remain available for the
callback path during the transition period but are not used under CS-tick.

### D6: Z80 I/O port space

Z80 `IN`/`OUT` instructions drive the port address onto the address bus and
assert `IORQ` instead of `MREQ`. From the CS decoder's perspective, the
address is on the bus and `resolve()` works identically. The `IORQ`/`MREQ`
distinction can be a separate bus signal bit consulted by the chip's tick.

---

## Test Plan

### Per-chip verification

For each MMIO chip migrated to CS-tick gating:
- Verify register reads return correct data for all register addresses.
- Verify register writes correctly update internal state.
- Verify undriven data bits preserve bus state (open-bus behaviour).
- Verify out-of-range addresses are handled (pass-through or wrap).

### Per-system integration tests

For each system migrated to CS-tick:
1. **Boot test:** System reaches main screen (BASIC prompt, title screen).
2. **Audio test:** Sound output matches pre-migration recording (where applicable).
3. **Input test:** Keyboard/joystick input reaches I/O chip registers.
4. **Timing test:** Cycle-accurate operations (raster effects, sprite
   multiplexing) still work on systems with test ROMs.

### Regression baseline

Before starting Phase 3, capture per-system baselines:
- Screenshot at known frame count after cold boot.
- Audio buffer hash over N frames.
- CPU cycle count at a known program counter.

Compare after each phase to detect regressions.

### Performance measurement

- Measure frames-per-second in release builds for C64, BBC Micro, and NES
  (if migrated) before and after CS-tick migration.
- CS-tick should be cost-neutral for MMIO (one integer compare per chip per
  tick replaces one callback dispatch) and cost-neutral for buffers
  (`service()` avoids redundant page lookup).

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| CS field doesn't fit in bus_state_t for high-chip-ID systems | Blocks Phase 3+ | Increase PageBits to reduce chip IDs. Per-system CsBitShift allows independent tuning. |
| CS-tick gating for VIC-II/SID introduces timing changes | Incorrect emulation | VIC-II and SID have complex timing requirements. Ensure the CS-tick register access path exercises the same code as the current manual dispatch. |
| C64 PLA banking is too complex for auto-wiring | Blocks Phase 6 | C64 already has indexed sub-table support. The PLA overlay mechanism (`set_overlay_mode_for_banking`) works orthogonally to CS-tick. |
| Z80 I/O port space doesn't map cleanly to resolve() | Blocks Phase 5 | Z80 I/O port addresses are 8-bit (or 16-bit for some peripherals). A separate `io_bus` MemoryBus instance for port space is one option; a mode bit in bus_state_t is another. |
| NES mapper diversity makes migration impractical | Blocks Phase 7 | NES migration is explicitly optional/deferred. The existing `nes_bus_t` is a working solution. |

---

## Resolved Questions

1. **MMIO-only chips get real chip IDs** (not sentinel-range IDs).
   `BusMap::apply()` will allocate IDs for zero-size MMIO slots above the
   buffer ID range. This is the clean path — chips compare a single integer.

2. **`resolve()` is called after address-driving chip ticks.**
   In the system tick loop: CPU (or DMA master) ticks first and drives
   address lines, then `resolve()` decodes the address into the CS field,
   then all other chip ticks run. This can be revisited later for systems
   where the CPU doesn't access the bus on every cycle.

## Open Questions

1. **DMA masters (VIC-II, DMC):** DMA masters place their own address on
   the bus and need their own `resolve()` call. The viewer mechanism already
   supports this (viewer 0 = CPU, viewer 1 = VIC-II on C64). The tick
   order must ensure the DMA master's resolve uses the correct viewer.

2. **Z80 I/O port space: one bus or two?**
   Options: (a) single MemoryBus with a mode bit, (b) separate `io_bus`
   MemoryBus for port space. Option (b) is simpler and matches the logical
   separation but doubles the bus infrastructure.

---

## Relationship to Other Plans

This plan **complements** and builds on:

- **MEMORYBUS_DECLARATIVE_MIGRATION_PLAN.md**: The declarative plan focuses
  on `BoardDeclaration` → `ResolvedBoardPlan` → runtime install. CS-tick is
  the runtime model that the resolved plan targets. Phase 2 of the
  declarative plan (resolve artifact extraction) naturally produces the CS
  configuration as part of the resolved plan.

- **Manifest MMIO addresses (commit d10fa27b)**: The CS-tick model requires
  accurate decode addresses in the manifest. This is now complete.

- **Chip implementation standards**: CS-tick gating in `tick()` should be
  added to the chip implementation checklist once it becomes the standard
  pattern.

---

## Manifest Audit

The following manifest issues were identified and need resolution:

### Fixed

- **Atari Vector POKEY/EAROM masks**: `0x0F` / `0x3F` (register-select
  bits) corrected to `~(SIZE-1)` convention. Now computed from the
  variant's SIZE constant.

### Remaining Issues

**Z80 I/O port addresses stored in `base_addr` field:**

Several Z80 systems store I/O port numbers in `base_addr`. These are
metadata, not memory-bus decode addresses. The base_addr field is documented
for memory address space, but Z80 IN/OUT uses a separate I/O space.

| System | Chip | base_addr | Actual I/O port | Status |
|--------|------|-----------|-----------------|--------|
| Einstein | TMS9929A | 0x0003 | 0x08–0x09 | Wrong |
| Einstein | Z80 CTC | 0x0008 | 0x10–0x17 | Wrong |
| Einstein | Z80 PIO | 0x0010 | 0x18–0x1F | Wrong |
| Memotech MTX | TMS9918A | 0x0001 | 0x01–0x03 | Verify |
| Memotech MTX | AY-3-8910 | 0x0003 | 0x06–0x07 | Verify |
| Memotech MTX | Z80 CTC | 0x0008 | 0x08–0x0B | Verify |

**Decision needed:** Should Z80 I/O port addresses live in `base_addr` (with
the I/O bus MemoryBus doing the decode), or should they use a separate field
or convention? This is blocked on the Z80 I/O bus design decision (Open
Question §2).

**CPC chips with addr_mask == 0 and non-zero base_addr:**

CPC CRTC (0xBC00), PPI (0xF400), Gate Array (0x7F00) have non-zero
`base_addr` but `addr_mask == 0`. These are Z80 I/O port-decoded chips.
The base_addr encodes the CPC's partial address decode (active-low bit
selection). No fix needed if they move to a dedicated I/O bus.

---

## Summary: Migration Order

```
Phase 1:  Infrastructure (ManifestBusSpec CS + real MMIO chip IDs)
Phase 2:  Pilot system (Atom / Apple 1 / VIC-20)
Phase 3:  Simple 6502 systems
Phase 4:  Z80 systems
Phase 5:  Complex systems (C64, C128, BBC)
Phase 6:  NES (optional / deferred)
Phase 7:  Cleanup (remove has_mmio, callback path, dead code)
```

Each phase is independently shippable and testable.
No phase requires all prior phases to be complete across all systems.
