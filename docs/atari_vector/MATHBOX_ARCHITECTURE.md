# Atari Math Box — Hardware Architecture

## Overview

The Math Box is a dedicated computation coprocessor used in Atari's vector
arcade machines — **Battlezone** (1980), **Red Baron** (1980), and **Tempest**
(1981).  It lives on a separate PCB (Atari document TM-183) and provides
16-bit multiply, divide, and rotation operations that the 6502 main CPU
cannot perform efficiently on its own.

The Math Box is built from **AMD Am2900-family** bit-slice components.  It is
*not* a general-purpose CPU — its instruction set is fixed in hardware via
microcode PROMs burned at the factory.

---

## Chip Inventory

| Chip | Qty | Package | Role |
|------|-----|---------|------|
| **Am2901** | 4 | 40-pin DIP | 4-bit ALU slices — four wired in parallel form a 16-bit ALU with 16 × 16-bit register file |
| **Am2902** | 1 | 16-pin DIP | Carry Look-Ahead Generator — eliminates ripple-carry delay between the four 2901 slices |
| **Am2909** or **Am2911** | 2 | 28-pin DIP | 4-bit microprogram address sequencers — cascaded for 8-bit µPC (256 micro-instructions) |
| **PROMs** (82S131/82S141) | 4–6 | 16/20-pin | Microcode store — each PROM contributes a field of the ~48-bit micro-word |
| **74LS-series glue** | ~10 | various | Latches, MUXes, buffers for data I/O, status flags, address decode |

---

## Block Diagram

```
  CPU data bus (D0-D7)
       │
       ▼
  ┌─────────┐
  │ Input    │──── 8-bit data latch (74LS374)
  │ Latch    │
  └────┬─────┘
       │ D[3:0] to each slice
       ▼
  ┌─────────┬─────────┬─────────┬─────────┐
  │ Am2901  │ Am2901  │ Am2901  │ Am2901  │  ← 4 slices
  │ bits    │ bits    │ bits    │ bits    │     16 registers × 4 bits each
  │ [3:0]   │ [7:4]   │ [11:8]  │ [15:12] │     = 16 registers × 16 bits total
  └──┬──┬───┴──┬──┬───┴──┬──┬───┴──┬──┬───┘
     Cn  C4    Cn  C4    Cn  C4    Cn  C4
     └───┘     └───┘     └───┘     └───┘
        └─────────┴─────────┘
                  │
            ┌─────┴─────┐
            │  Am2902   │  ← Carry Look-Ahead
            │  CLA Gen  │     (G,P from each slice → Cn+4 to next)
            └───────────┘
                  │
  ┌───────────────┴───────────────┐
  │  Y bus (16-bit ALU output)    │
  │  → result latch (output reg) │
  │  → status_r (bit 7 = ~BUSY)  │
  │  → lo_r / hi_r (byte access) │
  └───────────────────────────────┘

  Microprogram control:
  ┌─────────┬─────────┐
  │ Am2909  │ Am2909  │  ← 2 sequencers = 8-bit µPC
  │ [3:0]   │ [7:4]   │     → address into PROM bank
  └────┬────┴────┬────┘
       │         │
  ┌────┴─────────┴────┐
  │  PROM bank        │  ← 4-6 PROMs, ~48-bit microword
  │  (82S131 etc.)    │     fields: ALU_OP[8:0], SRC[2:0],
  │                   │     DST[2:0], A_ADDR[3:0], B_ADDR[3:0],
  │                   │     SHIFT, BRANCH_COND, NEXT_ADDR, ...
  └───────────────────┘
```

---

## Am2901 — The Core Bit-Slice ALU

The Am2901 is a 40-pin DIP containing:
- **16 × 4-bit registers** (R0–R15) — addressable via two independent 4-bit address buses (A and B)
- **4-bit ALU** — performs add, subtract, AND, OR, XOR, pass, complement
- **Q register** — 4-bit auxiliary shifter used in multiply/divide algorithms
- **Carry chain** — Cn (carry in), Cn+4 (carry out), plus G/P (generate/propagate) for look-ahead

### Am2901 Pinout (40-pin DIP)

