# NES Cartridges & Mappers — Technical Deep Dive

## The NES Cartridge Connector

The NES uses a **72-pin edge connector** (the Famicom uses a **60-pin** HV-compatible dual-row connector — more on that later). The 72-pin connector on the NES front-loader (HVC-001 US variant) is notoriously problematic due to its zero-insertion-force spring-loaded mechanism, which causes the oxidation contact issues legendary among retro enthusiasts.

### NES 72-pin Signal Breakdown

The cartridge port exposes essentially the full CPU and PPU buses:

**CPU side (RP2A03 / 2A07 PAL):**

| Signal group | Pins | Notes |
|---|---|---|
| A0–A14 | 15 address lines | CPU has 16-bit address space; A15 decoded on board |
| D0–D7 | 8 data lines | Bidirectional, open-bus behavior when nothing drives |
| R/W̄ | 1 | High = read, Low = write |
| M2 | 1 | Phase-2 clock, ~1.79 MHz NTSC / ~1.66 MHz PAL; data valid on rising edge |
| /ROMSEL | 1 | Active-low, asserted when CPU addresses $8000–$FFFF |
| /WRAM | 1 | Asserted for $6000–$7FFF (cartridge work RAM space) |
| IRQ̄ | 1 | Open-collector interrupt line to CPU |
| /EXP | Several | Expansion audio input mixed into console output |

**PPU side (RP2C02 NTSC / 2C07 PAL):**

| Signal group | Pins | Notes |
|---|---|---|
| PA0–PA13 | 14 PPU address lines | PPU has 14-bit address space |
| PD0–PD7 | 8 PPU data lines | Separate from CPU data bus |
| /RD, /WR | 2 | PPU bus read/write strobes |
| /A13 | 1 | Complement of PA13; useful for CIRAM (nametable) selection |
| CIRAM /CE | 1 | Enable for the 2KB internal VRAM on the PPU (nametables) |
| CIRAM A10 | 1 | The nametable mirroring control line — the single most important line for simple mappers |

The separation of CPU and PPU buses is fundamental: the cartridge must serve **two independent buses simultaneously**, each with its own address decoding and timing.

---

## Clocks and Timing

The master clock on NTSC hardware is **21.477272 MHz** (21.47727 MHz in practice), derived from a crystal. This is divided internally:

- **CPU clock = master ÷ 12 = ~1.7897 MHz** (each CPU cycle = ~559 ns)
- **PPU clock = master ÷ 4 = ~5.369 MHz** (3 PPU clocks per CPU clock)
- **APU** runs off CPU clock subdivisions

On PAL hardware the master crystal is **26.6017 MHz**, giving ~1.6629 MHz CPU and ~5.32 MHz PPU.

The **M2 signal** on the cartridge connector is the CPU phase-2 clock. Data is valid and stable during the high phase of M2. Mappers that need to count CPU cycles do so by counting M2 edges. The PPU sends its own /RD and /WR strobes; these are much shorter pulses (~186 ns for PPU accesses).

---

## Memory Map Overview

### CPU Memory Map (16-bit, $0000–$FFFF):

```
$0000–$07FF   2KB internal WRAM (mirrored to $1FFF)
$2000–$3FFF   PPU registers (mirrored every 8 bytes)
$4000–$401F   APU / I/O registers
$4020–$5FFF   Cartridge expansion space (used by some mappers)
$6000–$7FFF   Cartridge PRG-RAM / WRAM (battery-backed possible)
$8000–$FFFF   Cartridge PRG-ROM (32KB window, bank-switched by mappers)
```

### PPU Memory Map (14-bit, $0000–$3FFF):

```
$0000–$0FFF   CHR Pattern Table 0 (4KB) — typically cartridge CHR-ROM/RAM
$1000–$1FFF   CHR Pattern Table 1 (4KB) — cartridge CHR
$2000–$27FF   Nametable 0 — internal CIRAM or cartridge RAM
$2400–$2BFF   Nametable 1
$2800–$2FFF   Nametable 2
$2C00–$2FFF   Nametable 3
$3000–$3EFF   Mirror of $2000–$2EFF
$3F00–$3FFF   Palette RAM (internal to PPU, not on bus)
```

The PPU's internal **CIRAM** is only 2KB, providing two nametable pages. The CIRAM A10 line (connected to cartridge pin) controls which of the two physical CIRAM pages maps to which logical nametable — this is the mirroring mechanism. Mappers override this to implement 4-screen (extra VRAM on cart), single-screen, or dynamic mirroring.

---

## Cartridge Physical Components

A bare-minimum NROM (mapper 0) cartridge contains:

- **PRG-ROM**: The program ROM, 16KB or 32KB. Historically mask ROMs (e.g., Toshiba TC531001, Sharp LH532xxx series), parallel-interface, 5V, with access times typically **150–200 ns** — well within the ~350 ns half-cycle available at 1.79 MHz.
- **CHR-ROM**: Character/tile data ROM, 8KB typical for simple games. Same mask ROM technology.
- **CIRAM A10 wiring**: Just a PCB trace connecting PA10 or VRAM_A10 to select H or V mirroring. In NROM this is hardwired.
- **Bypass capacitors**: 100nF decoupling on VCC lines.

No active logic at all in the simplest case — just ROM chips and passive components.

---

## Mapper Architecture

"Mapper" is the community term for the MMC (Memory Management Controller) circuitry. Nintendo internally called these by their board designations (NES-NROM, NES-MMC1, etc.) and the MMC chips themselves were custom ASICs fabbed by Nintendo, Sharp, and others.

The mapper's job is threefold:
1. **PRG banking** — Present different 8/16/32KB windows of PRG-ROM into $8000–$FFFF
2. **CHR banking** — Present different 1/2/4/8KB windows of CHR data into PPU $0000–$1FFF
3. **Nametable control** — Override CIRAM A10/CE to implement mirroring modes or extra VRAM

### How Bank Switching Works

The CPU cannot directly address more than 32KB of PRG-ROM ($8000–$FFFF). A mapper solves this by intercepting CPU writes to ROM space. Since ROM is read-only, writes to $8000–$FFFF don't corrupt anything — the mapper ASIC watches the address and data bus and latches bank register values on these "phantom writes." The latch outputs drive the upper address lines of the ROM chip beyond A14, selecting which physical 16KB or 8KB bank maps to which CPU window.

For example, a 256KB PRG-ROM needs 4 additional address bits (A15–A18 on the ROM chip). The mapper provides these from its internal bank registers, while the CPU's A0–A14 still select bytes within the active bank.

---

## Key Mappers — Technical Detail

### iNES Mapper 0: NROM

- **Board**: NES-NROM-128 (16KB PRG) or NES-NROM-256 (32KB PRG)
- **PRG**: 16KB (mirrored to fill $8000–$FFFF) or 32KB fixed
- **CHR**: 8KB ROM, fixed, no banking
- **No mapper chip** — purely discrete logic or just direct wiring
- **Mirroring**: Fixed, hardwired on PCB (H or V)
- Games: *Donkey Kong*, *Super Mario Bros.*, *Excitebike*

---

### iNES Mapper 1: MMC1 (SxROM boards)

**Custom ASIC**: Nintendo MMC1 (Sharp S-LSIC2A807 or similar)

MMC1 uses a **5-bit serial shift register** interface. The CPU writes bit 0 of successive writes to $8000–$FFFF, 5 writes to complete one register load. Bit 7 of any write resets the shift register. This serial approach saves address decoding complexity.

**Internal registers** (selected by A14–A13 of the CPU address):

| Address range | Register | Function |
|---|---|---|
| $8000–$9FFF | Control | Mirroring (2 bits), PRG mode (2 bits), CHR mode (1 bit) |
| $A000–$BFFF | CHR bank 0 | 5-bit CHR bank for $0000 (or $0000–$1FFF in 8KB mode) |
| $C000–$DFFF | CHR bank 1 | 5-bit CHR bank for $1000 (ignored in 8KB CHR mode) |
| $E000–$FFFF | PRG bank | 4-bit PRG bank + PRG-RAM enable |

**PRG banking modes:**
- Mode 0/1: Switch full 32KB at $8000
- Mode 2: Fix $8000–$BFFF to first bank, switch $C000–$FFFF
- Mode 3: Switch $8000–$BFFF, fix $C000–$FFFF to last bank

**CHR banking modes:**
- Mode 0: Switch full 8KB at $0000
- Mode 1: Switch two independent 4KB banks at $0000 and $1000

**PRG-ROM capacity**: Up to 512KB (but standard MMC1 is limited to 256KB; SUROM/SXROM variants add an extra bit via CHR bank bit 4)  
**CHR capacity**: Up to 128KB CHR-ROM or CHR-RAM (8KB)

Games: *The Legend of Zelda*, *Metroid*, *Mega Man 2*

---

### iNES Mapper 2: UxROM

**Logic**: Discrete 74-series gates (74HC32, 74HC161 or equivalent)

Very simple — a single register maps the **switchable $8000–$BFFF** 16KB window, while $C000–$FFFF is hardwired to the last bank. Register write to any address in $8000–$FFFF. 3 bits = 8 banks = 128KB PRG. Some board variants decode more bits.

No CHR-ROM; CHR is **8KB RAM** (a single 6264 or equivalent SRAM chip).

Games: *Mega Man*, *Castlevania*, *DuckTales*

---

### iNES Mapper 3: CNROM

Single register written to $8000–$FFFF selects one of four 8KB **CHR-ROM** banks. PRG is fixed (16 or 32KB NROM-style). Implemented with a **74HC374** latch or similar.

Games: *Gradius*, *Arkanoid*

---

### iNES Mapper 4: MMC3 (TxROM/TxSROM boards)

**Custom ASIC**: Nintendo MMC3 (various revisions: MMC3A, MMC3B, MMC3C — revision matters for IRQ behavior)

The MMC3 is the most sophisticated commonly-used mapper and introduces **scanline-based IRQs**.

**Register interface** — normal parallel writes, address decoded by A0 and A1 (and A14, A13 for range):

| Address | A0 | Function |
|---|---|---|
| $8000 | 0 | Bank select: selects which bank register (0–7) is targeted, plus PRG/CHR mode bits |
| $8001 | 1 | Bank data: writes to the selected register |
| $A000 | 0 | Mirroring (H/V) |
| $A001 | 1 | PRG-RAM protect/enable |
| $C000 | 0 | IRQ latch value |
| $C001 | 1 | IRQ reload (clears counter) |
| $E000 | 0 | IRQ disable + acknowledge |
| $E001 | 1 | IRQ enable |

**Bank registers:**

- R0, R1: CHR 2KB banks (mapped at $0000/$0800 or $1000/$1800 depending on mode)
- R2, R3, R4, R5: CHR 1KB banks
- R6, R7: PRG 8KB banks at $8000 and $A000 (or $C000/$A000 in alternate mode)
- $C000 is always the second-to-last 8KB PRG bank; $E000 always the last

**CHR banking**: Two 2KB banks + four 1KB banks; can be remapped to either half of the PPU CHR space via the CHR inversion bit.

**PRG banking**: Two switchable 8KB banks; the last two 8KB banks of ROM are fixed or semi-fixed.

**The IRQ mechanism** — this is the MMC3's defining feature:

The MMC3 monitors **PPU A12** (the CHR address bit that distinguishes pattern table 0 from pattern table 1). It contains a **falling-edge detector on /PPU_A12** followed by a filter (the MMC3 requires A12 to be low for a certain number of CPU clocks before it counts a rising edge — this filters out spurious glitches from the CPU bus). Each qualifying rising edge of A12 decrements a counter. When the counter reaches zero (or reloads) it fires /IRQ.

The PPU naturally toggles A12 during rendering: A12 = 0 for background tiles (fetched from $0000–$0FFF), A12 = 1 for sprite tiles (fetched from $1000–$1FFF), assuming the standard configuration. This happens predictably 8 times per scanline (one tile = 8 pixels). By setting the reload value to 8 or multiples thereof, games can count scanlines precisely.

The **MMC3 IRQ bug** (difference between MMC3A and MMC3B/C): The A and B revisions differ in what resets the internal "clock divider" filter. Some games depend specifically on MMC3B or MMC3C behavior and glitch on A-revision hardware or emulators that implement the wrong revision.

Games: *Super Mario Bros. 3*, *Mega Man 3–6*, *Kirby's Adventure*, *Contra*

---

### iNES Mapper 5: MMC5 (ExROM boards)

**Custom ASIC**: Nintendo MMC5 — the most powerful Nintendo first-party mapper.

MMC5 adds capabilities that essentially extend the NES hardware:

**PRG banking**: Up to 1MB PRG-ROM. Can bank in 8KB, 16KB, or 32KB modes. $6000–$7FFF can be mapped to PRG-ROM or RAM. A 1KB "fill mode" can fill nametables with a programmable tile/attribute without using actual RAM.

