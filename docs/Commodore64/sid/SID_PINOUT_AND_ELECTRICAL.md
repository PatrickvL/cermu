# SID CHIP PINOUT AND ELECTRICAL INTERFACE
## Detailed Pin Specifications for Hardware Design and Emulation

---

## PHYSICAL PINOUT (DIP-28)

```
                         ┌─────────────┴─────────────┐
         CAP1A  (Filter) │ 1                     28 │ Vcc (+5V)
         CAP1B  (Filter) │ 2                     27 │ AUDIO OUT
         CAP2A  (Filter) │ 3                     26 │ EXT IN  
         CAP2B  (Filter) │ 4                     25 │ Vdd (+12V/+9V)
          /RES  (Input)  │ 5                     24 │ POTX (Analog In)
            Φ2  (Clock)  │ 6                     23 │ POTY (Analog In)
           R/W  (Input)  │ 7                     22 │ D7 (Data Bus)
           /CS  (Input)  │ 8                     21 │ D6 (Data Bus)
            A0  (Addr)   │ 9                     20 │ D5 (Data Bus)
            A1  (Addr)   │10                     19 │ D4 (Data Bus)
            A2  (Addr)   │11                     18 │ D3 (Data Bus)
            A3  (Addr)   │12                     17 │ D2 (Data Bus)
            A4  (Addr)   │13                     16 │ D1 (Data Bus)
           GND           │14                     15 │ D0 (Data Bus)
                         └───────────────────────────┘
                               (Top View)
```

---

## PIN ELECTRICAL SPECIFICATIONS

### POWER PINS

#### Pin 28 - Vcc (Digital Power)

```
6581:
  Voltage:      +5.0V DC ±5% (4.75V - 5.25V)
  Current:      70 mA typical, 100 mA maximum
  Ripple:       <50 mV p-p
  Bypass:       10 μF tantalum + 100 nF ceramic (close to pin)
  
8580:
  Voltage:      +5.0V DC ±5% (4.75V - 5.25V)  
  Current:      65 mA typical, 90 mA maximum
  Ripple:       <50 mV p-p
  Bypass:       10 μF tantalum + 100 nF ceramic (close to pin)
```

**PCB Recommendations:**
- Separate trace from CPU Vcc (minimize digital noise)
- Wide trace (minimum 20 mil for 100 mA)
- Star grounding to main power supply
- Keep bypass caps within 0.5" of pin

#### Pin 25 - Vdd (Analog Power)

```
6581:
  Voltage:      +12.0V DC ±5% (11.4V - 12.6V)
  Current:      25 mA typical, 40 mA maximum
  Ripple:       <10 mV p-p (audio-grade supply)
  Bypass:       10 μF tantalum + 100 nF ceramic
  Source:       Regulated from 9VAC via 7812 or similar

8580/6582:
  Voltage:      +9.0V DC ±5% (8.55V - 9.45V)
  Current:      25 mA typical, 35 mA maximum  
  Ripple:       <10 mV p-p
  Bypass:       10 μF tantalum + 100 nF ceramic
  Source:       Regulated from 9VAC via 7809 or similar
```

**Critical Notes:**
- Low noise essential (powers analog oscillators and filters)
- Poor regulation causes frequency drift and filter artifacts
- Never exceed maximum voltage (chip damage!)
- 6581 with 9V: May work but out of spec
- 8580 with 12V: **PERMANENT DAMAGE**

#### Pin 14 - GND (Ground)

```
  Connection:   Common digital and analog ground
  Quality:      Low-impedance path to system ground
  Multi-SID:    Star-ground configuration recommended
  PCB:          Wide trace or ground plane
```

**Grounding Strategy:**
- Single-point ground at power supply
- Minimize ground loops
- Keep analog and digital grounds separate until final junction
- Ground plane preferred on PCB

### DIGITAL INPUT PINS (TTL-Compatible)

**Pins: A0-A4 (9-13), R/W (7), /CS (8), /RES (5)**

```
Input Characteristics:
  VIL (Logic Low):     0V - 0.8V
  VIH (Logic High):    2.0V - Vcc
  IIL (Input Current): ±10 μA maximum
  Cin (Input Cap):     ~5-10 pF
  
Protection:
  6581: Limited ESD protection (FRAGILE!)
  8580: Improved ESD protection
  
Recommendations:
  - Drive with standard TTL or 74HCT logic
  - Series resistor (100Ω) for ESD protection
  - Keep traces short
  - Avoid floating inputs
```

**Timing Requirements:**
- Address stable before /CS asserted: 200 ns
- /CS pulse width minimum: 300 ns  
- Data setup time: 200 ns (writes)
- Data hold time: 30 ns (writes)

#### Pin 6 - Φ2 (Clock Input)

```
Input Characteristics:
  Frequency:      50 kHz - 1.05 MHz
  Standard:       0.985248 MHz (PAL) or 1.022727 MHz (NTSC)
  VIL:            0V - 0.8V
  VIH:            2.4V - Vcc
  Duty Cycle:     45% - 55% (50% nominal)
  Rise/Fall:      ≤25 ns
  
Connection:
  - Synchronous with 6502/6510 Φ2
  - TTL/CMOS compatible
  - Low jitter essential (<5 ns)
  - AC-coupled NOT recommended (DC levels matter)
```

**Clock Quality:**
- Frequency stability affects tuning accuracy
- Jitter causes timing artifacts in envelope/filter
- Use crystal-based clock source
- Avoid RC oscillators

### DATA BUS (Bidirectional)

**Pins: D0-D7 (15-22)**

```
Output Characteristics (when reading):
  VOL (Logic Low):     0V - 0.4V
  VOH (Logic High):    2.4V - Vcc  
  IOL (Sink Current):  1.6 mA @ VOL
  IOH (Source Current): -100 μA @ VOH
  
Input Characteristics (when writing):
  VIL:            0V - 0.8V
  VIH:            2.0V - Vcc
  IIL:            ±10 μA
  
Tri-State:
  High-Z when /CS high or R/W high (write mode)
  Disabled state leakage: <10 μA
```