| Pin | Signal | Direction | Description |
|-----|--------|-----------|-------------|
| 1 | A0 | Input | Register A address bit 0 |
| 2 | A1 | Input | Register A address bit 1 |
| 3 | A2 | Input | Register A address bit 2 |
| 4 | A3 | Input | Register A address bit 3 |
| 5 | RAM0 | I/O | Register file shift data (LSB) |
| 6 | RAM3 | I/O | Register file shift data (MSB) |
| 7 | Q0 | I/O | Q register shift data (LSB) |
| 8 | Q3 | I/O | Q register shift data (MSB) |
| 9 | D0 | Input | Direct data input bit 0 |
| 10 | D1 | Input | Direct data input bit 1 |
| 11 | D2 | Input | Direct data input bit 2 |
| 12 | D3 | Input | Direct data input bit 3 |
| 13 | I0 | Input | Micro-instruction bit 0 (ALU source) |
| 14 | I1 | Input | Micro-instruction bit 1 (ALU source) |
| 15 | I2 | Input | Micro-instruction bit 2 (ALU source) |
| 16 | I3 | Input | Micro-instruction bit 3 (ALU function) |
| 17 | I4 | Input | Micro-instruction bit 4 (ALU function) |
| 18 | I5 | Input | Micro-instruction bit 5 (ALU function) |
| 19 | I6 | Input | Micro-instruction bit 6 (ALU destination) |
| 20 | I7 | Input | Micro-instruction bit 7 (ALU destination) |
| 21 | I8 | Input | Micro-instruction bit 8 (ALU destination) |
| 22 | Cn | Input | Carry in |
| 23 | B0 | Input | Register B address bit 0 |
| 24 | B1 | Input | Register B address bit 1 |
| 25 | B2 | Input | Register B address bit 2 |
| 26 | B3 | Input | Register B address bit 3 |
| 27 | Y0 | Output (3-state) | ALU output bit 0 |
| 28 | Y1 | Output (3-state) | ALU output bit 1 |
| 29 | Y2 | Output (3-state) | ALU output bit 2 |
| 30 | Y3 | Output (3-state) | ALU output bit 3 |
| 31 | OE | Input | Output enable (active low) |
| 32 | Cn+4 | Output | Carry out |
| 33 | F=0 | Output | Zero flag (1 when F[3:0] = 0000) |
| 34 | F3 | Output | MSB of ALU result (sign bit for this slice) |
| 35 | OVR | Output | Overflow (carry into MSB ≠ carry out of MSB) |
| 36 | G | Output | Carry generate (for look-ahead) |
| 37 | P | Output | Carry propagate (for look-ahead) |
| 38 | CLK | Input | Clock — latches register writes on rising edge |
| 39 | VCC | Power | +5V |
| 40 | GND | Power | Ground |

### Micro-Instruction Encoding (I[8:0])

**I[2:0] — ALU Source Select:**

| I2 | I1 | I0 | R operand | S operand |
|----|----|----|-----------|-----------|
| 0 | 0 | 0 | A | Q |
| 0 | 0 | 1 | A | B |
| 0 | 1 | 0 | 0 | Q |
| 0 | 1 | 1 | 0 | B |
| 1 | 0 | 0 | 0 | A |
| 1 | 0 | 1 | D | A |
| 1 | 1 | 0 | D | Q |
| 1 | 1 | 1 | D | 0 |

**I[5:3] — ALU Function:**

| I5 | I4 | I3 | Function |
|----|----|-----|----------|
| 0 | 0 | 0 | R + S + Cn |
| 0 | 0 | 1 | S - R - 1 + Cn |
| 0 | 1 | 0 | R - S - 1 + Cn |
| 0 | 1 | 1 | R OR S |
| 1 | 0 | 0 | R AND S |
| 1 | 0 | 1 | /R AND S |
| 1 | 1 | 0 | R XOR S |
| 1 | 1 | 1 | R XNOR S |

**I[8:6] — ALU Destination:**

