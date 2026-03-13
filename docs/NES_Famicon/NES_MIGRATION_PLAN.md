# NES / Famicom System Migration Plan

## Goal

Restructure the NES/Famicom emulation from a monolithic `nes_system.cpp` + `nes_system.h` into a proper chip-per-type, object-oriented architecture where:

1. Each chip is a separate `ChipBase`-derived type in its own header
2. Two `bus_state_t` words model the CPU bus and PPU bus independently (matching hardware)
3. A flat memory + bank map replaces cascading if-else dispatch (C64 pattern)
4. Hardware-accurate pin layouts live in `_gui` files, compiled only when `CERMU_HAS_GUI` is defined
5. Code is header-only where possible, with limited visibility (`private`/`protected`)
6. No global or free functions — everything lives inside types
7. Mapper implementations are individually compilable, not nested private classes
8. Entities reusable across systems live in shared folders (`src/chip/`, `src/ports/`, `src/devices/`), not under any system
9. PPU bus shares bit positions with CPU bus for bridging signals — enables zero-cost bitmixing
10. Edge detection (A12, NMI) uses `bus_snapshot_` comparison, not callbacks

---

## Part 0 — Architecture Rationale

### Why Two Bus States?

The real NES has **two electrically independent buses**:

| Bus | Width | Clock | Devices |
|-----|-------|-------|---------|
| **CPU bus** | 16-bit addr + 8-bit data | ~1.79 MHz (M2) | 2A03 CPU/APU, 2KB WRAM, controller shift registers, cartridge PRG side |
| **PPU bus** | 14-bit addr + 8-bit data | ~5.37 MHz (PPU /RD, /WR) | RP2C02 PPU, cartridge CHR side, 2KB CIRAM |

The cartridge connector exposes **both buses simultaneously** — signals like `/ROMSEL`, `CIRAM A10`, `IRQ`, and `A12` bridge the two. Currently the codebase uses a single `bus_state_t pins_` for the CPU bus and separate `ppu_read()`/`ppu_write()` functions for the PPU bus. The migration makes the PPU bus an explicit `bus_state_t` too, enabling:

- Mappers that monitor PPU A12 (MMC3) to see it as a real signal on `ppu_bus_`
- Cycle-accurate CHR-ROM/RAM banking visible in the bus snapshot
- Future VS. System PPU variants and expansion audio on the CPU bus
- Pin-accurate layout rendering for both buses in the GUI

### Why a Unified Buffer + Bank Map?

The current `MemoryBus::mem_tick()` uses cascading `if (addr <= 0x1FFF) ... else if (addr <= 0x3FFF) ...` with virtual calls into `Cartridge::cpu_bus_tick()`, which dispatches through mapper virtual functions. This produces:

- **4+ branches** per memory access (hot path)
- **2 virtual calls** for cartridge reads (cpu_bus_tick → mapper::cpu_map_read)
- A separate `ppu_read()` path that re-dispatches through mapper virtuals

The C64 system solved this with a flat mem + precomputed bank tables:

```
offset = chip_id << PAGE_SHIFT
mask   = PAGE_MASK | -(chip == CHIP_RAM)
result = flat_mem[offset + (addr & mask)]
```

Zero branches for RAM/ROM reads. I/O and register pages dispatch through direct function pointers. Mode switches are a 16-byte `memcpy`.

For the NES, we adapt this pattern to handle **mapper bank switching** — the bank tables are regenerated whenever the mapper updates its internal registers, rather than being static per PLA mode.

---

## Part 1 — Target Directory Structure

```
src/systems/nes/
├── CMakeLists.txt                    # Build — lists all .cpp sources
├── README.md                         # Architecture overview
│
├── bus/                              # Bus infrastructure
│   ├── nes_bus.h                     #   nes_bus_t — flat mem, bank map, mem_tick
│   ├── nes_bus.cpp                   #   Bus implementation, bank table generation
│   ├── nes_bus_chips.h               #   CHIP ID enum (strategic numbering)
│   └── nes_bus_signals.h             #   NES-specific bus_state_t bit definitions,
│                                     #   PPU bus_state_t typedef + bitmix macros
│
├── cpu/                              # CPU (Ricoh 2A03 / 2A07)
│   └── nes_cpu.h                     #   NES CPU wrapper: 2-phase tick, IRQ/NMI,
│                                     #   DMA controller, audio sample generation
│                                     #   (header-only; fam65xx_t<> is the core)
│
├── ppu/                              # PPU (Ricoh RP2C02 / RP2C07)
│   ├── nes_ppu.h                     #   PPU : ChipBase — registers, rendering state
│   ├── nes_ppu.cpp                   #   clock(), bus tick, sprite eval, shifters
│   ├── nes_ppu_palette.h             #   Static NES palette (64-color LUT, header-only)
│   └── nes_ppu_gui.cpp               #   ImGui debug/settings/layout (ChipBase virtuals)
│                                     #   RP2C02 40-pin DIP layout (#ifdef CERMU_HAS_GUI)
│
├── apu/                              # APU (built into 2A03, GUI separate)
│   └── nes_apu_gui.cpp               #   ImGui debug/settings/layout for APU chip
│                                     #   RP2A03 APU-focused layout (#ifdef CERMU_HAS_GUI)
│
├── cartridge/                        # Cartridge + mapper hierarchy
│   ├── nes_cartridge.h               #   Cartridge : ChipBase — ROM/RAM buffers,
│   │                                 #   iNES header, SRAM, mapper dispatch
│   ├── nes_cartridge.cpp             #   load_from_buffer(), bus_tick(), SRAM I/O
│   ├── nes_cartridge_gui.cpp         #   Cartridge debug/layout (#ifdef CERMU_HAS_GUI)
│   ├── nes_mapper.h                  #   Mapper base class (abstract interface)
│   │
│   ├── mappers/                      #   One header per mapper (header-only)
│   │   ├── mapper_000_nrom.h         #     NROM — passive, no banking
│   │   ├── mapper_001_mmc1.h         #     MMC1 — serial shift register
│   │   ├── mapper_002_uxrom.h        #     UxROM — discrete PRG switching
│   │   ├── mapper_003_cnrom.h        #     CNROM — discrete CHR switching
│   │   ├── mapper_004_mmc3.h         #     MMC3 — A12 scanline IRQ
│   │   ├── mapper_005_mmc5.h         #     MMC5 — ExRAM, split screen, audio
│   │   ├── mapper_007_axrom.h        #     AxROM — 32KB PRG + single-screen
│   │   ├── mapper_009_mmc2.h         #     MMC2 — CHR auto-latch
│   │   ├── mapper_024_vrc6.h         #     VRC6 — pulse + sawtooth audio
│   │   ├── mapper_069_fme7.h         #     FME-7/5B — AY-3-8910 audio
│   │   ├── mapper_085_vrc7.h         #     VRC7 — YM2413 FM synthesis
│   │   └── ...                       #     (one file per mapper, added incrementally)
│   │
│   └── nes_mapper_factory.h          #   create_mapper(id) → unique_ptr<Mapper>
│
├── nsf/                              # NSF music player
│   ├── nes_nsf_cartridge.h           #   NsfCartridge : Cartridge — bankswitch ROM
│   ├── nes_nsf_player.h              #   NSF load/switch/info routines
│   └── nes_nsf_player.cpp            #   NSF player implementation
│
├── screen/                           # Text/tile utilities
│   ├── nes_screen_utils.h            #   Font upload, text rendering (header-only)
│   └── nes_screen_utils.cpp          #   Implementation
│
├── nes_system.h                      # NintendoSystem<V> : EmulatedSystem
└── nes_system.cpp                    # System lifecycle, tick loop, file loading

src/chip/input/                       # Cross-system input chips
├── cd4021.h                          #   CD4021 PISO shift register : ChipBase
│                                     #   (header-only; used by NES, SNES, etc.)
└── cd4021_gui.cpp                    #   CD4021 16-pin DIP layout + debug
│                                     #   (#ifdef CERMU_HAS_GUI)

src/ports/                       # Cross-system connector definitions
└── nes_ports.h                  #   NES 72-pin, FC 60-pin, controller 7-pin,
                                      #   FC 15-pin expansion, VS. System ports
                                      #   PortDefinition constants (header-only)

src/devices/input/                    # Cross-system peripheral devices
├── nes_standard_controller.h         #   NES standard gamepad : PeripheralDevice
├── nes_standard_controller.cpp       #   Host input binding + shift register protocol
├── nes_zapper.h                      #   NES Zapper light gun : PeripheralDevice
├── nes_zapper.cpp                    #   Reads PPU pixel under reticle for detection
├── nes_arkanoid_controller.h         #   Vaus paddle controller : PeripheralDevice
└── nes_arkanoid_controller.cpp       #   Paddle position via potentiometer protocol
```

