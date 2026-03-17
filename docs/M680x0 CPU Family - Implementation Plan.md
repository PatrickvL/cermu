# M680x0 CPU Family — Implementation Plan

> Scaffold by copying `src/chip/cpu/z80/` → `src/chip/cpu/m680x0/`, then replace all Z80-specifics with Motorola 680x0 equivalents.

---

## Phase 0: Scaffold (Copy & Rename)

| Step | Action |
|------|--------|
| 0.1 | Copy `src/chip/cpu/z80/` → `src/chip/cpu/m680x0/` |
| 0.2 | Rename every file: `z80_*` → `m680x0_*`, `zilog_z80*.hpp` → variant headers (`mc68000.hpp`, `mc68010.hpp`, `mc68020.hpp`, etc.), delete `u880.hpp` (no equivalent) |
| 0.3 | Rename `operations/z80_base_ops.inc.hpp` → `m680x0_ops.inc.hpp`, delete `z80_cb_ops.inc.hpp` / `z80_ed_ops.inc.hpp` (Z80 prefix-based; 68k uses different instruction grouping) |
| 0.4 | Global find/replace within the new folder: `z80` → `m680x0`, `Z80` → `M680x0`, `Zilog` → `Motorola`, `z80_t` → `m680x0_t` |
| 0.5 | Update `CMakeLists.txt` to add `m680x0_decoder` library, `m680x0_registry.cpp`, `m680x0_gui.cpp` |

---

## Phase 1: Traits & Types (`m680x0_traits.hpp`, `m680x0_types.hpp`)

### Traits — Replace `Z80Traits` with `M680x0Traits`

| Z80 field | M680x0 replacement |
|-----------|---------------------|
| `vendor` = "Zilog" | `vendor` = "Motorola" |
| `chip_id` = "Z80" | `chip_id` = "68000" / "68010" / "68020" / "68030" / "68040" |
| `core_flags` (NMOS/CMOS/UNDOCUMENTED) | `core_flags` (DATA_BUS_16, DATA_BUS_32, HAS_CACHE, HAS_MMU, HAS_FPU, HAS_BARREL_SHIFTER) |
| `address_bits` = 16 | `address_bits` = 24 (68000/010), 32 (68020+) |
| `max_clock_mhz_x10` | 80 (8 MHz 68000), 125 (12.5 MHz 68010), 160–330 (68020–68040) |
| `Z80PeripheralConfig` | Remove — 68000 is a standalone CPU, peripherals are separate chips |

### Types — Replace Z80 enums

| Z80 enum | M680x0 replacement |
|----------|---------------------|
| `Condition` (NZ/Z/NC/C/PO/PE/P/M) | `Condition` (T/F/HI/LS/CC/CS/NE/EQ/VC/VS/PL/MI/GE/LT/GT/LE) — 16 conditions |
| `Reg8` (B/C/D/E/H/L/A) | Remove — 68k uses uniform D0–D7/A0–A7 indexing |
| `Reg16` (BC/DE/HL/SP) | `DataReg` (D0–D7), `AddrReg` (A0–A7 where A7=USP/SSP) |
| `IntMode` (IM0/1/2) | Remove — 68k has 7-level autovectored/vectored interrupts |
| `Flags` (C/N/PV/X/H/Y/Z/S) | `Flags` (C=0x01, V=0x02, Z=0x04, N=0x08, X=0x10) — CCR bits |

### New: Addressing modes enum `AddrMode`

```
DataRegDirect, AddrRegDirect, AddrRegIndirect, AddrRegPostInc,
AddrRegPreDec, AddrRegDisp, AddrRegIndex, AbsShort, AbsLong,
PCDisp, PCIndex, Immediate
```

### New: Operation sizes enum `OpSize`

```
Byte = 0, Word = 1, Long = 2
```

---

## Phase 2: Registers (`m680x0_registers.inc.hpp`)

Replace the Z80 register file entirely:

| Z80 | M680x0 |
|-----|--------|
| `union { f, a; af; }` etc. | `uint32_t d[8]` — D0–D7 (data registers, full 32-bit) |
| `bc, de, hl` | `uint32_t a[8]` — A0–A7 (address registers, 32-bit; A7 = active SP) |
| `af_, bc_, de_, hl_` (shadow) | Remove — no shadow registers in 68k |
| `ix, iy` | Remove — 68k has A0–A6 address registers (no dedicated index regs) |
| `sp` (16-bit) | `uint32_t usp` (User SP), `uint32_t ssp` (Supervisor SP); A7 aliases active one |
| `pc` (16-bit) | `uint32_t pc` (24-bit effective on 68000, 32-bit on 68020+) |
| `i` (int vector page) | `uint32_t vbr` (Vector Base Register, 68010+; 0 on 68000) |
| `r` (refresh counter) | Remove |
| `im` (interrupt mode) | Remove — replaced by SR interrupt mask |
| `iff1, iff2` | `uint16_t sr` — full Status Register (T1/T0/S/M/I2/I1/I0 + CCR) |
| `wz` (MEMPTR) | Remove |
| `q` (flag tracker) | Remove |

### New registers needed

- `uint16_t sr` — Status register (supervisor byte + CCR)
- `uint32_t vbr` — Vector Base Register (68010+)
- `uint32_t sfc, dfc` — Source/Dest Function Codes (68010+)
- `uint32_t cacr, caar` — Cache control (68020+)
- `uint32_t msp, isp` — Master/Interrupt stack pointers (68020+)
- `uint16_t irc, ir, ird` — Instruction pipeline registers (prefetch)
- Internal state: `uint32_t eat` (effective address temp), `uint16_t opcode` (current instruction word)

---

## Phase 3: ALU (`m680x0_alu.inc.hpp`)

Replace Z80 ALU entirely:

| Z80 | M680x0 |
|-----|--------|
| 8-bit focused (sz53/parity tables) | Size-aware: `.B` (8), `.W` (16), `.L` (32) |
| Half-carry / Parity | Remove — not in 68k CCR |
| Precomputed flag tables | Size-parameterized flag computation (N/Z/V/C/X per operation) |

### New ALU operations (all taking `OpSize` parameter)

- `add`, `sub`, `cmp`, `and_op`, `or_op`, `eor` — standard arithmetic/logic
- `mulu`, `muls` — 16×16→32 multiply
- `divu`, `divs` — 32÷16→16q:16r divide
- `asl`, `asr`, `lsl`, `lsr`, `rol`, `ror`, `roxl`, `roxr` — shifts/rotates
- `btst`, `bset`, `bclr`, `bchg` — bit manipulation
- `neg`, `negx`, `not_op`, `clr`, `ext`, `swap`, `tst`
- `abcd`, `sbcd`, `nbcd` — BCD arithmetic
- `addx`, `subx` — extended arithmetic (uses X flag)

---

## Phase 4: Instruction Decoder / Execution Engine (`m680x0.hpp`, `operations/`)

The Z80 uses an 8-bit opcode byte with prefix tables (CB/ED/DD/FD). The 68000 uses a **16-bit instruction word** with a very different encoding scheme.

### Opcode decoding

Replace Z80's `x/y/z/p/q` bit field extraction with 68k's structure:

- Bits 15–12: instruction group
- Bits 11–6: operation mode / register / opmode
- Bits 5–0: effective address mode + register

### Instruction groups (by bits 15–12)