**Bus Contention:**
- Must not drive bus when SID is outputting (read mode with /CS low)
- Wait for Φ2 low before changing bus direction
- 74LS245 or similar bus transceiver recommended for buffering

### ANALOG FILTER PINS

**Pins 1-4: CAP1A, CAP1B, CAP2A, CAP2B**

```
Function:       External integrating capacitors for filter
Voltage Range:  0V - Vdd (swing with filter operation)
Impedance:      ~10 kΩ to capacitor
Current:        <1 mA
  
Connection:
  Pin 1 ──┤├── Pin 2   (C1: Matched capacitor)
  Pin 3 ──┤├── Pin 4   (C2: Matched capacitor)
  
Capacitor Requirements:
  Type:           Polystyrene (best), C0G/NP0 ceramic (good), X7R (acceptable)
  Tolerance:      ±5% or better (±2% for matched pairs)
  Voltage:        Minimum 16V rating (50V preferred)
  ESR:            Low (<1Ω @ 10kHz)
  Temperature:    ≤100 ppm/°C
  Matching:       Within 1% for stereo/multi-SID setups
```

**PCB Layout:**
- Mount capacitors as close as possible to SID pins
- Use short, direct traces (minimize parasitic inductance)
- Keep away from digital signals
- Ground plane underneath (but not between cap pins)

**Capacitor Value Selection:**

| SID Type | Standard | Alternative | Effect |
|----------|----------|-------------|---------|
| 6581 | 470 pF | 1000-2200 pF | Higher = lower Fmax |
| 8580 | 22 nF | 6.8-10 nF | Lower = higher Fmax |

**Effect on Filter Response:**
```
Fcutoff,max ≈ 1 / (2π × Rinternal × Cexternal)

6581 with 470pF:  Fmax ≈ 12 kHz
6581 with 2200pF: Fmax ≈ 3 kHz
8580 with 22nF:   Fmax ≈ 8 kHz
8580 with 6.8nF:  Fmax ≈ 15 kHz
```

### ANALOG OUTPUT PIN

#### Pin 27 - AUDIO OUT

```
Output Characteristics (6581):
  DC Level:       6.0V ±0.5V (varies between chips!)
  AC Swing:       ±1.5V (3V p-p maximum)
  Peak Output:    7.5V maximum, 4.5V minimum
  Impedance:      ~1 kΩ (with external resistor)
  Drive Capability: ~3 mA maximum
  Noise Floor:    -45 to -50 dB (relative to max)
  
Output Characteristics (8580):
  DC Level:       4.5V ±0.1V (more stable)
  AC Swing:       ±1.5V (3V p-p maximum)
  Peak Output:    6.0V maximum, 3.0V minimum
  Impedance:      ~1 kΩ (with external resistor)
  Drive Capability: ~3 mA maximum
  Noise Floor:    -55 to -60 dB
```

**Required External Components:**

```
                    SID Pin 27
                        │
                        ├── 1-10 μF (AC coupling)
                        │   (electrolytic, polar)
                        │
                   Audio Amplifier Input
                   (10kΩ - 100kΩ input impedance)
                        
                    SID Pin 27
                        │
                       1kΩ (pull-down resistor)
                        │
                       GND
```

**AC Coupling Capacitor:**
- **Value:** 1-10 μF (larger = better bass response)
- **Type:** Electrolytic (polarized!) or tantalum
- **Polarity:** Positive to SID pin, negative to amplifier
- **Voltage:** Minimum 16V rating
- **Purpose:** Blocks DC offset, passes AC audio signal

**Pull-Down Resistor:**
- **Value:** 1 kΩ ±5%
- **Power:** 1/4W sufficient
- **Purpose:** Establishes proper output impedance
- **Location:** SID pin 27 to ground, close to chip

**Frequency Response:**
```
Low-frequency cutoff (AC coupling): Fc = 1 / (2π × Rload × Ccoupling)

Example: 10 μF into 10 kΩ load
  Fc = 1 / (2π × 10000 × 10×10^-6) = 1.6 Hz

Recommendation: Use 10 μF for full bass response
```

#### Pin 26 - EXT IN (External Audio Input)

```
Input Characteristics:
  Voltage Range:  0V - 3V p-p AC (centered at Vdd/2)
  DC Bias:        Vdd/2 (6V for 6581, 4.5V for 8580)
  Impedance:      ~1 MΩ input impedance
  Bandwidth:      DC - 12 kHz
  
Connection:
  External Source ──┤├── (1-10 μF) ── Pin 26
                    
Routing:
  - Can be filtered (FILTEX bit in $D417)
  - Mixes with internal voices
  - Useful for audio processing effects
```

**8580 Digi-Boost Modification:**
```
For sample playback on 8580:

    Pin 26 ───/\/\/\─── GND
             470kΩ - 1MΩ

Effect:
  - Recreates DC bias lost in 8580 redesign
  - Sample playback volume restored
  - Disables true external audio input
  - Does not affect normal synthesis
```

### POTENTIOMETER INPUT PINS

**Pins 23-24: POTY, POTX**

```
Input Characteristics:
  Voltage Range:  0V - 5V (analog)
  Input Current:  <1 μA (high impedance)
  Conversion:     8-bit successive approximation ADC
  Update Rate:    Every 512 Φ2 clock cycles
  Resolution:     ~20 mV per step
  
Timing Capacitor Circuit:

    +5V
     │
    ┌┴┐ Potentiometer (470 kΩ typical)
    │ │ Variable resistance (0 - 470kΩ)
    └┬┘
     │─────────── POTX/POTY pin
     │
    ═══  Timing capacitor (1000 pF to GND)
     │
    GND

Operation:
  1. Capacitor discharges through pot
  2. SID measures discharge time
  3. Longer time = higher resistance
  4. Converts to 8-bit value (0-255)
  
ADC Formula:
  Reading ≈ (Rpot / Rmax) × 255
  
  Example: 235 kΩ pot = 255 × (235k / 470k) ≈ 127
```

**Mouse/Paddle Wiring:**