> **Cross-system rule**: Any entity that appears in more than one system lives
> in a shared folder, never under a system-specific directory:
>
> | Entity type | Shared location | Layout |
> |-------------|-----------------|--------|
> | Chips | `src/chip/<category>/` | `cpu/`, `io/`, `input/`, `video/`, `sound/`, `logic/`, `memory/` |
> | Port definitions | `src/ports/` | One header per port family |
> | Peripheral devices | `src/devices/<category>/` | `input/`, `storage/`, etc. |
>
> NES-specific chips (RP2C02, cartridge) stay under `src/systems/nes/`.
> Examples of shared entities:
> - **CD4021** — standard CMOS shift register used in NES, SNES, arcade boards → `src/chip/input/`
> - **NES controller port definitions** — used by NES, Famicom, VS. System, PlayChoice-10 → `src/ports/`
> - **NES Standard Controller** — peripheral device usable on any system with a `CONTROLLER_NES` port → `src/devices/input/`

### Design Principles

| Principle | Rationale |
|-----------|-----------|
| **One chip = one header** | Each hardware chip (2A03, 2C02, cartridge) is a self-contained `ChipBase` subclass. The header defines the type and bus interface. Layouts are NOT in the chip header — they live in `_gui` files. |
| **Layouts in `_gui` files, `CERMU_HAS_GUI` guarded** | All `ChipLayout` definitions and pin-state rendering live in files with a `_gui` suffix (e.g. `nes_ppu_gui.cpp`). Everything layout-related is compiled only when `#ifdef CERMU_HAS_GUI`. This keeps non-GUI builds (test runners, headless) free of ImGui dependencies. Matches the existing convention in VIC-II, TED, MOS6522, etc. |
| **Cross-system entities in shared folders** | Any entity used by multiple systems lives in a shared folder: chips in `src/chip/<category>/`, connector definitions in `src/ports/`, peripheral devices in `src/devices/<category>/`. Only system-specific chips (RP2C02, cartridge) stay in `src/systems/nes/`. |
| **Header-only where side-effect-free** | Palette LUTs, bus signal definitions, mapper bank logic, controller shift register — all compile down to inline code or constexpr data. |
| **`.cpp` only for rendering + I/O** | ImGui rendering, file I/O, and the system tick loop need translation units. The hot-path memory dispatch lives in the header via `inline` / `__attribute__((always_inline))`. |
| **Edge detection via `bus_snapshot_`** | All edge-sensitive signals (PPU A12 for MMC3, NMI for CPU) are detected by comparing the current bus state against `bus_snapshot_` — a copy stored at the end of each tick. No callbacks, no state machines in the mapper. Already proven in ChipBase (VIC-II, TED, MOS6522). |
| **Shared bus bits are position-aligned** | Signals that bridge CPU↔PPU buses (/IRQ, /RES) occupy the same bit index in both `bus_state_t` and `ppu_bus_state_t`. This enables direct bitmixing: `cpu |= ppu & SHARED_MASK`. |
| **No free functions** | Everything is a static member or a method. Namespace-level constants are `inline constexpr`. |
| **Mappers are not nested classes** | Each mapper is a standalone type in its own header, inheriting from `Mapper`. The factory instantiates by ID. This enables incremental addition and per-mapper testing. |

---

## Part 2 — Unified Memory Buffer for NES

### CHIP ID Numbering (Strategic)

Following the C64 pattern, CHIP IDs encode buffer offsets via `chip << PAGE_SHIFT`:

```cpp
// nes_bus_chips.h — NES chip IDs for flat mem addressing
//
// Buffer layout (PAGE_SHIFT = 11, 2KB pages):
//
//   CHIP_WRAM      = 0   →  offset 0x0000   (2KB CPU WRAM)
//   CHIP_CIRAM     = 1   →  offset 0x0800   (2KB PPU nametable VRAM)
//   CHIP_PALETTE   = 2   →  offset 0x1000   (32 bytes PPU palette, padded to 2KB page)
//   CHIP_PRG_RAM   = 3   →  offset 0x1800   (8KB cartridge WRAM, extends 4 pages)
//   CHIP_CHR_RAM   = 7   →  offset 0x3800   (8KB CHR-RAM, extends 4 pages)
//   CHIP_PRG_ROM   = 11  →  offset 0x5800   (up to 512KB, mapped via bank pointers)
//   CHIP_CHR_ROM   = 12  →  offset 0x6000   (up to 512KB, mapped via bank pointers)
//
// For PRG-ROM and CHR-ROM, we do NOT store them in the flat mem
// (they can be megabytes). Instead the bank map points into the cartridge's
// own ROM vector. Only WRAM, CIRAM, palette, PRG-RAM, and CHR-RAM live
// in the flat mem.
//
// The bank map tables use 8-bit entries where each entry is either:
//   - A pointer index into a page_pointers[] array (for ROM banks)
//   - A CHIP ID + page offset (for fixed RAM regions)

enum nes_chip_id_t : uint8_t {
    // Fixed regions in flat mem
    NES_CHIP_WRAM       = 0,    // 2KB CPU internal RAM
    NES_CHIP_CIRAM      = 1,    // 2KB PPU internal VRAM (nametables)
    NES_CHIP_PALETTE    = 2,    // 32B palette RAM (page-padded)
    NES_CHIP_PRG_RAM    = 3,    // 8KB cartridge work RAM ($6000-$7FFF)
    NES_CHIP_CHR_RAM    = 7,    // 8KB CHR-RAM (PPU pattern tables)

    // Bank-mapped (pointer-based, not in flat mem)
    NES_CHIP_PRG_ROM    = 11,   // Cartridge PRG-ROM (variable size)
    NES_CHIP_CHR_ROM    = 12,   // Cartridge CHR-ROM (variable size)

    // Special dispatch
    NES_CHIP_PPU_REGS   = 13,   // PPU register I/O ($2000-$3FFF)
    NES_CHIP_APU_IO     = 14,   // APU + I/O registers ($4000-$401F)
    NES_CHIP_UNMAPPED   = 15,   // Open bus / unmapped
};
```

### CPU Bank Map (16 × 4KB pages covering $0000–$FFFF)

```cpp
struct nes_bus_t {
    // Flat mem — contiguous allocation for all on-board RAM
    // Layout: WRAM(2KB) + CIRAM(2KB) + palette(2KB pad) + PRG-RAM(8KB) + CHR-RAM(8KB)
    // Total: 22KB fixed allocation
    uint8_t* flat_mem;
    uint8_t* allocated_buffer;

    // === CPU address space: 16 × 4KB pages ===
    //
    // Each page is a direct pointer to the backing memory for that 4KB window.
    // For read: cpu_read_page[addr >> 12] + (addr & 0x0FFF)
    // For write: cpu_write_page[addr >> 12] + (addr & 0x0FFF)
    //
    // Pages that map to I/O (PPU regs, APU regs, controller) are nullptr.
    // The hot path: if (page != nullptr) return page[addr & 0x0FFF];
    //               else dispatch_io(addr, bus);
    //
    alignas(64) const uint8_t* cpu_read_page[16];   // Read pointers (ROM or RAM)
    alignas(64) uint8_t* cpu_write_page[16];          // Write pointers (RAM only, nullptr for ROM)

    // === PPU address space: 16 × 1KB pages covering $0000–$3FFF ===
    //
    // PPU has 14-bit address space. We use 1KB granularity because:
    //   - CHR banking is often 1KB (MMC3, VRC4, etc.)
    //   - Nametable mirroring operates at 1KB (CIRAM pages)
    //   - Palette is at $3F00 within the $3000-$3FFF mirror
    //
    alignas(64) const uint8_t* ppu_read_page[16];    // 16 × 1KB = $0000–$3FFF
    alignas(64) uint8_t* ppu_write_page[16];          // nullptr for ROM pages

    // I/O dispatch — function pointers for pages that need register access
    struct io_handler_t {
        bus_state_t (*read)(void* context, bus_state_t bus);
        bus_state_t (*write)(void* context, bus_state_t bus);
        void* context;
    };
    io_handler_t cpu_io_handlers[16];  // One per 4KB CPU page (only pages 0x2-0x4 used)

    // --- Hot path: CPU read ---
    inline uint8_t cpu_read(uint16_t addr) const {
        const uint8_t page = addr >> 12;
        const uint8_t* ptr = cpu_read_page[page];
        if (LIKELY(ptr != nullptr)) {
            return ptr[addr & 0x0FFF];
        }
        // Slow path handled by caller (I/O dispatch)
        return 0xFF; // open bus placeholder
    }

    // --- Hot path: CPU write ---
    inline void cpu_write(uint16_t addr, uint8_t data) {
        const uint8_t page = addr >> 12;
        uint8_t* ptr = cpu_write_page[page];
        if (LIKELY(ptr != nullptr)) {
            ptr[addr & 0x0FFF] = data;
            return;
        }
        // Slow path: ROM writes → mapper register, I/O dispatch
    }
};
```

