# MC6809 Family — Implementation Plan

## 1. Pattern Alignment

The implementation follows the proven NTTP pattern used by both existing CPU families:

| Concern | fam65xx | Z80 | MC6809 (proposed) |
|---------|---------|-----|--------------------|
| Traits struct | `CPUTraits` | `Z80Traits` | `MC6809Traits` |
| Template | `fam65xx_t<const CPUTraits&>` | `z80_t<const Z80Traits&>` | `mc6809_t<const MC6809Traits&>` |
| Base class | `CpuChipBase` | `CpuChipBase` | `CpuChipBase` |
| Feature gating | `if constexpr` on flags | `if constexpr` on flags | `if constexpr` on flags |
| Variant headers | `mos6502.hpp`, `mos6510.hpp`, … | `zilog_z80a.hpp`, `u880.hpp`, … | `motorola_mc6809.hpp`, `motorola_mc6809e.hpp`, `hitachi_hd6309.hpp`, `hitachi_hd6309e.hpp` |
| Bus signals | bits 32–54 | bits 55–60 + aliased 32–34, 43–44 | bits 55–60 + aliased 32–34 |

---

## 2. Trait Axes

Three axes:

### ClockModel — enum, not a flag

Internal vs external clock is mutually exclusive and fundamentally changes the bus protocol. An enum field is more expressive (matches `BankingType` in fam65xx):

```cpp
enum class ClockModel : uint8_t {
    INTERNAL,   // MC6809:  generates E/Q from crystal, has MRDY input
    EXTERNAL    // MC6809E: E/Q supplied externally, has LIC/BUSY outputs, TSC input
};
```

This gates `tick()` structure via `if constexpr(Traits.has_internal_clock())` — the internal variant drives E/Q phasing, the external variant reads them as inputs.

### ExtendedISA — flag bit

Gates: extra registers (W, V, zero reg, MD), register-register operations (ADDR, ANDR, ORR, EORR, CMPR, etc.), block transfers (TFM), 32-bit operations (LDQ, STQ, MULD), bit manipulation (BAND/BOR/BEOR/LDBT/STBT family), immediate-to-memory logic (AIM/OIM/EIM/TIM), native mode (fewer cycles).

Same pattern as `ILLEGAL_OPCODES` in fam65xx — entire instruction handlers compile away when false.

### NativeDivide — separate flag bit

Gates: DIVD (16÷8→8,8) and DIVQ (32÷16→16,16). Called out separately because:

1. Division is the most complex ALU operation — its own handler with multi-cycle state machine
2. Architecturally distinct from the "more instructions on existing operations" nature of ExtendedISA
3. Keeps the division handler self-contained for testing and verification

Invariant: `has_native_divide()` implies `has_extended_isa()`. Enforced via `static_assert` in the template.

---

## 3. Chip Coverage

| Chip | Notes |
|------|-------|
| MC6809 | Motorola original, internal clock |
| MC6809E | External E/Q clock variant, used in CoCo, Dragon |
| HD6309 | Hitachi licensed clone with undocumented extended ISA — drop-in replacement in CoCo 3, some arcade boards |
| HD6309E | External clock variant of HD6309 |

---

## 4. Traits Struct

