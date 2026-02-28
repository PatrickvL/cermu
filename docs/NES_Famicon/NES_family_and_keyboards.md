# NES Family Console Variants — Technical Distinctions & Keyboard Support

---

## The Core Hardware Variants

### 1. Famicom (HVC-001) — Japan, July 1983

The original. All subsequent hardware derives from this design.

**CPU**: Ricoh **RP2A03G** (later RP2A03H). NTSC. 1.7897727 MHz (master clock 21.477272 MHz ÷ 12). Contains the 6502 core (with BCD mode disabled), the APU (five audio channels), OAM DMA, and the controller I/O shift registers — all on a single die.

**PPU**: Ricoh **RP2C02G** (later revisions C, E, F, G, H). NTSC. 5.369318 MHz (master ÷ 4). Generates composite NTSC color via an analog phase-shift color burst encoder built into the chip — the color signal is generated directly from the PPU's internal dot clock, not from a separate encoder.

**Cartridge connector**: **60-pin dual-row edge connector** (HVC card edge). Two rows of 30 pins each. Top-loading. The physical cartridge is compact and squarish compared to the NES casing.

**Controllers**: **Hardwired** — permanently attached cables, not detachable. Player 1 controller is a standard rectangle pad with A, B, Select, Start, D-pad. **Player 2 controller has a built-in monaural microphone** (used by a handful of games — *The Legend of Zelda* JP version used it for killing Pols Voice enemies). The microphone feeds a signal into the expansion bus.

**Expansion port**: **DA15 connector** (15-pin D-sub) on the underside of the unit. This is the most important distinguishing feature of the Famicom vs NES — it exposes significant hardware expansion capability including direct CPU data bus access, audio mixing input, controller I/O lines, and power. This is how the Famicom Disk System, keyboard, and other peripherals connect.

**Video output**: **RF modulator only** in original production units (channel 1 NTSC). No direct AV output from the motherboard to external ports on the earliest units. Later Famicom revisions added a **composite AV output** on the front face (a small circular proprietary connector, not standard RCA).

**Audio**: The APU output is mixed internally with the DA15 expansion audio input before the RF stage. The audio mixing is done with resistors; the expansion audio pin is mixed at roughly equal weight to the internal APU output.

**Motherboard revisions**: Multiple PCB revisions exist (HVC-CPU-01 through HVC-CPU-07) with minor component changes but no functional differences from a software perspective.

**Region lockout**: None (no CIC lockout chip — the Famicom predates the 10NES lockout system). Any valid Famicom cartridge runs on any Famicom.

---

### 2. NES (NES-001) — North America October 1985, Europe 1987

The front-loader "toaster." Functionally equivalent to the Famicom at the CPU/PPU level but with significant physical and electrical differences.

**CPU**: Ricoh **RP2A03** (NTSC North America) — functionally identical to the Famicom CPU. For PAL Europe/Australia releases: Ricoh **RP2A07** — modified die with different clock dividers and slightly different APU behavior (see PAL section).

**PPU**: Ricoh **RP2C02** (NTSC). Functionally identical to the Famicom PPU.

**Cartridge connector**: **72-pin edge connector**, front-loading. The infamous **zero-insertion-force (ZIF) spring-loaded tray mechanism** — the cartridge is inserted horizontally into a tray which is then pushed down, pressing the PCB edge contacts against spring-loaded pins. The 72-pin connector has more signals than the Famicom's 60-pin, largely from adding explicit ground and power pins and reorganizing the signal routing, not from adding new signals.

**The 10NES lockout chip**: The NES-001 introduced the **CIC (Checking Integrated Circuit)** lockout system. Two CIC chips (one in the console, one in the cartridge — called the 3193 or 6113 depending on generation) communicate over a serial line during power-on. The console CIC sends a challenge; the cartridge CIC must respond correctly or the console resets continuously (the "blinking red light" failure mode). This is entirely absent from the original Famicom. The NES-001 CIC is a custom PIC-derived chip.

**Controllers**: **Detachable**, using a proprietary 7-pin connector (DA-7 style). This connector carries: VCC, GND, Latch, Clock, Data Out (console→controller), Data In (controller→console), and the microphone signal line (present in the connector spec but not used by standard NES controllers). The removal of hardwired controllers was a deliberate redesign choice.