| Group | Hex | Operations |
|-------|-----|------------|
| 0000 | 0x0 | Bit manipulation / MOVEP / Immediate |
| 0001 | 0x1 | MOVE.B |
| 0010 | 0x2 | MOVE.L / MOVEA.L |
| 0011 | 0x3 | MOVE.W / MOVEA.W |
| 0100 | 0x4 | Miscellaneous (LEA, PEA, CHK, EXT, SWAP, TAS, TST, TRAP, LINK, UNLK, MOVEM, JSR, JMP, etc.) |
| 0101 | 0x5 | ADDQ / SUBQ / Scc / DBcc |
| 0110 | 0x6 | Bcc / BSR / BRA |
| 0111 | 0x7 | MOVEQ |
| 1000 | 0x8 | OR / DIV / SBCD |
| 1001 | 0x9 | SUB / SUBA / SUBX |
| 1010 | 0xA | (A-line: unimplemented, traps) |
| 1011 | 0xB | CMP / CMPA / CMPM / EOR |
| 1100 | 0xC | AND / MUL / ABCD / EXG |
| 1101 | 0xD | ADD / ADDA / ADDX |
| 1110 | 0xE | Shift / Rotate |
| 1111 | 0xF | (F-line: coprocessor / unimplemented) |

### Execution model change

- Z80: T-state granularity (step counter increments each clock)
- 68000: **Micro-cycle** model — each bus cycle is 4 clocks minimum. Internal operations take 2n clocks.
- The step counter approach still works but represents micro-operations, not T-states.

### Operation files

Instead of Z80's prefix-based split, organize by instruction group:

```
operations/
├── m680x0_move_ops.inc.hpp        # MOVE, MOVEA, MOVEQ, MOVEM, MOVEP
├── m680x0_arith_ops.inc.hpp       # ADD, SUB, CMP, MUL, DIV, NEG, CLR
├── m680x0_logic_ops.inc.hpp       # AND, OR, EOR, NOT
├── m680x0_shift_ops.inc.hpp       # ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR
├── m680x0_bit_ops.inc.hpp         # BTST, BSET, BCLR, BCHG
├── m680x0_branch_ops.inc.hpp      # Bcc, BRA, BSR, DBcc, Scc
├── m680x0_control_ops.inc.hpp     # JMP, JSR, RTS, RTE, RTR, TRAP, NOP, STOP, RESET
├── m680x0_misc_ops.inc.hpp        # LEA, PEA, CHK, LINK, UNLK, EXG, EXT, SWAP, TAS
├── m680x0_bcd_ops.inc.hpp         # ABCD, SBCD, NBCD
├── m680x0_ea_ops.inc.hpp          # Effective address calculation (shared by all)
└── inc_lint_prevention.hpp        # Reuse from Z80
```

---

## Phase 5: Bus Signals & Pin Definitions

Replace Z80 bus bits:

| Z80 signal (bit) | M680x0 signal (bit) | Notes |
|-------------------|----------------------|-------|
| `Z80_MREQ_BIT` (55) | `M68K_AS_BIT` | Address Strobe (active-low) |
| `Z80_IORQ_BIT` (56) | `M68K_LDS_BIT` / `M68K_UDS_BIT` | Lower/Upper Data Strobe |
| `Z80_M1_BIT` (57) | `M68K_FC0_BIT` / `M68K_FC1_BIT` / `M68K_FC2_BIT` | Function Code (3 bits) |
| `Z80_RFSH_BIT` (58) | Remove | |
| `Z80_HALT_BIT` (59) | `M68K_HALT_BIT` | |
| `Z80_WAIT_BIT` (60) | `M68K_DTACK_BIT` | Data Transfer Acknowledge |
| `BUS_RW_BIT` | `M68K_RW_BIT` | Read/Write (reuse standard) |
| — | `M68K_BR_BIT` | Bus Request |
| — | `M68K_BG_BIT` | Bus Grant |
| — | `M68K_BGACK_BIT` | Bus Grant Acknowledge |
| — | `M68K_BERR_BIT` | Bus Error |
| — | `M68K_IPL0_BIT` / `M68K_IPL1_BIT` / `M68K_IPL2_BIT` | Interrupt Priority Level (3 lines) |
| — | `M68K_VPA_BIT` | Valid Peripheral Address (auto-vector) |
| — | `M68K_E_BIT` | Enable (6800 peripheral clock) |
| — | `M68K_VMA_BIT` | Valid Memory Address |