```cpp
namespace mc6809 {

namespace MC6809CoreFlags {
    // Instruction set (bits 0–3)
    constexpr uint32_t EXTENDED_ISA      = (1 << 0);  // HD6309 full extended instruction set
    constexpr uint32_t NATIVE_DIVIDE     = (1 << 1);  // HD6309 DIVD/DIVQ

    // HD6309 native mode (bits 4–7)
    constexpr uint32_t NATIVE_MODE       = (1 << 4);  // Can enter native mode (fewer cycles)
};

namespace CoreFlags {
    constexpr uint32_t MOTOROLA_BASE = 0;
    constexpr uint32_t HITACHI_BASE  = MC6809CoreFlags::EXTENDED_ISA
                                     | MC6809CoreFlags::NATIVE_DIVIDE
                                     | MC6809CoreFlags::NATIVE_MODE;
};

struct MC6809Traits {
    const char*  vendor;
    const char*  chip_id;
    uint32_t     core_flags;
    ClockModel   clock_model;
    uint8_t      address_bits;        // always 16
    uint8_t      max_clock_mhz_x10;   // e.g. 10 = 1.0 MHz

    constexpr bool has(uint32_t flag) const { return (core_flags & flag) != 0; }
    constexpr bool has_internal_clock() const { return clock_model == ClockModel::INTERNAL; }
    constexpr bool has_external_clock() const { return clock_model == ClockModel::EXTERNAL; }
    constexpr bool has_extended_isa() const   { return has(MC6809CoreFlags::EXTENDED_ISA); }
    constexpr bool has_native_divide() const  { return has(MC6809CoreFlags::NATIVE_DIVIDE); }
    constexpr bool has_native_mode() const    { return has(MC6809CoreFlags::NATIVE_MODE); }
};

} // namespace mc6809
```

---

## 5. Concrete Trait Instances

```cpp
// motorola_mc6809.hpp
inline constexpr MC6809Traits MC6809Traits_v = {
    "Motorola", "MC6809",
    CoreFlags::MOTOROLA_BASE,
    ClockModel::INTERNAL, 16, 10   // 1.0 MHz
};
using MC6809 = mc6809_t<MC6809Traits_v>;

// motorola_mc6809e.hpp
inline constexpr MC6809Traits MC6809ETraits = {
    "Motorola", "MC6809E",
    CoreFlags::MOTOROLA_BASE,
    ClockModel::EXTERNAL, 16, 10
};
using MC6809E = mc6809_t<MC6809ETraits>;

// hitachi_hd6309.hpp
inline constexpr MC6809Traits HD6309Traits = {
    "Hitachi", "HD6309",
    CoreFlags::HITACHI_BASE,
    ClockModel::INTERNAL, 16, 20   // 2.0 MHz
};
using HD6309 = mc6809_t<HD6309Traits>;

// hitachi_hd6309e.hpp
inline constexpr MC6809Traits HD6309ETraits = {
    "Hitachi", "HD6309E",
    CoreFlags::HITACHI_BASE,
    ClockModel::EXTERNAL, 16, 20
};
using HD6309E = mc6809_t<HD6309ETraits>;
```

Four variants, two axes (2×2 matrix).

---

## 6. Register File Design

The 6809 has overlapping register groups. The HD6309 extends these to a three-level nesting (A/B → D, E/F → W, D/W → Q). Big-endian CPU on little-endian host requires inverted struct member order in unions:

```cpp
struct Registers {
    // --- Main accumulators ---
    // D = (A << 8) | B.  On LE host: low byte = B, high byte = A.
    union { struct { uint8_t b, a; }; uint16_t d; };

    // --- HD6309 extended accumulators ---
    // W = (E << 8) | F.  Same LE layout.
    union { struct { uint8_t f, e; }; uint16_t w; };

    // Q = (D << 16) | W — too wide for a union spanning two unions.
    // Accessor methods instead:
    //   uint32_t q() const { return (uint32_t(d) << 16) | w; }
    //   void set_q(uint32_t v) { d = v >> 16; w = v & 0xFFFF; }

    // --- Index & stack ---
    uint16_t x;     // Index X
    uint16_t y;     // Index Y
    uint16_t u;     // User stack pointer
    uint16_t s;     // Hardware stack pointer
    uint16_t pc;    // Program counter

    // --- System ---
    uint8_t  dp;    // Direct page register
    uint8_t  cc;    // Condition codes: E F H I N Z V C

    // --- HD6309 only ---
    uint16_t v;     // Vector/value register
    uint8_t  md;    // Mode/division register (NM, FM, IL bits)
};
```

**Design decisions:**