**Expansion port**: A **15-pin expansion connector** on the bottom of the NES-001 exists but is not directly accessible without disassembly in the same way the Famicom's DA15 is. Nintendo used this for the Zapper, R.O.B., etc. — but these connect via the controller ports, not the expansion port. The NES expansion port was essentially unused for peripherals.

**Video output**: A **multi-output AV connector** (not RCA — a proprietary multi-pin connector) providing composite video and stereo-formatted (but actually mono) audio, plus RF. The composite output was a significant improvement over the Famicom's RF-only original design.

**Audio expansion**: The NES-001 has a **pin 47 on the cartridge connector** (EXP) for cartridge audio, mixed into the audio output. However the mixing circuitry differs from the Famicom: the NES mixes cartridge audio at a lower level than the Famicom does, meaning the same cartridge audio hardware is quieter relative to the APU on a NES than on a Famicom. This affects emulation of games with expansion audio when targeting accurate output levels.

**RF modulator**: Present, output on channel 3 or 4 (switchable).

---

### 3. PAL NES — Europe/Australia 1987, other PAL regions subsequently

Physically identical to the NES-001 (same case, same ZIF mechanism, same 72-pin connector) but with different silicon.

**CPU**: Ricoh **RP2A07**. Master clock **26.601712 MHz**. CPU clock = master ÷ 16 = **1.6629823 MHz** (vs 1.7897 MHz NTSC). The RP2A07 also differs from the RP2A03 in its APU: the frame counter period is adjusted for PAL timing, and the audio channel wavelength tables differ (to compensate for the slower clock so that musical notes play at correct pitch). The BCD disable and 6502 core are otherwise the same.