```
C64 Control Port (9-pin D-sub):

Pin 5 ────┐
          ├── POTX (Pin 24)
Pin 9 ────┤
          └── 1000 pF ─── GND

Pin 5 ────┐
          ├── POTY (Pin 23)  
Pin 8 ────┤
          └── 1000 pF ─── GND
```

**Update Rate:**
- At 1.0 MHz: 512 μs per conversion
- Conversion frequency: ~1953 Hz
- Two pots = ~976 Hz per pot (alternating)

**Limitations:**
- 8-bit resolution only (256 positions)
- Relatively slow update rate
- Noise on pot inputs affects readings
- Capacitor value affects linearity

---

## SIGNAL TIMING DIAGRAMS

### WRITE CYCLE

```
         ___     ___     ___     ___     ___
Φ2   ___|   |___|   |___|   |___|   |___|   |___
         ↑       ↑       ↑       ↑
         
/CS  ─────────────────\_______________________/───
         <─ Tas ─>
         
A0-A4 ═══════════════<══ Valid Address ══════>════
                     <───── Tah ────>
                     
R/W  ────────────────────────\____________________  (Write = Low)
         
D0-D7 XXXXXXXXXXXXXXXXX<═══ Valid Data ═══>XXXXXXXX
                     <─ Tds ─><─ Tdh ─>
         
                     <────── Tw ──────>

Timing Parameters:
  Tas (Address Setup):    200 ns minimum
  Tds (Data Setup):       200 ns minimum  
  Tw (Write Pulse):       300 ns minimum
  Tah (Address Hold):     10 ns minimum
  Tdh (Data Hold):        30 ns minimum
```

### READ CYCLE

```
         ___     ___     ___     ___     ___
Φ2   ___|   |___|   |___|   |___|   |___|   |___
         ↑       ↑       ↑       ↑
         
/CS  ─────────────────\_______________________/───
         
A0-A4 ═══════════════<══ Valid Address ══════>════
                     <───── Tah ────>
                     
R/W  ──────────────────────────────────────────────  (Read = High)

D0-D7 ─────────────────────<═══ Data Out ═══>──────
                     <──── Tacc ────><─Tdh─>

Timing Parameters:
  Tacc (Access Time):     300 ns maximum
  Tah (Address Hold):     10 ns minimum
  Tdh (Data Hold):        20 ns minimum
  Tch (CS Hold):          0 ns minimum
```

### CLOCK REQUIREMENTS

```
         _______________                 _______________
Φ2   ___|               |_______________|               |___
     
     <──── Tc ────><──── Tcyc ─────><──── Tc ────>
     <─Tr─>        <─Tf─>
     
Specifications:
  Tcyc (Cycle Time):      1 μs - 20 μs (50 kHz - 1 MHz)
  Tc (High Width):        450 ns - 10 μs (typ 500 ns)
  Tr (Rise Time):         ≤25 ns (10%-90%)
  Tf (Fall Time):         ≤25 ns (90%-10%)
  Duty Cycle:             45% - 55%

Jitter Tolerance:
  Period jitter:          <±50 ns (<5%)
  Cycle-to-cycle:         <±10 ns (<1%)
```

---

## FILTER CAPACITOR DETAILED SPECS

### CAPACITOR CONNECTIONS

```
Internal Filter Stage 1:     Internal Filter Stage 2:
                                
     R_int                        R_int
Pin 1 ──┬─────┬── Pin 2      Pin 3 ──┬─────┬── Pin 4
        │     │                      │     │
      ──┴──  ─┴─ Vref             ──┴──  ─┴─ Vref
      ─────   │                   ─────   │
       Op1    │                    Op2    │
              GND                         GND
              
External Capacitors:
  C1 between Pins 1-2
  C2 between Pins 3-4
```

### CAPACITOR SPECIFICATIONS BY REVISION

#### 6581 Filter Capacitors

```
Recommended Values:
  Datasheet (1982):   2200 pF ±5%
  Datasheet (1986):   1000 pF ±5%
  C64 Production:     470 pF ±5% (most common)
  
Actual Usage:
  Early units (1982-1983):  1000-2200 pF
  Later units (1984-1992):  470 pF
  Some SX-64:               2200 pF (often wrong!)
  
Type:             Polystyrene film (best)
                  C0G/NP0 ceramic (good)
                  Avoid: X7R, Y5V (temperature drift)
Voltage:          50V minimum (safety margin)
Tolerance:        ±2% for matched pairs, ±5% acceptable single
Temperature Coef: <100 ppm/°C (polystyrene ~-120 ppm/°C)
ESR:              <1Ω @ 10 kHz
Physical:         Axial or radial lead film, or SMD ceramic
```

**Effect of Value on Frequency:**

| C1=C2 | Fmax (approx) | Bass Response | Treble Response |
|-------|---------------|---------------|-----------------|
| 330 pF | ~15 kHz | Weak | Extended |
| 470 pF | ~12 kHz | Good | Good (balanced) |
| 1000 pF | ~6 kHz | Better | Reduced |
| 2200 pF | ~3 kHz | Best | Poor (muffled) |

#### 8580 Filter Capacitors

```
Recommended Values:
  Datasheet:        6800 pF (6.8 nF)
  C64C Production:  22 nF (22000 pF)
  
Type:             Ceramic X7R or C0G
                  Film capacitors work but large physical size
Voltage:          16V minimum (25V preferred)
Tolerance:        ±10% acceptable (tighter filter than 6581)
Temperature Coef: X7R acceptable (±15% over -55 to +125°C)
ESR:              <1Ω @ 10 kHz
Physical:         SMD ceramic (0805 or larger) or radial lead
```

**Effect of Value on Frequency:**

| C1=C2 | Fmax (approx) | Character |
|-------|---------------|-----------|
| 6.8 nF | ~15 kHz | Extended range (datasheet) |
| 10 nF | ~12 kHz | Wide range |
| 22 nF | ~8 kHz | Standard C64C |
| 47 nF | ~4 kHz | Limited highs |