- W/E/F fields exist in the struct unconditionally (4 bytes — not worth conditionalizing with `std::conditional_t`). Access is gated by `if constexpr(has_extended_isa())` at every use site, and the compiler eliminates dead stores/loads.
- The zero register ("0") is not stored — reads return 0, writes are discarded. Handled in the TFR/EXG register decode logic.
- Q is computed via `q()`/`set_q()` accessors — can't union across two separate A:B / E:F unions; compiler optimizes the shift+OR.

---

## 7. Bus Signal Allocation

The 6809 reuses the Z80's bit positions (55–60) since they never share a `bus_state_t`. Input pins overlap the fam65xx range (35–39) for the same reason.

```cpp
// --- Shared (bits 32-34) ---
#define MC6809_RES_BIT     BUS_RES_BIT     // 32  Reset (active-low)
#define MC6809_IRQ_BIT     BUS_IRQ_BIT     // 33  IRQ (active-low)
#define MC6809_NMI_BIT     BUS_NMI_BIT     // 34  NMI (active-low, edge-triggered)

// --- Input pins (bits 35-42) ---
#define MC6809_FIRQ_BIT    35   // Fast IRQ (active-low) — unique to 6809!
#define MC6809_HALT_BIT    36   // Halt request (active-low)
#define MC6809_DMABREQ_BIT 37   // DMA / bus request (active-low)
#define MC6809_MRDY_BIT    38   // Memory ready — MC6809 only (slows E phase)
#define MC6809E_E_IN_BIT   40   // E clock input — MC6809E only
#define MC6809E_Q_IN_BIT   41   // Q clock input — MC6809E only
#define MC6809E_TSC_BIT    42   // Tri-state control — MC6809E only

// --- Output pins (bits 48-60) ---
#define MC6809_RW_BIT      BUS_RW_BIT      // 48  R/W (1=read, 0=write)
#define MC6809_BA_BIT      55   // Bus Available
#define MC6809_BS_BIT      56   // Bus Status
#define MC6809_E_OUT_BIT   57   // E clock output — MC6809 only
#define MC6809_Q_OUT_BIT   58   // Q clock output — MC6809 only
#define MC6809E_LIC_BIT    59   // Last Instruction Cycle — MC6809E only
#define MC6809E_BUSY_BIT   60   // Busy (double-byte op) — MC6809E only
```

BA/BS combinations encode processor state (hardware-observable):

| BA | BS | State |
|----|----|-------|
| 0  | 0  | Normal (running) |
| 0  | 1  | Interrupt acknowledge |
| 1  | 0  | SYNC acknowledge |
| 1  | 1  | Halted / bus granted |

---

## 8. Clock & Cycle Model

The 6809 uses an E/Q quadrature clock. Each bus cycle has four phases:

```
Q: __|‾‾‾|___|‾‾‾|___
E: ___|‾‾‾|___|‾‾‾|__
       ↑       ↑
       addr    data
       valid   valid
```