**PPU**: Ricoh **RP2C07**. Clock = master ÷ 5 = **5.320342 MHz**. The 2C07 generates PAL color timing instead of NTSC. A critical hardware difference: the **2C07 does not support the "short scanline" on odd frames** (the NTSC 2C02's "skip tick" where the pre-render scanline is 340 dots instead of 341 on odd frames). PAL games therefore have slightly different frame timing. Also, the PAL PPU has 312 scanlines per frame (vs 262) and VBlank is significantly longer (scanlines 241–311 vs 241–260). Sprites-per-scanline and rendering behavior are otherwise equivalent.

**CPU/PPU ratio**: PAL uses a **3.2 PPU clocks per CPU clock** ratio (not 3:1 as NTSC). This is why the master clock division is different — 26.601712 / 5 ÷ (26.601712 / 16) = 16/5 = 3.2.

**Timing implications for emulation**: Games not designed for PAL run at ~17% slower speed (60Hz NTSC vs 50Hz PAL output), music plays flat (slower CPU clock), and IRQ-based mapper timing (VRC4, etc.) fires at different points in the frame. The longer VBlank gives PAL games more CPU time per frame. Some NTSC games break on PAL hardware because they depend on the exact scanline count.

**CIC lockout**: A PAL-specific CIC variant prevents NTSC cartridges from running (and vice versa) — though the cartridge physical connector is identical.

**Color output**: PAL composite, 50Hz.

---

### 4. AV Famicom (HVC-101) — Japan, December 1993

A redesigned Famicom released near the end of the system's life, produced concurrently with the SNES to supply the continued Japanese Famicom market.

**CPU/PPU**: Same RP2A03 and RP2C02 as original Famicom. No change to core hardware behavior.

**Cartridge connector**: Same 60-pin Famicom slot. **Top-loading**, no ZIF mechanism. Simpler spring contacts — more reliable than the NES-001's ZIF tray.

**Controllers**: Now **detachable**, using the same 7-pin connector as NES controllers. The hardwired controller design of the original Famicom was abandoned. **The Player 2 microphone is gone** — the AV Famicom's P2 controller is a standard pad without microphone. The handful of games that used the microphone break on AV Famicom hardware when using the standard P2 controller, though some can be played with workarounds.

**Video output**: **Composite AV via standard RCA jacks** (yellow/white, or yellow/white/red for mono audio in both jacks). No RF modulator output — this is the first Famicom without RF. The composite video quality from the RCA jacks is significantly better than RF.

**Expansion port**: DA15 expansion port **retained** on the underside — the AV Famicom is compatible with FDS and other expansion peripherals.

**Physical size**: Smaller and lighter than the original HVC-001.

**Audio mixing**: The cartridge audio expansion mix ratio is similar to the original Famicom (different from the NES-001). This matters for accurate audio emulation.

**No region lockout**: Like the original Famicom, no CIC chip.

---

### 5. NES 2 (NES-101, "Top Loader") — North America / Europe, 1993

Nintendo's redesigned NES, released the same year as the AV Famicom and motivated by the same end-of-life cost reduction and reliability goals.

**CPU/PPU**: RP2A03 (NTSC) or RP2A07 (PAL). Same chips as NES-001.

**Cartridge connector**: **72-pin edge connector** retained for NES cartridge compatibility, but now **top-loading** with a simple friction-fit mechanism rather than the ZIF tray. No spring plate, no downward-pressing action. The cartridges drop straight in. This dramatically improves contact reliability — the NES-001's ZIF mechanism was the primary cause of the "blinking light" issue, not the 72-pin connector itself.

**CIC lockout**: The 10NES CIC is **absent** on the NES-101. Nintendo removed it for cost reduction, accepting that the market was mature enough that piracy prevention was no longer economically necessary. This means the NES-101 will run unlocked cartridges and even Famicom cartridges with a physical adapter.

**Controllers**: Detachable, same 7-pin connector as NES-001.

**Video output**: **RF only** — a significant step backward from the NES-001. No composite AV output on the standard NES-101. This was cost-reduction at the expense of video quality. Some modified or regional variants had composite added, and third-party AV modification kits became common.

**No expansion port**: The bottom expansion connector is absent.

**Audio**: Internally similar to NES-001. The cartridge audio mix point is the same (pin 47 EXP).

**Physical size**: Substantially smaller than the NES-001 toaster — a compact, rounded form factor.

---

### 6. Sharp Famicom Titler (AN-505) — Japan, 1989

A unique Sharp-built licensed Famicom variant with a **video title superimposer** built in.

**CPU/PPU**: Standard RP2A03 / RP2C02. Famicom-equivalent game compatibility.

**Cartridge connector**: 60-pin Famicom slot, top-loading.

**Titler hardware**: The device has a **video input** (composite) and can overlay the Famicom's PPU output onto an external video source. This was designed for creating custom title cards and graphics overlays on home video recordings — you could play a Famicom game and have it appear superimposed on camcorder footage.

**Video output**: **S-Video (Y/C)** output — the first Famicom-family device to offer S-Video. The PPU's analog color subcarrier signal is kept separate from the luma signal (the PPU's dot clock naturally generates these as separate signals before the encoder combines them). The Titler intercepts at this stage for higher quality. Composite output also present.

**Controllers**: Detachable, standard Famicom-compatible.

**Expansion port**: DA15 present.

**Keyboard**: The Titler had an **alphanumeric keyboard built into the unit** (or as a closely integrated peripheral on some configurations — sources differ). This keyboard was primarily for the titler/overlay text functionality, not for Famicom BASIC. Limited use for gaming.

---

### 7. Sharp Nintendo TV (C1 NES) — Japan, 1983–1989

A series of Sharp-manufactured CRT televisions with a **complete Famicom built into the television chassis**. Multiple screen sizes produced (14", 19", others).

**CPU/PPU**: Full RP2A03 / RP2C02. Complete Famicom hardware inside the TV.

**Cartridge slot**: 60-pin Famicom slot, mounted on the side or front of the television.

**Integration**: The PPU video output feeds directly into the TV's CRT driver circuitry without going through RF or composite. This gives potentially better image quality since no encode/decode cycle occurs.

**Controllers**: External, connected via standard Famicom controller ports on the TV body.

**Expansion**: DA15 port retained. FDS compatible.

**No separate display needed**: The self-contained nature was the selling point — a single unit for both TV viewing and Famicom gaming. Channel selection physically switches between the TV tuner and the internal Famicom video path.