**Critical Design Note:**
The dramatically different capacitor values (470 pF vs 22 nF = 47× difference!) mean the internal filter resistor networks are completely different between 6581 and 8580. This is NOT just a process shrink - the analog filter was redesigned.

### FILTER CAPACITOR TROUBLESHOOTING

| Symptom | Diagnosis | Fix |
|---------|-----------|-----|
| No filter effect | Wrong cap value (too large) | Use correct value |
| Filter too subtle | Wrong cap value (too small) | Use correct value |
| Bass missing (6581) | Caps = 2200 pF | Change to 470 pF |
| Harsh highs (8580) | Caps too small | Use 22 nF |
| Filter changes with temp | Wrong cap type (X7R, Y5V) | Use C0G or polystyrene |
| Filter drift in multi-SID | Unmatched caps | Use matched pair |

---

## VOLTAGE CONVERSION (6581 ↔ 8580 SWAP)

### Upgrading 6581 → 8580

**Required Modifications:**

1. **Vdd Voltage Regulator:**
```
Original:  7812 (12V regulator)
Replace:   7809 (9V regulator)

or use adjustable:
LM317: Set to 9.0V with resistor divider
```

2. **Filter Capacitors:**
```
Remove:  470 pF capacitors (C1, C2)
Install: 22 nF capacitors
```

3. **Optional Digi-Boost:**
```
Add: 470 kΩ - 1 MΩ resistor from EXT IN (pin 26) to GND
Purpose: Restore sample playback volume
```

### Downgrading 8580 → 6581

**Required Modifications:**

1. **Vdd Voltage Regulator:**
```
Original:  7809 (9V regulator)
Replace:   7812 (12V regulator)
```

2. **Filter Capacitors:**
```
Remove:  22 nF capacitors
Install: 470 pF capacitors (polystyrene or C0G)
```

**Risks:**
- 6581 more ESD-sensitive (handle with care)
- 6581 less reliable (higher failure rate)
- Filter behavior varies between chips
- Noise floor higher

---

## TYPICAL APPLICATION CIRCUIT

### 6581 STANDALONE CIRCUIT

```
                              +5V               +12V
                               │                 │
                               ├── 10μF ───┐     ├── 10μF ───┐
                               ├── 100nF ──┤     ├── 100nF ──┤
                               │           │     │           │
                              Vcc(28)     GND   Vdd(25)     GND
                               │                 │
                   ┌───────────┴─────────────────┴────────┐
                   │                                  SID  │
     470pF         │ 1  CAP1A                 AUDIO OUT 27 │──┤├── 10μF ──→ Amp
       ├─────────  │ 2  CAP1B                              │  │
     470pF         │ 3  CAP2A                              │ 1kΩ
       ├─────────  │ 4  CAP2B                   EXT IN 26 │  │
                   │ 5  /RES                              │ GND
6502 Φ2 ────────── │ 6  Φ2                        POTX 24 │── Pot circuit
6502 R/W ───────── │ 7  R/W                       POTY 23 │── Pot circuit
Decode ──────────  │ 8  /CS                              │
A0-A4 ──────────── │ 9-13  Address Bus                   │
Data Bus ───────── │15-22  Data Bus                      │
                   │14 GND                                │
                   └──────────────────────────────────────┘
                         │
                        GND
```

### 8580 STANDALONE CIRCUIT

```
                              +5V               +9V
                               │                 │
                               ├── 10μF ───┐     ├── 10μF ───┐
                               ├── 100nF ──┤     ├── 100nF ──┤
                               │           │     │           │
                              Vcc(28)     GND   Vdd(25)     GND
                               │                 │
                   ┌───────────┴─────────────────┴────────┐
                   │                                  SID  │
       22nF        │ 1  CAP1A                 AUDIO OUT 27 │──┤├── 10μF ──→ Amp
       ├─────────  │ 2  CAP1B                              │  │
       22nF        │ 3  CAP2A                              │ 1kΩ
       ├─────────  │ 4  CAP2B                   EXT IN 26 │──┼─→ Optional
                   │ 5  /RES                              │  │   digi-boost
                   │ 6  Φ2                        POTX 24 │  │   (470kΩ-1MΩ
                   │ 7  R/W                       POTY 23 │  │    to GND)
                   │ 8  /CS                              │  │
                   │ 9-13  Address Bus                   │ GND
                   │15-22  Data Bus                      │
                   │14 GND                                │
                   └──────────────────────────────────────┘
                         │
                        GND
```

---

## PCB LAYOUT GUIDELINES

### POWER SUPPLY ROUTING

```
Power Supply ──→ [Filter] ──┬──→ VIC-II Vdd
                            ├──→ CPU Vcc
                            └──→ SID Vdd/Vcc

Separate Traces:
  - Each IC gets own power trace from regulator
  - Star topology from regulators
  - Minimize shared current paths
```

**Trace Widths (for 100 mA max):**
- 1 oz copper: Minimum 10 mil (0.25mm)
- Recommended: 20-30 mil (0.5-0.75mm)
- Shorter is better (minimize voltage drop)

### GROUND PLANE

```
Recommended: Solid ground plane on bottom layer

If using traces:
  - Minimum 50 mil wide
  - Keep analog and digital separate until star point
  - Star point at power supply
```

### COMPONENT PLACEMENT

```
Priority: Close to SID

1. Bypass caps (Vcc, Vdd): <0.5" from pins
2. Filter caps: <0.5" from pins 1-4
3. Pull-down resistor: <1" from pin 27
4. AC coupling cap: <2" from pin 27

Keep away from SID:
  - High-speed digital (VIC-II)
  - Switching power supplies
  - RF sources
```

### SIGNAL INTEGRITY

**Clock (Φ2):**
- Keep trace <4" if possible
- 50Ω characteristic impedance
- Terminate if >6" (33Ω series resistor)
- Avoid vias (adds capacitance)

**Data/Address Bus:**
- Can be longer (low frequency)
- Buffer if >12" total length
- Terminate if multiple loads

**Audio Output:**
- Shielded cable for external connections
- Keep digital signals away from audio trace
- Ground shield at one end only (avoid ground loops)