### Address/data bus widths

- 68000: 23-bit address (A1–A23), 16-bit data (D0–D15), A0 implicit from UDS/LDS
- 68020+: 32-bit address, 32-bit data (future)

---

## Phase 6: Disassembler (`m680x0_decoder.hpp` / `.cpp`)

Complete rewrite — 68k disassembly is fundamentally different from Z80.

### String tables

```cpp
const char* dn_names[8] = {"D0","D1","D2","D3","D4","D5","D6","D7"};
const char* an_names[8] = {"A0","A1","A2","A3","A4","A5","A6","A7"};
const char* cc_names[16] = {"T","F","HI","LS","CC","CS","NE","EQ",
                            "VC","VS","PL","MI","GE","LT","GT","LE"};
const char* size_suffixes[3] = {".B",".W",".L"};
```

### EA formatting

Each effective address mode has its own format string:

| Mode | Format |
|------|--------|
| `Dn` | `"D0"–"D7"` |
| `An` | `"A0"–"A7"` |
| `(An)` | `"(A0)"` |
| `(An)+` | `"(A0)+"` |
| `-(An)` | `"-(A0)"` |
| `(d16,An)` | `"($1234,A0)"` |
| `(d8,An,Xi)` | `"($12,A0,D0.W)"` |
| `$xxxx.W` | absolute short |
| `$xxxxxxxx.L` | absolute long |
| `(d16,PC)` | `"($1234,PC)"` |
| `(d8,PC,Xi)` | `"($12,PC,D0.W)"` |
| `#imm` | `"#$1234"` |

### Instruction word decoding

Must read 1–5 words (2–10 bytes) per instruction, with extension words for EAs.

---

## Phase 7: Mini Assembler (`m680x0_asm.hpp`)

Rewrite with 68k instruction emission methods:

- Big-endian word emission (68k is big-endian, unlike Z80)
- EA encoding helpers
- Methods like `move_b_imm_d0(uint8_t)`, `jsr(uint32_t)`, `rts()`, `bra(int16_t)`, etc.

---

## Phase 8: GUI / Chip Layout (`m680x0_gui.cpp`)