### Bank Map Regeneration (Mapper Callback)

When a mapper updates its internal bank registers, it calls back into the bus to update page pointers:

```cpp
// Called by mapper after any register write that changes banking
void nes_bus_t::update_cpu_banks(const MapperBankConfig& config) {
    // Pages 0-1: $0000-$1FFF → WRAM (mirrored, always)
    cpu_read_page[0]  = cpu_read_page[1]  = &flat_mem[WRAM_OFFSET];
    cpu_write_page[0] = cpu_write_page[1] = &flat_mem[WRAM_OFFSET];

    // Pages 2-3: $2000-$3FFF → PPU registers (nullptr → I/O dispatch)
    cpu_read_page[2] = cpu_read_page[3] = nullptr;
    cpu_write_page[2] = cpu_write_page[3] = nullptr;

    // Page 4: $4000-$4FFF → APU/IO registers (nullptr → I/O dispatch)
    cpu_read_page[4] = nullptr;
    cpu_write_page[4] = nullptr;

    // Pages 5: $5000-$5FFF → Expansion (mapper-dependent)
    cpu_read_page[5]  = config.expansion_read;
    cpu_write_page[5] = config.expansion_write;

    // Pages 6-7: $6000-$7FFF → PRG-RAM (if enabled)
    if (config.prg_ram_enabled) {
        cpu_read_page[6]  = cpu_read_page[7]  = config.prg_ram_base;
        cpu_write_page[6] = config.prg_ram_write_protected ? nullptr : config.prg_ram_base;
        cpu_write_page[7] = config.prg_ram_write_protected ? nullptr : (config.prg_ram_base + 0x1000);
    } else {
        cpu_read_page[6] = cpu_read_page[7] = nullptr;  // open bus
        cpu_write_page[6] = cpu_write_page[7] = nullptr;
    }

    // Pages 8-15: $8000-$FFFF → PRG-ROM banks (from mapper config)
    for (int i = 0; i < 8; i++) {
        cpu_read_page[8 + i] = config.prg_pages[i];
        cpu_write_page[8 + i] = nullptr;  // ROM writes → mapper register dispatch
    }
}

void nes_bus_t::update_ppu_banks(const MapperChrConfig& config) {
    // 8 × 1KB CHR pages ($0000-$1FFF)
    for (int i = 0; i < 8; i++) {
        ppu_read_page[i]  = config.chr_pages[i];
        ppu_write_page[i] = config.chr_writable[i] ? const_cast<uint8_t*>(config.chr_pages[i]) : nullptr;
    }

    // 4 × 1KB nametable pages ($2000-$2FFF) — mirroring
    for (int i = 0; i < 4; i++) {
        uint8_t* ciram_page = &flat_mem[CIRAM_OFFSET + config.nt_page[i] * 0x400];
        ppu_read_page[8 + i]  = ciram_page;
        ppu_write_page[8 + i] = ciram_page;
    }

    // $3000-$3EFF mirrors $2000-$2EFF (handled identically — same pointers)
    for (int i = 0; i < 4; i++) {
        ppu_read_page[12 + i]  = ppu_read_page[8 + i];
        ppu_write_page[12 + i] = ppu_write_page[8 + i];
    }

    // Palette at $3F00-$3F1F is a special case — intercept in ppu_read/ppu_write
}
```

### Performance Comparison

| Operation | Current (if-else + virtual) | Migrated (page pointer) |
|-----------|---------------------------|------------------------|
| RAM read ($0000) | 2 branches + array index | 1 nullptr check + direct access |
| PRG-ROM read ($8000) | 3 branches + 2 virtuals | 1 nullptr check + direct access |
| PPU register read ($2000) | 2 branches + 1 virtual | 1 nullptr check + function ptr |
| CHR-ROM read (PPU) | 1 branch + 1 virtual | 1 nullptr check + direct access |
| Bank switch | Implicit (re-dispatches each access) | One-time page pointer update |

The page pointer approach converts per-access branching into per-bank-switch pointer updates. Since bank switches happen thousands of times less frequently than memory accesses, this is a massive net win.

---

## Part 3 — PPU Bus as Explicit bus_state_t

### PPU Bus State Definition

The PPU bus is a **separate** 64-bit word from the CPU bus, but signals that bridge
the two (through the cartridge connector) are placed at **identical bit positions**.
This enables direct bitmixing without shifts.

```cpp
// nes_bus_signals.h

// -----------------------------------------------------------------------
// CPU bus_state_t layout reminder (from system_lines.h):
//   Bits 0-7:    data
//   Bits 8-23:   addr (16-bit)
//   Bits 24-31:  bank
//   Bit 32:      /RES   (input, active low)
//   Bit 33:      /IRQ   (input, active low)
//   Bit 34:      /NMI   (input, active low)
//   Bit 35:      RDY
//   Bit 48:      R/W    (output)
//   ...
// -----------------------------------------------------------------------
//
// PPU bus_state_t layout:
//
//   Bits 0-7:    PD0-PD7  (PPU data bus, bidirectional)
//   Bits 8-21:   PA0-PA13 (PPU address bus, 14-bit)
//   Bit 22:      /RD      (PPU read strobe, active low)      — PPU-only
//   Bit 23:      /WR      (PPU write strobe, active low)     — PPU-only
//   Bit 24:      ALE      (address latch enable)              — PPU-only
//   Bit 25:      /A13     (active-low complement of PA13)     — PPU-only
//   Bit 26:      CIRAM /CE                                    — PPU-only
//   Bit 27:      CIRAM A10 (nametable mirroring, cart output) — PPU-only
//   Bit 28:      PA12 (exposed for mapper A12 edge detection) — PPU-only
//
//   === SHARED BITS — same positions as CPU bus_state_t ===
//   Bit 32:      /RES     (system reset, active low)
//   Bit 33:      /IRQ     (cartridge → CPU, active low)
//   Bit 34:      /NMI     (PPU → CPU, active low)
//
//   The shared region [32..34] can be bitmixed:
//     cpu_bus_ = (cpu_bus_ & ~PPU_CPU_SHARED_MASK) | (ppu_bus_ & PPU_CPU_SHARED_MASK);
//
//   This transfers /IRQ asserted by a cartridge mapper (MMC3 scanline IRQ)
//   and /NMI from the PPU directly onto the CPU bus with zero bit-shifting.
//

using ppu_bus_state_t = uint64_t;

// PPU bus field access
#define PPU_BUS_GET_DATA(b)       ((uint8_t)((b) & 0xFF))
#define PPU_BUS_SET_DATA(b, d)    ((b) = ((b) & ~0xFFULL) | ((d) & 0xFF))
#define PPU_BUS_GET_ADDR(b)       ((uint16_t)(((b) >> 8) & 0x3FFF))
#define PPU_BUS_SET_ADDR(b, a)    ((b) = ((b) & ~(0x3FFFULL << 8)) | (((uint64_t)(a) & 0x3FFF) << 8))
#define PPU_BUS_GET_BIT(b, bit)   (((b) >> (bit)) & 1)
#define PPU_BUS_SET_BIT(b, bit)   ((b) |= (1ULL << (bit)))
#define PPU_BUS_CLR_BIT(b, bit)   ((b) &= ~(1ULL << (bit)))

// PPU-only pin bit positions (22-28)
#define PPU_BUS_RD_BIT    22
#define PPU_BUS_WR_BIT    23
#define PPU_BUS_ALE_BIT   24
#define PPU_BUS_A13N_BIT  25
#define PPU_BUS_CIRAM_CE  26
#define PPU_BUS_CIRAM_A10 27
#define PPU_BUS_PA12_BIT  28

// Shared pin bit positions (SAME as CPU bus — from system_lines.h)
// PPU_BUS_RES_BIT = BUS_RES_BIT = 32
// PPU_BUS_IRQ_BIT = BUS_IRQ_BIT = 33
// PPU_BUS_NMI_BIT = BUS_NMI_BIT = 34

// Bitmix mask — covers all signals that bridge CPU ↔ PPU buses
// Through the cartridge 72-pin connector: /IRQ, /RES, /NMI
#define PPU_CPU_SHARED_MASK ( \
    BUS_BIT(BUS_RES_BIT) | \
    BUS_BIT(BUS_IRQ_BIT) | \
    BUS_BIT(BUS_NMI_BIT)   \
)

// Transfer shared signals from PPU bus onto CPU bus:
//   cpu_bus = PPU_CPU_BITMIX(cpu_bus, ppu_bus);
#define PPU_CPU_BITMIX(cpu, ppu) \
    (((cpu) & ~PPU_CPU_SHARED_MASK) | ((ppu) & PPU_CPU_SHARED_MASK))
```

