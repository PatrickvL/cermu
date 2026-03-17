# AY-3-8910 Family — NTTP Expansion Plan

## Current State

The existing `ay_3_8910_t` is a well-structured ~385-line header-only chip with runtime variant selection via `AYVariant` enum + `AYVariantTraits[]` lookup table. It handles four variants (8910/8912/8913/YM2149) but the variant only affects `ChipInfo` metadata at construction time — I/O port count and envelope precision are **declared in traits but never actually branched on** in the implementation. Every instance carries two I/O port bytes and a 16-step envelope regardless of variant.

This works fine for the three systems currently using it (Bomb Jack, Spectrum 128K, Amstrad CPC), but won't scale cleanly to the full family.

## Target Chip List

| Chip | Notes |
|------|-------|
| AY-3-8910 | 2 I/O ports — MSX1, Amstrad CPC, ZX Spectrum 128 |
| AY-3-8912 | 1 I/O port — various |
| AY-3-8913 | No I/O ports — compact embed variant |
| AY-3-8914 | Reshuffled registers — Intellivision |
| YM2149 (SSG) | Yamaha clone — MSX, Atari ST, used alongside FM chips |
| YM3439 | CMOS YM2149, some late MSX and arcade boards |
| AY8930 | Enhanced clone — adds per-channel envelope, noise period extension |

## Variant Differences

| Variant | What's different |
|---------|-----------------|
| **AY-3-8914** | Registers 0x00–0x0D are reshuffled (Intellivision mapping) |
| **YM2149/YM3439** | Internal ÷2 clock divider absent (chip expects pre-divided clock); 32-step envelope (half-step precision) |
| **AY8930** | Per-channel envelope, extended noise period (8-bit), duty cycle control, bank-switched extended registers |

The AY-3-8914's register remap and the AY8930's per-channel envelope + extended regs are not "runtime if-check" differences — they fundamentally change how `write_register` and `tick()` behave. Template-time dispatch is warranted.

## Template vs Runtime: The Decision

The `fam65xx` CPU family is the project gold standard for NTTP-based chip variants: `template <const CPUTraits& Traits> class fam65xx_t` with `if constexpr` for zero-overhead feature gating, `inline constexpr` trait instances, and per-variant wrapper headers with type aliases. The Z80 family follows the same pattern. The AY family should too.

The SN76489 uses runtime variant selection (like AY currently does), but its variant differences are trivial (just noise LFSR tap masks). The AY family has structural differences that justify compile-time dispatch.

## Trait Axes

| Axis | Type | Values | Affects |
|------|------|--------|---------|
| `io_port_count` | `uint8_t` | 0, 1, 2 | Whether I/O port reads/writes are active; pin count for GUI layout |
| `clock_divider` | `uint8_t` | 1 or 2 | Whether the chip internally divides its input clock (AY=÷2, YM=÷1). Informational for systems. |
| `envelope_steps` | `uint8_t` | 16, 32 | AY=16 levels (4-bit), YM=32 levels (5-bit half-step) |
| `register_map` | `RegisterMap` enum | `STANDARD`, `INTELLIVISION` | Whether register addresses pass through a remap table |
| `extended_mode` | `bool` | false, true | AY8930: per-channel envelope, extended noise, duty cycle, bank register |

The first four axes are simple and cheap — constexpr values, zero overhead. The fifth (`extended_mode`) is the structural break: the AY8930 adds state (per-channel envelope counters, duty cycle counters, bank register) and significantly changes `tick()`.

## Proposed Traits Struct

Following the `CPUTraits` gold standard — NTTP via `const&` reference, `inline constexpr` instances, helper methods:

```cpp
enum class AYRegisterMap : uint8_t {
    STANDARD,       // AY-3-8910/8912/8913, YM2149, YM3439
    INTELLIVISION,  // AY-3-8914 — shuffled register indices
};

struct AYTraits {
    const char*    vendor;
    const char*    chip_id;
    uint8_t        io_port_count;    // 0, 1, or 2
    uint8_t        clock_divider;    // 1 (YM) or 2 (AY internal ÷2)
    uint8_t        envelope_steps;   // 16 (AY) or 32 (YM half-step)
    AYRegisterMap  register_map;     // STANDARD or INTELLIVISION
    bool           extended_mode;    // AY8930 enhanced features

    // Helpers
    constexpr bool has_io_port_a() const { return io_port_count >= 1; }
    constexpr bool has_io_port_b() const { return io_port_count >= 2; }
    constexpr bool has_half_step_envelope() const { return envelope_steps == 32; }
    constexpr bool has_register_remap() const {
        return register_map == AYRegisterMap::INTELLIVISION;
    }
    constexpr bool has_extended_mode() const { return extended_mode; }
    constexpr uint8_t envelope_max() const { return envelope_steps - 1; }
};
```