**Revisions**: Sharp C1 (1983), and subsequent models through the late 1980s, with incremental improvements to the TV hardware while the Famicom hardware remained constant.

---

### 8. Famicom Box (HVC-FX) — Japan, 1986

A commercial/hospitality variant designed for **hotel and public installation use**.

**CPU/PPU**: Standard Famicom hardware.

**Cartridge slot**: A **lockable external slot** accepts standard Famicom cartridges, but the unit also has **built-in ROM** for a selection of pre-loaded games (15 games internally, titles including Mahjong, Golf, and others selected for broad appeal).

**Coin/timer control**: The unit connects to a **timer/coin controller** that restricts play to paid time increments. When the timer runs out, the game pauses and the display prompts for further payment.

**External controls**: A wired control panel (for the hotel room or arcade space) manages the timer, channel select between built-in games, and lockout.

**No consumer expansion**: No DA15 expansion port accessible to the user. No FDS capability.

**Video**: Standard composite output to a monitor/TV provided by the hotel.

**Region**: Japan only. Extremely rare today.

---

### 9. VS. System — Arcade, 1984–1990

Nintendo's NES-derived arcade platform. Two configurations:

#### VS. Unisystem (single board, single screen)
A dedicated arcade PCB containing:
- **CPU**: RP2A03 (NTSC clock)
- **PPU**: One of several variants: **RP2C03B, RP2C03G, RP2C04-0001, RP2C04-0002, RP2C04-0003, RP2C04-0004, RP2C05-01 through RP2C05-05**

The 2C03 outputs **RGB** (digital, 3 bits per channel = 512 theoretical colors, 54 used) instead of composite NTSC. The RGB output drives an arcade monitor directly.

The 2C04 and 2C05 variants have **shuffled palette index tables** — the same physical colors exist but the index numbers mapping to those colors are permuted differently in each variant. A game written for 2C04-0001 expects color index $35 to be a specific shade; on the 2C02 that index is a completely different color. Each VS. game is programmed for its specific PPU variant.

**Security chip**: A custom ASIC modifies the bit patterns returned when reading $4016 (controller 1) and $4017 (controller 2). Different VS. games use different security configurations — swapping which bits come from which register, inverting certain bits. This prevents generic NES ROMs from booting on VS. hardware and VS. ROMs from running on consumer hardware without the correct security chip. The security configurations are game-specific.

**Coin mechanism**: Standard arcade coin mechanism connected to the CPU's I/O registers.

**Game media**: VS. cartridges are specific PCBs (not consumer cartridges) fitting a VS.-specific slot.

#### VS. Dualsystem (two boards, two screens)
Two complete VS. Unisystem boards in a single cabinet, sharing a coin mechanism. Each board drives its own monitor. Used for two-player competitive games where each player has their own screen (*VS. Tennis*, *VS. Baseball*). The two boards communicate via a simple synchronization line.

---

### 10. PlayChoice-10 — Arcade, 1986–1991

A different arcade NES derivative, oriented toward a **try-before-you-buy** promotional model rather than dedicated arcade games.

**CPU**: RP2A03. Standard NTSC.