**Package**: 68000 = **64-pin DIP** (vs Z80's 40-pin). 68020 = 114-pin PGA. Start with 68000 DIP-64.

### 68000 Pinout (DIP-64)

```
Pin  1: D4          Pin 64: D5
Pin  2: D3          Pin 63: D6
Pin  3: D2          Pin 62: D7
Pin  4: D1          Pin 61: D8
Pin  5: D0          Pin 60: D9
Pin  6: /AS         Pin 59: D10
Pin  7: /UDS        Pin 58: D11
Pin  8: /LDS        Pin 57: D12
Pin  9: R/W         Pin 56: D13
Pin 10: /DTACK      Pin 55: D14
Pin 11: /BG         Pin 54: D15
Pin 12: /BGACK      Pin 53: GND
Pin 13: /BR         Pin 52: A23
Pin 14: VCC         Pin 51: A22
Pin 15: CLK         Pin 50: A21
Pin 16: GND         Pin 49: VCC
Pin 17: /HALT       Pin 48: A20
Pin 18: /RESET      Pin 47: A19
Pin 19: /VMA        Pin 46: A18
Pin 20: E           Pin 45: A17
Pin 21: /VPA        Pin 44: A16
Pin 22: /BERR       Pin 43: A15
Pin 23: /IPL2       Pin 42: A14
Pin 24: /IPL1       Pin 41: A13
Pin 25: /IPL0       Pin 40: A12
Pin 26: FC2         Pin 39: A11
Pin 27: FC1         Pin 38: A10
Pin 28: FC0         Pin 37: A9
Pin 29: A1          Pin 36: A8
Pin 30: A2          Pin 35: A7
Pin 31: A3          Pin 34: A6
Pin 32: A4          Pin 33: A5
```

---

## Phase 9: Variant Headers

| File | Variant | Clock | Address | Data | Notes |
|------|---------|-------|---------|------|-------|
| `mc68000.hpp` | MC68000 | 8 MHz | 24-bit | 16-bit | Original (1979) |
| `mc68008.hpp` | MC68008 | 8 MHz | 20/22-bit | 8-bit | Cost-reduced |
| `mc68010.hpp` | MC68010 | 8 MHz | 24-bit | 16-bit | Virtual memory, loop mode |
| `mc68020.hpp` | MC68020 | 16–33 MHz | 32-bit | 32-bit | Full 32-bit, cache, coprocessor |
| `mc68030.hpp` | MC68030 | 16–50 MHz | 32-bit | 32-bit | On-chip MMU |
| `mc68040.hpp` | MC68040 | 25–40 MHz | 32-bit | 32-bit | On-chip FPU, dual cache |

---

## Phase 10: Registry & Test Scaffold (`m680x0_registry.cpp`, test runner)

- Register all variants with `REGISTER_CHIP_TYPE`
- Create `tests/m680x0_processor_tests_runner.cpp` skeleton (can hook into existing 68k test suites later)

---

## Phase 11: CMakeLists.txt Integration

```cmake
# M680x0 Decoder library
add_library(m680x0_decoder STATIC
    src/chip/cpu/m680x0/m680x0_decoder.cpp
)

# Add to CHIP_SOURCES
set(CHIP_SOURCES
    ...
    src/chip/cpu/m680x0/m680x0_registry.cpp
)

# Add to GUI_SOURCES
set(GUI_SOURCES
    ...
    src/chip/cpu/m680x0/m680x0_gui.cpp
)

# Link
target_link_libraries(cermu_console ... m680x0_decoder)
```

---

## Execution Order

| Order | Phase | Effort | Depends On |
|-------|-------|--------|------------|
| 1 | Phase 0 — Copy & rename | Low | — |
| 2 | Phase 1 — Traits & Types | Medium | Phase 0 |
| 3 | Phase 2 — Register file | Medium | Phase 1 |
| 4 | Phase 5 — Bus signals & pins | Medium | Phase 1 |
| 5 | Phase 3 — ALU | High | Phase 2 |
| 6 | Phase 4 — Instruction decoder/execution | **Very High** | Phases 2, 3, 5 |
| 7 | Phase 6 — Disassembler | High | Phase 1 |
| 8 | Phase 7 — Mini assembler | Medium | Phase 6 |
| 9 | Phase 8 — GUI/Layout | Low–Medium | Phase 5 |
| 10 | Phase 9 — Variant headers | Low | Phase 1 |
| 11 | Phase 10 — Registry & tests | Low | Phase 9 |
| 12 | Phase 11 — CMake integration | Low | All |

---

## Key Architectural Differences (Z80 → 68000)

1. **Endianness**: Z80 is little-endian; 68000 is **big-endian** — affects all multi-byte loads/stores
2. **Word-aligned access**: 68000 requires word-aligned access for `.W`/`.L` (bus error on odd address) — Z80 has no alignment requirements
3. **Prefetch pipeline**: 68000 has a 2-word prefetch queue — must model `IRC`/`IR`/`IRD` for cycle accuracy
4. **Supervisor/User mode**: 68000 has dual stack pointers and privilege levels — Z80 has none
5. **Exception processing**: 68000 has a rich exception model (bus error, address error, illegal instruction, privilege violation, trace, etc.) with a vector table — much more complex than Z80's IM0/1/2
6. **Bus arbitration**: 68000 has a 3-wire protocol (BR/BG/BGACK) vs Z80's 2-wire (BUSREQ/BUSACK)
7. **No I/O space**: 68000 is memory-mapped I/O only — Z80's IORQ concept doesn't exist