---

## COMMON INTERFACE EXAMPLES

### C64 STANDARD INTERFACE

```
Address Decode:
  $D400-$D7FF → /CS (SID)
  
  Using 74LS138 (3-to-8 decoder):
    A8-A10 → Select inputs
    I/O1 → Enable input
    
  /CS = /I/O1 AND (A12=1) AND (A11=0) AND (A10=1)
  
Clock:
  6510 Φ2 → SID Φ2 (direct connection)
  
Control:
  6510 R/W → SID R/W
  Address bus A0-A4 → SID A0-A4
  Data bus D0-D7 ↔ SID D0-D7
```

### DUAL SID CONFIGURATION

```
SID #1: $D400-$D41F (Address decode: A5=0)
SID #2: $D420-$D43F (Address decode: A5=1)

or

SID #1: $D400-$D41F  
SID #2: $D500-$D51F (Separate range)

Decode Logic:
  /CS1 = /I/O AND AddressRange1
  /CS2 = /I/O AND AddressRange2
  
Shared:
  - Clock (Φ2)
  - R/W
  - Address bus (low bits)
  - Data bus (with buffering)
  
Separate:
  - /CS signals
  - Power supplies (separate bypass)
  - Audio outputs (stereo!)
  - Filter capacitors
```

**Matched Pairs:**
- Use same revision SIDs
- Use same capacitor values (within 1%)
- Calibrate filter cutoff together
- Test with stereo material

---

## TESTING AND VALIDATION

### POWER-ON TEST SEQUENCE

```assembly
1. Apply power (correct voltages!)
2. Assert /RES for ≥10 clock cycles
3. Release /RES
4. Write $00 to all registers
5. Write $0F to $D418 (volume)
6. Write test frequency to Voice 1
7. Write $11 to $D404 (Triangle + GATE)
8. Listen for tone

Expected: Pure triangle wave at test frequency
If silent: Check power, caps, chip may be dead
```

### FILTER TEST

```assembly
; Sweep filter cutoff while playing tone

    LDA #$D6       ; 440 Hz
    STA $D400
    LDA #$1C  
    STA $D401
    LDA #$11       ; Triangle + GATE
    STA $D404
    
    LDA #$01       ; Route V1 through filter
    STA $D417
    LDA #$1F       ; LP mode, full volume
    STA $D418
    
sweep:
    LDA fc_value   ; Sweep from $000 to $7FF
    STA $D415
    STA $D416
    
    ; Listen: Should hear filter sweep
    ; 6581: Non-linear, may be uneven
    ; 8580: Linear, smooth
```

### WAVEFORM COMBINATION TEST

```assembly
; Test waveform mixing behavior

    ; Try Triangle + Sawtooth
    LDA #$30       ; Both waveforms selected
    STA $D404
    
    ; Expected:
    ; 6581: Often silent or very quiet
    ; 8580: Audible combined waveform
```

### SAMPLE PLAYBACK TEST

```assembly
; 4-bit samples via volume register

    LDA #$01       ; Sample value 1
    STA $D418
    LDA #$02  
    STA $D418
    LDA #$04
    STA $D418
    ; ... continue
    
    ; Expected:
    ; 6581: Loud clicks/samples
    ; 8580: Very quiet or silent
```

---

## OSCILLATOR INTERNAL DETAILS

### PHASE ACCUMULATOR

```
Width:      24 bits (0 - 16,777,215)
Update:     Every Φ2 clock cycle
Operation:  Phase ← (Phase + Frequency) & 0xFFFFFF

Overflow:   Bit 24 carry-out generates sync pulse
Precision:  Sufficient for ±0.001% frequency accuracy
```

**Frequency Resolution:**
```
At 1 Hz:    Resolution = 0.0596 Hz (17 Fn steps per Hz)
At 100 Hz:  Resolution = 0.0596 Hz (same)
At 1000 Hz: Resolution = 0.0596 Hz (same - UNIFORM!)
At 3906 Hz: Resolution = 0.0596 Hz (65 steps to next Hz)

Contrast with:
  - Divide-by-N oscillators: Resolution decreases with frequency
  - SID: Constant resolution across entire range
```

### WAVEFORM GENERATOR DAC

**6581 DAC:**
- Type: Resistor ladder (R-2R or similar)
- Bits: 12-bit output
- Linearity: Poor (±3-5% INL typical)
- Resistor matching: ±30% (NMOS process limits)
- DC offset: ~50-100 mV between chips
- Harmonic distortion: 0.5-2% THD

**8580 DAC:**
- Type: Improved resistor ladder
- Bits: 12-bit output
- Linearity: Good (±0.5-1% INL)
- Resistor matching: ±5% (HMOS-II process)
- DC offset: ~10-20 mV
- Harmonic distortion: 0.1-0.3% THD

**Implications for Emulation:**
- 6581: Add slight non-linearity and noise
- 8580: Can use linear DAC model
- Both: 12-bit resolution essential

---

## NOISE GENERATOR INTERNALS

### LFSR Configuration

```
Polynomial: x^23 + x^18 + 1 (suspected, not officially documented)

Implementation:
  23-bit shift register
  Feedback: XOR of bit 22 and bit 17
  Shift rate: Oscillator frequency
  
Sequence Length: 2^23 - 1 = 8,388,607 samples

Bit Layout:
  [22][21][20]...[18][17]...[1][0]
   │                │  │
   └────── XOR ─────┴──┘
                    │
                 Feedback
```

**Output:**
- Top 12 bits of LFSR used as waveform output
- Updates each oscillator cycle
- NOT cryptographically random
- Deterministic sequence (repeatable)

**TEST Bit Effect:**
- Resets LFSR to known state (all bits 0 or specific pattern)
- Subsequent noise identical until next reset
- Used for synchronized noise effects

### Noise Frequency Effects

```
Low Freq (16-100 Hz):
  - Individual random values audible
  - "Bubbling" or "rumbling" sound
  - Good for thunder, engines
  
Mid Freq (100-1000 Hz):
  - Transitions to noise character
  - Good for wind, surf, breath
  
High Freq (1000+ Hz):
  - White noise character
  - Good for cymbals, hi-hats, hiss
  
Very High (3000+ Hz):
  - Bandwidth-limited noise
  - Filtered character due to sample rate
```