**Granularity: half-cycle** (matching fam65xx's PHI1/PHI2 split). Two `tick()` calls = one bus cycle:

- **Tick on E-low (setup phase):** CPU drives address bus, sets R/W. Q rises during this phase.
- **Tick on E-high (data phase):** Data transfer occurs. For reads, CPU samples data bus on falling E edge. For writes, CPU drives data bus.

This gives systems precise insertion points for DMA/VDG bus stealing (CoCo's MC6847 halts the CPU via HALT to steal bus cycles for video refresh).

For internal clock (`MC6809`): `tick()` toggles E/Q output pins internally.
For external clock (`MC6809E`): `tick()` reads E/Q input pins and synchronizes.

```cpp
bus_state_t tick(bus_state_t pins) {
    if constexpr (Traits.has_internal_clock()) {
        // Toggle E/Q, drive outputs
        e_phase_ ^= 1;
        if (e_phase_) {
            pins |= MC6809_E;    // E rises
        } else {
            pins &= ~MC6809_E;   // E falls — data valid, latch
        }
        // ... Q leads E by 90°
    } else {
        // Read E/Q from input pins, react to edges
        bool e_now = (pins & MC6809E_E_IN) != 0;
        bool e_prev = (bus_snapshot_ & MC6809E_E_IN) != 0;
        // Edge detection via snapshot comparison
        if (!e_prev && e_now) { /* rising E — begin data phase */ }
        if (e_prev && !e_now) { /* falling E — latch data */ }
    }
    // ... instruction execution state machine
    bus_snapshot_ = pins;
    return pins;
}
```

---

## 9. Indexed Addressing Mode Decoder

The 6809's indexed post-byte is the most complex addressing mode decoder in any 8-bit CPU. It deserves its own include file. The post-byte format:

```
Bit 7 = 0: 5-bit constant offset (-16 to +15) from R
Bit 7 = 1: Bits 4-0 select mode, bits 6-5 select register (X/Y/U/S)

  0x00: ,R+       auto-increment by 1
  0x01: ,R++      auto-increment by 2
  0x02: ,-R       auto-decrement by 1
  0x03: ,--R      auto-decrement by 2
  0x04: ,R        zero offset
  0x05: B,R       B accumulator offset
  0x06: A,R       A accumulator offset
  0x08: n8,R      8-bit signed offset
  0x09: n16,R     16-bit signed offset
  0x0B: D,R       D accumulator offset
  0x0C: n8,PCR    8-bit PC-relative
  0x0D: n16,PCR   16-bit PC-relative
  0x1F: [n16]     extended indirect

  Bit 4 = 1: indirect (adds extra level of indirection)
```

HD6309 adds:

- `0x07: E,R` and `0x0A: F,R` — E/F accumulator offsets
- `0x0E: W,R` — W register offset
- W as an index base register in certain modes

This maps cleanly to a `decode_indexed_postbyte()` function with `if constexpr(has_extended_isa())` for the HD6309-only modes.

---

## 10. HD6309 Extended ISA — `if constexpr` Gating

Same pattern as `IllegalOpcodes` in fam65xx. The 6809 uses prefix bytes $10 and $11 for page-2 and page-3 opcodes. The HD6309 fills in many "illegal" slots in all three opcode pages:

```cpp
bus_state_t op_addr(bus_state_t pins) {
    // ADDR: Add register to register (HD6309 only)
    if constexpr (has_extended_isa()) {
        uint8_t postbyte = this->get(REG_DL);
        uint8_t src = (postbyte >> 4) & 0x0F;
        uint8_t dst = postbyte & 0x0F;
        // ... register-to-register addition
    }
    return pins;
}

bus_state_t op_divd(bus_state_t pins) {
    // DIVD: Signed 16÷8 division (HD6309 only, separate from ExtendedISA)
    if constexpr (has_native_divide()) {
        // D ÷ operand → A (quotient), B (remainder)
        // ... multi-cycle division state machine
    }
    return pins;
}

bus_state_t op_tfm(bus_state_t pins) {
    // TFM: Block transfer (HD6309 only)
    if constexpr (has_extended_isa()) {
        // Four modes: r+,r+ / r-,r- / r+,r0 / r0,r+
        // Repeats until W == 0
        // ... interruptible block copy loop
    }
    return pins;
}
```

All HD6309-only handlers compile to nothing for MC6809/MC6809E — zero overhead.

---

## 11. File Structure

```
src/chip/cpu/mc6809/
├── mc6809_traits.hpp              # MC6809Traits, MC6809CoreFlags, ClockModel enum
├── mc6809_types.hpp               # Enums: Reg8, Reg16, Flags, Condition, InterruptType
├── mc6809.hpp                     # Main template: mc6809_t<const MC6809Traits&>
├── mc6809_registers.inc.hpp       # Register file struct + accessor helpers (included inline)
├── mc6809_alu.inc.hpp             # ALU operations (add, sub, logic, shifts, multiply, DAA)
├── mc6809_indexed.inc.hpp         # Indexed addressing mode post-byte decoder
├── mc6809_opcodes.hpp             # Opcode descriptors (mnemonic, addressing mode, cycles)
├── mc6809_opcode_tables.inc.hpp   # Dispatch tables for pages 0, 1 ($10), 2 ($11)
├── mc6809_decoder.hpp             # Disassembler / decoder
├── mc6809_asm.hpp                 # Assembler support (test infrastructure)
├── mc6809_gui.cpp                 # ImGui debug (registers, disassembly) — #ifdef CERMU_HAS_GUI
├── mc6809_registry.cpp            # Factory registration
├── operations/
│   ├── mc6809_inherent_ops.inc.hpp    # ABX, DAA, MUL, SEX, SWI, NOP, SYNC, CWAI, ...
│   ├── mc6809_load_store_ops.inc.hpp  # LD/ST for all registers
│   ├── mc6809_arithmetic_ops.inc.hpp  # ADD, SUB, ADC, SBC, CMP, NEG, INC, DEC, CLR, TST
│   ├── mc6809_logic_ops.inc.hpp       # AND, OR, EOR, COM, BIT
│   ├── mc6809_shift_ops.inc.hpp       # ASL, ASR, LSR, ROL, ROR
│   ├── mc6809_branch_ops.inc.hpp      # Bxx, LBxx, BSR, LBSR
│   ├── mc6809_stack_ops.inc.hpp       # PSH, PUL (S and U variants)
│   ├── mc6809_transfer_ops.inc.hpp    # TFR, EXG (+ HD6309 extended register set)
│   ├── mc6809_lea_ops.inc.hpp         # LEAX, LEAY, LEAU, LEAS
│   └── mc6809_hd6309_ops.inc.hpp      # All HD6309-only ops (gated by if constexpr)
├── motorola_mc6809.hpp            # MC6809 variant traits + using alias
├── motorola_mc6809e.hpp           # MC6809E variant traits + using alias
├── hitachi_hd6309.hpp             # HD6309 variant traits + using alias
└── hitachi_hd6309e.hpp            # HD6309E variant traits + using alias
```

Naming follows the Z80 pattern (`zilog_z80a.hpp` = `vendor_chip.hpp`).

---

## 12. Implementation Phases

### Phase 1 — Scaffold & Base Infrastructure

1. `mc6809_traits.hpp` — Traits struct, flags, clock model enum
2. `mc6809_types.hpp` — Register enums, flag bit positions, condition codes
3. `mc6809_registers.inc.hpp` — Register file struct with overlapping unions
4. `mc6809.hpp` — Template class skeleton with `init()`, `reset()`, `tick()` stubs
5. Four variant headers with traits instances and using aliases
6. Bus signal `#define`s

### Phase 2 — Core Instruction Execution

1. Opcode fetch + prefix detection ($10, $11 pages)
2. Addressing mode resolution (inherent, immediate, direct, extended, relative)
3. Indexed post-byte decoder (`mc6809_indexed.inc.hpp`)
4. ALU helpers: 8-bit add/sub/logic/shift, 16-bit add/sub, MUL, DAA
5. Page-0 inherent operations (NOP, ABX, DAA, SEX, MUL, SWI, SYNC, CWAI)
6. Page-0 load/store, arithmetic, logic, shift, branch, stack, transfer, LEA

### Phase 3 — Interrupt & Clock Model

1. IRQ/FIRQ/NMI/SWI vectoring (stack push of full or partial register set)
2. FIRQ fast path (pushes only CC and PC — partial state)
3. CWAI (AND CC then wait for interrupt) and SYNC (halt until interrupt)
4. Internal clock model: E/Q generation, MRDY wait-state insertion
5. External clock model: E/Q edge detection, LIC/BUSY/TSC handling
6. HALT and DMA/BREQ bus arbitration

### Phase 4 — HD6309 Extended ISA

1. Extended register access: E, F, W, V, zero register, MD
2. TFR/EXG with expanded register set (16 source × 16 dest)
3. Register-to-register operations (ADDR, ADCR, SUBR, SBCR, ANDR, ORR, EORR, CMPR)
4. Block transfer TFM (four modes, interruptible)
5. Immediate-to-memory operations (AIM, OIM, EIM, TIM)
6. Bit manipulation (BAND/BOR/BEOR/BIAND/BIOR/BIEOR/LDBT/STBT)
7. Extended load/store (LDW, STW, LDQ, STQ, LDE, LDF, STE, STF)
8. MULD (16×16→32), DIVD (16÷8), DIVQ (32÷16)
9. Native mode (MD.NM flag — reduced cycle counts for many instructions)

### Phase 5 — Testing & Integration

1. Opcode tables and decoder/disassembler
2. Assembler support for test infrastructure
3. Factory registration (`mc6809_registry.cpp`)
4. GUI debug panel (`mc6809_gui.cpp`)
5. Integration with an arcade system (or CoCo/Dragon) as proof-of-life

---

## 13. Key Engineering Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Tick granularity | Half-cycle (E-low / E-high) | Matches fam65xx pattern; enables cycle-accurate DMA/VDG bus stealing |
| Q register in struct | Computed via `q()`/`set_q()` accessors | Can't union across two separate A:B / E:F unions; compiler optimizes the shift+OR |
| W/E/F fields | Always present in struct | 4 bytes not worth `std::conditional_t`; access gated by `if constexpr` |
| Zero register | Not stored; decode-time constant 0 | No memory cost; handled in TFR/EXG register lookup |
| NativeDivide separate from ExtendedISA | Yes, distinct flag + `static_assert` invariant | Division is a complex multi-cycle state machine; worth isolating |
| ClockModel | Enum field, not flag bits | Mutually exclusive; affects `tick()` structure, not just behavior gating |
| Indexed mode decoder | Own `.inc.hpp` | 20+ sub-modes, each variant-aware; too complex to inline in opcode handlers |
| Prefix pages ($10, $11) | Separate dispatch tables | Standard 6809 pattern; cleaner than folding into one 768-entry table |

---

## 14. Confidence & Caveats

**Confidence: 0.9**

The pattern is directly transferable from fam65xx and Z80. The 6809 is well-documented and the trait axes map cleanly.

**Key uncertainties:**

- **Cycle-exact E/Q timing for MC6809E:** The external clock variant has subtleties around TSC tri-stating and BUSY signaling during double-byte operations. Real hardware timing diagrams needed during Phase 3.
- **HD6309 native mode cycle counts:** Some sources disagree on exact cycle counts when NM bit is set. Need to cross-reference multiple sources (Sean Riddle's research, the HD6309 technical reference).
- **TFM interruptibility:** Block transfer is interruptible, but the exact behavior on interrupt (does W save to stack? is it re-entrant?) needs verification against hardware tests.
- **Division edge cases:** DIVD/DIVQ behavior on divide-by-zero and overflow (sets V flag, result undefined) — need to match HD6309 silicon behavior.

**How to improve:** Once Phase 2 is complete, run against Motorola's published instruction timing tables and any available 6809 test suites (e.g., test programs from the CoCo community).

The pattern is directly transferable from fam65xx and Z80. The 6809 is well-documented and the trait axes map cleanly.

Key uncertainties:

Cycle-exact E/Q timing for MC6809E: The external clock variant has subtleties around TSC tri-stating and BUSY signaling during double-byte operations. Real hardware timing diagrams needed during Phase 3.
HD6309 native mode cycle counts: Some sources disagree on exact cycle counts when NM bit is set. Need to cross-reference multiple sources (Sean Riddle's research, the HD6309 technical reference).
TFM interruptibility: Block transfer is interruptible, but the exact behavior on interrupt (does W save to stack? is it re-entrant?) needs verification against hardware tests.
Division edge cases: DIVD/DIVQ behavior on divide-by-zero and overflow (sets V flag, result undefined) — need to match HD6309 silicon behavior.
How to improve: Once Phase 2 is complete, run against Motorola's published instruction timing tables and any available 6809 test suites (e.g., test programs from the CoCo community).