## Concrete Trait Instances

```cpp
// --- Standard GI parts ---
inline constexpr AYTraits AY_3_8910_Traits = {
    "General Instrument", "AY-3-8910",
    2, 2, 16, AYRegisterMap::STANDARD, false
};
inline constexpr AYTraits AY_3_8912_Traits = {
    "General Instrument", "AY-3-8912",
    1, 2, 16, AYRegisterMap::STANDARD, false
};
inline constexpr AYTraits AY_3_8913_Traits = {
    "General Instrument", "AY-3-8913",
    0, 2, 16, AYRegisterMap::STANDARD, false
};
inline constexpr AYTraits AY_3_8914_Traits = {
    "General Instrument", "AY-3-8914",
    2, 2, 16, AYRegisterMap::INTELLIVISION, false
};

// --- Yamaha clones ---
inline constexpr AYTraits YM2149_Traits = {
    "Yamaha", "YM2149",
    2, 1, 32, AYRegisterMap::STANDARD, false
};
inline constexpr AYTraits YM3439_Traits = {
    "Yamaha", "YM3439",
    2, 1, 32, AYRegisterMap::STANDARD, false
};

// --- Enhanced clone ---
inline constexpr AYTraits AY8930_Traits = {
    "Microchip", "AY8930",
    2, 2, 16, AYRegisterMap::STANDARD, true  // extended_mode
};
```

## Template Class Shape

```cpp
template <const AYTraits& Traits>
class ay_psg_t : public SoundChipBase {
    // Register remap applied in write_register / read_register
    static constexpr uint8_t map_reg(uint8_t r) {
        if constexpr (Traits.has_register_remap()) {
            return intellivision_reg_map_[r & 0x0F];
        } else {
            return r & 0x0F;
        }
    }

    // Envelope uses Traits.envelope_max() instead of hardcoded 15
    void advance_envelope() {
        constexpr uint8_t max_level = Traits.envelope_max();
        // ... 16 or 32 step logic
    }

    // I/O ports conditionally compiled
    void set_io_port_a(uint8_t data) {
        if constexpr (Traits.has_io_port_a()) { io_port_a_ = data; }
    }

    // AY8930 extended state only present when needed
    struct ExtendedState {  // only instantiated if extended_mode
        uint8_t duty_cycle[3]{};
        uint8_t env_step_per_ch[3]{};
        // ... per-channel envelope counters, bank register, etc.
    };
    std::conditional_t<Traits.has_extended_mode(), ExtendedState, EmptyBase> ext_;
};
```

## Key `if constexpr` Branch Points

Walking through the current implementation, here's where variant dispatch goes:

1. **`write_register()` / `read_register()`** — Apply `map_reg()` for register remap (AY-3-8914)
2. **`advance_envelope()`** — Use `envelope_max()` (15 vs 31); AY8930 uses per-channel envelope state
3. **`get_sample()`** — AY8930: read per-channel envelope instead of shared; apply duty cycle
4. **`tick()`** — AY8930: tick per-channel envelope counters; handle extended noise period
5. **I/O port accessors** — Dead-code-eliminate for 0-port variants
6. **`read_register()`** — I/O port registers return 0xFF when port doesn't exist (Traits-gated)
7. **Constructor / `init()`** — Skip I/O port initialization for 0-port variants; initialize extended state for AY8930

## File Organization

```
src/chip/sound/ay_psg/
├── ay_psg.hpp                 # Template class + register table
├── ay_psg_traits.hpp          # AYTraits definition + all inline constexpr instances
├── ay_3_8910.hpp              # using AY_3_8910 = ay_psg_t<AY_3_8910_Traits>;
├── ay_3_8912.hpp              # using AY_3_8912 = ay_psg_t<AY_3_8912_Traits>;
├── ay_3_8913.hpp              # using AY_3_8913 = ay_psg_t<AY_3_8913_Traits>;
├── ay_3_8914.hpp              # using AY_3_8914 = ay_psg_t<AY_3_8914_Traits>;
├── ym2149.hpp                 # using YM2149 = ay_psg_t<YM2149_Traits>;
├── ym3439.hpp                 # using YM3439 = ay_psg_t<YM3439_Traits>;
├── ay8930.hpp                 # using AY8930 = ay_psg_t<AY8930_Traits>;
└── ay_psg_gui.cpp             # Chip layouts for all variants (pin counts differ)
```

## Impact on Existing Systems

All three current consumers use the chip through the same API surface:

| System | Current | After |
|--------|---------|-------|
| Bomb Jack | `ay_3_8910_t ay_[3]` | `AY_3_8910 ay_[3]` (type alias, no logic change) |
| Spectrum 128K | `ay_3_8910_t ay_(AYVariant::AY_3_8912)` | `AY_3_8912 ay_` |
| Amstrad CPC | `ay_3_8910_t ay_(AYVariant::AY_3_8912)` | `AY_3_8912 ay_` |