---

## FILTER CIRCUIT TOPOLOGY

### Internal Structure (Simplified)

```
Input ──→ [LP Stage] ──→ [HP Stage] ──→ [BP tap]
             │              │              │
             ↓              ↓              ↓
          LP Out         HP Out         BP Out
             │              │              │
             └──────┬───────┴──────┬───────┘
                    │              │
              [Mode Select]   [Resonance FB]
                    │              │
                    └──────┬───────┘
                           │
                      [Output Mix]
                           │
                        Output

External Components:
  C1, C2: Timing capacitors (set frequency range)
  
Internal Components (6581):
  - Resistor ladder (voltage-controlled)
  - Op-amps (analog processing)  
  - Resonance feedback path
  - Mode switches (LP/HP/BP)
  
Internal Components (8580):
  - Improved resistor matching
  - Better op-amp design
  - Linear control response
```

### Filter Transfer Functions

**Low-Pass (12 dB/octave):**
```
H(f) = 1 / √(1 + (f/Fc)^4)

Where:
  f = Signal frequency
  Fc = Cutoff frequency (from FC register + caps)
  
Slope: -12 dB/octave above Fc
       (-40 dB/decade)
```

**High-Pass (12 dB/octave):**
```
H(f) = (f/Fc)^2 / √(1 + (f/Fc)^4)

Slope: -12 dB/octave below Fc

6581 Special: Output attenuated -3dB relative to LP/BP
8580: Equal level (0dB)
```

**Band-Pass (6 dB/octave each side):**
```
H(f) = (f/Fc) / √(1 + (f/Fc)^4)

Slope: -6 dB/octave above AND below Fc
Center frequency: Fc
Bandwidth: Depends on resonance
```

**Resonance Effect:**
```
Q Factor:
  6581: Q ≈ 0.5 + (RES × 0.25)    → Q_max ≈ 4
  8580: Q ≈ 0.5 + (RES × 0.75)    → Q_max ≈ 12
  
Peak Amplitude:
  Gain_peak ≈ Q (at resonance=15)
  6581: ~12 dB peak maximum
  8580: ~22 dB peak maximum
```

---

## REVISION-SPECIFIC EMULATION FLAGS

### Configuration Structure (Example)

```c
typedef enum {
    SID_6581_R2,
    SID_6581_R3,
    SID_6581_R4,
    SID_6581_R4AR,
    SID_8580_R5,
    SID_6582      // Same as 8580 electrically
} sid_revision_t;

struct sid_config {
    sid_revision_t revision;
    
    // Power characteristics
    float vdd_voltage;          // 12.0 or 9.0
    
    // Filter parameters  
    float filter_cap_pf;        // 470, 1000, 2200, 22000
    float filter_fc_linearity;  // 0.0=linear, 1.0=sigmoid
    float filter_fc_variance;   // 0.0=perfect, 0.3=±30%
    float filter_res_strength;  // 0.0-1.0 multiplier
    bool  filter_hp_3db_atten;  // true for 6581
    
    // Waveform generation
    bool  waveform_mix_enabled; // Better on 8580
    float dac_linearity;        // 0.95 for 6581, 0.99 for 8580
    
    // Sample playback
    float dc_offset_voltage;    // 6.0 for 6581, 0.5 for 8580
    bool  volume_click_enabled; // true for 6581
    
    // Noise characteristics
    float noise_floor_db;       // -45 for 6581, -60 for 8580
};
```

### Preset Configurations

```c
// 6581 R3 (most common target)
sid_config sid_6581_r3 = {
    .revision = SID_6581_R3,
    .vdd_voltage = 12.0,
    .filter_cap_pf = 470.0,
    .filter_fc_linearity = 0.7,     // Moderate sigmoid
    .filter_fc_variance = 0.20,     // ±20% between chips
    .filter_res_strength = 0.3,     // Weak resonance
    .filter_hp_3db_atten = true,
    .waveform_mix_enabled = false,  // Poor mixing
    .dac_linearity = 0.95,
    .dc_offset_voltage = 6.0,
    .volume_click_enabled = true,
    .noise_floor_db = -45.0
};

// 8580 R5 (modern standard)
sid_config sid_8580_r5 = {
    .revision = SID_8580_R5,
    .vdd_voltage = 9.0,
    .filter_cap_pf = 22000.0,
    .filter_fc_linearity = 0.0,     // Linear
    .filter_fc_variance = 0.05,     // ±5%
    .filter_res_strength = 1.0,     // Full resonance
    .filter_hp_3db_atten = false,
    .waveform_mix_enabled = true,   // Better mixing
    .dac_linearity = 0.99,
    .dc_offset_voltage = 0.5,
    .volume_click_enabled = false,
    .noise_floor_db = -60.0
};
```

---

## CRITICAL IMPLEMENTATION NOTES

### Phase Accumulator Overflow

```c
// 24-bit accumulator per voice
uint32_t phase[3];
uint16_t frequency[3];

void update_oscillator(int voice) {
    phase[voice] = (phase[voice] + frequency[voice]) & 0xFFFFFF;
    
    // Sync pulse generation
    if ((phase[voice] & 0x800000) && !(prev_phase[voice] & 0x800000)) {
        // Overflow occurred - generate sync pulse
        sync_pulse[voice] = true;
    }
}
```

### Waveform Selection

