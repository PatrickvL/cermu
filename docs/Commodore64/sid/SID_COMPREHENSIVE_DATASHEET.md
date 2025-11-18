# MOS Technology 6581/8580 SID Chip - Comprehensive Technical Datasheet
## All Revisions and Variants (1981-1993)

**Document Version:** 1.0  
**Date:** November 18, 2025  
**Compiled from:** Official datasheets, reverse-engineering efforts, and empirical measurements

---

## Table of Contents

1. [Overview and History](#overview-and-history)
2. [Chip Revisions and Variants](#chip-revisions-and-variants)
3. [Physical and Electrical Specifications](#physical-and-electrical-specifications)
4. [Pin Configuration](#pin-configuration)
5. [Register Map](#register-map)
6. [Functional Blocks](#functional-blocks)
7. [Revision-Specific Differences](#revision-specific-differences)
8. [Known Bugs and Quirks](#known-bugs-and-quirks)
9. [Filter Characteristics](#filter-characteristics)
10. [Waveform Generation](#waveform-generation)
11. [Timing Specifications](#timing-specifications)
12. [Application Notes](#application-notes)

---

## 1. Overview and History

### 1.1 General Description

The MOS Technology 6581/8580 SID (Sound Interface Device) is a single-chip, 3-voice electronic music synthesizer and sound effects generator designed for the MOS 65XX processor family. It was designed by Robert "Bob" Yannes and his team in just five months during the latter half of 1981.

**Key Features:**
- Three independent programmable audio oscillators
- Four waveforms per oscillator (triangle, sawtooth, pulse, noise)
- Programmable ADSR envelope generator per voice
- Multi-mode resonant filter (low-pass, high-pass, band-pass, notch)
- Ring modulation and oscillator sync capabilities
- Two built-in 8-bit A/D converters (for paddles/mice)
- External audio input
- Mixed-signal design (digital + analog)

### 1.2 Design Philosophy

Bob Yannes was inspired by professional synthesizers (Moog, ARP, Sequential Circuits) and wanted to create a true musical instrument chip, not just a simple tone generator. Approximately 75% of his original feature list made it into the final design due to time constraints.

**Patent Information:**
- U.S. Patent 4,677,890
- Filed: February 27, 1983
- Issued: July 7, 1987
- Expired: July 7, 2004

### 1.3 Applications

- Commodore 64 (1982-1994)
- Commodore 128 (1985-1989)
- Commodore CBM-II series
- Commodore MAX Machine
- Various third-party hardware (SID Symphony, HardSID, SIDStation)

---

## 2. Chip Revisions and Variants

### 2.1 Complete Revision Timeline

| Model | Package Marking | Date Code Range | Production Period | Packaging | Estimated Qty | Notes |
|-------|----------------|-----------------|-------------------|-----------|---------------|-------|
| **6581 R1** | "MOS 6581" | 4981-0882 | 1981-1982 | Ceramic DIP-28 | 50-100 | Prototype only, CES/dev units |
| **6581 R2** | "MOS 6581" or "CSG 6581" | 0882-4985 | 1982-1985 | Plastic DIP-28 | Millions | First production variant |
| **6581 R3** | "MOS 6581 R3" or "6581 CBM" | ~1983-0986 | 1983-1986 | Plastic DIP-28 | Millions | Most common variant |
| **6581 R4** | "MOS 6581 R4" or "CSG 6581 R4" | 4985-2590 | 1985-1990 | Plastic DIP-28 | Millions | Silicon grade improved |
| **6581 R4 AR** | "MOS 6581 R4 AR" or "CSG 6581 R4 AR" | 2286-4392 | 1986-1992 | Plastic DIP-28 | Millions | Adjusted silicon grade |
| **6582** | "MOS 6582" or "CSG 6582" | ~1986 | 1986 | Plastic DIP-28 | Limited | HMOS-II process, 9V |
| **6582 A** | "MOS 6582 A" or "CSG 6582 A" | ~1989-1992 | 1989-1992 | Plastic DIP-28 | Limited | Service replacement |
| **8580 R5** | "MOS 8580R5" or "CSG 8580R5" | 1986-4893 | 1986-1993 | Plastic DIP-28 | Millions | HMOS-II redesign |

### 2.2 Production Locations

Chips were manufactured in multiple facilities:
- **United States:** Various MOS/CSG facilities
- **Hong Kong:** 6582, some 8580R5
- **Philippines:** 6582 A, many 8580R5 (especially 1989+)

**Manufacturer Markings:**
- "MOS" - MOS Technology branding
- "CSG" - Commodore Semiconductor Group branding
- Both markings appeared simultaneously (same week datecodes) indicating parallel production lines

### 2.3 Date Code Format

Format: **WWYY**
- WW = Week of year (01-52)
- YY = Year (last 2 digits)

Examples:
- 2082 = Week 20 of 1982
- 4392 = Week 43 of 1992

---

## 3. Physical and Electrical Specifications

### 3.1 Package Details

**Package Type:** 28-pin Dual In-line Package (DIP-28)
- Pin spacing: 0.1" (2.54mm)
- Row spacing: 0.6" (15.24mm)  
- Package width: ~0.6" (15.24mm)
- Package length: ~1.4" (35.56mm)

**Packaging Evolution:**
- R1: Ceramic DIP-28 (white/beige ceramic)
- R2-R5: Plastic DIP-28 (black epoxy)

### 3.2 Power Requirements

#### 6581 Series (NMOS Process)

| Supply | Voltage | Tolerance | Current | Power | Pin |
|--------|---------|-----------|---------|-------|-----|
| Vcc | +5V DC | ±5% (4.75-5.25V) | ~70 mA | ~350 mW | 28 |
| Vdd | +12V DC | ±5% (11.4-12.6V) | ~25 mA | ~300 mW | 25 |
| **Total** | - | - | ~95 mA | **~650 mW** | - |

**Notes:**
- 6581 is very sensitive to voltage variations
- Separate regulated supplies recommended for Vcc and Vdd
- Bypass capacitors required: 10μF electrolytic + 100nF ceramic on each rail
- Draws more power when filter is active with resonance

#### 8580/6582 Series (HMOS-II Process)

| Supply | Voltage | Tolerance | Current | Power | Pin |
|--------|---------|-----------|---------|-------|-----|
| Vcc | +5V DC | ±5% (4.75-5.25V) | ~65 mA | ~325 mW | 28 |
| Vdd | +9V DC | ±5% (8.55-9.45V) | ~25 mA | ~225 mW | 25 |
| **Total** | - | - | ~90 mA | **~550 mW** | - |

**Advantages:**
- Lower power consumption (~15% reduction)
- Runs cooler (important for reliability)
- Less susceptible to power supply noise
- More tolerant of voltage variations

### 3.3 Operating Conditions

| Parameter | Min | Typ | Max | Units |
|-----------|-----|-----|-----|-------|
| Operating Temperature | 0 | 25 | 70 | °C |
| Storage Temperature | -55 | - | 150 | °C |
| Φ2 Clock Frequency | 0.05 | 1.0 | 1.05 | MHz |
| Input Voltage (digital) | -0.3 | - | Vcc+0.3 | V |
| Input Voltage (analog) | -0.3 | - | +17 | V |

**Critical Notes:**
- 6581 is EXTREMELY sensitive to ESD (Electrostatic Discharge)
- Improper handling often damages filter circuitry permanently
- 8580 has improved ESD protection
- All inputs have protection circuitry, but insufficient on 6581

### 3.4 Clock Requirements

**Φ2 Input (Pin 6):**
- Standard frequency: 0.985248 MHz (PAL) or 1.022727 MHz (NTSC)
- Clock must be TTL-compatible
- Rise/fall times: ≤25ns
- Duty cycle: 45-55% (nominally 50%)

**Clock Timing:**
```
Tcyc (Clock Cycle Time):    1-20 μs
Tc (Clock High Width):      450-10,000 ns (typ 500ns)
Tr/Tf (Rise/Fall Time):     ≤25 ns
```

---

## 4. Pin Configuration

```
         ┌─────┴─────┐
   CAP1A │ 1      28 │ Vcc (+5V)
   CAP1B │ 2      27 │ AUDIO OUT
   CAP2A │ 3      26 │ EXT IN
   CAP2B │ 4      25 │ Vdd (+12V/+9V)
    /RES │ 5      24 │ POTX
     Φ2  │ 6      23 │ POTY
     R/W │ 7      22 │ D7
     /CS │ 8      21 │ D6
     A0  │ 9      20 │ D5
     A1  │10      19 │ D4
     A2  │11      18 │ D3
     A3  │12      17 │ D2
     A4  │13      16 │ D1
     GND │14      15 │ D0
         └───────────┘
```

### 4.1 Pin Descriptions

#### Power Pins

**Pin 28 - Vcc (+5V DC)**
- Digital logic power supply
- Must be well-regulated and filtered
- Separate 5V trace from CPU Vcc recommended
- Bypass with 10μF + 100nF capacitors

**Pin 25 - Vdd (+12V for 6581, +9V for 8580/6582)**
- Analog section power supply
- Powers oscillators, filters, and output stage
- Must be well-regulated and low-noise
- Bypass with 10μF + 100nF capacitors
- **CRITICAL:** Using wrong voltage damages chip!

**Pin 14 - GND**
- Common ground for digital and analog sections
- Single-point grounding recommended
- Star ground configuration for multi-SID systems

#### Filter Capacitor Pins

**Pins 1-2 - CAP1A/CAP1B**
- External integrating capacitor for filter stage 1
- Connect matched capacitor between pins 1 and 2

**Pins 3-4 - CAP2A/CAP2B**  
- External integrating capacitor for filter stage 2
- Connect matched capacitor between pins 3 and 4

**Capacitor Values by Revision:**

| Chip | Datasheet Value | Typical C64 Value | Frequency Range | Notes |
|------|----------------|-------------------|-----------------|-------|
| 6581 R1 | 2200 pF | 1000 pF | 30 Hz - 12 kHz | Early datasheet spec |
| 6581 R2/R3 | 2200 pF | 470 pF | 30 Hz - 12 kHz | Most common in production |
| 6581 R4/R4AR | 2200 pF | 470 pF | 30 Hz - 12 kHz | Some units: 1000-2200 pF |
| 8580 R5 | 6800 pF | 22 nF (22000 pF) | 30 Hz - 12 kHz | Completely different range |
| 6582/6582A | 6800 pF | 22 nF | 30 Hz - 12 kHz | Same as 8580 |

**Capacitor Effects:**
- **Lower values** (470 pF): Higher cutoff frequencies, better bass control
- **Higher values** (2200 pF): Lower cutoff frequencies, datasheet recommendation
- **Mismatched capacitors:** Cause filter tracking problems in multi-SID setups
- **Material:** Polystyrene or C0G/NP0 ceramic preferred (temperature stable)

**Filter Frequency Formula:**
```
Fmax ≈ 1 / (2π × R × C)
Where R is internal resistance (~10kΩ nominal)
```

#### Audio Pins

**Pin 27 - AUDIO OUT**
- Final composite audio output
- Open-source buffer output
- DC level: ~6V on 6581, ~4.5V on 8580
- Output swing: ~3V peak-to-peak maximum
- **MUST be AC-coupled:** Use 1-10 μF electrolytic capacitor to amplifier
- Output impedance: Requires 1kΩ pull-down resistor to ground
- Load impedance: >10kΩ recommended

**Pin 26 - EXT IN (External Audio Input)**
- External audio can be mixed/filtered with SID output
- Input impedance: ~1 MΩ
- Input level: 0-3V peak-to-peak
- Can be routed through filter (FILT EX bit in register $D417)
- **8580 digi-boost hack:** Connect via 470kΩ-1MΩ resistor to ground to restore DC bias for sample playback

#### Control Pins

**Pin 5 - /RES (Reset, active low)**
- TTL-compatible input
- Pull low to reset SID to initial state
- Resets all oscillators, envelopes, and registers
- Minimum pulse width: 10 clock cycles recommended

**Pin 6 - Φ2 (Phase 2 Clock)**
- System clock input (TTL-compatible)
- Standard: 0.985248 MHz (PAL) or 1.022727 MHz (NTSC)
- Acceptable range: 0.05-1.05 MHz
- Synchronous with 6502/6510 Φ2 clock

**Pin 7 - R/W (Read/Write)**
- TTL-compatible input
- High = Read operation
- Low = Write operation  
- Must be stable before /CS goes low

**Pin 8 - /CS (Chip Select, active low)**
- TTL-compatible input
- Enables SID for read/write operations
- Address decoding: Typically $D400-$D7FF (C64 I/O space)

#### Address Pins

**Pins 9-13 - A0-A4 (Address Bus)**
- 5-bit address input (selects 1 of 32 locations)
- Only 29 registers implemented ($00-$1C)
- Addresses $1D-$1F mirror earlier registers
- TTL-compatible inputs

#### Data Bus

**Pins 15-22 - D0-D7 (Bidirectional Data Bus)**
- 8-bit data interface
- Tri-state outputs when not selected
- TTL-compatible I/O
- Valid data during Φ2 high

#### Analog Input Pins

**Pin 24 - POTX (Potentiometer X)**
- Analog input for paddle/mouse X-axis
- Input range: 0-5V
- Internal 8-bit A/D converter
- Conversion rate: Every 512 clock cycles
- Typical pot value: 470kΩ
- Timing capacitor: 1000 pF to ground

**Pin 23 - POTY (Potentiometer Y)**
- Same as POTX for Y-axis
- Independent 8-bit A/D converter

---

## 5. Register Map

### 5.1 Complete Register Table

| Addr | Hex | Register Name | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | Type |
|------|-----|---------------|----|----|----|----|----|----|----|----|------|
| **Voice 1** |||||||||||||
| 0 | $D400 | FREQ1 LO | F7-F0 (Frequency bits 7-0) |||||||| W |
| 1 | $D401 | FREQ1 HI | F15-F8 (Frequency bits 15-8) |||||||| W |
| 2 | $D402 | PW1 LO | PW7-PW0 (Pulse Width bits 7-0) |||||||| W |
| 3 | $D403 | PW1 HI | - | - | - | - | PW11 | PW10 | PW9 | PW8 | W |
| 4 | $D404 | CONTROL1 | NOISE | PULSE | SAW | TRI | TEST | RING | SYNC | GATE | W |
| 5 | $D405 | AD1 | ATK3-ATK0 | DCY3-DCY0 |||||||| W |
| 6 | $D406 | SR1 | STN3-STN0 | RLS3-RLS0 |||||||| W |
| **Voice 2** |||||||||||||
| 7 | $D407 | FREQ2 LO | F7-F0 |||||||| W |
| 8 | $D408 | FREQ2 HI | F15-F8 |||||||| W |
| 9 | $D409 | PW2 LO | PW7-PW0 |||||||| W |
| 10 | $D40A | PW2 HI | - | - | - | - | PW11 | PW10 | PW9 | PW8 | W |
| 11 | $D40B | CONTROL2 | NOISE | PULSE | SAW | TRI | TEST | RING | SYNC | GATE | W |
| 12 | $D40C | AD2 | ATK3-ATK0 | DCY3-DCY0 |||||||| W |
| 13 | $D40D | SR2 | STN3-STN0 | RLS3-RLS0 |||||||| W |
| **Voice 3** |||||||||||||
| 14 | $D40E | FREQ3 LO | F7-F0 |||||||| W |
| 15 | $D40F | FREQ3 HI | F15-F8 |||||||| W |
| 16 | $D410 | PW3 LO | PW7-PW0 |||||||| W |
| 17 | $D411 | PW3 HI | - | - | - | - | PW11 | PW10 | PW9 | PW8 | W |
| 18 | $D412 | CONTROL3 | NOISE | PULSE | SAW | TRI | TEST | RING | SYNC | GATE | W |
| 19 | $D413 | AD3 | ATK3-ATK0 | DCY3-DCY0 |||||||| W |
| 20 | $D414 | SR3 | STN3-STN0 | RLS3-RLS0 |||||||| W |
| **Filter** |||||||||||||
| 21 | $D415 | FC LO | - | - | - | - | - | FC2 | FC1 | FC0 | W |
| 22 | $D416 | FC HI | FC10-FC3 (Filter Cutoff bits 10-3) |||||||| W |
| 23 | $D417 | RES/FILT | RES3-RES0 | FLTEX | FILT3 | FILT2 | FILT1 | W |
| 24 | $D418 | MODE/VOL | 3OFF | HP | BP | LP | VOL3-VOL0 |||| W |
| **Read-Only** |||||||||||||
| 25 | $D419 | POTX | PX7-PX0 (Pot X value 0-255) |||||||| R |
| 26 | $D41A | POTY | PY7-PY0 (Pot Y value 0-255) |||||||| R |
| 27 | $D41B | OSC3 | O7-O0 (Oscillator 3 output) |||||||| R |
| 28 | $D41C | ENV3 | E7-E0 (Envelope 3 output) |||||||| R |

### 5.2 Register Details

#### Frequency Registers (FREQ LO/HI)

**16-bit frequency control per voice**

Formula: `Fout = (Fn × Fclk) / 16,777,216 Hz`

For standard 1.0 MHz clock:
- `Fout = Fn × 0.059604645 Hz`
- Minimum: 0.0596 Hz (Fn = 1)
- Maximum: 3906.1875 Hz (Fn = 65535)
- Resolution: 0.059604645 Hz/step

**Range:** ~8 octaves (16 Hz to 4 kHz practical)

**Equal-Tempered Scale:**
| Note | Frequency | Fn Value (PAL) | Fn Value (NTSC) |
|------|-----------|----------------|-----------------|
| C-1 | 32.703 Hz | 548 ($0224) | 537 ($0219) |
| A-4 | 440 Hz | 7382 ($1CD6) | 7217 ($1C31) |
| C-8 | 4186.01 Hz | 70230 ($11256) | 68633 ($10BF9) |

#### Pulse Width Registers (PW LO/HI)

**12-bit pulse width control (bits 0-11 used)**

Formula: `PWout = (PWn / 40.95) %`

- Minimum: 0% (PWn = 0) → constant LOW output
- 50% square wave: PWn = 2048 ($800)
- Maximum: 99.976% (PWn = 4095 = $FFF) → constant HIGH output

**Notes:**
- Only affects PULSE waveform
- Can be swept smoothly for phase-modulation effects
- Values 0 and 4095 produce DC output (often filtered out)

#### Control Register (Each Voice)

**Bit 0 - GATE:**
- 0 = Release cycle begins
- 1 = Attack/Decay/Sustain cycles

**Bit 1 - SYNC:**
- Voice 1: Syncs to Voice 3
- Voice 2: Syncs to Voice 1
- Voice 3: Syncs to Voice 2
- Slave oscillator resets when master oscillator wraps
- Creates "hard sync" effects

**Bit 2 - RING (Ring Modulation):**
- Voice 1: Ring mod with Voice 3
- Voice 2: Ring mod with Voice 1
- Voice 3: Ring mod with Voice 2
- Only affects triangle waveform
- Creates metallic, bell-like timbres

**Bit 3 - TEST:**
- 1 = Resets and holds oscillator at zero
- Locks noise generator
- Pulse output held at DC level
- Used for synchronization or special effects
- Must be cleared for normal operation

**Bits 4-7 - Waveform Selection:**
- **Bit 4 - TRIANGLE:** Mellow, flute-like tone, low harmonics
- **Bit 5 - SAWTOOTH:** Bright, brassy, rich in harmonics
- **Bit 6 - PULSE:** Variable timbre via pulse width, hollow to reedy
- **Bit 7 - NOISE:** Pseudo-random, 23-bit LFSR-based

**Waveform Combination:**
- **IMPORTANT:** Waveforms are NOT additive
- Multiple waveforms selected = Complex interaction
- 6581: Often produces silence or unpredictable results
- 8580: Better approximation of binary AND (but still imperfect)
- **NOISE WARNING:** Selecting noise with other waveforms can lock up noise generator (requires TEST bit reset)

#### Attack/Decay Register (AD)

**Bits 4-7 - Attack Rate (ATK0-ATK3):**
Determines rise time from zero to peak amplitude

**Bits 0-3 - Decay Rate (DCY0-DCY3):**
Determines fall time from peak to sustain level

**Envelope Rate Table (based on 1.0 MHz Φ2):**

| Value | Attack Time | Decay/Release Time | Cycles |
|-------|-------------|-------------------|--------|
| 0 | 2 ms | 6 ms | 2,000 / 6,000 |
| 1 | 8 ms | 24 ms | 8,000 / 24,000 |
| 2 | 16 ms | 48 ms | 16,000 / 48,000 |
| 3 | 24 ms | 72 ms | 24,000 / 72,000 |
| 4 | 38 ms | 114 ms | 38,000 / 114,000 |
| 5 | 56 ms | 168 ms | 56,000 / 168,000 |
| 6 | 68 ms | 204 ms | 68,000 / 204,000 |
| 7 | 80 ms | 240 ms | 80,000 / 240,000 |
| 8 | 100 ms | 300 ms | 100,000 / 300,000 |
| 9 | 250 ms | 750 ms | 250,000 / 750,000 |
| 10 | 500 ms | 1.5 s | 500,000 / 1,500,000 |
| 11 | 800 ms | 2.4 s | 800,000 / 2,400,000 |
| 12 | 1 s | 3 s | 1,000,000 / 3,000,000 |
| 13 | 3 s | 9 s | 3,000,000 / 9,000,000 |
| 14 | 5 s | 15 s | 5,000,000 / 15,000,000 |
| 15 | 8 s | 24 s | 8,000,000 / 24,000,000 |

**Scaling for other clock frequencies:**
```
Actual_Time = Table_Time × (1 MHz / Φ2_Frequency)
```

**Envelope Behavior:**
- Exponential curves (not linear)
- Attack is fastest at low amplitudes, slows near peak
- Decay/Release fastest at high amplitudes, slows near zero
- GATE can be toggled at any point (creates complex envelopes)

#### Sustain/Release Register (SR)

**Bits 4-7 - Sustain Level (STN0-STN3):**
- 16 linear levels from 0 (silent) to 15 (peak)
- Amplitude held while GATE = 1 after decay
- Value 8 = 50% of peak amplitude
- Value 15 = 100% peak amplitude

**Bits 0-3 - Release Rate (RLS0-RLS3):**
- Same rates as Decay (see table above)
- Begins when GATE cleared to 0

#### Filter Cutoff (FC LO/HI)

**11-bit filter cutoff frequency control**
- Bits 3-7 of FC LO unused
- Controls filter corner/center frequency
- Approximate range: 30 Hz to 12 kHz (depends on capacitors)

**6581 Characteristic:**
- Non-linear response (sigmoid on log scale)
- Varies significantly between chips (±30% tolerance)
- "Character" depends on manufacturing batch

**8580 Characteristic:**
- Linear response  
- Consistent between chips (±5% tolerance)
- More predictable but less "warm"

#### Resonance/Filter Routing (RES/FILT)

**Bits 4-7 - Resonance (RES0-RES3):**
- 16 linear levels
- Emphasizes frequencies at cutoff point
- 0 = No resonance
- 15 = Maximum resonance (can self-oscillate)

**6581:** Resonance effect very subtle (resistor ladder tolerances)
**8580:** Stronger resonance effect, more pronounced

**Bits 0-3 - Filter Routing:**
- **Bit 0 (FILT1):** Route Voice 1 through filter
- **Bit 1 (FILT2):** Route Voice 2 through filter
- **Bit 2 (FILT3):** Route Voice 3 through filter
- **Bit 3 (FILTEX):** Route external input through filter

**Note:** Voices not routed through filter go directly to output

#### Mode/Volume Register (MODE/VOL)

**Bit 7 - 3 OFF:**
- 1 = Disconnect Voice 3 from direct audio path
- Voice 3 still available via OSC3/ENV3 registers
- Used when Voice 3 provides modulation only

**Bits 4-6 - Filter Mode:**
- **Bit 4 (LP):** Low-pass filter
- **Bit 5 (BP):** Band-pass filter  
- **Bit 6 (HP):** High-pass filter
- **Multiple modes can be selected simultaneously (additive)**
- LP+HP = Notch (band-reject) filter

**Filter Slopes:**
- Low-pass: 12 dB/octave
- High-pass: 12 dB/octave
- Band-pass: 6 dB/octave (each side)

**6581 Filter Mode Differences:**
- High-pass output: -3 dB attenuation (makes sound bassier)
- Filter can self-oscillate at high resonance
- Non-linear distortion (harmonic generation)

**8580 Filter Mode Differences:**
- All modes at equal level
- Less distortion (cleaner sound)
- More precise tracking

**Bits 0-3 - Volume (VOL0-VOL3):**
- 16 linear volume levels
- 0 = Silent
- 15 = Maximum volume
- Controls ALL SID output (not per-voice)
- **6581 Bug:** Volume changes create audible clicks (DC offset modulation)
- **8580 Fix:** Volume changes are silent (DC offset removed)

#### Read-Only Registers

**$D419 - POTX (Potentiometer X Reading)**
- 8-bit value (0-255)
- 0 = Minimum resistance (pot at ground)
- 255 = Maximum resistance (pot at +5V)
- Updated every 512 clock cycles

**$D41A - POTY (Potentiometer Y Reading)**
- Same as POTX for Y axis

**$D41B - OSC3 (Oscillator 3 Output)**
- Upper 8 bits of oscillator 3's accumulator
- **Triangle:** Counts 0→255→0 (saw up, saw down)
- **Sawtooth:** Counts 0→255, wraps to 0
- **Pulse:** Jumps between 0 and 255
- **Noise:** Pseudo-random values
- Updated continuously (read at any time)
- **Use:** Modulation source, random numbers, timing

**$D41C - ENV3 (Envelope 3 Output)**
- Current envelope generator value for Voice 3
- 8-bit value (0-255)
- Reflects ADSR state
- **Use:** Filter sweeps, modulation effects
- Requires GATE to be active for meaningful output

---

## 6. Functional Blocks

### 6.1 Oscillator/Waveform Generator

**Architecture:**
- 24-bit phase accumulator per voice
- Accumulator increments by Fn each clock cycle
- Overflow generates sync pulse
- Top 12 bits feed waveform generator

**Phase Accumulation:**
```
Phase[n+1] = (Phase[n] + Frequency) & 0xFFFFFF
```

**Waveform Generation Methods:**

**Triangle:**
- Phase bits used: 23-12 (top 12 bits)
- If bit 23 = 0: Output = Phase[23:12]
- If bit 23 = 1: Output = ~Phase[23:12] (inverted)
- Creates symmetric triangle wave

**Sawtooth:**
- Direct output of Phase[23:12]
- Linear ramp from 0 to 4095

**Pulse:**
- Comparator: Phase[23:12] < PulseWidth
- If true: Output = 4095 (HIGH)
- If false: Output = 0 (LOW)
- Creates variable-width pulse wave

**Noise:**
- 23-bit Linear Feedback Shift Register (LFSR)
- Polynomial: x^23 + x^18 + 1 (suspected configuration)
- Shift rate: Determined by oscillator frequency
- Pseudo-random sequence (not true random)
- Sequence length: 8,388,607 samples before repeat
- TEST bit resets LFSR to known state

### 6.2 Envelope Generator

**State Machine:**

```
GATE=0, all states → RELEASE
GATE=1:
  RELEASE → ATTACK
  ATTACK (reached peak) → DECAY
  DECAY (reached sustain) → SUSTAIN
  SUSTAIN → (holds until GATE=0)
```

**Implementation:**
- Exponential counter (not linear)
- 8-bit output (0-255)
- Multiplies oscillator output: `Output = Waveform × (Envelope / 255)`
- Rate counters run continuously, state determines behavior

**Critical Timing:**
- Rates given are for FULL cycle (0→peak or peak→0)
- Partial cycles scale proportionally
- Re-gating during attack/decay creates complex shapes

### 6.3 Filter

**Architecture:**
- 12 dB/octave (2-pole) resonant filter
- Switched-capacitor design
- Voltage-controlled cutoff frequency
- Resonance via positive feedback

**Signal Flow:**
```
Voice 1 ──┬──→ (if FILT1=0) ──┐
          └──→ (if FILT1=1) ──┤
Voice 2 ──┬──→ (if FILT2=0) ──┤
          └──→ (if FILT2=1) ──┼──→ FILTER ──┐
Voice 3 ──┬──→ (if FILT3=0) ──┤             ├──→ MODE SELECT ──→ MIX ──→ OUTPUT
          └──→ (if FILT3=1) ──┤             │
EXT IN ───┬──→ (if FILTEX=0) ─┤             │
          └──→ (if FILTEX=1) ─┘             │
                                             │
Unfiltered voices ────────────────────────────┘
```

**Filter Modes (additive combinations):**
- LP only: Classic low-pass
- HP only: Classic high-pass
- BP only: Band-pass
- LP+BP: Low-pass with peak
- HP+BP: High-pass with peak
- LP+HP: Notch filter (band-reject)
- LP+BP+HP: All-pass with resonant peak

### 6.4 Output Stage

**Mixer:**
- Sums filtered and non-filtered signals
- Digital multiplication by volume register
- Final analog output buffer

**Output Characteristics (6581):**
- DC offset: ~6V
- AC component: ±1.5V (3V p-p max)
- Output impedance: ~1kΩ (with external resistor)
- DC offset varies between chips (±0.5V)

**Output Characteristics (8580):**
- DC offset: ~4.5V (more stable)
- AC component: ±1.5V (3V p-p max)
- Output impedance: ~1kΩ
- DC offset more consistent (±0.1V)

---

## 7. Revision-Specific Differences

### 7.1 6581 R1 (Prototype)

**Production:**
- Date codes: 4981-0882 (1981-1982)
- Quantity: ~50-100 units
- Package: Ceramic DIP-28
- Used in: CES demo units, development systems

**Characteristics:**
- Full 12-bit filter cutoff range (documentation suggests)
- Unknown filter behavior (insufficient data)
- Likely had most bugs/quirks
- No production units in consumer C64s

### 7.2 6581 R2 (First Production)

**Production:**
- Date codes: 0882-4985
- Quantity: Millions
- Package: Plastic DIP-28
- Used in: Original "breadbin" C64 (1982-1985)

**Characteristics:**
- **Filter:** Extremely non-linear, varies wildly between chips
- **DC offset:** High (~6-7V), excellent for sample playback
- **Waveform mixing:** Poor (most combinations silent or distorted)
- **Sound:** "Dirty", "rough", strong bass distortion
- **Noise floor:** Relatively high (~-40 to -50 dB)
- **Package marking:** Usually "MOS 6581" only

**Preferred by:**
- Musicians seeking "character" and distortion
- Sample playback applications
- Bass-heavy music (Hubbard, Galway style)

### 7.3 6581 R3 (Most Common)

**Production:**
- Date codes: ~1983-0986
- Quantity: Millions (most common variant)
- Package: Plastic DIP-28
- Used in: Breadbin C64, early C64C

**Characteristics:**
- **Filter:** Slightly more consistent than R2, still non-linear
- **DC offset:** Moderate-high (~6V)
- **Waveform mixing:** Marginally better than R2
- **Sound:** Balanced "6581 character" without extreme roughness
- **Package marking:** "MOS 6581 R3" or "6581 CBM"

**Preferred by:**
- Most demoscene musicians (target platform for compatibility)
- General music composition
- Good compromise between character and predictability

**Notes:**
- Most music was composed targeting R3
- Filter response still varies ±20% between chips
- Some musicians can identify specific chip batches by sound

### 7.4 6581 R4 (Improved Silicon)

**Production:**
- Date codes: 4985-2590
- Quantity: Millions
- Package: Plastic DIP-28
- Silicon: HMOS-II "HC-30" grade (process still NMOS!)
- Used in: Later breadbin, C64C (1985-1990)

**Characteristics:**
- **Filter:** More consistent than R2/R3 (±10-15% variance)
- **DC offset:** Moderate (~5.5-6V)
- **Waveform mixing:** Slightly improved
- **Sound:** Cleaner than R2/R3, retains 6581 "warmth"
- **Package marking:** "MOS 6581 R4" or "CSG 6581 R4"

**Preferred by:**
- Musicians wanting 6581 sound with more predictability
- Some claim "best of both worlds"
- Good sample playback AND reasonable filter

### 7.5 6581 R4 AR (Adjusted Revision)

**Production:**
- Date codes: 2286-4392 (1986-1992!)
- Quantity: Millions
- Package: Plastic DIP-28
- Used in: C64C, late production

**Characteristics:**
- **Minor adjustment to silicon grade from R4**
- **No die change** (electrically nearly identical to R4)
- Filter: Same as R4
- DC offset: Same as R4
- **Package marking:** "MOS 6581 R4 AR" or "CSG 6581 R4 AR"

**Notes:**
- Longest production run of any 6581 variant
- Most common in later C64Cs
- Some batches used 2200 pF filter caps instead of 470 pF

### 7.6 6582 (HMOS-II Consumer Variant)

**Production:**
- Date codes: ~1986
- Quantity: Limited (thousands?)
- Package: Plastic DIP-28
- Process: HMOS-II (9V operation!)
- Used in: Limited production, mainly service replacements

**Characteristics:**
- **Electrically identical to 8580 R5** (same die!)
- Die marking shows "8580R5"
- Package marking shows "6582"
- Requires 9V Vdd (not 12V!)
- Filter caps: 22 nF (same as 8580)

**Usage:**
- Dr. Evil Laboratories SID Symphony cartridge
- Creative Micro Designs products
- Service replacement for defective 8580s
- Some PC sound cards (Innovation SSI-2001)

### 7.7 6582 A (Later Service Part)

**Production:**
- Date codes: 1989-1992
- Quantity: Very limited
- Package: Plastic DIP-28
- Manufactured: Primarily Philippines

**Characteristics:**
- Same as 6582 (identical die)
- Package marking: "6582 A" or "6582A"
- Used exclusively as service replacement parts

### 7.8 8580 R5 (HMOS-II Redesign)

**Production:**
- Date codes: 1986-4893 (1986-1993)
- Quantity: Millions
- Package: Plastic DIP-28
- Process: HMOS-II
- Used in: C64C (1986+), C64G, C128 DCR

**Characteristics:**
- **Filter:** Linear cutoff response, consistent between chips
- **DC offset:** Very low (~0.1-0.5V) - samples quiet without modification
- **Waveform mixing:** Much better (closer to binary AND)
- **Sound:** "Clean", "precise", less distortion
- **Noise floor:** Lower (~-55 to -60 dB)
- **Package marking:** "MOS 8580R5" or "CSG 8580R5"
- **Power:** More efficient, runs cooler

**Improvements:**
- Better digital/analog separation (less crosstalk noise)
- More stable waveform generators
- Combined waveforms produce cleaner results
- Filter more predictable and spec-compliant

**Regressions (from 6581 perspective):**
- Sample playback very quiet (DC offset removed)
- Filter less "characterful" (no distortion)
- High-pass mode not -3dB attenuated (less bassy)
- No "warmth" from analog imperfections

**Digi-Boost Modification:**
- Solder 470kΩ-1MΩ resistor from EXT IN (pin 26) to GND
- Recreates DC bias for sample playback
- Restores volume to near-6581 levels
- Does not affect normal operation
- Disables true external audio input

### 7.9 Die Markings vs. Package Markings

| Package Says | Die Actually Says | Notes |
|--------------|-------------------|-------|
| 6581 | 6581 | Genuine R2/R3 |
| 6581 R3 | 6581 R3 | Genuine R3 |
| 6581 R4 | 6581 R4 | Genuine R4 |
| 6581 R4 AR | 6581 R4 | Same die as R4 |
| 6582 | 8580R5 | Rebadged 8580! |
| 6582 A | 8580R5 | Rebadged 8580! |
| 8580R5 | 8580R5 | Genuine 8580 |

**Warning:** Many counterfeit/remarked SIDs appeared after 2007, especially on eBay. Often have defective filters or dead channels.

---

## 8. Known Bugs and Quirks

### 8.1 Volume Register Click (6581 Only)

**Description:**
- Writing to volume register ($D418) produces audible click
- Caused by DC offset modulation in analog output stage
- Each write creates transient at new volume level

**Root Cause:**
- Poor separation between analog and digital circuits
- DC offset voltage present on output (~6V nominal)
- Varies between chips (5.5-7V range)

**Exploitation (4-bit Sample Playback):**
```
Rapidly write to $D418 at 4-44 kHz:
  Volume = sample_value & $0F
  
Creates pseudo-DAC with 4-bit resolution
Quality depends on DC offset stability
```

**Fixed in 8580:**
- Better analog/digital separation
- DC offset nearly eliminated (<0.5V)
- Volume changes now silent
- Sample playback requires workaround

### 8.2 Waveform Combination Behavior

**Specified Behavior:**
- Multiple waveforms selected = Binary AND of outputs

**Actual Behavior (6581):**
- Most combinations produce silence or near-silence
- Behavior varies between chip revisions
- Some combinations produce unexpected harmonics
- Triangle+Sawtooth often silent
- Pulse+anything often distorted

**Actual Behavior (8580):**
- Better approximation of binary AND
- Still not perfect (some combinations weak)
- More predictable between chips
- Cleaner waveforms overall

**Common Useful Combinations (8580):**
- Pulse+Triangle: Interesting harmonic blend
- Pulse+Sawtooth: Complex timbre

**Recommendation:**
- Use single waveforms for predictable results
- Test combinations on target hardware
- 8580 more reliable for waveform mixing

### 8.3 Noise Waveform Lock-Up

**Description:**
- Selecting noise with other waveforms can lock noise generator
- Noise output becomes silent and stuck

**Trigger Conditions:**
- Noise bit set while pulse/saw/triangle also set
- Particularly problematic on 6581
- Less frequent but still possible on 8580

**Recovery:**
- Set TEST bit (register $Dx04, bit 3 = 1)
- Clear TEST bit (bit 3 = 0)
- Or: Toggle /RES pin low then high
- Noise generator resets and resumes operation

**Prevention:**
- Only select noise waveform alone
- If mixing needed, use separate voices

### 8.4 Test Bit Side Effects

**Normal Use:**
- Resets oscillator phase to 0
- Holds oscillator at 0 while set
- Resets noise LFSR

**Side Effects:**
- Pulse output held at DC level (depends on comparison logic state)
- Can create clicks when toggled
- Some revisions have glitches on TEST transitions

**Applications:**
- Synchronize to external events
- Reset phase for deterministic playback
- Special effect when combined with pulse waveform
- 8-bit sample playback (Otto Järvinen technique)

### 8.5 Filter Cutoff Non-Linearity (6581)

**Description:**
- Filter cutoff frequency does not track linearly with FC register value
- Response curve resembles sigmoid on logarithmic frequency scale
- Extreme variation between chips (same FC value = different frequencies)

**Variation Range:**
- ±30% cutoff frequency between chips for same FC value
- ±20% variation within same batch
- Some chips have "dead spots" where cutoff barely changes

**Impact:**
- Music sounds different on different C64s
- Filter sweeps unpredictable
- Requires per-chip tuning for precise work

**8580 Improvement:**
- Linear cutoff response
- ±5% variation between chips
- Predictable sweep behavior

### 8.6 Filter Resonance Weakness (6581)

**Description:**
- Resonance has minimal effect on 6581
- Peak at cutoff frequency is subtle (~3-6 dB)
- High resonance values (12-15) barely audible

**Root Cause:**
- Resistor ladder tolerances in feedback path
- Analog circuit design limitations

**8580 Improvement:**
- Stronger resonance (up to 12-15 dB peak)
- Can achieve self-oscillation at maximum resonance
- More "synth-like" behavior

### 8.7 High-Pass Filter Attenuation (6581)

**Description:**
- High-pass mode output is -3 dB attenuated compared to LP/BP modes
- Creates bassier overall sound
- Mixing HP with LP creates asymmetric notch

**Root Cause:**
- Intentional or accidental design in analog summing stage

**8580 Change:**
- All filter modes at equal level
- Notch filter is symmetric
- Less bass emphasis

### 8.8 Combined Waveform Weaknesses

**Triangle + Sawtooth (6581):**
- Often completely silent
- Some chips produce very quiet output
- Unpredictable between revisions

**Triangle + Pulse (6581):**
- Weak output (10-30% of single waveform)
- Harmonic content unpredictable
- Better on later revisions

**8580 Improvements:**
- Triangle + Sawtooth: Audible (though quiet)
- Most combinations produce usable output
- Closer to "MIN" function than "AND"

---

## 9. Filter Characteristics

### 9.1 Filter Cutoff Frequency

**6581 Typical Response (with 470 pF caps):**

| FC Register | Approx Freq (Hz) | Notes |
|-------------|------------------|-------|
| $000 | 30-100 | Chip dependent |
| $100 | 80-200 | |
| $200 | 150-400 | |
| $300 | 250-700 | |
| $400 | 400-1200 | |
| $500 | 600-1800 | |
| $600 | 900-2500 | Wide variance |
| $700 | 1300-3500 | |
| $7FF (max) | 8000-14000 | Theoretical 12 kHz |

**Response Curve:** Sigmoid-like on log scale

**8580 Typical Response (with 22 nF caps):**

| FC Register | Approx Freq (Hz) | Notes |
|-------------|------------------|-------|
| $000 | ~20 | Baseline |
| $200 | ~400 | Linear progression |
| $400 | ~1200 | |
| $600 | ~2400 | |
| $7FF (max) | ~12000 | Close to spec |

**Response Curve:** Linear on linear scale

### 9.2 Filter Frequency vs. Capacitor Value

**General Relationship:**
```
Fcutoff ≈ K / C
Where:
  K = Constant (depends on internal resistances)
  C = External capacitor value
```

**6581 Cutoff Ranges:**

| Capacitor Value | Approximate Fmax | Applications |
|----------------|------------------|--------------|
| 330 pF | ~15 kHz | Extended highs, thin sound |
| 470 pF | ~12 kHz | C64 standard, balanced |
| 1000 pF | ~6 kHz | Better bass control |
| 2200 pF | ~3 kHz | Datasheet spec, muffled highs |

**8580 Cutoff Ranges:**

| Capacitor Value | Approximate Fmax | Applications |
|----------------|------------------|--------------|
| 6.8 nF | ~15 kHz | Datasheet recommendation |
| 10 nF | ~12 kHz | Extended range |
| 22 nF | ~8 kHz | C64 standard |
| 47 nF | ~4 kHz | Heavy filtering |

**Critical Issue:**
Many C64 units shipped with incorrect capacitor values:
- 6581 with 2200 pF: Filter too dull, misses bass
- 8580 with wrong values: Over/under filtering

**Diagnostic Test:**
- Last Ninja (Dungeon level): Should have deep bass rumble
- If bass missing: Wrong capacitor values likely

### 9.3 Resonance Behavior

**6581:**
- Weak resonance (Q factor ~2-4)
- Maximum setting barely audible
- Resistor tolerance issues
- Some self-oscillation possible at extreme settings

**8580:**
- Strong resonance (Q factor ~6-12)
- Maximum setting creates pronounced peak
- Reliable self-oscillation
- More "synthesizer-like"

### 9.4 Filter Modes and Slopes

**Low-Pass (LP):**
- 12 dB/octave rolloff above cutoff
- Passes low frequencies, attenuates highs
- Most commonly used mode
- "Full-bodied" sound

**High-Pass (HP):**
- 12 dB/octave rolloff below cutoff
- Passes high frequencies, attenuates lows
- "Thin, buzzy" sound
- **6581:** -3 dB output level (bassier when mixed)
- **8580:** 0 dB output level (equal to LP)

**Band-Pass (BP):**
- 6 dB/octave rolloff on each side
- Passes frequencies near cutoff
- "Thin, open" sound
- Useful for vowel formants

**Notch (LP + HP):**
- Attenuates frequencies at cutoff
- Passes lows and highs
- **6581:** Asymmetric (HP is -3dB)
- **8580:** Symmetric

**All-Pass (LP + BP + HP):**
- Doesn't filter (all passed)
- Resonance creates peak at cutoff
- Phase response altered
- Useful for phasing effects

---

## 10. Waveform Generation

### 10.1 Oscillator Frequency Formula

**Calculation:**
```
Fout = (Fn × Fclk) / 16,777,216 Hz

For 1.0 MHz clock:
Fout = Fn × 0.059604645 Hz

For PAL (0.985248 MHz):
Fout = Fn × 0.0587 Hz

For NTSC (1.022727 MHz):
Fout = Fn × 0.0610 Hz
```

**Frequency Range (1 MHz clock):**
- Minimum: 0.0596 Hz (Fn = 1)
- Maximum: 3906.1875 Hz (Fn = 65535)
- Resolution: 0.0596 Hz per step

**Note:** Sufficient resolution for:
- Any tuning system (equal temperament, just intonation, microtonal)
- Smooth portamento (no audible steps)
- Precise detuning/chorus effects

### 10.2 Waveform Characteristics

#### Triangle Wave
- **Harmonic Content:** Only odd harmonics (1f, 3f, 5f, ...)
- **Amplitude:** 0-4095 (12-bit)
- **Symmetry:** Perfect (50% duty cycle)
- **Sound:** Mellow, flute-like, soft
- **Use:** Bass, pads, soft leads
- **Generation:** Phase bit 23 selects up/down ramp

#### Sawtooth Wave
- **Harmonic Content:** All harmonics (1f, 2f, 3f, ...)
- **Amplitude:** 0-4095
- **Symmetry:** Linear ramp
- **Sound:** Bright, brassy, cutting
- **Use:** Leads, brass sounds, buzzy tones
- **Generation:** Direct phase accumulator top 12 bits

#### Pulse Wave
- **Harmonic Content:** Variable (depends on pulse width)
- **Amplitude:** 0 or 4095 (digital)
- **Duty Cycle:** 0-99.976% (12-bit control)
- **Sound:** Square (50%) to thin pulse
- **Use:** Versatile - leads, bass, effects
- **Special Cases:**
  - PW=0: Constant LOW (DC)
  - PW=2048: Square wave (50% duty)
  - PW=4095: Constant HIGH (DC)
  
**Pulse Width Sweep:**
- Creates "phasing" effect (moving harmonics)
- Rapid changes create rhythmic timbral variations
- Standard synthesis technique

#### Noise Wave
- **Source:** 23-bit Linear Feedback Shift Register
- **Polynomial:** x^23 + x^18 + 1 (suspected)
- **Period:** 8,388,607 samples before repeat
- **Rate:** Determined by oscillator frequency
- **Sound:** White noise character, bandwidth limited by frequency
- **Use:** Percussion, sound effects, wind/surf, snares/cymbals

**Frequency Effects:**
- Low frequency: Rumbling, random low tones
- Mid frequency: General noise
- High frequency: Hissing, white noise

### 10.3 Waveform DAC Implementation

**6581:**
- Resistor ladder DAC (12-bit)
- ±30% resistor tolerance (NMOS process)
- Non-linearities create harmonic distortion
- Variation between chips
- "Analog warmth" from imperfections

**8580:**
- Improved resistor matching (HMOS-II)
- ±5-10% tolerance
- More linear response
- Less harmonic distortion
- "Cleaner" but less character

---

## 11. Timing Specifications

### 11.1 Read Cycle Timing (1 MHz Clock)

| Parameter | Symbol | Min | Typ | Max | Units |
|-----------|--------|-----|-----|-----|-------|
| Clock Cycle Time | Tcyc | 1 | 1 | 20 | μs |
| Clock High Width | Tc | 450 | 500 | 10000 | ns |
| Clock Rise/Fall | Tr, Tf | - | - | 25 | ns |
| Read Setup Time | Trs | 0 | - | - | ns |
| Read Hold Time | Trh | 0 | - | - | ns |
| Access Time | Tacc | - | - | 300 | ns |
| Address Hold | Tah | 10 | - | - | ns |
| Chip Select Hold | Tch | 0 | - | - | ns |
| Data Hold | Tdh | 20 | - | - | ns |

### 11.2 Write Cycle Timing

| Parameter | Symbol | Min | Typ | Max | Units |
|-----------|--------|-----|-----|-----|-------|
| Address Setup | Tas | 200 | - | - | ns |
| Data Setup | Tds | 200 | - | - | ns |
| Write Pulse Width | Tw | 300 | - | - | ns |
| Data Hold (Write) | Tdh | 30 | - | - | ns |

### 11.3 Register Update Latency

**Digital Registers (immediate):**
- Frequency, pulse width, control: 1 clock cycle
- Envelope rates: Effective next rate cycle
- Filter cutoff: 1 clock cycle

**Analog Settling Time:**
- Filter cutoff change: ~10-100 μs (capacitor charging)
- Volume change: ~1-10 μs
- Envelope output: Continuous (rate-limited)

### 11.4 Potentiometer Reading

- **Update rate:** Every 512 clock cycles
- **At 1 MHz:** Updates every 512 μs (~1953 Hz)
- **Conversion time:** ~512 clock cycles per pot
- **Accuracy:** 8-bit (0-255 range)

---

## 12. Application Notes

### 12.1 Basic Initialization Sequence

```assembly
; Recommended SID initialization
    LDA #$00
    LDX #$00
init_loop:
    STA $D400,X    ; Clear all SID registers
    INX
    CPX #$1D       ; 29 registers
    BNE init_loop
    
    LDA #$0F       ; Set volume to maximum
    STA $D418
    
    ; Now configure voices as needed
```

### 12.2 Playing a Simple Note

```assembly
; Play A-4 (440 Hz) on Voice 1
; For PAL C64 (0.985248 MHz)

    LDA #$D6       ; Fn = 7382 ($1CD6) for 440 Hz
    STA $D400      ; Frequency LO
    LDA #$1C
    STA $D401      ; Frequency HI
    
    LDA #$00       ; Pulse width (optional, only if pulse selected)
    STA $D402
    LDA #$08       ; PW = $800 for square wave
    STA $D403
    
    LDA #$00       ; Attack=0, Decay=0 (instant)
    STA $D405
    LDA #$F0       ; Sustain=15 (max), Release=0
    STA $D406
    
    LDA #$41       ; Pulse waveform + GATE
    STA $D404      ; Trigger note
    
    ; Later: Clear GATE to release
    LDA #$40       ; Pulse waveform, GATE=0
    STA $D404
```

### 12.3 Filter Programming Example

```assembly
; Setup low-pass filter on Voice 1
; Cutoff at ~1000 Hz, moderate resonance

    LDA #$00       ; Cutoff low bits
    STA $D415
    LDA #$20       ; Cutoff ~FC=$200 (~1 kHz on 6581)
    STA $D416
    
    LDA #$51       ; Resonance=5, Filter Voice 1
    STA $D417
    
    LDA #$1F       ; Low-pass mode, full volume
    STA $D418
```

### 12.4 Hardware Configuration

#### 6581 Circuit (12V)

```
         +12V (Vdd)
           │
           ├── 10μF ──┐
           ├── 100nF ─┤
           │          │
        Pin 25        GND
        
         +5V (Vcc)
           │
           ├── 10μF ──┐
           ├── 100nF ─┤
           │          │
        Pin 28        GND

    Pin 1 ──┤├── Pin 2   (470pF - 2200pF)
    Pin 3 ──┤├── Pin 4   (470pF - 2200pF)
    
    Pin 27 ──┤├── (1-10μF) ──→ Audio Amp
             │
           1kΩ  
             │
            GND
```

#### 8580 Circuit (9V)

```
         +9V (Vdd)
           │
           ├── 10μF ──┐
           ├── 100nF ─┤
           │          │
        Pin 25        GND
        
         +5V (Vcc)
           │
           ├── 10μF ──┐
           ├── 100nF ─┤
           │          │
        Pin 28        GND

    Pin 1 ──┤├── Pin 2   (22nF)
    Pin 3 ──┤├── Pin 4   (22nF)
    
    Pin 27 ──┤├── (1-10μF) ──→ Audio Amp
             │
           1kΩ  
             │
            GND
            
    Optional Digi-Boost:
    Pin 26 ───/\/\/\─── GND  (470kΩ - 1MΩ)
```

### 12.5 Multi-SID Configuration

**Dual SID Stereo:**
- SID 1: Address $D400-$D41F (left channel)
- SID 2: Address $D420-$D43F or $D500-$D51F (right channel)
- Requires address decoding logic
- Both AUDIO OUT pins to separate amp channels

**Matched Capacitors Critical:**
- Use same capacitor values
- Preferably from same batch
- Ensures filter tracking between chips
- Important for chorusing/detuning effects

### 12.6 Common Pitfalls

**Don't:**
- Mix 6581 and 8580 without voltage conversion
- Use unregulated power supplies
- Forget bypass capacitors
- Select multiple waveforms assuming additive behavior
- Assume filter response is linear (6581)
- Handle chips without ESD precautions (especially 6581)

**Do:**
- Test filter sweeps on actual hardware
- Provide clean, separate power supplies
- Use quality capacitors (low tolerance, temperature stable)
- Reset SID on initialization
- Consider target chip revision when composing

---

## 13. Revision Comparison Summary

### 13.1 Feature Matrix

| Feature | 6581 R2/R3 | 6581 R4/R4AR | 8580 R5 | Notes |
|---------|------------|--------------|---------|-------|
| **Process** | NMOS | NMOS (HC-30) | HMOS-II | 8580 more efficient |
| **Vdd** | 12V | 12V | 9V | **Incompatible!** |
| **Power** | 650 mW | 650 mW | 550 mW | 8580 runs cooler |
| **Filter Caps** | 470 pF typ | 470 pF typ | 22 nF | **Different values!** |
| **Filter Linearity** | Poor | Fair | Good | 8580 predictable |
| **Filter Variance** | ±30% | ±15% | ±5% | 8580 consistent |
| **Resonance** | Weak | Weak | Strong | 8580 pronounced |
| **HP Attenuation** | -3 dB | -3 dB | 0 dB | 6581 bassier |
| **Waveform Mix** | Poor | Poor | Good | 8580 usable |
| **DC Offset** | High (6V) | Medium (5.5V) | Low (0.5V) | Affects samples |
| **Sample Playback** | Excellent | Excellent | Poor* | *Unless modified |
| **Noise Floor** | High | Medium | Low | 8580 cleaner |
| **ESD Sensitivity** | Very High | High | Low | 6581 fragile |
| **Durability** | Poor | Fair | Excellent | 8580 robust |

### 13.2 Sonic Character Summary

**6581 R2:**
- "Dirty, raw, aggressive"
- Extreme filter variance
- Strong bass distortion
- High noise floor
- Favored for: Classic game music, heavy bass, gritty sounds

**6581 R3:**
- "Balanced vintage character"
- Moderate filter variance
- Good bass response
- Target platform for most music
- Favored for: Demoscene music, general composition

**6581 R4/R4AR:**
- "Cleaned-up 6581"
- Better filter consistency
- Retains 6581 warmth
- Less extreme than R2/R3
- Favored for: Predictable 6581 sound, modern compositions

**8580 R5:**
- "Clean, precise, modern"
- Linear filter response
- Accurate waveform mixing
- Low noise floor
- Favored for: Modern demoscene, precise synthesis, complex sounds

### 13.3 Chip Selection Guide

**Choose 6581 if:**
- You want classic C64 "sound"
- Bass distortion is desired
- Sample playback is primary use
- Targeting vintage sound aesthetic
- Don't mind chip-to-chip variation

**Choose 8580 if:**
- You want predictable, consistent behavior
- Waveform mixing is important
- Low noise floor required
- Filter precision matters
- Don't need sample playback (or will add digi-boost)

**"Best" Revision (Subjective):**
- Musicians: Split between R3 and R5
- Demoscene modern: Predominantly R5
- Vintage purists: R2 or R3
- Practical choice: R4 or R5 (more available, more durable)

---

## 14. Advanced Topics

### 14.1 Sample Playback Techniques

#### Traditional Volume Register Method (6581)

```assembly
; 4-bit samples at ~8 kHz

sample_play:
    LDA sample_data,X
    AND #$0F           ; 4-bit sample
    STA $D418          ; Volume register
    
    ; Delay ~125 cycles for 8 kHz
    ; (repeat)
```

**Quality:** 4-bit, ~8-44 kHz possible
**CPU:** 100% utilization at high sample rates
**Works on:** 6581 excellently, 8580 requires modification

#### Pulse-Width Method (Both chips)

```assembly
; 8-bit samples via pulse width modulation
; Voice must be enabled with pulse waveform

    LDA sample_data,X
    STA $D402          ; PW low byte
    LDA #$00
    STA $D403          ; PW high byte
```

**Quality:** 8-bit, but high carrier noise
**CPU:** 100% utilization
**Works on:** Both 6581 and 8580

#### Test-Bit Method (Otto Järvinen, 2008)

```assembly
; 5-6 bit resolution, 16 kHz
; Uses TEST bit + brief waveform enable

    LDA sample_data,X
    STA $D402          ; PW = sample value
    LDA #$49           ; Square + TEST
    STA $D404
    ; Brief enable
    LDA #$41           ; Square, no TEST
    STA $D404
    LDA #$49           ; Back to TEST
    STA $D404
```

**Quality:** 5-6 bit, 8-16 kHz
**CPU:** High overhead (24+ cycles/sample)
**Works on:** Both chips, low amplitude

#### Mahoney Method (2014)

```assembly
; 8-bit via filter/volume register combinations
; Requires empirical calibration tables

    LDY sample_data,X
    LDA volume_table,Y ; Lookup calibrated value
    STA $D418
```

**Quality:** 8-bit, 44.1 kHz
**CPU:** 100% (22 cycles/sample minimum)
**Works on:** 6581 best, 8580 with careful calibration

### 14.2 Oscillator Synchronization (SYNC)

**Mechanism:**
- Master oscillator's overflow resets slave oscillator phase
- Slave waveform repeats at master's frequency
- Creates rich harmonic spectra

**Configuration:**
```assembly
; Voice 1 sync'd to Voice 3

    ; Voice 3: Master (low frequency)
    LDA #$00
    STA $D40E
    LDA #$10       ; ~244 Hz
    STA $D40F
    LDA #$21       ; Sawtooth, GATE
    STA $D412
    
    ; Voice 1: Slave (varying frequency)
    LDA #$00
    STA $D400
    LDA #$40       ; ~977 Hz
    STA $D401
    LDA #$13       ; Triangle, SYNC, GATE
    STA $D404
```

**Effect:** Voice 1 plays at Voice 3's frequency (244 Hz) but with harmonics from 977 Hz ratio

**Applications:**
- "Hard sync" leads
- Brass-like timbres  
- Sweeping slave frequency creates dramatic effect

### 14.3 Ring Modulation (RING MOD)

**Mechanism:**
- Replaces triangle output with XOR of two triangle phases
- Creates non-harmonic overtones
- Metallic, bell-like character

**Configuration:**
```assembly
; Voice 1 ring-modulated with Voice 3

    ; Voice 3: Modulator
    LDA #$00
    STA $D40E
    LDA #$20       ; ~488 Hz
    STA $D40F
    LDA #$11       ; Triangle, GATE
    STA $D412
    
    ; Voice 1: Carrier (must select triangle!)
    LDA #$00
    STA $D400
    LDA #$30       ; ~732 Hz
    STA $D401
    LDA #$15       ; Triangle, RING, GATE
    STA $D404
```

**Effect:** Non-harmonic sidebands at Fcarrier ± Fmodulator

**Applications:**
- Bell sounds
- Gong effects
- Metallic percussion
- Special effects

**Note:** Only affects triangle waveform output!

### 14.4 Voice 3 as Modulation Source

**OSC3 Register ($D41B):**
- Read oscillator 3 output without audio output
- Set 3OFF bit in $D418 (bit 7 = 1)
- Voice 3 silent but still generates modulation data

**Applications:**

**Vibrato:**
```assembly
; 7 Hz triangle for vibrato
    LDA #$72       ; ~7.1 Hz
    STA $D40E
    LDA #$00
    STA $D40F
    LDA #$11       ; Triangle, GATE
    STA $D412
    
    LDA #$8F       ; 3OFF + LP + max volume
    STA $D418
    
vibrato_loop:
    LDA $D41B      ; Read OSC3
    LSR            ; Scale down
    LSR
    CLC
    ADC base_freq_lo ; Add to base frequency
    STA $D400      ; Modulate Voice 1
    ; Continue...
```

**Sample & Hold (random filter):**
```assembly
    LDA #$11       ; Triangle waveform
    STA $D412
    LDA #$8F       ; 3OFF=1
    STA $D418
    
s_and_h:
    LDA $D41B      ; Random-ish value
    STA $D416      ; Modulate filter cutoff
    ; Delay, repeat
```

**ENV3 Register ($D41C):**
- Read envelope 3 output
- Use for filter sweeps (wah-wah)
- Dynamic modulation effects

### 12.5 Optimizing for Different Revisions

**For 6581:**
- Use single waveforms (avoid combinations)
- Leverage filter distortion for character
- Test on multiple chips if possible
- Use aggressive envelope curves (complement slow filter)
- Sample playback works natively

**For 8580:**
- Can use waveform combinations
- Filter sweeps more predictable
- Less aggressive envelopes needed
- Must implement digi-boost for samples
- Lower noise allows quieter passages

**Cross-Compatible:**
- Stick to single waveforms
- Moderate filter usage (avoid extremes)
- Test on both chip types
- Document target revision
- Consider dual-version releases

---

## 15. Technical Reference Data

### 15.1 Equal-Tempered Scale Table (A=440 Hz)

**PAL (0.985248 MHz) Frequency Values:**

| Note | Frequency (Hz) | Fn Value (Hex) | Fn Value (Dec) |
|------|---------------|----------------|----------------|
| C-0 | 16.352 | $0112 | 274 |
| C-1 | 32.703 | $0224 | 548 |
| C-2 | 65.406 | $0448 | 1096 |
| C-3 | 130.813 | $0891 | 2193 |
| A-4 | 440.000 | $1CD6 | 7382 |
| C-4 | 261.626 | $1121 | 4385 |
| C-5 | 523.251 | $2243 | 8771 |
| C-6 | 1046.502 | $4486 | 17542 |
| C-7 | 2093.005 | $890B | 35083 |
| C-8 | 4186.009 | $11256 | 70230 |

### 15.2 Envelope Rate Detailed Table

| Value | Attack (ms) | Decay/Release (ms) | Cycles @ 1MHz | Rate (dB/s) |
|-------|-------------|-------------------|---------------|-------------|
| 0 | 2 | 6 | 2k / 6k | Fast |
| 1 | 8 | 24 | 8k / 24k | |
| 2 | 16 | 48 | 16k / 48k | |
| 3 | 24 | 72 | 24k / 72k | |
| 4 | 38 | 114 | 38k / 114k | |
| 5 | 56 | 168 | 56k / 168k | Medium |
| 6 | 68 | 204 | 68k / 204k | |
| 7 | 80 | 240 | 80k / 240k | |
| 8 | 100 | 300 | 100k / 300k | |
| 9 | 250 | 750 | 250k / 750k | |
| 10 | 500 | 1500 | 500k / 1.5M | Slow |
| 11 | 800 | 2400 | 800k / 2.4M | |
| 12 | 1000 | 3000 | 1M / 3M | |
| 13 | 3000 | 9000 | 3M / 9M | Very slow |
| 14 | 5000 | 15000 | 5M / 15M | |
| 15 | 8000 | 24000 | 8M / 24M | Extremely slow |

**Envelope Shape:**
- Attack: Exponential rise (fast→slow)
- Decay/Release: Exponential fall (fast→slow)
- NOT linear ramps

### 15.3 Filter Resonance Response

**6581:**
- Resonance 0-7: Barely noticeable
- Resonance 8-11: Subtle emphasis
- Resonance 12-15: Moderate peak (~3-6 dB)
- Self-oscillation: Rare, only some chips at maximum

**8580:**
- Resonance 0-3: Subtle
- Resonance 4-7: Noticeable emphasis
- Resonance 8-11: Strong peak (~6-10 dB)
- Resonance 12-15: Very strong (~10-15 dB), self-oscillates

---

## 16. Emulation Considerations

### 16.1 Critical Differences to Emulate

**For Accurate 6581 Emulation:**
1. Non-linear filter cutoff response
2. Per-chip filter variance (randomize or use measurements)
3. Weak resonance
4. HP mode -3 dB attenuation
5. Poor waveform combination (silence or artifacts)
6. DC offset on output (6V nominal, ±0.5V variation)
7. Volume register clicks
8. Noisy DAC (add slight distortion)

**For Accurate 8580 Emulation:**
1. Linear filter cutoff response
2. Consistent filter between instances
3. Strong resonance
4. Equal filter mode levels
5. Better waveform combinations (closer to binary AND)
6. Minimal DC offset (<0.5V)
7. Silent volume changes
8. Clean DAC

### 16.2 Behavior Differences

**Waveform Combination:**
- 6581: Use lookup tables from real chip measurements
- 8580: Approximate with MIN function or simple AND
- Both: Still have irregularities (document specific behaviors)

**Filter Modeling:**
- 6581: Non-linear transfer function, add tolerances
- 8580: Linear transfer function, tight tolerances
- Both: Account for capacitor value effects

**Envelope Generator:**
- Exponential curves (use rate tables or continuous calculation)
- State machine with GATE-dependent transitions
- Can be interrupted/restarted at any amplitude

### 16.3 Cycle-Accurate Timing

**Phase Accumulator:**
- Updates every clock cycle
- 24-bit precision required for accuracy
- Overflow generates sync pulses

**Envelope Generator:**
- Separate counter per voice
- Rate-based updates (not every cycle)
- Requires state tracking

**Filter:**
- Continuous-time analog process
- Can be modeled as discrete filter (IIR/FIR)
- Cutoff change requires settling time simulation

### 16.4 Reference Implementations

**reSID (cycle-based):**
- Accurate waveform tables from measurements
- Envelope generator state machine
- Filter modeled as discrete IIR
- Separate 6581/8580 models

**reSIDfp (floating-point):**
- Improved filter accuracy
- Better waveform interpolation
- Lower CPU overhead than reSID
- More recent, actively maintained

---

## 17. Appendices

### 17.1 Chip Identification Guide

**Visual Inspection:**

1. **Read package marking:**
   - "6581" alone → R2
   - "6581 R3" → R3
   - "6581 R4" → R4
   - "6581 R4 AR" → R4 AR
   - "8580R5" → R5
   - "6582" or "6582 A" → Actually 8580 die!

2. **Check manufacturer:**
   - "MOS" → Various periods
   - "CSG" → Later production (post-1989)
   - Commodore logo → CSG production

3. **Read datecode:**
   - Format: WWYY (week, year)
   - Example: 2084 = Week 20 of 1984

**Electrical Testing:**

1. **Measure Vdd:**
   - 12V → 6581
   - 9V → 8580 or 6582

2. **Check filter capacitors:**
   - 470-2200 pF → 6581
   - 22 nF → 8580/6582

3. **Play test music:**
   - Compare filter sweep behavior
   - Check sample playback volume
   - Listen for noise floor

### 17.2 Filter Capacitor Selection Guide

**For 6581:**
- **Standard:** 470 pF (polystyrene or C0G ceramic)
- **Extended bass:** 1000 pF
- **Datasheet spec:** 2200 pF (often too dull)
- **Tolerance:** ±5% or better
- **Voltage rating:** 50V minimum
- **Temperature coefficient:** <100 ppm/°C

**For 8580:**
- **Standard:** 22 nF (X7R or C0G ceramic)
- **Datasheet:** 6.8 nF (rare)
- **Tolerance:** ±10% acceptable
- **Voltage rating:** 16V minimum
- **Temperature coefficient:** X7R acceptable

**Quality Checklist:**
- Match both capacitors (within 1%)
- Use same type/manufacturer
- Temperature-stable dielectric
- Low ESR (Equivalent Series Resistance)
- Physically small (minimize trace length)

### 17.3 Common Problems and Solutions

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| No sound at all | Volume=0, or chip dead | Check $D418, swap chip |
| Filter doesn't work | Wrong caps, dead chip, ESD damage | Check cap values, test without filter |
| Loud hum/buzz | Poor power supply, bad caps | Add filtering, check voltage regulation |
| Samples very quiet (8580) | Missing DC bias | Add digi-boost resistor |
| Filter sounds wrong | Incorrect capacitor values | Check datasheet values for chip |
| Distorted output | Overloading, bad output coupling | Check 1kΩ resistor, AC coupling cap |
| Chip gets very hot | Wrong voltage! | **STOP! Check Vdd immediately** |
| Noise locked up | Waveform combination with noise | Set/clear TEST bit to reset |

### 17.4 Historical Production Notes

**Total Production Estimate:**
- 6581 (all variants): ~12-17 million units
- 8580: ~8-12 million units
- Total SID production: ~20-30 million units

**Availability Today (2025):**
- 6581: Scarce, prices $20-80+ depending on revision
- 8580: More available, prices $15-50
- Counterfeit/remarked chips common
- Working chips increasingly rare
- Filter failures most common defect

**Modern Alternatives:**
- SwinSID: ATmega-based replacement
- ARMSID: ARM-based replacement
- FPGASID: FPGA-based replacement
- None sound exactly like original (close approximations)

---

## 18. References and Resources

### Primary Sources

1. MOS Technology 6581 Datasheet (1982)
2. MOS Technology 8580 Datasheet (1986) - Note: Contains errors
3. Commodore 64 Programmer's Reference Guide, Appendix O
4. U.S. Patent 4,677,890 (SID patent)

### Technical Analysis

5. SID Article v0.2 - https://github.com/ImreOlajos/SID-Article
6. Technical SID Information - http://www.sidmusic.org/sid/sidtech2.html  
7. Oxyron SID Register Reference - https://oxyron.de/html/registers_sid.html
8. SID Schematics (reverse-engineered) - https://github.com/libsidplayfp/SID_schematics

### Empirical Measurements

9. Stone Oakvalley's Authentic SID Collection (SOASC) - http://www.6581-8580.com
10. kompjut0r SID Shootout - http://kompjut0r.blogspot.com/2015/12/c64-sid-shootout.html
11. Mahoney Sample Playback Technical Details - https://livet.se/mahoney/

### Software Emulation

12. reSID - https://github.com/libsidplayfp/resid
13. reSIDfp - https://github.com/libsidplayfp/libsidplayfp

### Community Resources

14. CSDb (C64 Scene Database) - https://csdb.dk
15. Lemon64 Forums - https://www.lemon64.com
16. SID music community - http://www.sidmusic.org

---

## Document History

**Version 1.0 - November 18, 2025**
- Initial comprehensive compilation
- Sourced from datasheets, empirical measurements, and community knowledge
- Covers all known revisions (1981-1993)
- Includes application notes and troubleshooting

---

## License

This document compiles information from public domain sources, expired patents, and community research. Use freely for educational, development, and preservation purposes.

**Acknowledgments:**
- Bob Yannes (SID designer)
- Community reverse-engineering efforts
- Stone Oakvalley (SOASC measurements)
- reSID/reSIDfp developers
- Countless demoscene musicians and coders

---

**END OF DATASHEET**