The aligned bit layout means the cartridge's `ppu_bus_tick()` can assert `/IRQ`
(bit 33) on the PPU bus word, and the system tick loop transfers it to the CPU
bus with a single mask-OR. No bit shifting, no conditional logic.

### Edge Detection via `bus_snapshot_`

Signal edge detection (e.g., MMC3's A12 rising edge, NMI falling edge) is
always done by comparing the **current** bus state to a `bus_snapshot_` stored
at the end of the previous tick. This is the established pattern in ChipBase:

```cpp
// At the end of every tick function (PPU, Cartridge, etc.):
bus_snapshot_ = current_ppu_bus;

// At the start of the next tick — detect rising edge on PA12:
bool pa12_was_low = !PPU_BUS_GET_BIT(bus_snapshot_, PPU_BUS_PA12_BIT);
bool pa12_is_high =  PPU_BUS_GET_BIT(ppu_bus_,      PPU_BUS_PA12_BIT);
bool pa12_rising  =  pa12_was_low && pa12_is_high;

if (pa12_rising) {
    mapper_->clock_irq_counter();  // MMC3 scanline counter
}
```

This replaces the `on_ppu_a12_rise()` callback — the cartridge chip's tick
function does the comparison itself. No callback, no observer pattern, no
extra state beyond what ChipBase already provides.

**For the PPU bus**, a second snapshot variable is needed since `ChipBase::bus_snapshot_`
stores the CPU bus. The PPU and Cartridge chips carry their own:

```cpp
class PPU : public ChipBase {
    // ...
    ppu_bus_state_t ppu_bus_snapshot_ = 0;  // snapshot of PPU bus at end of tick
};

class Cartridge : public ChipBase {
    // ...
    ppu_bus_state_t ppu_bus_snapshot_ = 0;  // for A12 edge detection in mapper
};
```

---

## Part 4 — Chip-per-Type Breakdown

### 4.1 PPU (`nes_ppu.h`)

```
class PPU : public ChipBase {
public:
    // Construction with region
    explicit PPU(bool pal = false);

    // === Bus interfaces ===
    bus_state_t cpu_bus_tick(bus_state_t bus);       // CPU reads/writes $2000-$2007
    uint8_t     cpu_peek(uint16_t addr) const;       // Debug read (no side effects)

    ppu_bus_state_t ppu_bus_tick(ppu_bus_state_t bus); // PPU internal memory access

    // === Timing ===
    void clock();                                     // One PPU dot (called 3× per CPU cycle)
    bool frame_complete() const;
    bool nmi_pending() const;

    // === Video output ===
    const uint32_t* framebuffer() const;              // 256×240 RGBA
    static constexpr int WIDTH = 256;
    static constexpr int HEIGHT = 240;

    // === Bank map integration ===
    void set_bus(nes_bus_t* bus);                      // PPU reads CHR via bus page pointers

    // === ChipBase GUI (in nes_ppu_gui.cpp, #ifdef CERMU_HAS_GUI) ===
    bool has_debug_content()   const override;
    bool has_layout_content()  const override;
    void render_debug_content()   override;
    void render_layout_content()  override;

private:
    struct Registers { ... } regs_;
    struct InternalState { ... } internal_;
    std::array<uint8_t, 256> oam_;
    std::array<uint32_t, WIDTH * HEIGHT> screen_;
    nes_bus_t* bus_;  // for CHR reads via page pointers

    // bus_snapshot_ (inherited from ChipBase) stores CPU bus snapshot.
    // PPU bus gets its own snapshot for NMI edge detection:
    ppu_bus_state_t ppu_bus_snapshot_ = 0;
    // Set at end of every clock(): ppu_bus_snapshot_ = ppu_bus_;
    // NMI edge = /NMI low now && /NMI high in snapshot
};
```

**Key changes**:
- PPU reads CHR data through `bus_->ppu_read_page[]` instead of `cart->ppu_read()` (eliminates virtual dispatch on every tile fetch)
- `ppu_bus_snapshot_` stores PPU bus state at end of each `clock()` for NMI edge detection
- Layout definition lives in `nes_ppu_gui.cpp` under `#ifdef CERMU_HAS_GUI`, not in the chip header

### 4.2 Cartridge (`nes_cartridge.h`)

```
class Cartridge : public ChipBase {
public:
    // Load from iNES buffer
    bool load_from_buffer(const uint8_t* data, size_t size, const std::string& filepath);

    // === CPU bus interface ===
    // Cartridge responds to $4020-$FFFF on CPU bus
    // ROM writes ($8000-$FFFF) are forwarded to mapper register handler
    bus_state_t cpu_bus_tick(bus_state_t bus);

    // === Mapper delegation ===
    Mapper* mapper() const;

    // === Bus integration ===
    // Called by the system after mapper register writes to
    // regenerate bus page pointers from current mapper state
    void update_bank_map(nes_bus_t* bus) const;

    // === Battery-backed SRAM ===
    bool load_sram(const std::string& path);
    bool save_sram(const std::string& path) const;

    // === Properties ===
    uint8_t mapper_id() const;
    bool battery_backed() const;
    Mirror mirror_mode() const;

    // === ChipBase GUI ===
    bool has_debug_content()  const override;
    bool has_layout_content() const override;

private:
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> chr_memory_;   // ROM or RAM
    std::vector<uint8_t> prg_ram_;
    std::unique_ptr<Mapper> mapper_;
    // ... header info, SRAM path
};
```

**Key change**: `Cartridge` is now a `ChipBase` (not a plain class), making it registrable in the Hardware menu with proper layout, debug, and settings rendering.

### 4.3 Mapper (`nes_mapper.h`)

```
class Mapper {
public:
    virtual ~Mapper() = default;

    // === PRG bank configuration ===
    // Returns page pointers for CPU $8000-$FFFF (8 × 4KB pages)
    virtual void get_prg_bank_config(MapperBankConfig& config) const = 0;

    // === CHR bank configuration ===
    // Returns page pointers for PPU $0000-$1FFF (8 × 1KB pages)
    // plus nametable mirroring (4 × 1KB page indices into CIRAM)
    virtual void get_chr_bank_config(MapperChrConfig& config) const = 0;

    // === Register writes ===
    // The bus calls this when CPU writes to $8000-$FFFF (ROM range)
    // Returns true if banking changed (bus needs page pointer update)
    virtual bool cpu_write(uint16_t addr, uint8_t data) = 0;

    // === IRQ (optional) ===
    virtual bool irq_state() const { return false; }
    virtual void irq_clear() {}

    // === PPU bus observation ===
    // Called every PPU tick with the current PPU bus state.
    // The mapper compares against its own ppu_bus_snapshot_ to detect
    // A12 rising edges (MMC3), pattern table fetches (MMC2/MMC4), etc.
    // Returns true if banking changed.
    virtual bool ppu_bus_tick(ppu_bus_state_t bus, ppu_bus_state_t snapshot) {
        (void)bus; (void)snapshot; return false;
    }

    // === Reset ===
    virtual void reset() = 0;

    // === Mirroring ===
    virtual Mirror mirror_mode() const { return Mirror::HORIZONTAL; }

protected:
    // Mapper accesses ROM vectors through Cartridge*
    const uint8_t* prg_rom_ = nullptr;
    size_t prg_rom_size_ = 0;
    const uint8_t* chr_rom_ = nullptr;
    size_t chr_rom_size_ = 0;
    uint8_t* chr_ram_ = nullptr;
    uint8_t* prg_ram_ = nullptr;
};
```

**Key change**: Mappers produce `MapperBankConfig` / `MapperChrConfig` structs containing direct pointers into ROM/RAM. The bus copies these pointers into its page tables. Mappers no longer do address translation per access — they precompute the translation once when registers change.

### 4.4 Controller — uses `CD4021` from `src/chip/input/`

The NES controller contains a CD4021 8-bit parallel-in/serial-out shift
register. Since this chip appears in SNES controllers and other systems,
it lives in `src/chip/input/cd4021.h` as a cross-system `ChipBase`:

```
// src/chip/input/cd4021.h — CD4021 PISO shift register (header-only)

class CD4021 : public ChipBase {
public:
    CD4021();

    // Parallel load: latch 8 bits from external pins
    void latch(uint8_t data);

    // Serial read: shift out one bit (MSB first), returns D0
    uint8_t shift_out();

    // === ChipBase GUI (in cd4021_gui.cpp, #ifdef CERMU_HAS_GUI) ===
    bool has_layout_content() const override;
    void render_layout_content() override;  // 16-pin DIP layout

private:
    uint8_t shift_register_ = 0;
};
```

The NES system wraps this into its controller handling:

```
// In NintendoSystem — controller is a CD4021 instance + button state
CD4021 controller_[2];       // Player 1 & 2
uint8_t button_state_[2]{};  // Current button bitmask

// $4016 write: latch both controllers
void latch_controllers() {
    controller_[0].latch(button_state_[0]);
    controller_[1].latch(button_state_[1]);
}

// $4016/$4017 read: serial shift one bit
uint8_t read_controller(int port) {
    return controller_[port].shift_out() & 0x01;
}
```

**Key changes**:
- CD4021 lives in `src/chip/input/` — reusable across NES, SNES, etc.
- Layout (16-pin DIP) is in `cd4021_gui.cpp` under `#ifdef CERMU_HAS_GUI`
- NES system composes the chip, doesn't subclass it

### 4.5 NES CPU Wrapper (`nes_cpu.h`)

Not a new ChipBase — the CPU is `RICOH_2A03` (already a `ChipBase` through `fam65xx_t<>`). This header provides NES-specific helpers:

```
// nes_cpu.h — NES-specific CPU helpers (header-only)

namespace nes_cpu {

// DMA controller state (OAM DMA at $4014)
struct DMAState {
    uint8_t page = 0;
    uint8_t addr = 0;
    uint8_t data = 0;
    bool active = false;
    bool dummy_cycle = true;

    void trigger(uint8_t page_number) {
        page = page_number;
        addr = 0;
        active = true;
        dummy_cycle = true;
    }

    bool is_active() const { return active; }
};

} // namespace nes_cpu
```

---

## Part 5 — Hardware-Accurate Pin Layouts Per Chip

All layout definitions live in files with a `_gui` suffix and are compiled
only when `CERMU_HAS_GUI` is defined. This matches the existing codebase
convention (`vic_gui.cpp`, `vicii_gui.cpp`, `ted7360_gui.cpp`, etc.).

```cpp
// Example: nes_ppu_gui.cpp
#ifdef CERMU_HAS_GUI

static const ChipLayout& get_rp2c02_layout() {
    static const ChipLayout layout = { /* 40-pin DIP ... */ };
    return layout;
}

void PPU::render_layout_content() {
    const auto& layout = get_rp2c02_layout();
    auto pin_states = get_ppu_pin_states(this, &layout, bus_snapshot_, ppu_bus_snapshot_);
    render_chip_layout(layout, pin_states);
}

#endif // CERMU_HAS_GUI
```

### RP2C02 (PPU) — 40-pin DIP

Already defined in current `nes_ppu_gui.cpp`. Migration keeps it in `ppu/nes_ppu_gui.cpp`
under `#ifdef CERMU_HAS_GUI`:

```
Pin 1:  R/W             Pin 40: VDD (+5V)
Pin 2:  D0              Pin 39: ALE (addr latch enable)
Pin 3:  D1              Pin 38: MA0 (mux addr 0)
Pin 4:  D2              Pin 37: MA1
Pin 5:  D3              Pin 36: MA2
Pin 6:  D4              Pin 35: MA3
Pin 7:  D5              Pin 34: MA4
Pin 8:  D6              Pin 33: MA5
Pin 9:  D7              Pin 32: MA6
Pin 10: A2 (CPU addr)   Pin 31: MA7
Pin 11: A1              Pin 30: A8  (PPU addr hi)
Pin 12: A0              Pin 29: A9
Pin 13: /CS             Pin 28: A10
Pin 14: EXT0            Pin 27: A11
Pin 15: EXT1            Pin 26: A12
Pin 16: EXT2            Pin 25: A13
Pin 17: EXT3            Pin 24: /RD (VRAM read strobe)
Pin 18: CLK (master)    Pin 23: /WE (VRAM write strobe)
Pin 19: /INT (NMI out)  Pin 22: /RES (reset)
Pin 20: VSS (GND)       Pin 21: VOUT (composite video)
```

### RP2A03 (CPU/APU) — 40-pin DIP

Already defined in `ricoh_2a03.h` and `nes_apu_gui.cpp`. The CPU layout is canonical. The APU GUI file (`apu/nes_apu_gui.cpp`) keeps the APU-focused variant highlighting SND1/SND2, all under `#ifdef CERMU_HAS_GUI`.

### CD4021 (Controller Shift Register) — 16-pin DIP

Lives in `src/chip/input/cd4021_gui.cpp` (cross-system, `#ifdef CERMU_HAS_GUI`):

```
Pin 1:  P5 (parallel in 5)  Pin 16: VDD
Pin 2:  P6                  Pin 15: LATCH (parallel/serial)
Pin 3:  QS (serial out)     Pin 14: CLK
Pin 4:  P1                  Pin 13: P4
Pin 5:  P0                  Pin 12: P3
Pin 6:  P7                  Pin 11: D (serial in)
Pin 7:  /Q7 (complement)    Pin 10: P2
Pin 8:  VSS                 Pin  9: Q7 (serial out)
```

### 72-pin Cartridge Connector (NES) / 60-pin (Famicom)

Defined in `src/ports/nes_ports.h` as `PortDefinition` constants — already partially in `nes_system.cpp`, just needs extraction to the shared connectors folder (used by NES, Famicom, VS. System, PlayChoice-10).

---

## Part 6 — Ports and Peripheral Devices

The current codebase has a well-developed Port/Peripheral framework
(`src/core/port.hpp`) with wired-AND signal propagation, host input
binding, and device registration. The NES system already uses it:

- `Port` instances for controller port 1, controller port 2, expansion port
- `PortDefinition` with `NESControllerBit` signal lines (CLK, LATCH, D0, D3, D4)
- `NESExpansionBit` signal lines (D0–D4, OUT0–OUT2, CLK, LATCH, /IRQ)
- NES vs Famicom variants (removable vs hardwired controllers, 48-pin vs 15-pin expansion)

**What's missing**: NES-specific `PeripheralDevice` subclasses. Currently the NES
system hardcodes its controller handling (reading button state and running a shift
register internally), bypassing the `Port` ↔ `PeripheralDevice` signal
protocol entirely. The migration must make these real peripherals.

### 6.1 NES Peripheral Device Hierarchy

Following the pattern established by the C64 input devices (`JoystickDevice`,
`LightpenDevice`, `Commodore1351Mouse`, etc. in `src/devices/input/`):

The directory tree for NES peripheral devices is shown in Part 1 under `src/devices/input/`.

> **Why shared folders, not `src/systems/nes/`?**
> NES controller ports appear on NES, Famicom, VS. System, and PlayChoice-10.
> The standard controller and Zapper work across all of them. Famicom controllers
> also work via adapters on SNES. Following the cross-system rule, any entity
> used by multiple products goes in a shared folder — this applies equally to
> connector definitions (`src/ports/`), chips (`src/chip/`), and
> peripheral devices (`src/devices/input/`).

### 6.2 NES Standard Controller (`NesStandardController`)

```cpp
// src/devices/input/nes_standard_controller.h

class NesStandardController : public PeripheralDevice {
public:
    // Button bitmask (matches hardware bit order, MSB first through shift register)
    enum Button : uint8_t {
        A      = 0x80,
        B      = 0x40,
        SELECT = 0x20,
        START  = 0x10,
        UP     = 0x08,
        DOWN   = 0x04,
        LEFT   = 0x02,
        RIGHT  = 0x01,
    };

    NesStandardController();

    // --- PeripheralDevice interface ---
    const char* get_name() const override { return "NES Standard Controller"; }
    const char* get_id()   const override { return "nes_gamepad"; }
    PortType get_port_type() const override { return PortType::CONTROLLER_NES; }
    void reset() override;

    // --- Signal protocol ---
    // System drives CLK and LATCH via Port::write_system_signals().
    // Device responds via on_signal_change() → updates output D0.
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override;

    // --- Host input (keyboard / gamepad) ---
    bool accepts_host_input() const override { return true; }
    int get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override;
    const HostInputBinding& get_host_input_binding() const override;
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

private:
    uint8_t button_state_ = 0;      // 8 buttons, active-high bitmask
    uint8_t shift_register_ = 0;    // Latched copy, shifted out MSB first
    bool latch_was_high_ = false;   // Edge detection for LATCH signal
    uint32_t output_signals_ = 0xFFFFFFFF;  // Active-low output (D0 line)
    HostInputBinding binding_{};
};
```

### 6.3 Signal Protocol Flow

The real hardware protocol and how it maps to the `Port` framework:

```
System tick loop:
  1. System writes LATCH=HIGH via port->write_system_signals(NES_LATCH, HIGH)
     → PeripheralDevice::on_signal_change() called
     → Controller latches all 8 button states into shift register

  2. System writes LATCH=LOW
     → on_signal_change() detects falling edge
     → D0 now reflects bit 7 (A button) of shift register

  3. System reads port->read_signals() → gets D0 bit
     → Wired-AND of system output (all 1s) and device output (D0)

  4. System pulses CLK HIGH then LOW (8 times)
     → Each rising edge of CLK: shift register <<= 1, D0 = next bit
     → Bits come out: A, B, SELECT, START, UP, DOWN, LEFT, RIGHT
```

This replaces the current hardcoded `Controller::read()` / `Controller::write()`
with proper `Port` signal protocol. The system tick loop now drives
CLK/LATCH through the port, and the device responds via signal change callbacks.

### 6.4 Integration with CD4021 Chip

The `NesStandardController` internally contains (composes) a `CD4021` instance:

```cpp
void NesStandardController::on_signal_change(uint32_t signal_state) {
    bool latch_high = !(signal_state & (1u << NESControllerBit::NES_LATCH)); // active-low

    if (latch_high && !latch_was_high_) {
        // Rising edge: parallel load
        cd4021_.latch(button_state_);
    }
    latch_was_high_ = latch_high;

    bool clk_high = !(signal_state & (1u << NESControllerBit::NES_CLK));
    if (clk_high) {
        // Clock pulse: shift out one bit
        uint8_t bit = cd4021_.shift_out();
        // Drive D0 line (active-low: 0 = button pressed)
        if (bit)
            output_signals_ &= ~(1u << NESControllerBit::NES_D0);
        else
            output_signals_ |= (1u << NESControllerBit::NES_D0);
        port_->notify_device_output_changed(output_signals_);
    }
}
```

The `CD4021` chip (in `src/chip/input/`) handles the shift register logic.
The `NesStandardController` peripheral (in `src/devices/input/`) handles:
- Host input binding (keyboard keys / SDL gamepad)
- Button state management
- Signal protocol (LATCH/CLK edge detection)
- `Port` integration (signal I/O)

### 6.5 Port Setup After Migration

The existing `setup_ports()` code moves to `src/ports/nes_ports.hpp`
as static `PortDefinition` constants (shared — used by NES, Famicom, VS. System,
PlayChoice-10), with the setup function remaining in `nes_system.cpp`:

```cpp
// src/ports/nes_ports.h — header-only, cross-system
namespace NesPorts {

inline const PortDefinition NES_CONTROLLER_1 = {
    PortType::CONTROLLER_NES,
    "Controller Port 1",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    false, false  // not internal, not bus
};

inline const PortDefinition NES_CONTROLLER_2 = { /* ... */ };
inline const PortDefinition NES_EXPANSION = { /* ... */ };
inline const PortDefinition FC_CONTROLLER_1 = {
    PortType::CONTROLLER_NES,
    "Controller I (hardwired)",
    PortSignals::NES_CONTROLLER_SIGNALS,
    PortSignals::NES_CONTROLLER_SIGNAL_COUNT,
    true, false   // internal (hardwired, cannot detach via UI)
};
inline const PortDefinition FC_CONTROLLER_2 = { /* ... microphone on Famicom */ };
inline const PortDefinition FC_EXPANSION_15PIN = { /* ... */ };

} // namespace NesPorts
```

**Device Registration**: `NesStandardController` is registered in the `DeviceRegistry`
with `PortType::CONTROLLER_NES`, so it auto-appears in the port attachment UI for
NES and Famicom controller ports.

### 6.6 Future NES Peripherals

| Device | Port | Notes |
|--------|-----------|-------|
| **NES Zapper** | `CONTROLLER_NES` | Light gun — reads PPU pixel color at aim position; trigger on D4. Uses `CONTROLLER_NES` port but drives D3/D4 expansion bits. |
| **NES Arkanoid Vaus** | `CONTROLLER_NES` | Paddle — potentiometer value via serial protocol on D0, fire button on D3. |
| **Famicom Keyboard** | `FC_EXPANSION_15PIN` | Family BASIC keyboard — matrix scan via expansion port. |
| **Famicom Disk System** | `FC_EXPANSION_15PIN` | FDS drive — expansion RAM adapter + disk I/O. |
| **Four Score** | `CONTROLLER_NES` | 4-player adapter — chains two additional shift registers. |
| **Power Pad** | `CONTROLLER_NES` | Dance mat — 12 pressure sensors via serial scan. |

Each is a `PeripheralDevice` subclass in a shared folder (`src/devices/input/` for
input peripherals, `src/devices/storage/` for FDS), registered with the appropriate
`PortType`. None live under `src/systems/nes/` — they all serve multiple products.

---

## Part 7 — Incremental Migration Steps

The migration is designed to be done in small, testable steps. Each step compiles and passes existing tests before proceeding.

### Phase 1: Extract — Pull Types Out of Monolith (No Behavior Change)

| Step | Action | Files Created | Risk |
|------|--------|---------------|------|
| 1.1 | Create directory structure | `bus/`, `cpu/`, `ppu/`, `apu/`, `cartridge/`, `cartridge/mappers/`, `nsf/`, `screen/` under `src/systems/nes/`; shared `src/ports/` and `src/devices/input/` | None |
| 1.2 | Extract `nes_bus_chips.h` — CHIP ID enum (new, no code moves yet) | `bus/nes_bus_chips.h` | None |
| 1.3 | Extract `nes_bus_signals.h` — PPU bus typedefs, bitmix macros, signal macros | `bus/nes_bus_signals.h` | None |
| 1.4 | Move PPU class declaration → `ppu/nes_ppu.h` (keep impl in `nes_system.cpp` for now) | `ppu/nes_ppu.h` | Low — header split |
| 1.5 | Move PPU implementation → `ppu/nes_ppu.cpp` | `ppu/nes_ppu.cpp` | Low |
| 1.6 | Move PPU palette → `ppu/nes_ppu_palette.h` | `ppu/nes_ppu_palette.h` | None |
| 1.7 | Move PPU GUI (layout + debug + settings) → `ppu/nes_ppu_gui.cpp` (layout under `#ifdef CERMU_HAS_GUI`) | `ppu/nes_ppu_gui.cpp` | Low |
| 1.8 | Move APU GUI (layout + debug + settings) → `apu/nes_apu_gui.cpp` (layout under `#ifdef CERMU_HAS_GUI`) | `apu/nes_apu_gui.cpp` | Low |
| 1.9 | Extract Cartridge class → `cartridge/nes_cartridge.h` + `.cpp` | 2 files | Medium |
| 1.10 | Add Cartridge GUI → `cartridge/nes_cartridge_gui.cpp` (layout + debug, `#ifdef CERMU_HAS_GUI`) | 1 file | Low |
| 1.11 | Extract Mapper base → `cartridge/nes_mapper.h` | 1 file | Low |
| 1.12 | Extract each mapper → `cartridge/mappers/mapper_NNN_name.h` (000, 001, 002, 003, 004) | 5 files | Medium |
| 1.13 | Create mapper factory → `cartridge/nes_mapper_factory.h` | 1 file | Low |
| 1.14 | Create CD4021 in cross-system location → `src/chip/input/cd4021.h` (header-only) | 1 file | Low |
| 1.15 | Create CD4021 GUI → `src/chip/input/cd4021_gui.cpp` (16-pin DIP layout, `#ifdef CERMU_HAS_GUI`) | 1 file | Low |
| 1.16 | Extract connector definitions → `src/ports/nes_ports.h` (shared, header-only) | 1 file | Low |
| 1.17 | Create NES standard controller → `src/devices/input/nes_standard_controller.h` + `.cpp` | 2 files | Medium |
| 1.18 | Create NES Zapper → `src/devices/input/nes_zapper.h` + `.cpp` | 2 files | Medium |
| 1.19 | Register NES peripherals in `DeviceRegistry` | 0 files (existing registry) | Low |
| 1.20 | Remove old `controller/` directory — NES system uses `CD4021` directly | Delete dir | Low |
| 1.21 | Move NSF files → `nsf/` | Move 3 files | Low |
| 1.22 | Move screen utils → `screen/` | Move 2 files | Low |
| 1.23 | Update CMakeLists.txt with new source list (incl. `src/chip/input/`, `src/ports/`, `src/devices/input/`) | Modified | Medium |
| 1.24 | Compile + run nestest — verify identical behavior | — | Gate |

### Phase 2: Introduce Unified Buffer + Bank Map (Behavior Change)

| Step | Action | Risk |
|------|--------|------|
| 2.1 | Create `bus/nes_bus.h` with `nes_bus_t` struct (page pointer tables) | Low |
| 2.2 | Create `bus/nes_bus.cpp` with `update_cpu_banks()` and `update_ppu_banks()` | Medium |
| 2.3 | Add `MapperBankConfig` / `MapperChrConfig` to `nes_mapper.h` | Low |
| 2.4 | Implement `get_prg_bank_config()` for Mapper000 (simplest) | Low |
| 2.5 | Wire NROM page pointers in `nes_bus_t::init()` | Medium |
| 2.6 | Replace `MemoryBus::mem_tick()` reads with `nes_bus_t::cpu_read()` for pages 0-1 (RAM) | Low |
| 2.7 | Replace PRG-ROM reads ($8000-$FFFF) with page pointer path | Medium |
| 2.8 | Replace PPU CHR reads with `ppu_read_page[]` | Medium |
| 2.9 | Compile + run nestest with NROM — verify identical output | Gate |
| 2.10 | Implement bank config for Mapper001 (MMC1) | Medium |
| 2.11 | Implement bank config for Mapper002 (UxROM) | Low |
| 2.12 | Implement bank config for Mapper003 (CNROM) | Low |
| 2.13 | Implement bank config for Mapper004 (MMC3) — includes A12 tracking | High |
| 2.14 | Remove old `MemoryBus` class | Medium |
| 2.15 | Full test suite — verify all mapper tests pass | Gate |

### Phase 3: Introduce PPU Bus State

| Step | Action | Risk |
|------|--------|------|
| 3.1 | Define `ppu_bus_state_t` in `bus/nes_bus_signals.h` with aligned shared bits (32-34) | Low |
| 3.2 | Define `PPU_CPU_SHARED_MASK` and `PPU_CPU_BITMIX` macro | Low |
| 3.3 | Add `ppu_bus_` and `ppu_bus_snapshot_` members to `nes_bus_t` | Low |
| 3.4 | Refactor PPU internal reads to go through `ppu_bus_state_t` | Medium |
| 3.5 | Store `ppu_bus_snapshot_` at end of every PPU tick and cartridge tick | Low |
| 3.6 | Implement A12 edge detection in MMC3 mapper via snapshot comparison | Medium |
| 3.7 | Replace `ppu->scanline()` callback with A12 monitoring in cartridge tick | High |
| 3.8 | Add `PPU_CPU_BITMIX` to system tick loop for /IRQ and /NMI transfer | Low |
| 3.9 | Verify MMC3 games (SMB3, Kirby, Battletoads) work correctly | Gate |

### Phase 4: Make All Chips Proper ChipBase Subclasses

| Step | Action | Risk |
|------|--------|------|
| 4.1 | Make `Cartridge : public ChipBase` (debug/layout in `nes_cartridge_gui.cpp`) | Low |
| 4.2 | Register `CD4021` controllers as ChipBase instances (layout in `cd4021_gui.cpp`) | Low |
| 4.3 | Remove `ChipPlaceholder` for cartridge in chip registration | Low |
| 4.4 | Add CIRAM (2KB VRAM) as `MemoryChip` with layout | Low |
| 4.5 | Update `register_nes_chips()` to use new types | Low |
| 4.6 | Verify Hardware menu shows all chips with layouts (requires `CERMU_HAS_GUI` build) | Gate |

### Phase 5: Eliminate Global/Free Functions

| Step | Action | Risk |
|------|--------|------|
| 5.1 | Move `nes_palette[]` global into `PPU::palette()` static | Low |
| 5.2 | Move NSF free functions into `NsfPlayer` type | Medium |
| 5.3 | Move screen utils free functions into `ScreenUtils` type | Low |
| 5.4 | Move connector definitions into `NesPorts` namespace | Low |
| 5.5 | Audit for remaining non-member functions; encapsulate | Low |
| 5.6 | Final clean build + test suite | Gate |

---

## Part 8 — Mapper Bank Config Structures

These are the structs that mappers produce and the bus consumes:

```cpp
// Forward: mirrors match real hardware nametable indices into CIRAM
enum class Mirror : uint8_t {
    HORIZONTAL,      // CIRAM pages [0,0,1,1]
    VERTICAL,        // CIRAM pages [0,1,0,1]
    ONESCREEN_LO,    // CIRAM pages [0,0,0,0]
    ONESCREEN_HI,    // CIRAM pages [1,1,1,1]
    FOUR_SCREEN,     // 4KB on-cart RAM, no CIRAM sharing
};

struct MapperBankConfig {
    // 8 × 4KB page pointers for $8000-$FFFF
    const uint8_t* prg_pages[8] = {};

    // PRG-RAM at $6000-$7FFF
    uint8_t* prg_ram_base = nullptr;
    bool prg_ram_enabled = true;
    bool prg_ram_write_protected = false;

    // Expansion space $5000-$5FFF (MMC5, etc.)
    const uint8_t* expansion_read = nullptr;
    uint8_t* expansion_write = nullptr;
};

struct MapperChrConfig {
    // 8 × 1KB page pointers for PPU $0000-$1FFF
    const uint8_t* chr_pages[8] = {};
    bool chr_writable[8] = {};  // true for CHR-RAM pages

    // Nametable mirroring — 4 entries, each selects a CIRAM page (0 or 1)
    // For FOUR_SCREEN: indices 2,3 point to on-cart extra VRAM
    uint8_t nt_page[4] = {0, 1, 0, 1};  // default: vertical
};
```

### Example: Mapper000 (NROM) Configuration

```cpp
// mapper_000_nrom.h

class Mapper000 : public Mapper {
public:
    Mapper000(uint8_t prg_banks, uint8_t chr_banks)
        : prg_banks_(prg_banks), chr_banks_(chr_banks) {}

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if (prg_banks_ <= 1) {
            // NROM-128: 16KB mirrored at $8000 and $C000
            for (int i = 0; i < 4; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
            for (int i = 0; i < 4; i++) config.prg_pages[4 + i] = prg_rom_ + (i * 0x1000);
        } else {
            // NROM-256: 32KB at $8000-$FFFF
            for (int i = 0; i < 8; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
        }
        config.prg_ram_enabled = false;  // NROM has no WRAM
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        for (int i = 0; i < 8; i++) {
            config.chr_pages[i] = chr_rom_ + (i * 0x400);
            config.chr_writable[i] = (chr_rom_ == chr_ram_);
        }
        // Mirroring from cartridge header (fixed for NROM)
    }

    bool cpu_write(uint16_t, uint8_t) override { return false; }  // No registers

    void reset() override {}

private:
    uint8_t prg_banks_, chr_banks_;
};
```

---

## Part 9 — Tick Loop After Migration

```cpp
// nes_system.cpp — NintendoSystem::clock() after migration

template<NintendoVariant V>
void NintendoSystem<V>::clock() {
    // === PPU tick (3 per CPU cycle) ===
    ppu_->clock();
    ppu_tick_counter_++;

    if (ppu_tick_counter_ < 3) return;
    ppu_tick_counter_ = 0;

    // === DMA stall ===
    if (dma_.is_active()) {
        handle_dma_cycle();
        return;
    }

    // === CPU PHI2: address out, RW set ===
    cpu_bus_ = cpu_->tick<Phase::PHI2>(cpu_bus_);

    // === Bus service ===
    uint16_t addr = BUS_GET_ADDR(cpu_bus_);
    bool is_read = BUS_GET_BIT(cpu_bus_, BUS_RW_BIT);

    uint8_t page = addr >> 12;

    if (is_read) {
        const uint8_t* rp = bus_.cpu_read_page[page];
        if (LIKELY(rp != nullptr)) {
            BUS_SET_DATA(cpu_bus_, rp[addr & 0x0FFF]);
        } else {
            // I/O dispatch (PPU regs, APU, controllers, mapper expansion)
            cpu_bus_ = bus_.cpu_io_handlers[page].read(
                bus_.cpu_io_handlers[page].context, cpu_bus_);
        }
    } else {
        uint8_t* wp = bus_.cpu_write_page[page];
        if (LIKELY(wp != nullptr)) {
            wp[addr & 0x0FFF] = BUS_GET_DATA(cpu_bus_);
        } else if (page >= 8) {
            // ROM write → mapper register
            uint8_t data = BUS_GET_DATA(cpu_bus_);
            if (cartridge_->mapper()->cpu_write(addr, data)) {
                // Banking changed — regenerate page pointers
                cartridge_->update_bank_map(&bus_);
            }
        } else {
            // I/O dispatch (PPU regs, APU, controller ports)
            // Controller I/O ($4016/$4017) is handled via Port:
            //   Write $4016 bit 0 → port->write_system_signals(LATCH, val)
            //     → PeripheralDevice::on_signal_change() → CD4021::latch()
            //   Read $4016/$4017 → port->read_signals() reads D0 from device
            //     Each read pulses CLK → CD4021::shift_out() → next bit
            cpu_bus_ = bus_.cpu_io_handlers[page].write(
                bus_.cpu_io_handlers[page].context, cpu_bus_);
        }
    }

    // === A12 edge detection for mappers (MMC3, etc.) ===
    // Cartridge compares current PPU bus against its snapshot.
    // If banking changed, update page pointers.
    if (cartridge_->mapper()->ppu_bus_tick(bus_.ppu_bus_, bus_.ppu_bus_snapshot_)) {
        cartridge_->update_bank_map(&bus_);
    }

    // === Transfer shared signals from PPU bus → CPU bus via bitmix ===
    // /IRQ (mapper scanline IRQ) and /NMI (PPU vblank) are at the same
    // bit positions in both bus words — direct mask-OR, no shifting.
    cpu_bus_ = PPU_CPU_BITMIX(cpu_bus_, bus_.ppu_bus_);

    // === CPU PHI1: internal ops + APU clock ===
    cpu_bus_ = cpu_->tick<Phase::PHI1>(cpu_bus_);

    // === Also transfer CPU-side IRQ sources (APU frame/DMC IRQ) ===
    if (cpu_->apu_irq()) BUS_CLR_BIT(cpu_bus_, BUS_IRQ_BIT);

    // === Store bus snapshots for next tick's edge detection ===
    bus_.cpu_bus_snapshot_ = cpu_bus_;
    bus_.ppu_bus_snapshot_ = bus_.ppu_bus_;

    // === Audio sample generation (decimated ~37:1) ===
    if (++audio_counter_ >= audio_divisor_) {
        audio_counter_ = 0;
        audio_buffer_.push_back(cpu_->generate_audio_sample());
    }

    total_cycles_++;
}
```

**Key differences from old tick loop**:
1. `/NMI` and `/IRQ` are transferred via `PPU_CPU_BITMIX()` — one mask-OR replaces conditional set/clear
2. Mapper A12 edge detection uses `ppu_bus_tick(current, snapshot)` — no callbacks
3. `bus_snapshot_` for both CPU and PPU buses is stored at the **end** of the tick, ready for next tick's edge comparison
4. `clear_nmi()` is gone — the NMI line is a real signal on the bus, not an event

---

## Part 10 — Testing Strategy

| Test | Purpose | Files |
|------|---------|-------|
| **nestest.nes** | CPU instruction accuracy (existing) | `tests/nes/` |
| **PPU pattern table renders** | CHR page pointer correctness | New unit test |
| **Mapper000 banking** | NROM-128 mirroring, NROM-256 flat | New unit test |
| **Mapper001 serial register** | MMC1 shift register + mode switching | New unit test |
| **Mapper004 IRQ counter** | MMC3 A12 edge counting + scanline accuracy | New unit test |
| **Bank switch stress** | Rapid bank switching doesn't corrupt page pointers | New unit test |
| **NSF playback** | NSF cartridge + player still functional | Existing manual test |
| **Save state** | Serialization covers new bus_t layout | Modified existing |

---

## Part 11 — Risk Assessment and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| PPU CHR read through page pointers breaks timing | Medium | High | Keep `ppu_read()` as fallback path behind a flag; A/B test |
| MMC3 A12 tracking via `bus_snapshot_` comparison vs callback | Medium | High | Validate with blargg's MMC3 test ROMs and Battletoads; compare snapshot timing window |
| Mapper register write detection on nullptr write page | Low | Medium | Well-defined: all ROM pages have `cpu_write_page = nullptr` |
| NSF cartridge bank switching doesn't map to page pointers | Low | Medium | NsfCartridge overrides `update_bank_map()` |
| Save state format changes | Certain | Low | Version the save state header; old states incompatible |
| `PPU_CPU_BITMIX` transfers stale /NMI after PPU already cleared it | Low | High | PPU drives /NMI on the PPU bus word directly — it's a level, not a latch. The bus_snapshot_ comparison in the CPU detects the edge. |
| CD4021 in `src/chip/input/` creates new build dependency | Low | Low | CMakeLists links `chip_input` lib; NES and future SNES both depend on it |

---

## Confidence: 0.90

**Strong**:
- C64 pattern is proven in this codebase — direct translation to NES
- Phase 1 (extract-only) is zero-risk and testable independently
- Page pointer approach eliminates most virtual dispatch in the hot path
- Each mapper is independently testable in its own header
- `bus_snapshot_` edge detection is the established pattern (VIC-II, TED, 6522 all use it)
- Bitmix alignment is zero-cost — just careful bit numbering, no runtime overhead

**Key uncertainties**:
- MMC3 A12-edge IRQ timing through snapshot comparison needs validation against hardware-accurate test ROMs (SMB3 raster effects, Battletoads level 2)
- MMC5 split screen and ExRAM interception is inherently complex and may need a hybrid approach (page pointers + per-access callback for the ExRAM attribute substitution)
- Performance of `ppu_write_page` nullptr check on write-protected CHR-ROM pages — this is a branch in the PPU hot path, but it's a predictable branch (CHR-ROM is read ~99.9% of the time)

**How to improve confidence**:
- Run Mesen's accuracy test suite after Phase 2
- Profile the page-pointer hot path vs current if-else chain with a cycle counter
- Validate A12 tracking with blargg's MMC3 test ROMs
- Add unit test: assert `PPU_CPU_SHARED_MASK` bit positions match `BUS_IRQ_BIT`, `BUS_NMI_BIT`, `BUS_RES_BIT` at compile time (`static_assert`)