```c
uint16_t generate_waveform(uint32_t phase, uint8_t control) {
    uint16_t output = 0;
    bool tri = control & 0x10;
    bool saw = control & 0x20;
    bool pulse = control & 0x40;
    bool noise = control & 0x80;
    
    if (test_bit) {
        output = pulse ? 0xFFF : 0;  // DC level
        return output;
    }
    
    int waveform_count = tri + saw + pulse + noise;
    
    if (waveform_count == 1) {
        // Single waveform - straightforward
        if (tri) output = generate_triangle(phase);
        if (saw) output = generate_sawtooth(phase);
        if (pulse) output = generate_pulse(phase, pulse_width);
        if (noise) output = generate_noise(lfsr_state);
    }
    else if (waveform_count > 1) {
        // Multiple waveforms - complex behavior!
        if (is_8580) {
            output = waveform_combine_8580(phase, control);
        } else {
            output = waveform_combine_6581(phase, control);
            // Often returns 0 or very low value!
        }
        
        // Special: noise with others can lock LFSR
        if (noise && (tri || saw || pulse)) {
            if (should_lock_noise()) {  // Probabilistic
                lfsr_locked = true;
            }
        }
    }
    
    return output;
}
```

### Envelope Generator State Machine

```c
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_IDLE
} envelope_state_t;

uint8_t envelope_value[3];
envelope_state_t envelope_state[3];
uint32_t envelope_counter[3];

void update_envelope(int voice) {
    bool gate = control[voice] & 0x01;
    
    // State transitions
    if (!gate && envelope_state[voice] != ENV_RELEASE) {
        envelope_state[voice] = ENV_RELEASE;
    }
    else if (gate && envelope_state[voice] == ENV_RELEASE) {
        envelope_state[voice] = ENV_ATTACK;
    }
    
    // Process current state
    switch(envelope_state[voice]) {
        case ENV_ATTACK:
            envelope_counter[voice] += attack_rate[voice];
            envelope_value[voice] = calculate_attack_curve(counter);
            if (envelope_value[voice] >= 255) {
                envelope_state[voice] = ENV_DECAY;
            }
            break;
            
        case ENV_DECAY:
            envelope_counter[voice] += decay_rate[voice];
            envelope_value[voice] = calculate_decay_curve(counter, sustain[voice]);
            if (envelope_value[voice] <= sustain[voice]) {
                envelope_state[voice] = ENV_SUSTAIN;
            }
            break;
            
        case ENV_SUSTAIN:
            envelope_value[voice] = sustain[voice];
            break;
            
        case ENV_RELEASE:
            envelope_counter[voice] += release_rate[voice];
            envelope_value[voice] = calculate_release_curve(counter);
            if (envelope_value[voice] == 0) {
                envelope_state[voice] = ENV_IDLE;
            }
            break;
    }
}
```

**Exponential Curve Modeling:**
```c
// Attack: Fast at start, slow at end
uint8_t calculate_attack_curve(uint32_t counter) {
    // Approximate exponential: y = 255 * (1 - e^(-t/τ))
    float normalized = counter / (float)total_attack_cycles;
    return 255 * (1.0 - exp(-3.0 * normalized));
}

// Decay/Release: Fast at start, slow near zero  
uint8_t calculate_decay_curve(uint32_t counter, uint8_t sustain) {
    float normalized = counter / (float)total_decay_cycles;
    uint8_t start_level = 255;
    return sustain + (start_level - sustain) * exp(-3.0 * normalized);
}
```

---

## AUDIO OUTPUT MIXING

### Final Output Calculation

```c
int16_t calculate_sid_output() {
    int32_t sum = 0;
    
    // For each voice
    for (int v = 0; v < 3; v++) {
        if (v == 2 && voice3_off) continue;  // 3OFF bit
        
        uint16_t waveform = generate_waveform(phase[v], control[v]);
        uint8_t envelope = envelope_value[v];
        
        // Voice output = waveform × envelope
        int16_t voice_output = (waveform * envelope) >> 8;  // Scale to 8-bit
        
        // Routing: filtered or direct
        if (filter_routing[v]) {
            filtered_input[v] = voice_output;
        } else {
            sum += voice_output;
        }
    }
    
    // Add external input if routed
    if (filter_ext_routing) {
        filtered_input[3] = external_input;
    }
    
    // Process through filter
    int16_t filtered = apply_filter(filtered_input);
    sum += filtered;
    
    // Apply volume
    sum = (sum * volume) >> 4;  // 4-bit volume scale
    
    // Add DC offset (6581) or keep centered (8580)
    if (is_6581) {
        sum += dc_offset;  // ~6V offset creates clicks
    }
    
    // Clamp to output range
    if (sum > 32767) sum = 32767;
    if (sum < -32768) sum = -32768;
    
    return (int16_t)sum;
}
```

---

## PERFORMANCE OPTIMIZATION TIPS

### Critical Path

**Every Clock Cycle:**
1. Update phase accumulators (3×)
2. Generate waveforms (3×)
3. Check sync conditions
4. Update noise LFSR

**Lower Frequency (every N cycles):**
1. Envelope generator updates
2. Filter coefficient recalculation
3. Pot reading (every 512 cycles)

### Optimization Strategies

```c
// Pre-calculate waveform tables (triangle, saw)
uint16_t triangle_table[4096];  // Index = phase[23:12]
uint16_t sawtooth_table[4096];  // Direct mapping

// Cache frequently-used calculations
float filter_coefficient[2048];  // Pre-calculate for all FC values

// Only recalculate filter when FC or RES changes
if (fc_changed || res_changed) {
    recalculate_filter();
}

// Use lookup tables for envelope curves
uint8_t attack_table[256][16];   // [time][rate]
uint8_t decay_table[256][16];    // [time][rate]
```

---

## TROUBLESHOOTING GUIDE

### No Audio Output

```
Check List:
1. Vcc present and correct? (5V ±0.25V)
2. Vdd present and correct? (12V or 9V depending on chip)
3. Bypass capacitors installed?
4. Filter capacitors installed and correct value?
5. Pull-down resistor (1kΩ) on AUDIO OUT?
6. AC coupling capacitor to amp?
7. Volume register = 0? (Should be 1-15)
8. GATE bit set?
9. Waveform selected?
10. Frequency ≠ 0?
```

### Weak or Distorted Audio

```
6581 Checks:
- Filter caps = 470pF (not 2200pF)
- Vdd = 12V (not 9V!)
- DC offset normal variation (~6V ±0.5V)

8580 Checks:
- Filter caps = 22nF (not 470pF)
- Vdd = 9V (not 12V! - DAMAGE!)
- Sample playback needs digi-boost

Both:
- Clean power supplies (low ripple)
- Good ground connection
- Proper AC coupling
- Not over-driving amp input
```