| I8 | I7 | I6 | Y output | RAM write | Q write |
|----|----|----|----------|-----------|---------|
| 0 | 0 | 0 | F | — | Q←F |
| 0 | 0 | 1 | F | — | — |
| 0 | 1 | 0 | A | B←F | Q←F |
| 0 | 1 | 1 | F | B←F | — |
| 1 | 0 | 0 | F | — | Q÷2 |
| 1 | 0 | 1 | F | — | — |
| 1 | 1 | 0 | A | B←F/2 | Q÷2 |
| 1 | 1 | 1 | F | B←F/2 | — |

---

## Execution Model

### CPU → Math Box Handshake

1. **CPU writes** to one of 32 I/O addresses. The low 5 bits of the address
   become the **opcode** (loaded into a latch).  The 8-bit data byte is
   latched separately.

2. The opcode latch triggers the **microsequencer** — it resets the µPC to an
   entry point indexed by the 5-bit opcode.  The BUSY flag goes **high**.

3. The microsequencer steps through PROM addresses at the **math box clock
   rate** (~6 MHz on BZ/Tempest — roughly 4× the 6502's 1.5 MHz).  Each
   micro-step takes 1 clock cycle (~167 ns).

4. Each micro-word controls: which Am2901 registers to read (A/B address),
   which ALU operation, where to write back (register file or Q shifter),
   shift direction, carry-in, and the next µPC address (including conditional
   branches on carry/zero/sign flags).

5. After the microprogram completes (typically 4–20 micro-steps), the
   sequencer halts.  BUSY goes **low**.  The result is latched on the Y bus
   output register.

6. **CPU reads** `status_r()` (bit 7 = 0 → done), then `lo_r()` / `hi_r()`
   for the 16-bit result.

### Timing per Operation

| Operation | µ-steps (approx.) | Time @ 6 MHz |
|-----------|-------------------|--------------|
| Register load (0x00–0x0A, etc.) | ~2 | ~333 ns |
| Rotation (0x0B, 0x11) | ~8–12 | ~1.3–2 µs |
| Perspective + divide (0x12–0x13) | ~15–20 | ~2.5–3.3 µs |
| Division (0x14) | ~16 | ~2.7 µs |
| Distance (0x1D–0x1E) | ~6 | ~1 µs |

At the 6502's 1.5 MHz clock, even the longest operation (~3 µs) completes in
roughly 4–5 CPU cycles.  The 6502 firmware typically does not poll the BUSY
flag — it writes the command, executes a few unrelated instructions, and reads
the result.

---

## Memory-Mapped I/O Addresses

### Battlezone / Red Baron

| Address Range | Signal | Direction |
|---------------|--------|-----------|
| $1800 | MATHBOX STATUS | Read (bit 7 = 0 → done) |
| $1804 (RB) / $1810 (BZ) | MATHBOX LO | Read (result low byte) |
| $1806 (RB) / $1818 (BZ) | MATHBOX HI | Read (result high byte) |
| $1860–$187F | MATHBOX GO | Write (offset = opcode, data = operand) |

### Tempest

| Address Range | Signal | Direction |
|---------------|--------|-----------|
| $6040 | MATHBOX STATUS | Read (shared with EAROM control) |
| $6060 | MATHBOX LO | Read (result low byte) |
| $6070 | MATHBOX HI | Read (result high byte) |
| $6080–$609F | MATHBOX GO | Write (offset = opcode, data = operand) |

---

## Math Box Operations (Software View)

The 32 write addresses ($00–$1F) map to either **register loads** or
**computation triggers**.  The 16 internal registers are:

| Register | Symbol | Primary role |
|----------|--------|--------------|
| REG0 | reg_[0] | Rotation matrix element / cos θ |
| REG1 | reg_[1] | Rotation matrix element / sin θ |
| REG2 | reg_[2] | X coordinate / accumulator |
| REG3 | reg_[3] | Y coordinate / accumulator |
| REG4 | reg_[4] | Transformed X / delta |
| REG5 | reg_[5] | Transformed Y / delta |
| REG6 | reg_[6] | Division step counter (8-bit) |
| REG7 | reg_[7] | Result buffer / dividend |
| REG8 | reg_[8] | Result buffer / quotient |
| REG9 | reg_[9] | Scratch / remainder |
| REGa | reg_[0xA] | Division dividend alternative |
| REGb | reg_[0xB] | Division divisor alternative |
| REGc | reg_[0xC] | Scratch — multiply partial product |
| REGd | reg_[0xD] | Distance / division temporary |
| REGe | reg_[0xE] | Sign save / window midpoint |
| REGf | reg_[0xF] | Mode flag (rotation: 0=full, -1=delta) |

### Command Summary

| Offset | Operation | Description |
|--------|-----------|-------------|
| $00–$0A | Loads | Load register halves (low/high byte) |
| $0B | Rotation (delta) | Set regF=-1, subtract, then rotate |
| $0C | Load REG6 | Division step counter |
| $0D–$10 | Loads | Load REGa, REGb halves |
| $11 | Rotation (full) | Set regF=0, then rotate |
| $12 | Perspective part 2 | Cross-multiply + round; if full mode → divide |
| $13 | Division | Divide REG9:REG8 by REG7 |
| $14 | Division (alt) | Divide REGa:REGb by REG7 |
| $15–$16 | Loads | Load REG7 |
| $17–$19 | Readbacks | Read REG7, REG8, REG9 to result |
| $1A–$1B | Loads | Load REG8 |
| $1C | Window test | Binary search midpoint convergence |
| $1D | Distance | Manhattan → Euclidean approximation |
| $1E | Max + 3/8 min | Distance approximation subroutine |
| $1F | Self-test | No-op (returns immediately) |

---

## Cycle-Exact Emulation Feasibility

### What Would Be Required

1. **Am2901 as `ChipBase`** — well-documented 40-pin DIP with clean I/O.
   The DECL macro would cover the 9-bit micro-instruction, A/B addresses,
   D input, Y output, Q register, and status flags.

2. **Am2902 Carry Look-Ahead** — pure combinational logic (no state).
   Trivial chip.

3. **Am2909/Am2911 Microsequencer** — 4-bit address sequencer with 4-deep
   stack.  Two cascaded for 8-bit µPC.

4. **PROM microcode dumps** — the PROM contents define the entire
   instruction set.  These are **not included in standard ROM sets** and
   would need physical board dumps.

5. **Board-level composite** — a `MathBoxBoard` class wiring 4× Am2901
   (carry-chained via Am2902), 2× Am2909, PROM array, input/output latches,
   and the BUSY flag flip-flop.

### Assessment

| Aspect | Verdict |
|--------|---------|
| Technically possible | Yes — all chips are well-documented |
| Fits cermu ChipBase model | Yes — Am2901 fits LogicChipBase with DECL, pin layout, debug fields |
| Missing prerequisite | PROM microcode dumps (not in public ROM sets) |
| Observable benefit | Near zero — the 6502 never sees BUSY=1, results are bit-identical |
| Performance cost | ~80× more work per operation (4–20 micro-steps × 4 slices vs. 1 function call) |
| Reuse potential | High — Am2901 appears in I, Robot and many non-Atari machines |

### Current Implementation

The current implementation (`mathbox.hpp`) uses the **algorithmic approach**
from MAME's `mathbox.cpp` by Eric Smith — it directly computes the results of
each operation in C++, producing bit-identical output to the hardware.  All
operations complete instantaneously (status always returns 0x00 = done).

This is the same strategy used by every known emulator, because:
- The results are mathematically identical
- The 6502 firmware does not depend on sub-operation timing
- The PROM microcode dumps are not publicly available

---

## References

- Atari TM-183 — Math Box Schematics (original Atari engineering document)
- AMD Am2900 Family Data Book (1979): [bitsavers.org PDF](http://bitsavers.org/components/amd/bitslice/1979_AMD_2900family.pdf)
- MAME `mathbox.cpp` / `mathbox.h` — Eric Smith's algorithmic implementation
- MAME `bzone.cpp` — Battlezone/Red Baron system driver (memory maps)
- MAME `tempest.cpp` — Tempest system driver (memory maps)
- Wikipedia: [AMD Am2900](https://en.wikipedia.org/wiki/AMD_Am2900)