The public API stays identical: `latch_address()`, `write_register()`, `read_register()`, `tick()`, `get_sample()`, I/O port accessors. `WriteOnlySynthAdapter<ay_psg_t<T>, true>` works unchanged because the adapter is already templated on `Chip`.

## DAC Table Consideration

The YM2149's DAC curve differs slightly from the AY-3-8910. The existing table is AY-measured (Matthew Westcott). For full accuracy, `AYTraits` could carry a `dac_model` enum selecting between two tables:

```cpp
enum class AYDACModel : uint8_t { AY, YM };
// In AYTraits:
AYDACModel dac_model;   // AY = logarithmic 16-level, YM = different curve + 32 levels
```

This is a secondary concern — the AY table works well enough for YM2149 in practice, and can be refined later.

---

## Implementation Plan

### Phase 1 — Scaffold (no behavior changes)

1. Create `src/chip/sound/ay_psg/` directory
2. Create `ay_psg_traits.hpp` with `AYTraits` struct + all trait instances (8910, 8912, 8913, 8914, YM2149, YM3439, AY8930)
3. Create `ay_psg.hpp` — move the existing `ay_3_8910_t` implementation into `template <const AYTraits& Traits> class ay_psg_t`, replacing `variant_` runtime checks with `if constexpr` on Traits
4. Create per-variant headers (`ay_3_8910.hpp`, `ay_3_8912.hpp`, etc.) with type aliases
5. Move GUI code to `ay_psg_gui.cpp`, parameterized by pin count from Traits

### Phase 2 — Wire up variant dispatch

6. Register remap — add `intellivision_reg_map_[]` constexpr table, apply in `map_reg()`
7. Envelope precision — parameterize `advance_envelope()` on `Traits.envelope_steps`
8. I/O port gating — conditionally compile port state and accessors
9. Clock divider — expose via `Traits.clock_divider` for systems to use (no internal effect — systems already pass the correct AY clock rate)

### Phase 3 — AY8930 extended mode

10. Define `ExtendedState` struct (per-channel envelope, duty cycle, bank register, extended noise period)
11. Add `if constexpr (Traits.has_extended_mode())` branches in `tick()` and `get_sample()`
12. Bank-switched register access for AY8930's extended register set

### Phase 4 — Update consumers

13. Update Bomb Jack: `ay_3_8910_t` → `AY_3_8910`
14. Update Spectrum 128K: constructor variant param → `AY_3_8912` type
15. Update Amstrad CPC: same
16. Delete old `src/chip/sound/ay_3_8910.hpp` and `ay_3_8910_gui.cpp`
17. Verify build + all three systems run correctly

### Phase 5 — DAC refinement (optional, deferred)

18. Add `AYDACModel` to traits, provide separate YM DAC table
19. Wire into `get_sample()` via `if constexpr`

---

## Open Questions / Caveats

- **AY-3-8914 register remap:** Working from the known Intellivision mapping (registers 0–3 swapped with 4–7, etc.) but should verify against a primary source before implementing
- **AY8930 extended mode:** The riskiest part — the chip's bank register mechanism and per-channel envelope are complex; should be implemented incrementally, ideally with a test ROM or known-good reference output
- **Clock divider trait:** Informational for systems — the chip itself doesn't need to know (systems already configure the correct input clock rate). Exposing it in traits makes system setup self-documenting
- **Template instantiation bloat:** 7 variants × ~400 lines of tick/sample code. In practice this is fine — only instantiated variants get compiled, and these are header-only hot-path functions that benefit from per-variant optimization
- **`WriteOnlySynthAdapter`:** Already templated on `Chip` so it adapts automatically, but system code that stores `ay_3_8910_t*` as a concrete type will need updating

CAVEATS:

AY-3-8914 register remap: I'm working from the known Intellivision mapping (registers 0–3 swapped with 4–7, etc.) but haven't verified against a primary source — worth cross-checking before implementing
AY8930 extended mode is the riskiest part — the chip's bank register mechanism and per-channel envelope are complex; this should be implemented incrementally, ideally with a test ROM or known-good reference output
The clock_divider trait is informational for systems — the chip itself doesn't need to know (systems already configure the correct input clock rate). But exposing it in traits makes system setup self-documenting
POTENTIAL CONCERNS:

Template instantiation bloat: 7 variants × ~400 lines of tick/sample code. In practice this is fine — only instantiated variants get compiled, and these are header-only hot-path functions that benefit from per-variant optimization
WriteOnlySynthAdapter is already templated on Chip so it adapts automatically, but system code that stores ay_3_8910_t* as a concrete type will need updating
The Spectrum system uses a template Traits::has_ay_sound conditional — it creates the AY chip conditionally. This pattern works fine with the new type