### Filter Not Working

```
Symptoms:
- No filter sweep effect
- Filter sounds "stuck"
- Distorted filter response

Causes:
1. Wrong capacitor values (most common!)
2. Dead filter section (ESD damage on 6581)
3. Incorrect filter mode selection
4. Voices not routed through filter (FILT bits)
5. No waveform selected
6. Volume = 0
```

### Sample Playback Issues

```
On 6581:
- Should work immediately via volume register
- If quiet: Check DC offset voltage
- If distorted: May be normal (chip variance)

On 8580:
- Quiet by design (DC offset removed)
- Solution: Add 470kΩ-1MΩ resistor (pin 26 to GND)
- Or: Use pulse-width technique
- Or: Use test-bit technique
```

---

## DATECODE INTERPRETATION

Format: **WWYY** (Week-Week Year-Year)

**Example Decoding:**

```
2082:
  WW = 20 (20th week of year)
  YY = 82 (1982)
  Date: Mid-May 1982
  
4392:
  WW = 43 (43rd week)
  YY = 92 (1992)
  Date: Late October 1992
```

**Production Period Estimation:**

| Datecode Range | Likely Revision |
|----------------|-----------------|
| 4981-2082 | R1 (prototype) |
| 0882-4985 | R2 |
| 1083-0986 | R3 |
| 4985-2286 | R4 |
| 2286-4392 | R4 AR |
| 3686-4893 | 8580 R5 |
| 1086-1492 | 6582/6582A |

**Warning:** Datecode alone is insufficient for exact revision identification. Always read package marking.

---

## MULTI-SID CONSIDERATIONS

### Synchronization

```
Critical for stereo/multi-voice:
  - Use same Φ2 clock for all SIDs
  - Reset all SIDs simultaneously
  - Matched filter capacitors (±1%)
  - Same revision if possible
  - Calibrate filter tracking
```

### Address Decoding

```
Example Dual SID:

SID 1: $D400-$D41F
  Decode: /I/O AND (A5=0) → /CS1
  
SID 2: $D420-$D43F  
  Decode: /I/O AND (A5=1) → /CS2

Example Quad SID:

SID 1: $D400-$D41F (A6=0, A5=0)
SID 2: $D420-$D43F (A6=0, A5=1)
SID 3: $D440-$D45F (A6=1, A5=0)
SID 4: $D460-$D47F (A6=1, A5=1)
```

### Stereo Mixing

```
Hardware:
  SID1 AUDIO OUT ──→ Left Channel Amp
  SID2 AUDIO OUT ──→ Right Channel Amp
  
Software:
  - Separate control of each SID
  - Pan effects via voice distribution
  - Stereo width via detuning
  - Pseudo-reverb via delayed echoes
```

---

## REFERENCE VOLTAGES AND LEVELS

### Internal Voltage Levels (Approximate)

```
6581:
  Logic LOW:    0V - 0.4V
  Logic HIGH:   4.5V - 5.0V
  Analog Vref:  6.0V (nominal, varies ±0.5V)
  
8580:
  Logic LOW:    0V - 0.4V  
  Logic HIGH:   4.5V - 5.0V
  Analog Vref:  4.5V (nominal, ±0.1V)
```

### Audio Signal Levels

```
6581 Output (Pin 27):
  DC Offset:      6.0V ±0.5V (chip dependent!)
  AC Component:   ±1.5V maximum (3V p-p)
  Typical Music:  ±0.5V to ±1.0V p-p
  SNR:            45-50 dB
  
8580 Output (Pin 27):
  DC Offset:      4.5V ±0.1V
  AC Component:   ±1.5V maximum (3V p-p)
  Typical Music:  ±0.5V to ±1.0V p-p
  SNR:            55-60 dB

After AC Coupling (both):
  Centered at:    0V (DC blocked)
  Swing:          ±1.5V maximum
  Into high-Z:    Clean sine wave
  Into 10kΩ:      May need buffering
```

---

## PRACTICAL DESIGN EXAMPLES

### Minimal SID Test Circuit

```
Components needed:
  - SID chip (6581 or 8580)
  - Appropriate power supplies (5V + 12V or 9V)
  - 2× filter capacitors (470pF or 22nF)
  - 2× bypass caps per supply (10μF + 100nF)
  - 1× 1kΩ resistor (audio out pull-down)
  - 1× 10μF capacitor (AC coupling)
  - 1× audio amplifier (LM386 or similar)
  - Breadboard, wires, speaker
  
Minimal Test:
  1. Wire power and ground
  2. Install filter and bypass caps
  3. Connect clock (1 MHz)
  4. Tie /RES high
  5. Connect address/data/control to microcontroller
  6. Output to amplifier via AC coupling
  7. Program simple tone
```

### Addressing in 6502 Systems

```assembly
; C64 standard SID address
SID_BASE    = $D400

; Voice 1 registers
FREQ1_LO    = SID_BASE + $00
FREQ1_HI    = SID_BASE + $01
PW1_LO      = SID_BASE + $02  
PW1_HI      = SID_BASE + $03
CONTROL1    = SID_BASE + $04
ATTACK1     = SID_BASE + $05
SUSTAIN1    = SID_BASE + $06

; Voice 2 registers (+$07 offset)
; Voice 3 registers (+$0E offset)

; Filter registers  
FILT_FC_LO  = SID_BASE + $15
FILT_FC_HI  = SID_BASE + $16
FILT_RES    = SID_BASE + $17
MODE_VOL    = SID_BASE + $18

; Read-only registers
POTX        = SID_BASE + $19
POTY        = SID_BASE + $1A
OSC3        = SID_BASE + $1B
ENV3        = SID_BASE + $1C
```

---

**END OF PINOUT AND ELECTRICAL REFERENCE**

For comprehensive register functionality, see main datasheet.
For quick lookup during coding, see quick reference card.