**PPU**: **RP2C03** — RGB output (same as VS. System's base PPU variant). The 2C03 is used consistently across PlayChoice-10 units.

**Second processor**: A **Z80 CPU** running at ~6 MHz handles the **instruction display system** — a separate screen mounted above the game screen shows game instructions, a demo attract mode, and a title screen. The Z80 has its own RAM and ROM (the instruction ROM is a separate chip in each PlayChoice game cartridge alongside the NES program/character data).

**Dual-monitor**: Two separate monitors — upper for instructions (Z80 driven), lower for gameplay (PPU driven). The instruction display is black and white or limited color; the game display is full NES color via the 2C03's RGB output.

**Timer/coin**: Coin purchase buys time credit. When time expires, game pauses.

**PlayChoice cartridges**: Custom boards fitting the PC-10 slot, containing the standard NES PRG-ROM, CHR-ROM, plus an additional instruction ROM for the Z80 system and the CIC equivalent.

**Up to 10 game slots**: The cabinet has a selection dial for up to 10 different game PCBs installed simultaneously. The player selects a game from the menu, inserts coins, and plays.

**No mapper complexity**: Most PlayChoice games use NROM or simple mappers — the PlayChoice library is heavily weighted toward early NES games.

---

## Hardware Comparison Matrix

| Feature | Famicom HVC-001 | NES-001 | PAL NES | AV Famicom HVC-101 | NES-101 | Sharp Titler | Famicom Box | VS. System | PlayChoice-10 |
|---|---|---|---|---|---|---|---|---|---|
| Cart connector | 60-pin | 72-pin | 72-pin | 60-pin | 72-pin | 60-pin | 60-pin | VS.-specific | PC-10 specific |
| Loading direction | Top | Front | Front | Top | Top | Top | Top | — | — |
| CPU | RP2A03 | RP2A03 | RP2A07 | RP2A03 | RP2A03/07 | RP2A03 | RP2A03 | RP2A03 | RP2A03 |
| PPU | RP2C02 | RP2C02 | RP2C07 | RP2C02 | RP2C02/07 | RP2C02 | RP2C02 | RP2C03/04/05 | RP2C03 |
| Video out | RF (+AV later) | RF+composite | RF+composite | Composite RCA | RF only | S-Video+comp | Composite | RGB | RGB |
| Controllers | Hardwired | Detachable | Detachable | Detachable | Detachable | Detachable | Fixed panel | Arcade | Arcade |
| P2 microphone | Yes | No | No | No | No | No | No | No | No |
| DA15 expansion | Yes | No | No | Yes | No | Yes | No | No | No |
| CIC lockout | No | Yes | Yes (PAL) | No | No | No | No | Security chip | Security chip |
| Expansion audio mix | Full level | Reduced | Reduced | Full level | Reduced | Full level | Full level | N/A | N/A |
| Frame rate | 60.098 Hz | 60.098 Hz | 50.007 Hz | 60.098 Hz | 60.098/50 Hz | 60.098 Hz | 60.098 Hz | 60.098 Hz | 60.098 Hz |

---

## Keyboard Support

### Official Nintendo Keyboard: Family BASIC Keyboard (HVC-007)

The **Family BASIC Keyboard** is the primary keyboard peripheral for the Famicom family, released in Japan in June 1984, co-developed with Hudson Soft.

#### Physical and Electrical Design

The keyboard is a full-width unit that sits in front of the Famicom. It connects via **two plugs** that insert into the **DA15 expansion port** on the underside of the Famicom. (Some sources describe it as using the controller ports — it depends on revision. The primary interface is the DA15 expansion bus.)

The keyboard has **72 keys** in a standard Japanese JIS layout, including:
- Full alphanumeric keys
- Function keys F1–F8
- GRAPH key (switches to graphic character input mode)
- KANA key (switches between Latin and Katakana character input)
- INS, DEL, HOME, CLR keys
- Four cursor direction keys
- A STOP key (like the C64's RUN/STOP)
- A カナ/かな (kana/romaji) toggle

The keyboard uses a **matrix scan** approach. The CPU reads the keyboard state by the software in the Family BASIC cartridge writing to keyboard control lines via the expansion port, then reading back the row data.

#### Required Cartridge: Family BASIC (HVC-BASIC)

The keyboard is useless without the **Family BASIC cartridge**, which contains a **Microsoft BASIC interpreter** (developed by Microsoft Japan and Hudson Soft) in ROM. The BASIC is a fairly capable interpreter:
- Standard BASIC with LINE, CIRCLE, PAINT graphics commands that drive the PPU
- SPRITE commands to define and move hardware sprites
- PLAY command for music (drives the APU channels)
- LOAD/SAVE commands using the Family Data Recorder

Three versions of the cartridge were released: **V1.0, V1.1, V2.0, V3.0**. V3.0 added enhanced sprite commands and additional math functions.

The BASIC runs in a 32KB RAM address space. Programs are stored in PRG-RAM within the system RAM; the BASIC cartridge provides 8KB of program storage in battery-backed RAM. Without the Data Recorder, saving is limited to what fits in the cartridge's SRAM.

#### Family Data Recorder (HVC-008)

An audio **cassette tape interface** peripheral. Connects to the DA15 expansion port alongside or via the keyboard. Programs written in Family BASIC are saved as audio-encoded data (Kansas City Standard or similar FSK encoding) to standard compact cassettes. Load and save use the LOAD and SAVE BASIC commands. Transfer speed was slow (approximately 1200 baud equivalent) but provided unlimited storage for programs.

#### Compatibility

The Family BASIC keyboard works **only on original Famicom (HVC-001) and AV Famicom (HVC-101)**, both of which have the DA15 expansion port. It is **incompatible with the NES-001 and NES-101** which have no exposed DA15 port. There was no official NES keyboard released in North America or Europe.

---

### Sharp Famicom Titler Keyboard

As noted above, the AN-505 Titler had keyboard input capability integrated into or closely bundled with the unit. This keyboard was oriented toward the **title overlay feature** — typing text to superimpose on video — rather than as a BASIC programming interface. It did not run Family BASIC.

---

### Famicom Network System (HVC-NET) — 1988

Nintendo released a **modem peripheral** for the Famicom in Japan called the Famicom Network System (also associated with the Family Computer Network System). It connected to the DA15 expansion port and allowed connecting to Nintendo's network service (Famicom Tsūshin Network) for downloading game data, stock prices, weather, and sports scores — an early online service.

Some configurations of this network setup included a **small 24-key remote-style keyboard** for entering usernames, addresses, and alphanumeric input into network services. This was not a full keyboard in the Family BASIC sense — it was more like a TV remote with alphanumeric input. It connected via the expansion port.

---

### Third-Party Keyboards (Japan)

Several Japanese third-party manufacturers produced keyboard peripherals for the Famicom that connected either via the expansion port or by replacing the standard controller connections:

**Famicom Keyboard by Hori**: A simpler membrane keyboard intended primarily for data entry in specific software titles that supported it.

**Various Data Entry Keypads**: Several business-oriented Famicom software titles (tax preparation software, address book software sold in Japan) came bundled with numeric keypads or limited keyboards that connected via the expansion bus. These were game-specific accessories.

---

### No Official NES Keyboard (North America / Europe)

Nintendo **never released an official keyboard** for the North American or European NES. The absence of the DA15 expansion port on the NES-001 made it architecturally more difficult to add meaningful expansion peripherals — the controller ports are the only practical expansion interface, and they provide very limited bandwidth (a simple serial shift register).

Third-party companies produced a few unofficial keyboard accessories for the NES but these were niche products with essentially no software support, and they were never significant in the market.

---

### Power Pad / Family Fun Fitness (peripherally relevant)

Not a keyboard, but worth noting: the **Power Pad** (NES) / **Family Fun Fitness** (Famicom) is a floor mat with 12 pressure-sensitive buttons arranged in a grid. It connects to the controller port and reads as two controller inputs (both ports used simultaneously for 12 buttons). Some have noted its conceptual kinship with a keyboard as a grid of input zones, but it's entirely separate hardware.

---

## Summary: Keyboard Availability by Console

| Console | Official keyboard | Notes |
|---|---|---|
| Famicom HVC-001 | **Yes** — HVC-007 | Full 72-key, requires Family BASIC cartridge + DA15 port |
| AV Famicom HVC-101 | **Yes** — HVC-007 | DA15 port retained; compatible |
| NES-001 (NTSC/PAL) | **No** | No DA15 port; no official keyboard released |
| NES-101 | **No** | No expansion port |
| Sharp Titler AN-505 | **Partial** | Built-in/bundled, titler use only |
| Sharp Nintendo TV | **No** | TV form factor; no keyboard peripheral |
| Famicom Box | **No** | Hotel/commercial, no keyboard support |
| VS. System | **No** | Arcade; operator controls only |
| PlayChoice-10 | **No** | Arcade; no keyboard input |
| Famicom Network System | **Partial** | Small numeric/alpha keypad for network services, not general purpose |

The Family BASIC keyboard ecosystem (HVC-007 + HVC-008 + Family BASIC cartridge) represents a genuine, if modest, home computer experience layered on top of the Famicom — one of the more interesting historical footnotes of the platform, and entirely invisible to the North American and European NES market.