**CHR banking**: Up to 1MB CHR-ROM. Can switch in 1KB granularity across the full CHR space, independently for sprite and background rendering. 8 independent 1KB CHR bank registers for background, 8 more for sprites.

**Expanded nametable/attribute**: MMC5 provides additional 1KB RAM banks that can be mapped as nametables, supporting 4-screen or other exotic configurations.

**Split screen mode**: MMC5 can render the left portion of the screen using a different scroll position than the right — something the standard PPU cannot do. This uses a per-column tile counter that MMC5 monitors against a programmable split column.

**ExRAM ($5C00–$5FFF, 1KB)**: An additional 1KB of RAM on the MMC5 that can serve as extra nametable, extended attribute data (allowing per-tile palette selection beyond the standard 2-bit per 2×2 tile), or general-purpose RAM.

**Extended attribute mode**: Instead of using 2-bit palette per 32×32 pixel block (the PPU's hardware limit), MMC5 ExRAM in attribute mode provides **per-8×8-tile palette selection** — four times the color granularity. This requires MMC5 to substitute attribute data mid-frame.

**PCM audio channel**: MMC5 adds an 8-bit PCM audio DAC mixed into the audio output pin. Register $5011 = PCM value, clocked by CPU writes.

**Pulse wave channels**: Two additional square-wave channels (similar to the 2A03's internal channels but without sweep hardware), registers at $5000–$5003 and $5004–$5007.

**Scanline counter**: Independent of the PPU, MMC5 has its own scanline/frame counter with IRQ capability.

**Multiplier**: MMC5 contains a hardware 8×8→16-bit unsigned multiplier. Write to $5205 and $5206; read $5205/$5206 for the 16-bit result. This is a significant computational aid for fixed-point math-intensive games.

Games: *Castlevania III*, *Just Breed*, *Laser Invasion*

---

### iNES Mapper 9 & 10: MMC2 / MMC4 (PxROM/FxROM)

**Custom ASIC**: Nintendo MMC2 / MMC4

These mappers introduce **latch-based CHR bank switching** — the CHR bank register updates automatically based on which specific tile addresses the PPU reads.

MMC2 watches for the PPU fetching tile $FE or $FD from either pattern table. When $FE is fetched, it latches one CHR bank register into effect; when $FD is fetched, it latches another. This allows seamless CHR bank switching mid-frame without explicit writes, as long as certain "trigger tiles" appear in the graphics data.

Game: *Mike Tyson's Punch-Out!!* (MMC2)

---

### iNES Mapper 19: Namco 163 (N163)

**Custom ASIC**: Namco 163

The N163 is Namco's late-generation mapper and is extraordinary in capability:

**PRG banking**: Four 8KB switchable windows covering $8000–$FFFF, last bank fixed.

**CHR banking**: 8KB space with 1KB bank granularity, 8 registers.

**Internal RAM**: 128 bytes of internal RAM, accessible via address register at $F800 and data port at $4800. This RAM is also used by the audio engine.

**8-channel wavetable synthesizer**: This is the N163's signature feature. The audio engine reads 4-bit PCM samples from the internal 128-byte RAM and outputs up to 8 simultaneous channels. Each channel specifies: sample address (in the 128-byte RAM), sample length, frequency, and volume. The number of active channels is configurable (1–8); fewer channels = higher effective sample rate per channel. Maximum playback rate per channel is approximately **(master_clock / 15) / num_channels ≈ 119,318 Hz / num_channels**.

Games: *Final Fantasy III* (JP), *Megami Tensei II*, *Klax*

---

### iNES Mapper 21/23/25: VRC2 / VRC4 (Konami)

**Custom ASIC**: Konami VRC4

The VRC series uses **A0/A1 on the cartridge bus** (or sometimes A1/A2 depending on the board variant — this causes the multiple mapper numbers for the same chip, as different board revisions swap those pins).

**PRG banking**: Two switchable 8KB banks at $8000/$A000; $C000 and $E000 fixed.

**CHR banking**: Eight 1KB CHR bank registers, supporting up to 512KB CHR.

**IRQ**: A scanline timer composed of a prescaler and a scanline counter. The prescaler runs off M2 (CPU clock) and counts 341 (or ~341.25) cycles to approximate one scanline duration (341 PPU clocks ÷ 3 = ~113.67 CPU clocks). The IRQ counter decrements each "scanline" and fires /IRQ at zero. This is a **CPU-clock-based IRQ** rather than a PPU-event-based IRQ like MMC3, which makes it slightly less accurate (scanline length isn't exactly 341.25 CPU clocks evenly) but simpler.

---

### iNES Mapper 24/26: VRC6 (Konami)

**Custom ASIC**: Konami VRC6

Similar PRG/CHR banking to VRC4 but adds **three additional audio channels**: two variable-duty-cycle pulse waves (7 duty cycle settings, 12-bit period) and one **sawtooth wave generator** (accumulator-based, adds a value to a running sum every 1/7th of a wave period, creating a ramp).

Games: *Akumajō Densetsu* (JP Castlevania III), *Madara*, *Esper Dream 2*

---

### iNES Mapper 85: VRC7 (Konami)

**Custom ASIC**: Konami VRC7

Contains an **OPL2-compatible FM synthesis chip** (essentially a stripped-down version of the Yamaha YM2413/OPLL). The VRC7 provides 6 FM channels with a subset of the full YM2413 register set — it's actually a YM2413 derivative with a simplified patch ROM. Each channel is a 2-operator FM voice with a built-in instrument table (15 preset patches + 1 user-definable patch).

Games: *Lagrange Point*, *Tiny Toon Adventures 2* (JP)

---

### FME-7 / Sunsoft 5B (iNES Mapper 69)

**Custom ASIC**: Sunsoft FME-7

Uses a command/data register pair ($8000 = command, $A000 = data). PRG: three switchable 8KB banks + fixed last bank, plus $6000 bank can be mapped to ROM or RAM. CHR: eight 1KB banks.

The **Sunsoft 5B** variant adds a **Yamaha YM2149** (AY-3-8910 compatible) — three-channel square-wave + noise synthesis, the same chip used in many home computers of the era (ZX Spectrum, Amstrad CPC, Atari ST). Games using the audio hardware: *Gimmick!* — considered by many to have the best audio on the platform.

---

## PRG-RAM and Battery Backup

PRG-RAM typically uses a **6264 (8KB) or 62256 (32KB) SRAM** chip. Battery backup uses a small lithium coin cell (CR2032 or CR2016) wired through a diode to the SRAM VCC, so when the cartridge is unpowered the battery sustains the SRAM's ~3.5V data retention threshold. The diode prevents reverse current from the 5V supply into the battery.

Some advanced boards (like SXROM variants) use MMC1 bit 4 of the PRG bank register to disable the SRAM's /CE when not needed, reducing current draw.

---

## CHR-RAM vs CHR-ROM

CHR-ROM is mask ROM pre-programmed with tile data — **read-only** from the PPU's perspective, fast, cheap. CHR-RAM (usually a **6116 2KB or 6264 8KB SRAM**) is writable and used by games that generate or modify their own tile data at runtime (e.g., for decompression, animation tricks, or text rendering with variable fonts). Most UxROM and all MMC1-with-CHR-RAM games use this approach.

---

## The Famicom (HVC-001) Differences

The Famicom uses a **60-pin dual-row slot** on the motherboard — 28 pins per row on each side of the connector. Signals are largely equivalent but the form factor is very different. The Famicom also exposes the **expansion port** (bottom connector) which provides direct access to the CPU data bus, enabling Famicom Disk System or other hardware to interact with the system. The Famicom Disk System uses a RAM adapter cartridge that provides 32KB of WRAM and a BIOS ROM, while the FDS unit itself connects via the expansion port.

---

## iNES and NES 2.0 Format

The `.nes` ROM file format (iNES) encodes mapper number in the high nybbles of header bytes 6 and 7 (and NES 2.0 adds byte 8 for mapper bits 8–11, providing a 12-bit mapper number). The header also encodes PRG-ROM size (×16KB units), CHR-ROM size (×8KB units), mirroring, and battery-backup flags. NES 2.0 additionally specifies PRG-RAM and CHR-RAM sizes, sub-mapper variants (critical for distinguishing MMC3A from B/C), and region (NTSC/PAL/Dendy).

---

## Summary Table of Common Mappers

| Mapper | Name | PRG max | CHR max | Notable feature |
|---|---|---|---|---|
| 0 | NROM | 32KB | 8KB | No mapper chip |
| 1 | MMC1 | 512KB | 128KB | Serial interface, flexible banking |
| 2 | UxROM | 256KB+ | 8KB RAM | Discrete logic only |
| 3 | CNROM | 32KB | 32KB | CHR banking only |
| 4 | MMC3 | 512KB | 256KB | Scanline IRQ via A12 |
| 5 | MMC5 | 1MB | 1MB | Split screen, ExRAM, multiplier, audio |
| 9 | MMC2 | 128KB | 128KB | Auto-latching CHR banks |
| 19 | N163 | 512KB | 128KB+8ch wavetable | Wavetable audio |
| 21/25 | VRC4 | 512KB | 512KB | CPU-timed IRQ |
| 24/26 | VRC6 | 256KB | 256KB | Pulse + sawtooth audio |
| 69 | FME-7/5B | 512KB | 256KB | AY-3-8910 audio (5B) |
| 85 | VRC7 | 512KB | 256KB | YM2413 FM audio |

The NES cartridge system is a remarkably well-thought-out bus architecture — exposing both CPU and PPU buses to the cartridge was a deliberate design choice that enabled an enormous range of custom silicon to extend the console's capabilities far beyond what Nintendo initially envisioned.

---

# Complete NES/Famicom Cartridge Technical Classification for Emulation

This is a comprehensive reference covering every distinct hardware configuration (board/mapper) with full technical detail relevant to emulator implementation. Mappers are organized by manufacturer lineage, since that reflects actual chip families. iNES mapper numbers are given where applicable; NES 2.0 submapper distinctions are noted where they affect emulation behavior.

---

## Foundational Concepts for Emulation

Before the mapper catalogue, some cross-cutting emulation concerns:

### Open Bus Behavior
When no device drives the CPU data bus, the value read is the last byte that appeared on the bus — typically the high byte of the address, because the address lines capacitively couple into the data lines. This is **not** always 0xFF. Correctly implementing open bus is essential for several games.

### Bus Conflicts
On mappers with no dedicated write-decode logic (UxROM, CNROM, AxROM, BxROM, GxROM), when the CPU writes to ROM space, the ROM is simultaneously outputting the byte at that address onto the data bus while the CPU is trying to write a value. This is a **bus conflict**. The result is a bitwise AND of the written value and the ROM byte at that address. Some games deliberately write a value equal to the ROM byte (no conflict) while others rely on the AND behavior. Emulators must implement this or certain games behave incorrectly.

### PPU Timing and the 2C02/2C07
The PPU renders 262 scanlines (NTSC) or 312 scanlines (PAL) per frame. Scanlines 0–239 are visible, 240 is post-render idle, 241–260 are VBlank (NMI fires at start of scanline 241 dot 1), scanline 261 (−1) is the pre-render line. Each scanline is 341 PPU clocks wide. The pre-render scanline is 340 clocks on even frames, 341 on odd frames (the "skip tick"). PAL has 311 visible+vblank+prerender, different timing. All IRQ-generating mappers must account for this.

### CPU/PPU Clock Relationship
NTSC: 3 PPU clocks per CPU clock. PAL: 3.2 PPU clocks per CPU clock (PPU clock = 5 CPU clocks / ~3.2 — actually PAL uses a different clock ratio). For emulation, the standard approach is to run the PPU 3 ticks per CPU tick on NTSC.

### Address Line Aliasing
Many mappers only partially decode their register addresses. A mapper that responds to writes at $8000 may also respond to $8001–$9FFF if it only checks A15 and A14. This aliasing must be emulated — some games rely on it.

---

## Part I: Nintendo Official Mappers

---

### NROM — iNES Mapper 000

**Boards**: NES-NROM-128, NES-NROM-256, HVC-NROM (Famicom)

**No active mapper logic.** Pure passive routing.

**PRG**: 16KB (NROM-128) mirrored at $8000–$BFFF and $C000–$FFFF, or 32KB (NROM-256) covering $8000–$FFFF. Address lines A0–A13 route directly to ROM. A14 ignored on NROM-128.

**CHR**: 8KB ROM, hardwired to PPU $0000–$1FFF. All 13 CHR address lines (CA0–CA12) go straight to ROM.

**Mirroring**: Hardwired on PCB by which PPU pin CIRAM A10 is connected to. Either PA10 (vertical mirroring) or PA11 (horizontal mirroring). Some boards solder bridge.

**PRG-RAM**: None (no $6000 space)

**Emulation notes**:
- Simplest possible mapper. $6000–$7FFF reads return open bus.
- NROM-128: mirror the 16KB — address bit 14 of the PRG-ROM is always 0.
- No writes to ROM do anything (no bus conflicts in practice since nothing latches).
- Games: *Super Mario Bros.*, *Donkey Kong*, *Excitebike*, *Balloon Fight*, *Ice Climber*, *Pinball*

---

### MMC1 — iNES Mapper 001

**Boards**: NES-SAROM, NES-SBROM, NES-SCROM, NES-SEROM, NES-SFROM, NES-SGROM, NES-SHROM, NES-SJROM, NES-SKROM, NES-SLROM, NES-SNROM, NES-SOROM, NES-SUROM, NES-SXROM (the S-board family). Famicom equivalent: HVC-SLROM etc.

**Chip**: Nintendo custom ASIC, marketed as MMC1. Several silicon revisions exist (MMC1, MMC1A, MMC1B) but behavioral differences are minor and not emulation-critical.

#### Register Interface
Serial shift register, **5 bits**. CPU writes to any address $8000–$FFFF:
- Bit 7 = 1: Immediately reset shift register; set PRG mode to 3 (fix last bank at $C000)
- Bit 7 = 0: Shift bit 0 of written value into shift register from MSB end (i.e., first write = bit 0, fifth write = bit 4 in final register). On the **fifth** write, the accumulated 5-bit value is transferred to one of four internal registers based on address bits 14–13 of the fifth write address.

**Important timing**: The shift register ignores consecutive writes on consecutive CPU cycles. Only one write per two CPU cycles is accepted. This prevents a CPU read-modify-write instruction's spurious write from triggering a register load.

#### Internal Registers (selected by A14:A13 of the address bus on the 5th write):

**$8000–$9FFF — Control Register:**
```
Bit 4:   CHR banking mode (0=8KB, 1=4KB×2)
Bit 3:   PRG banking mode high bit
Bit 2:   PRG banking mode low bit  
Bit 1:   Mirroring high bit
Bit 0:   Mirroring low bit
```
PRG modes:
- 0, 1: 32KB switching at $8000 (bank select low bit ignored)
- 2: Fix $8000–$BFFF = first bank; switch $C000–$FFFF
- 3: Switch $8000–$BFFF; fix $C000–$FFFF = last bank (power-on default)

Mirroring modes:
- 0: Single screen, lower bank (nametable always from CIRAM page 0)
- 1: Single screen, upper bank (nametable always from CIRAM page 1)
- 2: Vertical mirroring
- 3: Horizontal mirroring

**$A000–$BFFF — CHR Bank 0:** 5-bit bank number. In 8KB mode, selects full 8KB. In 4KB mode, selects $0000–$0FFF.

**$C000–$DFFF — CHR Bank 1:** 5-bit bank number. In 4KB mode, selects $1000–$1FFF. Ignored in 8KB mode.

**$E000–$FFFF — PRG Bank:**
```
Bit 4:   PRG-RAM disable (1=disable chip enable on WRAM)
Bit 3–0: PRG bank (14-bit address selection into PRG-ROM)
```

#### PRG-ROM Capacity Variants
Standard MMC1: 4-bit PRG bank = 16 banks × 16KB = 256KB max.

**SUROM** (512KB PRG): Uses CHR Bank 0 bit 4 as PRG address bit 18. The CHR registers effectively steal one bit for addressing. When CHR mode is 8KB, both CHR banks share bit 4 of CHR0.

**SXROM** (256KB PRG + 32KB CHR-RAM + battery): Uses CHR banks to select 8KB WRAM bank as well as extended CHR. Register bit interpretation depends on CHR mode and board.

**SOROM** (256KB PRG + 16KB PRG-RAM): CHR bank bit 3 selects which 8KB PRG-RAM bank is active in $6000–$7FFF.

#### CHR-RAM Boards
SGROM, SNROM: No CHR-ROM. 8KB CHR-RAM. CHR bank registers ignored for banking but mirroring bits and PRG bank bits still function.

#### Emulation Notes
- Power-on state: all registers = 0 except PRG mode set to 3. The shift register starts clear.
- The consecutive-write filter: track last CPU cycle a write occurred; ignore if it's the immediately next cycle. This is triggered by instructions like `STA $8000` followed by another `STA $8001` in the same routine — rare but possible.
- WRAM ($6000–$7FFF): PRG-RAM disable bit should actually disable reads/writes (return open bus on read). Some older emulators ignored this. On SNROM, the /WRAM enable is tied to the PRG-RAM disable bit AND requires A13 high.
- **Games**: *The Legend of Zelda*, *Zelda II*, *Metroid*, *Mega Man 2*, *Mega Man 3*, *Final Fantasy*, *Tetris* (Nintendo version), *Bionic Commando*, many others

---

### UxROM — iNES Mapper 002

**Boards**: NES-UOROM (256KB), NES-UNROM (128KB), HVC-UN1ROM (Famicom 128KB variant with different pinout). Also used by several third parties with discrete logic.

**Logic**: Discrete 74-series. Typically a **74HC32** (OR gates for address decode) + **74HC161** or **74HC163** (4-bit counter used as latch). Some boards use 74HCT373 latches.

#### Banking
- **$8000–$BFFF**: Switchable 16KB PRG bank. Controlled by writing to $8000–$FFFF (A15 = 1 selects the register). Low 4 bits (UOROM) or 3 bits (UNROM) select the bank.
- **$C000–$FFFF**: Fixed to **last** 16KB bank of ROM (hardwired — the highest address line of the ROM chip is tied high).
- **CHR**: 8KB CHR-RAM (6264 SRAM). No CHR-ROM on this board.
- **Mirroring**: Fixed, hardwired (usually vertical).

#### Bus Conflicts
UxROM is the canonical bus-conflict mapper. When writing a bank number, the ROM simultaneously outputs the byte at the current PC+offset. The written byte is AND'd with the ROM byte. Properly written games put the bank number in a ROM location where the byte value equals the bank number. Emulators must implement bus conflicts for accuracy, as some games (particularly homebrew) may trigger them.

**UN1ROM** board variant: The address decoding is slightly different — /ROMSEL drives the latch clock instead of M2. This affects the timing of when the register latches in edge cases, but for practical emulation it's the same behavior.

#### Emulation Notes
- Writes can go to any address $8000–$FFFF; A0–A14 don't matter for register select.
- On power-on: bank 0 at $8000–$BFFF, last bank at $C000–$FFFF.
- $6000–$7FFF: Open bus (no RAM on these boards).
- **Games**: *Mega Man*, *Castlevania*, *DuckTales*, *Contra*, *Paperboy*, *Ninja Gaiden* (Tecmo used their own discrete logic board with identical behavior)

---

### CNROM — iNES Mapper 003

**Boards**: NES-CNROM, HVC-CNROM

**Logic**: Single **74HC174** (hex D flip-flop) or **74HC374** latch.

#### Banking
- **PRG**: Fixed 16KB or 32KB, no switching. Identical to NROM for PRG.
- **CHR**: 8KB bank switching. Writing to $8000–$FFFF latches bits 1–0 (or 1–0 depending on variant) as CHR bank number. 2 bits = 4 banks = 32KB max CHR-ROM.

#### Bus Conflicts
Yes, same issue as UxROM. The written byte is AND'd with the PRG-ROM byte at the write address.

#### Security Variant (iNES 003, submapper 3)
Some CNROM boards (notably used by *Panesian* adult games) have a security latch: only specific data patterns enable CHR bank access; otherwise CHR reads return a fixed pattern. Emulators need submapper 3 for this behavior.

#### Emulation Notes
- Some boards only decode A15 (respond to full $8000–$FFFF range); others decode more bits.
- CHR bus conflict: the PPU reads are irrelevant since CHR is ROM, but CPU write bus conflicts apply.
- **Games**: *Gradius*, *Arkanoid*, *Hyper Sports*, *Kung Fu*, many others

---

### AxROM — iNES Mapper 007

**Boards**: NES-AMROM, NES-ANROM, NES-AOROM, NES-APROM

**Logic**: Discrete 74-series gates.

#### Banking
- **PRG**: Single 32KB bank switching, covering full $8000–$FFFF. 3 bits = 8 banks = 256KB max.
- **CHR**: 8KB CHR-RAM only. No CHR-ROM.
- **Mirroring**: Controlled by **bit 4** of the register write. 0 = single-screen lower (CIRAM page 0), 1 = single-screen upper (CIRAM page 1). This is single-screen mirroring, not H/V. All four logical nametables map to the same 1KB.

#### Register
Write to $8000–$FFFF:
```
Bit 4:   Nametable select (single-screen page)
Bit 2–0: PRG 32KB bank
```

#### Bus Conflicts
Yes. Same mechanism as UxROM — data line conflict during writes to ROM space.

#### Emulation Notes
- Single-screen mirroring is unusual: both CIRAM A10 and CIRAM /CE driven by mapper output.
- $6000–$7FFF: Open bus.
- **Games**: *Battletoads*, *Wizards & Warriors*, *Marble Madness*, *RC Pro-Am*

---

### BxROM — iNES Mapper 034

**Boards**: NES-BNROM

**Logic**: Discrete.

#### Banking
- **PRG**: 32KB bank switching (same as AxROM bank size), 2 bits, 4 banks = 128KB.
- **CHR**: 8KB CHR-RAM.
- **Mirroring**: Fixed, hardwired.
- Write to $8000–$FFFF, bits 1–0.

#### Bus Conflicts
Yes.

**Note**: Mapper 034 is shared with **NINA-001** (see below), differentiated by NES 2.0 submapper.

---

### GxROM / MxROM — iNES Mapper 066

**Boards**: NES-GNROM, NES-MHROM

**Logic**: Single 74HC174 latch.

#### Banking
- **PRG**: 32KB switching, bits 5–4 of written value. 2 bits = 4 banks = 128KB max.
- **CHR**: 8KB switching, bits 1–0 of written value. 2 bits = 4 banks = 32KB max.
- Write to $8000–$FFFF.

#### Bus Conflicts
Yes.

#### Emulation Notes
- Both PRG and CHR banking in one register write.
- MHROM is a smaller capacity variant (often called "HiROM" colloquially — not related to SNES HiROM).
- **Games**: *Super Mario Bros. + Duck Hunt* multi-cart, *Gumshoe*, *Track & Field*

---

### MMC2 — iNES Mapper 009

**Boards**: NES-PNROM

**Chip**: Nintendo MMC2 ASIC

#### Banking
- **PRG**: 
  - $8000–$9FFF: Switchable 8KB bank. Register write to $A000–$AFFF, bits 3–0.
  - $A000–$FFFF: Fixed to last three 8KB banks.
- **CHR**: 4KB × 2 banks with **automatic latching**.

#### CHR Latch Mechanism (Critical for emulation)
This is the defining feature. MMC2 monitors the PPU address bus in real time and switches CHR banks based on specific tile reads:

Two sets of two registers:
- **Latch 0 banks** (for $0000–$0FFF): Written at $B000–$BFFF (latch=0 bank) and $C000–$CFFF (latch=1 bank)
- **Latch 1 banks** (for $1000–$1FFF): Written at $D000–$DFFF (latch=0 bank) and $E000–$EFFF (latch=1 bank)

Each pattern table half ($0000–$0FFF and $1000–$1FFF) has an independent **1-bit latch** that selects which of its two bank registers is active. The latch value changes automatically:
- When PPU reads tile at address $0FD8–$0FDF (tile $FD in left table): Set latch 0 → 0
- When PPU reads tile at address $0FE8–$0FEF (tile $FE in left table): Set latch 0 → 1
- When PPU reads tile at address $1FD8–$1FDF (tile $FD in right table): Set latch 1 → 0
- When PPU reads tile at address $1FE8–$1FEF (tile $FE in right table): Set latch 1 → 1

The trigger address is actually **$xFD8–$xFDF** (PPU A12:A4 = specific pattern). The latch switches **during** the PPU tile fetch (not after), so the fetched tile itself uses the *new* bank. This is important: the triggering tile is fetched using the post-switch bank.

**Mirroring**: H/V controlled by register at $F000–$FFFF, bit 0.

#### Emulation Notes
- The CHR latch mechanism requires cycle-accurate PPU address monitoring during rendering. The latch switches on any read within the trigger tile address range — including the attribute fetch that happens on the same PPU clock cycle as tile address $xx38.
- In practice, the game places tiles $FD and $FE at strategic positions to force bank switches at the right moment during rendering.
- This mapper is used **exclusively** by *Mike Tyson's Punch-Out!!* and *Punch-Out!!* (a single game in two SKUs).

---

### MMC4 — iNES Mapper 010

**Boards**: NES-FKROM

**Chip**: Nintendo MMC4 ASIC

Identical to MMC2 except:
- **PRG**: 16KB switching at $8000–$BFFF; $C000–$FFFF fixed to last 16KB bank.
- PRG bank register at $6000–$6FFF (not $A000).
- CHR latch mechanism identical to MMC2.
- 8KB CHR-RAM + battery-backed PRG-RAM.

**Games**: *Fire Emblem*, *Fire Emblem Gaiden* (Famicom-only)

---

### MMC3 — iNES Mapper 004

**Boards**: NES-TBROM, NES-TEROM, NES-TFROM, NES-TGROM, NES-TKROM, NES-TKSROM, NES-TLROM, NES-TL1ROM, NES-TL2ROM, NES-TNROM, NES-TSROM, NES-TVROM, NES-TR1ROM

**Chip**: Nintendo MMC3, revisions A, B, C.

#### Register Interface
Normal parallel writes, address decoded on A0 and A14:A13 (address range $8000–$FFFF, with A0 distinguishing register pairs):

| Address | A0 | Register |
|---|---|---|
| $8000 | 0 | Bank select |
| $8001 | 1 | Bank data |
| $A000 | 0 | Mirroring |
| $A001 | 1 | PRG-RAM protect |
| $C000 | 0 | IRQ latch |
| $C001 | 1 | IRQ reload |
| $E000 | 0 | IRQ disable / acknowledge |
| $E001 | 1 | IRQ enable |

Any write to $8000 (even addresses) updates bank select; any write to $8001 (odd addresses) writes data to currently selected bank. The $8000/$8001 are not specifically those addresses — any address in range with A0=0 hits bank select, A0=1 hits bank data. So $8002 also acts as bank select, $8003 as bank data, etc. The full range $8000–$9FFF (A15=1, A14=0... actually the decode is A15=1, A13=0, A14=variable).

Actually more precisely: MMC3 decodes **A15=1** (ROM range), and uses **A14, A13, A0** to distinguish the 8 registers. $8000–$9FFF has A14=0,A13=0; $A000–$BFFF has A14=0,A13=1; $C000–$DFFF has A14=1,A13=0; $E000–$FFFF has A14=1,A13=1. Within each pair, A0 distinguishes the two registers.

#### Bank Select Register ($8000 writes):
```
Bit 7:   PRG ROM bank mode
         0: $8000–$9FFF switchable, $C000–$DFFF fixed to second-last bank
         1: $C000–$DFFF switchable, $8000–$9FFF fixed to second-last bank
Bit 6:   CHR A12 inversion
         0: 2KB CHR banks at $0000–$0FFF, 1KB at $1000–$1FFF
         1: 2KB CHR banks at $1000–$1FFF, 1KB at $0000–$0FFF
Bit 2–0: R register select (0–7)
```

#### Bank Registers (written via $8001):

| R | What it controls | Bits used | Notes |
|---|---|---|---|
| 0 | CHR 2KB at lower addr (A12=0, CHR A12 per invert bit) | 7–0 (low bit ignored) | Selects 2KB, so bit 0 masked |
| 1 | CHR 2KB at lower addr + $0800 | 7–0 (low bit ignored) | |
| 2 | CHR 1KB at upper addr | 7–0 | |
| 3 | CHR 1KB at upper addr + $0400 | 7–0 | |
| 4 | CHR 1KB at upper addr + $0800 | 7–0 | |
| 5 | CHR 1KB at upper addr + $0C00 | 7–0 | |
| 6 | PRG 8KB bank (at $8000 or $C000 per mode) | 5–0 | |
| 7 | PRG 8KB bank (at $A000) | 5–0 | |

Fixed banks: second-to-last 8KB at $C000 (or $8000), last 8KB at $E000. Always. These never move regardless of bank mode — only which of ($8000 or $C000) is R6 vs fixed swaps.

PRG-RAM ($6000–$7FFF): Always 8KB. Controlled by $A001:
- Bit 7: 1 = RAM enabled; 0 = disabled (reads return open bus)
- Bit 6: 1 = write-protect (reads allowed, writes ignored); 0 = writable

Some boards tie this pin differently or don't implement it — emulators should implement it but some games break if WRAM protection isn't handled.

Mirroring ($A000): Bit 0: 0 = vertical, 1 = horizontal. (Note: TxSROM and TQROM use 4-screen nametable — see below.)

#### IRQ Mechanism (Critical for Accuracy)

The MMC3 monitors **PPU address line A12**. The rising edge (0→1 transition) of A12 clocks a scanline counter. However there is a crucial filter:

A12 must be held **low** for a "sufficiently long time" before a rising edge is counted. The exact threshold is debated; the most accurate emulations require A12 to be low for approximately **12 M2 cycles** (CPU clocks, since M2 = CPU clock). Some implementations use a shorter threshold.

**Why this filter exists**: The PPU address bus transitions through multiple values during a single clock cycle. Without filtering, every cycle that contains a 0→1 on A12 would count, giving spurious counts. The filter ensures only "real" scanline boundaries (where the PPU switches from background tiles at $0xxx to sprite tiles at $1xxx or vice versa) are counted.

**IRQ counter behavior**:
- $C000 write: Load the IRQ latch register (value for next reload).
- $C001 write: Reload the counter immediately from the latch. **If counter is currently 0**, also immediately clock (decrement) the counter. This is an MMC3 quirk.
- $E000 write: Disable IRQ, acknowledge any pending IRQ.
- $E001 write: Enable IRQ.
- On each qualifying A12 rising edge: if counter == 0, reload from latch; else decrement. If counter transitions to 0 **and** IRQ is enabled, assert /IRQ.

Actually, the exact reload-then-decrement vs decrement-then-reload ordering varies by revision:
- **MMC3A**: Counter reloads when it hits 0, and on $C001 it's unclear — certain games behave differently
- **MMC3B/C**: The $C001 reload-at-zero quirk is the canonical behavior for most games

#### NES 2.0 Submapper 1 (MMC3 with MC-ACC)
Some Acclaim boards use a slightly different MMC3-compatible chip (MC-ACC) that has different IRQ acknowledge behavior — the IRQ is acknowledged on $E000 write (same) but the exact timing differs.

#### Special Board Variants

**TKSROM, TQROM**: 8KB CHR-RAM + CHR-ROM combined. A register bit selects whether a given CHR bank accesses RAM or ROM. TQROM: CHR bank register bit 6 selects RAM vs ROM for that bank.

**TxSROM**: 4-screen nametable. Extra 2KB CIRAM on cart. Mirroring register repurposed: bit 0 selects which nametable SRAM page for the top half.

**TNROM**: Battery-backed PRG-RAM 8KB.

**TR1ROM**: 4-screen via extra VRAM. Similar to TxSROM.

#### Emulation Notes
- MMC3 is one of the hardest mappers to emulate accurately due to IRQ timing.
- The A12 filter timing varies across revisions. A common approach: count the number of PPU cycles since A12 was last low; if > threshold, count the edge.
- Rendering must be emulated with PPU accuracy because A12 depends on which address the PPU is fetching. A12=1 during sprite tile fetches (if sprites use $1000 pattern table) and during background fetches from $1000.
- Test ROM: *Battletoads* is notorious for requiring accurate MMC3 IRQ behavior.
- **Games**: *Super Mario Bros. 3*, *Kirby's Adventure*, *Mega Man 3–6*, *Ninja Gaiden II & III*, *Contra*, *Batman*, *Teenage Mutant Ninja Turtles II & III*, *Double Dragon II*

---

### MMC5 — iNES Mapper 005

**Boards**: NES-ELROM, NES-EKROM, NES-ETROM, NES-EWROM, NES-EOROM (ExROM family)

**Chip**: Nintendo MMC5 ASIC — the most capable Nintendo first-party mapper.

#### Register Map ($5000–$5FFF range)

All registers at $5000–$5FFF are in the CPU expansion space:

```
$5100       PRG mode (bits 1–0)
$5101       CHR mode (bits 1–0)
$5102–$5103 PRG-RAM protect A and B (both must = %10 and %01 respectively to enable WRAM writes)
$5104       ExRAM mode (bits 1–0)
$5105       Nametable mapping (8 bits, 2 bits × 4 nametables)
$5106       Fill tile (value written into fill-mode nametable)
$5107       Fill attribute (2 bits, replicated to all 8×8 tiles in fill-mode nametable)
$5113       PRG-RAM bank ($6000–$7FFF)
$5114–$5117 PRG bank registers 0–3
$5120–$5127 CHR bank registers 0–7 (sprite set)
$5128–$512B CHR bank registers 8–11 (background set)
$512C       Upper CHR bank bits (for >256KB CHR)
$5200       Split mode
$5201       Split scroll value
$5202       Split tile
$5203       IRQ scanline target
$5204       IRQ status and enable
$5205–$5206 Multiplier inputs (write separately, read $5205 for lo byte, $5206 for hi byte of product)
$5C00–$5FFF ExRAM (1KB, accessible as memory map)
```

#### PRG Banking (complex)

**PRG mode** ($5100 bits 1–0):
- Mode 0: One 32KB bank covering $8000–$FFFF, selected by $5117 bits 6–0
- Mode 1: Two 16KB banks. $5115 selects $8000–$BFFF, $5117 selects $C000–$FFFF
- Mode 2: $8000–$BFFF → $5115 (16KB), $C000–$DFFF → $5116 (8KB), $E000–$FFFF → $5117 (8KB)
- Mode 3: Four 8KB banks. $5114→$8000, $5115→$A000, $5116→$C000, $5117→$E000

Bank registers encode ROM vs RAM:
- Bit 7 = 0: selects PRG-ROM bank (bits 6–0 = bank number)
- Bit 7 = 1: selects PRG-RAM bank (bits 1–0 = which 8KB of PRG-RAM)

PRG-RAM: Up to 64KB (4 × 8KB banks), accessed in $6000–$7FFF. Protected by $5102/$5103 write-enable pattern. EWROM board has 32KB battery-backed RAM.

#### CHR Banking (very complex)

**CHR mode** ($5101 bits 1–0):
- Mode 0: One 8KB CHR bank for all ($5127 or $512B)
- Mode 1: Two 4KB banks ($5123/$512B for $0000/$1000)
- Mode 2: Four 2KB banks
- Mode 3: Eight 1KB banks (most flexible, most common in ExROM games)

**Two independent CHR bank sets**:
- **Sprite banks** ($5120–$5127): Eight 1KB bank registers for sprite tile fetches
- **Background banks** ($5128–$512B): Four 1KB bank registers for background tile fetches

Which set is active depends on **whether the PPU is currently fetching sprite tiles or background tiles**, determined by the PPU rendering state that MMC5 monitors internally. MMC5 tracks the PPU's internal state by watching the PPU address bus and /RD signal.

Bank register high bit selects CHR-ROM vs CHR-RAM. $512C provides additional high-order address bits for >256KB CHR.

#### Nametable Mapping ($5105)

Two bits per nametable (nametables 0–3), four nametables = 8 bits total:
- 00: CIRAM page 0 (internal NES VRAM)
- 01: CIRAM page 1 (internal NES VRAM)
- 10: ExRAM (MMC5's internal 1KB)
- 11: Fill-mode (generates tile/attribute from $5106/$5107, no actual RAM read)

This allows any combination of internal VRAM, ExRAM, and fill-mode across the four nametable slots — enabling 4-screen scrolling, unique backgrounds, or fill-based static screens.

#### ExRAM ($5C00–$5FFF)

1KB of RAM on the MMC5 chip itself. **ExRAM mode** ($5104):
- Mode 0: ExRAM used as extended attribute memory (PPU uses it during rendering)
- Mode 1: ExRAM used as attribute data only
- Mode 2: ExRAM is general-purpose RAM (read/write by CPU, not visible to PPU)
- Mode 3: ExRAM is read-only to CPU

**Extended attribute mode**: In mode 0 or 1, during background rendering, the PPU normally reads attribute data from $23C0/$27C0/$2BC0/$2FC0. MMC5 intercepts this and instead provides data from ExRAM indexed by the current tile position. Each byte in ExRAM encodes:
- Bits 7–6: Upper CHR bank bits (bits 9–8 of the 10-bit bank number), allowing per-tile bank switching
- Bits 1–0: Palette select for this 8×8 tile

This gives **per-8×8-pixel palette selection** (vs the PPU's native per-32×32-pixel) and **4096 possible CHR banks** (instead of the standard 256). It creates the appearance of much higher-resolution attribute data.

#### Split Screen ($5200–$5202)

MMC5 can render the left portion of the screen with a different tile/scroll than the right. The split is column-based.

$5200:
```
Bit 7:   Enable split screen
Bit 6:   Side (0=left side split, 1=right side split)
Bit 4–0: Split X tile column (0–31)
```
$5201: Y scroll value for the split region  
$5202: NT tile offset for split region

When rendering reaches the split boundary column, MMC5 substitutes different PPU address values onto the bus, effectively making the PPU read from a different nametable location. This requires cycle-level PPU address interception.

#### IRQ ($5203–$5204)

MMC5 has an **in-frame scanline counter** independent of PPU address bus tricks:

$5203: Target scanline number (0–255)  
$5204 read:
- Bit 7: Frame status (1 = currently in-frame, i.e., scanlines 0–239 being rendered)
- Bit 6: IRQ pending (cleared on read)

$5204 write bit 7: IRQ enable

MMC5 internally counts scanlines by monitoring the PPU rendering activity. When the scanline counter matches $5203 and IRQ is enabled, /IRQ is asserted. The counter resets at the start of each frame.

Emulation of MMC5's scanline detection requires tracking when the PPU transitions from one scanline to the next based on PPU address bus patterns (specifically the point where the PPU reads dummy sprite tiles at the end of each visible scanline).

#### Multiplier ($5205–$5206)

Write: $5205 = multiplicand A, $5206 = multiplicand B (both 8-bit unsigned)  
Read: $5205 = low byte of product, $5206 = high byte of product  
Result is available on the **same cycle** as the write — there's no latency. The multiplication is combinatorial.

#### Audio

MMC5 provides three audio channels mixed into the console output:

**Pulse 1** ($5000–$5003) and **Pulse 2** ($5004–$5007): Identical register layout to the 2A03's internal pulse channels, **except** there is no sweep unit. Same 4-bit volume, 2-bit duty cycle, 11-bit period, length counter, envelope. The length counter and envelope function identically to 2A03 internal channels.

**PCM** ($5010–$5011):
- $5010 bit 0: IRQ enable on PCM channel; bit 7: read mode (1) vs write mode (0)
- $5011: Write 8-bit PCM sample directly. The value is immediately output as a DC-biased analog level.
- In read mode: reading $5010 returns the current PCM value and triggers IRQ if enabled.

PCM can be clocked by CPU writes at arbitrary rates limited by the CPU's ability to write ($5011) — typically games use DMA or NMI-driven playback.

The two pulse channels use the same timer relationship as 2A03 pulses: output period = (register value + 1) × 16 CPU clocks. The PCM channel is a direct DAC.

Audio mixing: MMC5 audio drives the EXP pin on the cartridge connector, which feeds into the NES audio mixer. The mix ratio differs between NTSC NES, Famicom, and PAL NES.

#### Emulation Notes
- MMC5 is the most complex mapper to implement correctly.
- Accurate CHR bank switching between sprite and background fetches requires knowing the PPU's internal dot-level rendering state.
- Extended attribute mode requires intercepting PPU attribute fetches and substituting ExRAM data instead.
- Split screen requires intercepting PPU tile/attribute address generation mid-scanline.
- The multiplier is trivial to implement but easy to forget.
- PRG-RAM protect: both registers $5102 and $5103 must simultaneously hold specific values or WRAM writes are silently discarded. $5102 must be %10 and $5103 must be %01 (only bits 1–0 matter in each).
- **Games**: *Castlevania III: Dracula's Curse* (US), *Just Breed*, *Laser Invasion*, *Metal Slader Glory*, *Uncharted Waters*, *Romance of the Three Kingdoms II*

---

## Part II: Konami Mappers (VRC Series)

Konami's VRC (Video ROM Controller) series are custom ASICs. Multiple VRC chip variants exist; differences between boards are often just address line swaps (A0/A1 or A1/A2 physically swapped), which maps to different iNES mapper numbers for the same chip.

---

### VRC1 — iNES Mapper 075

**Chip**: Konami VRC1

**Registers**: Writes to $8000–$FFFF; address decoded via A14:A12:

| Address | Function |
|---|---|
| $8000 | PRG bank 0 ($8000–$9FFF), bits 3–0 |
| $9000 | Mirroring (bit 0=H/V) + CHR high bits (bit 1 = CHR0 bit 4, bit 2 = CHR1 bit 4) |
| $A000 | PRG bank 1 ($A000–$BFFF), bits 3–0 |
| $C000 | PRG bank 2 ($C000–$DFFF), bits 3–0 |
| $E000 | CHR bank 0 ($0000–$0FFF), bits 3–0 |
| $F000 | CHR bank 1 ($1000–$1FFF), bits 3–0 |

**PRG**: Three switchable 8KB banks + last 8KB fixed at $E000. 4-bit bank = 16 banks = 128KB max.
**CHR**: Two 4KB banks. 5-bit effective address (4 bits from $E000/$F000 + 1 bit from $9000) = 32 banks = 128KB.

**Famicom-only**: VRC1 was only used in Japan. US equivalents don't exist.

**Games**: *Exciting Soccer* (JP), *Tetsuwan Atom* (JP)

---

### VRC2 — iNES Mapper 022, 023, 025

**Chip**: Konami VRC2 (and VRC4 — same chip, different board wiring)

VRC2 and VRC4 are actually the same ASIC but with different address lines connected. The mapper numbers reflect which physical A0/A1 lines are swapped:

- Mapper 022: VRC2a (A0, A1 normal)
- Mapper 023: VRC2b (A0 and A1 inputs swapped on the chip vs board trace)
- Mapper 025: VRC2c (A1 and A0 further swapped)

For the purposes of emulation: VRC2 is a simpler subset of VRC4 (no IRQ, simpler CHR banking granularity).

#### VRC2 Registers

Decoded by A14:A13:A12 (coarse) and A0:A1 (fine, the swapped pair):

| Coarse address | A1A0 = 00 | A1A0 = 01 | A1A0 = 10 | A1A0 = 11 |
|---|---|---|---|---|
| $8000 | PRG bank 0 lo | PRG bank 0 hi | PRG bank 0 lo | PRG bank 0 hi |
| $9000 | Mirroring | Mirroring | Mirroring | Mirroring |
| $A000 | PRG bank 1 lo | PRG bank 1 hi | PRG bank 1 lo | PRG bank 1 hi |
| $B000–$EFFF | CHR banks 0–7 (nibble pair per bank) | | | |

(The exact interleaving of A0/A1 in address decode varies per VRC2 variant — this is what the mapper numbers encode.)

**PRG**: 8KB banks, two switchable at $8000 and $A000, last two fixed.
**CHR**: Eight 1KB banks covering $0000–$1FFF. Each bank takes two writes (low nibble, high nibble) to set a full bank number.

---

### VRC4 — iNES Mapper 021, 023, 025 (overlaps VRC2)

VRC4 adds:
1. An independent **IRQ timer** (see below)
2. Finer CHR bank granularity and more PRG flexibility

**PRG mode register** ($9000 low bit): 
- 0: $8000 switchable + $C000 fixed to second-last bank
- 1: $8000 fixed to second-last bank + $C000 switchable
(Both $A000 and $E000 always track their own banks; $E000 always = last bank.)

**VRC4 IRQ Mechanism**:

Register $F000 (low) = IRQ latch low byte  
Register $F001 (? via A0/A1 decode) = IRQ latch high nibble (bits 3–0)  
Register $F002: IRQ control
```
Bit 2: Enable IRQ after acknowledgment
Bit 1: Enable IRQ now
Bit 0: Cycle mode (0=scanline, 1=CPU cycle)
```
Register $F003: IRQ acknowledge (reading or writing clears pending IRQ; copies Enable-after to Enable)

**IRQ counter**: 16-bit (but only 12 effective bits). A prescaler running off the CPU clock counts to 341 (approximately one scanline's worth of CPU cycles). Each time the prescaler wraps, the 8-bit IRQ counter increments. When the counter overflows from $FF to $00, /IRQ is asserted if enabled.

In **CPU cycle mode** (bit 0 = 1): the 8-bit counter increments every CPU cycle, bypassing the prescaler.

The prescaler target is **341** CPU clocks per scanline (which is 341 PPU clocks / 3 = ~113.67 CPU clocks — but the prescaler uses **341** as its target, which is the PPU clock count, not CPU clock count). This means the scanline mode is slightly inaccurate relative to actual PPU scanlines. In emulation, the standard implementation counts CPU cycles with a prescaler that triggers at 341.

**Mirroring**: Register $9000 bits 1–0: 0=vertical, 1=horizontal, 2=single-screen lower, 3=single-screen upper.

**Games**: *Contra* (Famicom JP version uses VRC4), many Konami Famicom titles

---

### VRC3 — iNES Mapper 073

**Chip**: Konami VRC3. Unique among VRC chips — used only for one game and has an unusual feature set.

**PRG**: 16KB switching at $8000–$BFFF; $C000–$FFFF fixed to last bank. Register at $F000 bits 3–0.

**CHR**: 8KB CHR-RAM, no banking. All PPU CHR accesses go to SRAM.

**IRQ**: Unusually, this mapper has a **16-bit IRQ counter** directly (no prescaler):
- $8000: IRQ latch bits 3–0
- $9000: IRQ latch bits 7–4
- $A000: IRQ latch bits 11–8
- $B000: IRQ latch bits 15–12
- $C000: IRQ control (bit 1 = enable, bit 0 = acknowledge)
- $D000: IRQ acknowledge

When enabled, the counter increments every CPU cycle. At $FFFF → $0000 overflow, /IRQ fires.

**Games**: *Salamander* (JP version of *Life Force*) — exclusively.

---

### VRC6 — iNES Mapper 024, 026

**Chip**: Konami VRC6

Two board variants with A0/A1 swapped:
- Mapper 024: VRC6a
- Mapper 026: VRC6b

#### Banking (similar to VRC4):
- PRG: 16KB at $8000, 8KB at $C000, last 8KB fixed at $E000
- CHR: Eight 1KB banks

#### VRC6 Audio Registers:

**Pulse channel 1** ($9000–$9002):
```
$9000: Duty cycle (bits 6–4, 7 possible values), volume (bits 3–0), bit 7 = halt envelope
$9001: Period low byte
$9002: Period high nibble (bits 3–0), enable (bit 7)
```
Pulse period: (period_register + 1) × CPU clocks per half-cycle. 7 duty cycle modes: 6.25%, 12.5%, 18.75%, 25%, 31.25%, 37.5%, 43.75%. When bit 7 of $9000 is set, output is always at full volume (overrides duty cycle).

**Pulse channel 2** ($A000–$A002): Identical register layout.

**Sawtooth channel** ($B000–$B002):
```
$B000: Accumulator rate (6 bits, bits 5–0)
$B001: Period low byte
$B002: Period high nibble + enable (bit 7)
```
The sawtooth generates a ramp wave. Internal 8-bit accumulator adds the rate value 7 times per complete wave cycle, then resets. Output is the accumulator value (top 5 bits drive the DAC for a 5-bit amplitude, giving the characteristic stepped ramp). The accumulator increments on the falling edge of an internal counter derived from the period register.

#### IRQ: Same mechanism as VRC4 (scanline prescaler at 341 cycles or direct CPU cycle mode).

**Games**: *Madara*, *Esper Dream 2*, *Akumajō Densetsu* (JP Castlevania III, where VRC6 provides dramatically richer audio than the US MMC3 version)

---

### VRC7 — iNES Mapper 085

**Chip**: Konami VRC7

**PRG/CHR banking**: Same architecture as VRC4 (six 8KB PRG banks, eight 1KB CHR banks, IRQ).

#### VRC7 FM Audio

The VRC7 integrates an **FM synthesis core** derived from the Yamaha YM2413 (OPLL). It provides **6 FM voice channels** using 2-operator FM synthesis.

Access via two registers:
- $E000: Address register (write FM chip register number)
- $E010: Data register (write value to addressed FM register)

**FM register map** (subset of YM2413):
- $00–$07: User-defined instrument (2-op FM parameters: AM/FM/EG type, multipliers, feedback, attack/decay/sustain/release rates, key scale level)
- $10–$15: Frequency low byte (F-number bits 7–0) for channels 0–5
- $20–$25: Sustained/key-on, octave, frequency high bit for channels 0–5  
- $30–$35: Volume + instrument select for channels 0–5

Each channel selects from 15 **hardcoded instrument patches** stored in the VRC7's internal ROM (these are a subset of the YM2413's built-in patches) or uses the user-defined patch (slot 0).

FM operators: Each operator has an ADSR envelope, an amplitude modulation (tremolo) enable, frequency modulation (vibrato) enable, key scale level, and multiplier (which multiplies the base frequency). The 2-operator topology is either FM (op1 modulates op2) or additive.

The VRC7 output is a signed value that feeds into the EXP audio pin. The effective sample rate of the FM engine is derived from the master clock ÷ 72 (same ratio as YM2413: ~3.58 MHz / 72 ≈ 49,716 Hz update rate on NTSC).

**Emulation**: Many emulators use a software YM2413 core and feed it the VRC7 register writes. The key difference from a real YM2413 is the internal instrument ROM data, which is slightly different (the VRC7 ROM was extracted from hardware via careful analysis).

**Games**: *Lagrange Point* (Famicom exclusive — extraordinary FM audio)

---

## Part III: Namco Mappers

---

### Namco 108 Family — iNES Mappers 088, 095, 118, 119, 206

**Chip**: Namco 108 (also known as Namco 3446)

This chip underlies several seemingly different mappers. Its register interface is almost identical to MMC3 but **without IRQ** and with different CHR bank semantics:

| Register | MMC3 equivalent |
|---|---|
| $8000 bank select | Same |
| $8001 bank data | Same |
| No $C000–$FFFF IRQ registers | |

**Namco 108 bank registers:**
- R0: CHR 2KB bank at $0000 (bit 0 ignored)
- R1: CHR 2KB bank at $0800
- R2: CHR 1KB bank at $1000
- R3: CHR 1KB bank at $1400
- R4: CHR 1KB bank at $1800
- R5: CHR 1KB bank at $1C00
- R6: PRG 8KB at $8000
- R7: PRG 8KB at $A000
- $C000 and $E000 fixed to last two 8KB banks

No A12 inversion, no IRQ. This makes the mapper simpler to emulate than MMC3.

**Mapper 088**: Namco 108, fixed vertical mirroring. CHR banks 0/1 are 2KB, banks 2–5 are 1KB but bank registers address 64-bank space (bits 6 only). Used for 64KB CHR.

**Mapper 095**: Namco 108, CIRAM A10 controlled by CHR bank bit 5 (top bit) rather than dedicated mirroring register. Effectively: mirroring determined by CHR bank data.

**Mapper 118**: MMC3 with additional nametable control. The CHR bank high bit of R0/R1 controls CIRAM A10 for that half of the screen. Provides per-scanline mirroring-equivalent effects.

**Mapper 119**: TQROM-equivalent. Some CHR banks are RAM, selected by a bit in the CHR bank register.

**Mapper 206**: Pure Namco 108 without mirroring control — mirroring hardwired.

---

### Namco 163 — iNES Mapper 019

**Chip**: Namco 163 (also called Namco 175 / 340 in sub-variants)

The most sophisticated Namco mapper. Combines extensive banking with an 8-channel wavetable audio engine.

#### Register Interface
Uses a windowed addressing scheme common in Namco chips. The chip has an internal address pointer:

**$E000–$EFFF**: Write sets internal address (bits 6–0) + auto-increment enable (bit 7). Read returns audio channel data.
**$4800**: Read/write to internal 128-byte RAM at current internal address. Each access optionally auto-increments the address pointer.

**PRG banking** (via direct address decode):
- $8000–$87FF: CHR bank 0 (1KB) — also controls CIRAM for nametable 0 if bit 7=1 means use NT RAM, else CHR-ROM
- $8800–$8FFF: CHR bank 1
- $9000–$9FFF: CHR bank 2
- $9800–$9FFF: CHR bank 3
- $A000–$A7FF: CHR bank 4
- $A800–$AFFF: CHR bank 5
- $B000–$B7FF: CHR bank 6
- $B800–$BFFF: CHR bank 7
- $C000–$C7FF: Nametable page select for NT 0 and NT 1
- $C800–$CFFF: Nametable page select for NT 2 and NT 3
- $D000–$D7FF: Nametable page select for NT 0 (alternate)
- $D800–$DFFF: Nametable page select for NT 1 (alternate)
- $E000–$E7FF: PRG bank at $8000, + sound disable bit
- $E800–$EFFF: PRG bank at $A000, + CHR ROM/RAM select bits
- $F000–$F7FF: PRG bank at $C000
- $F800–$FFFF: Internal address register for 128-byte RAM access

**PRG**: Three switchable 8KB banks at $8000/$A000/$C000, last 8KB fixed at $E000.

**CHR**: Eight 1KB banks. Each register holds an 8-bit bank number — up to 256KB CHR. High bit can select CIRAM (nametable RAM) instead of CHR-ROM, allowing nametables to use CHR space — for 4-screen or exotic configurations.

**Nametable mapping**: Namco 163 provides 4-screen nametable capability by mapping CHR-ROM or the internal RAM into the PPU's nametable space.

#### Namco 163 Audio Engine

The 128-byte internal RAM serves as the wavetable sample memory and channel register space.

**Audio channel layout** in RAM (each channel occupies 8 bytes, placed at the end of the 128-byte space):

With N active channels, channel data starts at address `0x40 + (8 - N) * 8`. Register $F800 bits 6–4 encode (8 - num_channels), so num_channels = 8 - ((reg >> 4) & 7).

Per-channel registers (at internal RAM offset `base + 8 * channel_index`):

| Offset | Bits | Meaning |
|---|---|---|
| 0 | 7–0 | Frequency low byte |
| 1 | 7–0 | Phase low byte (current sample position, low) |
| 2 | 7–0 | Frequency middle byte |
| 3 | 7–0 | Phase middle byte |
| 4 | 3–0 | Frequency high nibble |
| 4 | 7–4 | Phase high nibble |
| 5 | 7–0 | Wave length + start address: bits 3–0 = (256 - wave_length), bits 7–4 = wave address bits 7–4 |
| 6 | 7–0 | Wave address low (addr of sample start in 128-byte RAM, × 2 for 4-bit samples) |
| 7 | 3–0 | Volume (0–15) |
| 7 | 7–4 | (Ignored) |

**Audio engine operation**: The Namco 163 uses the CPU clock to run a sequencer that cycles through all active channels. For each channel it:
1. Adds the 18-bit frequency value to the 18-bit phase accumulator.
2. Extracts the current sample address: `(phase_high >> 4) + wave_addr`, bounded by wave_length.
3. Reads the 4-bit sample at that address from the 128-byte RAM (two 4-bit samples packed per byte).
4. Applies volume: final output = sample × volume.
5. Accumulates into output DAC.

The **sequencer clock rate** = master_clock / 15 (approximately 1.43 MHz on NTSC). All active channels share this clock, each getting `clock / num_channels` updates per second. Therefore:

**Effective sample rate per channel** = (21.477272 MHz / 15) / num_channels = 1,431,818 Hz / num_channels

For 8 channels: ~178,977 Hz. For 1 channel: ~1,431,818 Hz.

**Wave length** = 256 - register_value. Sample addresses wrap within the wave, playing in a loop.

**Audio output**: Mixed into the EXP audio pin. The mixing ratio between Namco 163 audio and 2A03 audio is not well-characterized and varies by emulator. Hardware measurements suggest the Namco 163 output level is comparable to the 2A03 triangle channel.

**Emulation note**: CPU writes to the 128-byte RAM during audio playback can cause artifacts because the audio engine reads from the same RAM. Writes during the engine's read window produce glitches on real hardware; accurate emulators must model this contention.

#### Mapper 019 Sub-variants

**Namco 175** (submapper 1): No 4-screen/CIRAM nametable control. CHR high bit does not select CIRAM. Simpler version used for some titles.

**Namco 340** (submapper 2): No audio engine. Just PRG/CHR banking and 4-screen nametable support.

**Games**: *Final Fantasy III* (JP Famicom), *Megami Tensei* series (JP), *Pac-Land*, *Mappy-Land*, *Dragon Buster II*, *Klax*, *Rolling Thunder*, *Galaxian* (Famicom)

---

## Part IV: Bandai Mappers

---

### Bandai FCG — iNES Mappers 016, 153, 157, 159

Bandai produced several related mapper ASICs collectively using a common register interface. The key distinction is the type of save hardware:

**FCG-1 / FCG-2** (Mapper 016 base): EEPROM-based save (24C01/24C02 I²C EEPROM), or no save.

**Mapper 153**: FCG with 8KB PRG-RAM instead of EEPROM (used for *Famicom Jump II*).

**Mapper 157**: Datach Joint ROM System — barcode reader. See below.

**Mapper 159**: FCG with X24C01 EEPROM (slightly different I²C protocol).

#### FCG Register Map

Writes to $6000–$7FFF (FCG-1) or $8000–$FFFF (FCG-2, LZ93D50), depending on variant:

| Register | Function |
|---|---|
| $00–$07 | CHR 1KB bank registers 0–7 |
| $08 | PRG 16KB bank at $8000 |
| $09 | Mirroring (bits 1–0: 0=H, 1=V, 2=single-low, 3=single-high) |
| $0A | IRQ enable (bit 0), IRQ acknowledge (write) |
| $0B | IRQ counter low byte |
| $0C | IRQ counter high byte |
| $0D | EEPROM control (for FCG with EEPROM) |

**PRG**: 16KB switching at $8000, last 16KB fixed at $C000. 4-bit bank = 16 banks = 256KB max.

**CHR**: Eight 1KB banks. 128KB CHR-ROM max.

**IRQ**: 16-bit counter, decrements every CPU cycle. When it reaches 0, asserts /IRQ. Counter loaded from $0B/$0C. Writing to $0A (bit 0 = 0) also reloads/disables.

#### EEPROM Interface ($0D)

The FCG communicates with a small external EEPROM (typically 128 or 256 bytes) via a **serial interface** on pin $0D:

- Bit 5: SCL (I²C clock line)
- Bit 6: SDA write (I²C data output)
- Bit 7: SDA read (I²C data input)

The CPU bit-bangs the I²C protocol manually. The EEPROM stores save data persistently without a battery (EEPROM retains data at power-off). This was innovative but EEPROM write lifetime is limited.

**X24C01 variant** (Mapper 159): Uses a slightly different command protocol — the device address is not required on write, simplifying software. Emulation must distinguish the two EEPROM types.

---

### Datach Joint ROM System — iNES Mapper 157

Used for Bandai's Datach barcode reader peripheral. A barcode scanner connects to the Famicom expansion port and the mapper includes FCG banking.

**Emulation**: Mapper 157 is FCG with additional logic for injecting barcode bit-stream data. When the scanner reads a barcode, it produces a serial bitstream that maps to bits returned on reads of the EEPROM data pin. Emulators can either ignore this (Datach games require the scanner for full functionality) or implement a virtual barcode input.

---

### Bandai Karaoke Studio — iNES Mapper 188

**Unusual**: Not a traditional game cartridge. Used for the Karaoke Studio peripheral that connected a microphone and displayed lyrics.

**PRG**: 16KB banking. Special bank select logic: bit 3 selects "BIOS" bank (lower 8KB of cart ROM from microphone control unit vs game ROM). Not commonly emulated accurately.

---

## Part V: Sunsoft Mappers

---

### Sunsoft-1 — iNES Mapper 184

Simple CHR banking only. Write to $6000–$7FFF:
- Bits 5–0: CHR bank for $0000–$0FFF (4KB)
- Bits 6–4 (of high nibble): CHR bank for $1000–$1FFF

PRG fixed (16KB mirrored or 32KB). No PRG switching.

**Games**: *Atlantis no Nazo*, *Fantasy Zone*, *Spy vs. Spy*

---

### Sunsoft-2 — iNES Mapper 093, 089

**Mapper 089** (Sunsoft-2 with CHR banking and mirroring):

Write to $8000–$FFFF:
```
Bit 7:    CHR bank bit 3 (high bit)
Bits 6–4: CHR bank bits 2–0
Bit 3:    Nametable select (single-screen: 0=CIRAM lower, 1=CIRAM upper)
Bits 2–0: PRG bank (16KB, at $8000; $C000 fixed to last bank)
```

**Mapper 093**: Write to $8000–$FFFF, bit 4 = PRG bank, rest ignored. No CHR banking.

---

### Sunsoft-3 — iNES Mapper 067

**Chip**: Sunsoft (FF3155) — used in one game (*Fantasy Zone II*)

**PRG**: Two switchable 16KB banks (at $8000/$C000) or $C000 fixed.
**CHR**: Four 2KB bank registers.
**Mirroring**: Controllable.
**IRQ**: 16-bit counter in two 8-bit halves. Write high byte to $C800 (with data in bits 7–0), then low byte to $D800. Enable via $E800; acknowledge via $F800. Counter decrements every CPU cycle.

A quirk: the high/low IRQ byte writes alternately toggle on consecutive writes to the same address, rather than being distinct registers. This is a hardware oddity that must be emulated.

**Games**: *Fantasy Zone II* (only user of this mapper)

---

### Sunsoft-4 — iNES Mapper 068

**Chip**: Sunsoft (After Burner chip)

**PRG**: 16KB switching at $8000, $C000 fixed to last bank.
**CHR**: Four 2KB banks for $0000–$1FFF.
**Nametable control**: Two 1KB banks for nametables, selected from CHR-ROM (not CIRAM). This allows nametable data to come from cartridge ROM. Bit 4 of the nametable bank registers selects ROM vs CIRAM.
**Mirroring**: H/V/Single-low/Single-high.

**Emulation note**: The nametable-from-ROM feature is unusual and easy to get wrong. The PPU nametable addresses must route to CHR-ROM based on the nametable bank registers.

**Games**: *After Burner*, *Maharaja*, *Ninjakun Majō no Böken*

---

### FME-7 / Sunsoft 5B — iNES Mapper 069

**Chip**: Sunsoft FME-7 (base); Sunsoft 5B adds YM2149 audio.

#### Banking via Command/Data Interface:
- **$8000–$9FFF** write: Command register (bits 3–0 select target register 0–13)
- **$A000–$BFFF** write: Data register (writes to currently selected register)

This address/data scheme avoids needing multiple independent register address ranges.

**Registers 0–7**: CHR 1KB banks 0–7. 8 bits each. Up to 256KB CHR.

**Registers 8–11**: PRG banks:
- R8: Bank at $6000 (bits 5–0 = bank; bit 7 = 1 selects RAM, bit 6 = RAM write-protect)
- R9: Bank at $8000 (8KB)
- R10: Bank at $A000 (8KB)
- R11: Bank at $C000 (8KB)
- $E000 fixed to last 8KB.

R8 is important: the $6000–$7FFF space can be mapped to **PRG-ROM** (unusual) or PRG-RAM (normal), with write protection. This allows ROM to appear in the normally-WRAM range.

**Register 12**: Mirroring (bits 1–0: 0=V, 1=H, 2=single-low, 3=single-high)

**Register 13**: IRQ control:
- Bit 0: IRQ enable
- Bit 7: Counter enable (separate from IRQ enable)
Write to this register also acknowledges IRQ.

**Registers 14–15** (via $C000/$E000 writes):
- $C000 write: IRQ counter low byte
- $E000 write: IRQ counter high byte

**IRQ**: 16-bit counter, decrements every CPU cycle when counter-enable is set. IRQ fires at underflow if IRQ-enable is set.

The two-level enable (counter enable vs IRQ enable) means the counter can run without generating IRQs — useful for preloading timing.

#### Sunsoft 5B Audio (additional hardware on 5B variant)

The 5B adds a **Yamaha YM2149** (software-compatible with AY-3-8910) on the same FME-7 command space via additional command values:

Command $0D (register 13 of the YM2149's address space): YM2149 address register via data writes
Command $0E: YM2149 data write

**YM2149 / AY-3-8910 audio**:
- 3 square-wave tone channels (A, B, C), each with a 12-bit period register
- 1 noise generator (5-bit period)
- 1 envelope generator (16-bit period, 8 envelope shapes)
- Per-channel tone/noise enable and 4-bit volume (or envelope routing)
- Port A/B I/O (not used in NES context)

Period formula: f_out = f_clock / (2 × 16 × period) = 1,789,773 / (32 × period) Hz on NTSC.

Channels A/B/C can independently enable tone, noise, or both. Volume is either fixed (4-bit) or follows the envelope generator.

**Emulation**: The 5B mapper is mapper 069; distinguishing 5B from FME-7 is done by checking whether the game writes to the YM2149 registers (standard FME-7 games do not use commands $0D/$0E for audio). NES 2.0 sub-mapper field can distinguish them.

**Games**: *Batman: Return of the Joker* (FME-7), *Gimmick!* (5B — widely regarded as having the best NES-family audio ever; the 5B audio is spectacular), *Hebereke* (5B)

---

## Part VI: Irem Mappers

---

### Irem G-101 — iNES Mapper 032

**Chip**: Irem custom

**PRG**: 
- Normally: $8000 and $A000 each get one switchable 8KB bank; $C000 and $E000 fixed to last two banks.
- Mode 1 ($9000 bit 1 set): $8000 fixed to second-last; $C000 switchable.
- 6-bit bank numbers = 512KB PRG max.

**CHR**: Eight 1KB banks.

**Mirroring**: Bit 0 of $9000: 0=V, 1=H. **Except**: $9000 bit 1 enables single-screen mode (lower nametable only).

**Emulation note**: The PRG mode switch and mirroring control share register $9000. Accurately separating these bits is needed.

**Games**: *Image Fight*, *Major League*, *Kaiketsu Yanchamaru 3*

---

### Irem H-3001 — iNES Mapper 065

**Chip**: Irem H-3001

**PRG**: Three switchable 8KB banks ($8000, $A000, $C000), last fixed. 7-bit bank = 128 banks = 1MB PRG.

**CHR**: Eight 1KB banks. 7-bit = 128 banks = 128KB.

**Mirroring**: H/V controllable.

**IRQ**: 16-bit counter, CPU-cycle based. Unusual format: $B001 writes high byte, $B003 writes low byte. $B000 enable/disable/acknowledge. On enable, counter reloads from latch.

**Games**: *Daiku no Gen-san 2*, *Spartan-X 2*

---

## Part VII: Taito Mappers

---

### TC0190 / TC0350 — iNES Mapper 033

**Chip**: Taito TC0190 (no IRQ) or TC0350 (with IRQ)

**TC0190 (Mapper 033)**:

Registers at $8000–$FFFF decoded by A14:A13:

| Range | Function |
|---|---|
| $8000 | PRG 8KB at $8000 + mirroring bit 0 |
| $8001 | PRG 8KB at $A000 |
| $8002 | CHR 2KB at $0000 |
| $8003 | CHR 2KB at $0800 + mirroring bit 1 |
| $A000 | CHR 1KB at $1000 |
| $A001 | CHR 1KB at $1400 |
| $A002 | CHR 1KB at $1800 |
| $A003 | CHR 1KB at $1C00 |

PRG: $C000 and $E000 fixed to last two 8KB banks.  
Mirroring: Bits from $8000 and $8003.

**TC0350 (Mapper 048)**: Adds IRQ (scanline counter via A12 edge counting, similar in concept to MMC3 but different implementation). IRQ registers at $C000–$FFFF.

**Games**: *Captain America* (TC0350), *Flintstones* (TC0350), *Jetsons* (TC0350), *Don Doko Don* (TC0190)

---

## Part VIII: Jaleco Mappers

Jaleco produced a long series of JF (Jaleco Famicom) boards across many games. Most are relatively simple.

### JF-05 through JF-10 — iNES Mapper 087

Simple CHR 2-bank switching. Write to $6000–$7FFF: bits 1–0 select 8KB CHR bank (bit 0 goes to CHR A13, bit 1 to CHR A14 — with bit swapping depending on board). 2 bits = 32KB CHR. PRG fixed (16 or 32KB). No save. No IRQ.

**Games**: *Bases Loaded*, *City Connection*

---

### JF-11/12/14 — iNES Mapper 140

Write to $6000–$7FFF (upper nibble) = PRG 32KB bank (2 bits), lower nibble = CHR 8KB bank (4 bits). Both PRG and CHR banked in one write. No IRQ.

**Games**: *Bio Senshi Dan*, *Mississippi Satsujin Jiken*

---

### JF-16 — iNES Mapper 078 (shared with Irem Holy Diver)

One of the few mappers where **two different hardware implementations share the same mapper number** with different mirroring behavior — distinguished by NES 2.0 submapper:

Write to $8000–$FFFF:
- Bits 2–0: PRG 16KB bank at $8000
- Bits 7–4: CHR 8KB bank

**Submapper 1 (Irem Holy Diver)**: Bit 3 = mirroring (0=V, 1=H)  
**Submapper 3 (Jaleco JF-16)**: Bit 3 = nametable select (single-screen: 0=lower, 1=upper)

Emulators using mapper 078 without submapper information default to one behavior, which breaks the other game.

**Games**: *Holy Diver* (Irem), *Uchuusen Cosmo Carrier* (Jaleco)

---

### SS88006 — iNES Mapper 018

**Chip**: Jaleco SS88006

More sophisticated mapper with 12-bit nibble-based register writes and IRQ.

Registers written to $8000–$FFFF, each "register" composed of two nibble-writes to successive addresses:

- $8000 + $8001: PRG bank 0 low/high nibble (→ $8000–$9FFF)
- $8002 + $8003: PRG bank 1
- $A000 + $A001: PRG bank 2
- (PRG bank 3 = fixed last bank)
- $C000–$DFFF: Eight CHR 1KB bank registers (pairs of nibble writes each)
- $E000 + $E001: IRQ counter bits 11–8 and 7–4
- $E002 + $E003: IRQ counter bits 3–0 and... (4 nibbles for a 12-bit IRQ counter)
- $F000: IRQ enable/disable
- $F001: IRQ acknowledge + counter reload
- $F002: IRQ counter size select (bits 1–0: mask for 4/8/12/16-bit counter width)
- $F003: Mirroring

**IRQ**: The counter is maskable to 4, 8, 12, or 16 effective bits. This allows fine-grained timing that overflows quickly (4-bit = fires every 16 cycles) or slowly (16-bit). CPU-cycle based.

**Games**: *Insector X*, *Pizza Pop!*, *Desert Commander*

---

## Part IX: Other Japanese Third-Party Mappers

---

### Namco 109 — iNES Mapper 206 (disambiguation)

See Namco 108 family above. Mapper 206 covers several Namco 108/109 variants with minimal banking.

---

### Tengen RAMBO-1 — iNES Mapper 064

**Chip**: Tengen RAMBO-1 (designed by Tengen, Atari's NES division)

RAMBO-1 is an enhanced MMC3 clone with additional features:

**Differences from MMC3**:
1. **CHR banking**: R0/R1 (the 2KB banks) can be split: bit 5 of bank select register enables "1KB mode" for R0 and R1, making them select 1KB banks instead of 2KB. This gives 10 independent CHR 1KB registers instead of MMC3's 6.
2. **PRG mode**: Additional mode (bit 6 of bank select = 1) enables a third switchable PRG 8KB bank at $C000 (MMC3 fixes this to second-last bank in some modes).
3. **IRQ**: RAMBO-1's IRQ can be triggered by M2 (CPU clock) edges **or** A12 rising edges, selectable. In A12 mode it functions like MMC3's IRQ but the filter differs. In M2 mode it's a CPU cycle counter.
4. **IRQ pulse mode**: Bit 5 of IRQ control: in "pulse" mode, the IRQ fires for only 3 M2 cycles then auto-clears.

**Register map**: Same addresses as MMC3 but with different bit interpretations in the bank select register and IRQ control registers.

**Emulation**: RAMBO-1 shares the mapper 064 number and is frequently misimplemented as MMC3. The extra CHR banks and the M2-mode IRQ are the critical differences. Games like *Skull & Crossbones*, *Gauntlet* (Tengen), and *Klax* (Tengen version) use this chip.

---

### Tengen MIMIC-1 — iNES Mapper 028 (not original — actually mapper 028 is Action 53)

Actually, Tengen's MIMIC-1 chip is a clone of the MMC3 used on their licensed board versions. Functionally equivalent to MMC3 for emulation purposes.

---

### Action 53 — iNES Mapper 028

**Not a commercial cartridge mapper** — a community-designed mapper for NES homebrew multicarts. Uses register at $5000 and $8000:
- $5000: Register select
- $8000: Bank data

Supports NROM, AxROM, UxROM, and CNROM game types within a unified bank architecture for compiling multiple games onto one cartridge.

---

### Camerica / Codemasters — iNES Mapper 071

**Chip**: Camerica custom (used by Codemasters for their unauthorized NES releases)

Very simple. Write to $8000–$FFFF: bits 3–0 = 16KB PRG bank at $8000. $C000 fixed to last bank. CHR is 8KB RAM.

**Mapper 071 variant (Fire Hawk)**: Bit 4 of writes to $9000–$9FFF controls mirroring (single-screen low/high). Standard mapper 071 has fixed mirroring.

**Games**: *Bee 52*, *Micro Machines*, *Quattro Sports*, *Fire Hawk*

---

### Codemasters GoldFinger — iNES Mapper 072 (disambiguation needed)

Actually the Jaleco JF-17 uses this number. Codemasters used mapper 071 primarily.

---

### Nina-001 — iNES Mapper 034 (submapper 1)

Registered under the same mapper number as BxROM but distinguished by NES 2.0 submapper.

Writes to $7FFD–$7FFF (the last 3 bytes of WRAM space — unusual!):
- $7FFD: CHR 4KB bank at $0000
- $7FFE: CHR 4KB bank at $1000
- $7FFF: PRG 32KB bank

Since these addresses are in the $6000–$7FFF WRAM range rather than ROM space, **no bus conflicts** occur.

**Games**: *AVE games*, several rare titles

---

### NINA-006 / NINA-03 — iNES Mapper 079, 113

Simple CHR + PRG banking. Write to $4100–$5FFF (expansion area):
- Mapper 079: bits 3–0 = CHR 8KB bank, bit 3 = PRG 32KB bank
- Mapper 113: various CHR banking with multi-bit fields

These use the otherwise-unused expansion address space $4020–$5FFF for register writes, avoiding bus conflicts with ROM.

---

### Color Dreams — iNES Mapper 011

**Chip**: Color Dreams custom

Write to $8000–$FFFF:
- Bits 3–0: PRG 32KB bank
- Bits 7–4: CHR 8KB bank

Bus conflicts apply. No save, no IRQ, fixed mirroring.

Used extensively by unlicensed Color Dreams and later Wisdom Tree (Christian-themed games) releases.

**Games**: *Bible Adventures*, *Ghostbusters II* (unlicensed), *Crystal Mines*

---

### Active Enterprises — iNES Mapper 228

Used for multicarts with unusual banking:
- Writes to $8000–$FFFF encode: CHR bank (bits 3–0), PRG bank (bits 7–4), PRG size mode (bit 14), mirroring (bit 13).
- PRG can be 16KB or 32KB based on a flag.

---

## Part X: Famicom Disk System (FDS)

### Technical Architecture

The FDS is not a cartridge in the traditional sense but deserves inclusion as the dominant Famicom game delivery mechanism in Japan (1986–1990) and its emulation involves unique hardware.

#### Hardware Components

**RAM Adapter** (the cartridge-shaped unit that plugs into the Famicom slot):
- Contains 32KB **PRG-RAM** (battery-backed) at $6000–$DFFF
- Contains 8KB **CHR-RAM** (no battery) for PPU
- Contains the BIOS ROM at $E000–$FFFF (8KB)
- Contains the **2C33** FDS ASIC for disk I/O and audio

The RAM adapter plugs into the cartridge slot and connects via a ribbon cable to the disk drive unit.

#### 2C33 ASIC Registers ($4020–$4097):

| Address | Direction | Description |
|---|---|---|
| $4020 | R/W | Disk status |
| $4021 | R | Disk data register |
| $4022 | R/W | Drive control |
| $4023 | W | Drive enable |
| $4024 | W | Write data (disk write) |
| $4025 | W | FDS control register |
| $4026 | W | External connector output |
| $4030 | R | Disk status flags (read side) |
| $4031 | R | Read data register |
| $4032 | R | Drive status |
| $4033 | R | External connector input |
| $4040–$407F | R/W | Wave table RAM (64 entries × 6-bit) |
| $4080 | W | Volume envelope |
| $4082 | W | Frequency low byte |
| $4083 | W | Frequency high byte + envelope control |
| $4084 | W | Mod envelope |
| $4085 | W | Modulation counter |
| $4086 | W | Mod frequency low |
| $4087 | W | Mod frequency high |
| $4088 | W | Mod table write |
| $4089 | W | Master volume + wave write enable |
| $408A | W | Envelope speed |

#### FDS Disk Format

Disks are **Famicom Disk System Quick Disk** format: a proprietary magnetic disk with ~64KB per side. Data is stored as FM-encoded serial bitstream at ~96,400 bits/second. The 2C33 handles serial-to-parallel conversion.

**Block structure**:
- Block 1: Disk info (game name, manufacturer, disk number)
- Block 2: File count
- Block 3–N: File headers (file name, address, size, type)
- Block 3–N (data): File data blocks

Files can be of type program (loaded to $6000–$DFFF) or character data (loaded to CHR-RAM $0000–$1FFF).

**Loading process**: The BIOS manages disk I/O, loading files from disk to RAM. Games run from RAM ($6000–$DFFF), not directly from disk. Total usable PRG space is 32KB per load; games requiring more data swap disk sides.

#### FDS Audio

The FDS adds a sophisticated wavetable synthesizer:

**Wavetable channel**: 64-sample wave stored in the 64-entry wave table RAM ($4040–$407F). Each entry is 6-bit signed. A frequency accumulator advances through the table at a rate determined by $4082/$4083. The channel has a separate **volume envelope** with attack/decay/sustain behavior. Output is 6-bit × volume envelope.

**Modulation channel**: A separate frequency modulation unit. Has its own 64-entry modulation table (separate from the wave table), frequency register, and counter. The modulation output modifies the main channel's frequency on a per-step basis, creating FM-like effects or vibrato.

The combination allows rich timbres: a static waveform modulated by a time-varying LFO, or complex frequency sweeps.

**Envelope speed** ($408A): Global divider for both envelope units. Scales the attack/decay rates.

**Emulation of FDS audio** is complex because the interaction between the wave channel, volume envelope, and frequency modulation is subtle. The modulation table can be written during playback, enabling dynamic timbre changes.

#### FDS Disk I/O Emulation

For emulation purposes, FDS disk images are typically stored as **.fds** files (flat binary dump of disk sides, preceded by a small header). The emulator must:
1. Simulate the disk motor and head movement latency
2. Serialize/deserialize the gap marks and CRC bytes in the disk data stream
3. Handle the IRQ generated when the disk data register is ready (disk I/O is interrupt-driven)
4. Emulate the end-of-disk gap marker and head-reset timing

Approximate disk read latency: ~200ms for the head to position, then data streams at ~6,400 bytes/second effective throughput.

---

## Part XI: VS. System

The VS. System is Nintendo's arcade hardware based on NES internals. Cartridges are **50-pin** VS. System boards, not consumer cartridges, but are emulated within the NES family.

### VS. System Hardware Differences

- Uses a **RP2C03** (or 2C04, 2C05) PPU instead of the standard 2C02. These have different **palette** data — the hardware palette ROM contains a differently-ordered set of colors. Emulators must swap palette mappings for VS. games.
- Uses a **security chip** (different versions: VS-Unisystem, VS-Dualsystem) that XORs or transforms joystick input reads ($4016/$4017) with different bit patterns per game.
- **VS-Dualsystem**: Two NES boards side by side sharing display, for two-player vs. games on separate screens.

#### VS. PPU Variants and Palettes

| PPU | Palette notes |
|---|---|
| RP2C03B / 2C03G | Standard palette, used in early VS. games |
| RP2C04-0001 | Shuffled palette map (emulators must remap palette indices) |
| RP2C04-0002 | Different shuffle |
| RP2C04-0003 | Different shuffle |
| RP2C04-0004 | Different shuffle |
| RP2C05-01 through 05 | Same palette shuffle variants, different security |

The 2C04 and 2C05 contain identical color data to the 2C03 but with the **palette indices permuted**. A game written for 2C04-0001 reads color $0A and expects to get, say, dark green — but in the standard 2C02, $0A is a different color. Emulators must apply the correct palette permutation table for each VS. PPU variant.

#### Security ($4016/$4017 Protection)

The VS. System security chip modifies the bits returned when the CPU reads joystick data from $4016 and $4017. Different games use different security schemes — some bits are swapped between the two registers, some bits are inverted. Emulators maintain a table of per-game security configurations, typically keyed on the game's CRC or PRG checksum.

---

## Part XII: Miscellaneous and Late Mappers

---

### MMC6 — iNES Mapper 004 (submapper 1)

The MMC6 is a variant of MMC3 used on only two games (*StarTropics*, *Zoda's Revenge*). It replaces the external 6264 PRG-RAM chip with **1KB of RAM internal to the chip**.

Key difference for emulation:
- PRG-RAM is only 1KB (internal), not 8KB
- The 1KB is accessible at $7000–$73FF (mirrored through $6000–$7FFF)  
- $A001 has different bit interpretation for the 1KB RAM: bits 5–4 control upper half (512B) read/write enable, bits 1–0 control lower half
- If RAM is disabled, reads return open bus (not 0 or FFh)

Emulators that emulate mapper 004 without distinguishing submapper 1 may provide 8KB RAM to StarTropics, which still works because the game only uses 1KB — but the protection bits behave differently.

---

### Nintendo DiskCard (Famicom) Special Cases

Some Famicom games released on disk were later ported to cartridge with new mappers. The mapper choice for the cartridge version is independent of the disk version's original requirements.

---

### Mapper 228 (Active Enterprises) — See Above

---

### Holy Diver / Mapper 078 — See Jaleco section

---

### AxROM Variant BxROM Overlap

Mapper 034 (BxROM) and mapper 034 (NINA-001) share an iNES mapper number but different hardware. NES 2.0 sub-mapper 1 = NINA-001; submapper 2 = BxROM. Without NES 2.0, the disambiguation requires the presence of CHR-ROM (NINA-001 has it; BxROM uses CHR-RAM) in the ROM file.

---

### Mapper 080 — Taito X1-005

Two variants sharing mapper 080:

**X1-005 Variant A**: Registers at $7EF0–$7EFF (unusual WRAM-space registers). CHR 1KB banking (8 registers), PRG 8KB banking (3 registers + fixed last), mirroring via register.

**X1-005 Variant B**: Adds a battery-backed 10-byte SRAM for save data (not a full SRAM chip — only 10 bytes are implemented).

---

### Mapper 082 — Taito X1-017

Extended version of X1-005. Adds a separate 2KB window mapped by registers into specific PPU areas, nametable bank switching from CHR space.

---

### Mapper 086 — Jaleco JF-13

Peculiar: has a **Yamaha YM2413-compatible FM synthesizer** (same chip family as VRC7) on the cartridge. Only one game uses it: *Moero!! Pro Yakyuu* (Bases Loaded spinoff). The YM2413 access is via $4011 (a normally-APU-DMC register) repurposed for mapper audio data writes, with address latching at $6000–$6FFF.

---

### Mapper 190 — Magic Kid Googoo

Simple 8-bit PRG bank switching at $8000, plus Mirroring. CHR from RAM. Very small homebrew/unlicensed mapper.

---

### Multicart Mappers

Several mappers exist primarily for multicartridges (multiple games on one cart):

**Mapper 115**: KT-008 multicart (subset of MMC3 banking with additional outer bank register)

**Mapper 255**: 110-in-1 style multicarts; outer PRG/CHR bank + inner game mapper emulation

**Mapper 200/201/202/240/242**: Various pirate/multicart chipsets combining simple bank registers with game-detection logic

These typically maintain an "outer" bank (selecting which game's data area) and an "inner" bank register (standard mapper banking within the selected game). Emulation of multicarts generally requires implementing the outer bank layer on top of an existing mapper.

---

## Part XIII: Pirate/Bootleg Mappers (Emulation-Relevant)

Chinese and Taiwanese unlicensed manufacturers produced hundreds of pirate cartridges with custom mapper hardware. The most emulation-relevant:

**Mapper 132 (TXC 05-00002-010)**: 4-register system via $4100/$4101/$4102/$4103. XOR-based bank select.

**Mapper 172 / 173 (TXC variants)**: More complex TXC chips with different XOR patterns.

**Mapper 176**: Large PRG multicart mapper with 4-byte outer bank registers.

**Mapper 241 (BNROM-like pirate)**: Write to $8000, bits 3–0 = PRG 32KB bank, bit 4 = CHR bank.

**Mapper 163 / 164**: Family Computer pirate multicart boards with shift-register-based game selection and bank switching.

---

## Summary: Emulation Classification Matrix

| Category | Mappers | Key emulation challenge |
|---|---|---|
| No mapper logic | 000 | Open bus behavior |
| Single-register PRG | 002, 007, 003, 066, 034 | Bus conflicts |
| Serial-register PRG/CHR | 001 (MMC1) | 5-bit serial, consecutive-write filter |
| A12-edge IRQ | 004 (MMC3), 064 (RAMBO-1) | PPU cycle-accurate A12 monitoring, filter timing |
| CPU-cycle IRQ | 018, 021-025, 067, 069, 065, 032 | Prescaler accuracy |
| Dual-register command/data | 069 (FME-7), 019 (N163) | Register pointer, auto-increment |
| CHR auto-latch | 009 (MMC2), 010 (MMC4) | Real-time PPU address monitoring |
| Full extended hardware | 005 (MMC5) | PPU state tracking, ExRAM, split screen |
| FM audio | 085 (VRC7), 086 (JF-13) | YM2413/OPL2 FM synthesis |
| Wavetable audio | 019 (N163) | Multi-channel accumulator synthesis |
| PSG audio | 069 (5B) | AY-3-8910 / YM2149 |
| Pulse/sawtooth audio | 024/026 (VRC6) | Sawtooth accumulator |
| Disk system | FDS | Disk serial protocol, FDS audio |
| VS. System | Various | Palette remapping, joystick security |
| EEPROM save | 016, 159 | I²C bit-bang protocol |
| Nametable from CHR | 068 (Sunsoft-4), 019 (N163) | PPU nametable address routing |
| Attribute from cart | 005 (MMC5) | Per-tile attribute substitution |

The combinatorial complexity of accurately emulating all these mappers — particularly the cycle-accurate IRQ mechanisms, audio synthesis, and PPU address interception — explains why NES emulation accuracy remains an active area of development even decades after the hardware's production ended.  

---

## References

- [NESDev Wiki](https://wiki.nesdev.org/w/index.php/NES_2.0)
- [NESDev Forum](https://forums.nesdev.org/viewtopic.php?f=3&t=12415)