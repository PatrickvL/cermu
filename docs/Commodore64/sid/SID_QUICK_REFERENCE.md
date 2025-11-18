# SID CHIP QUICK REFERENCE CARD
## For Emulator Development

---

## REVISION QUICK ID

```
Package     Die        Vdd   Caps     Process    Notes
--------    -------    ---   ----     -------    -----
6581        6581       12V   470pF    NMOS       R2/R3 (unmarked or "R3")
6581 R4     6581 R4    12V   470pF    NMOS       Later production
6581 R4 AR  6581 R4    12V   470pF    NMOS       Same die as R4
6582        8580R5!    9V    22nF     HMOS-II    Rebadged 8580
6582 A      8580R5!    9V    22nF     HMOS-II    Rebadged 8580
8580R5      8580R5     9V    22nF     HMOS-II    Redesigned chip
```

---

## POWER REQUIREMENTS

```
6581 Series:  Vcc = +5V @ 70mA,  Vdd = +12V @ 25mA  (650mW total)
8580 Series:  Vcc = +5V @ 65mA,  Vdd = +9V @ 25mA   (550mW total)

⚠️  WRONG VOLTAGE = DEAD CHIP!
```

---

## KEY DIFFERENCES MATRIX

| Feature              | 6581          | 8580          |
|----------------------|---------------|---------------|
| Filter Linearity     | Sigmoid/Log   | Linear        |
| Filter Variance      | ±30%          | ±5%           |
| Resonance Strength   | Weak (3-6dB)  | Strong (15dB) |
| HP Mode Level        | -3dB          | 0dB           |
| Waveform Mixing      | Poor/Silent   | Better/Usable |
| DC Offset (samples)  | HIGH (6V)     | LOW (0.5V)    |
| Volume Clicks        | YES (loud)    | NO (silent)   |
| Noise Floor          | -45dB         | -60dB         |
| Bass Distortion      | Strong        | Minimal       |
| Sound Character      | Dirty/Warm    | Clean/Precise |

---

## FILTER CAPACITOR VALUES

```
6581:  470pF - 2200pF  (typically 470pF in C64)
       Lower = higher cutoff, better bass control
       Higher = lower cutoff, datasheet spec

8580:  22nF            (standard C64C)
       6.8nF           (datasheet recommendation)
```

---

## REGISTER MAP QUICK REFERENCE

```
$D400-06: Voice 1 (Freq, PW, Control, ADSR)
$D407-0D: Voice 2 (Freq, PW, Control, ADSR)
$D40E-14: Voice 3 (Freq, PW, Control, ADSR)
$D415-16: Filter Cutoff (11-bit)
$D417:    Resonance + Filter Routing
$D418:    Mode/Volume (3OFF, HP, BP, LP, VOL)

READ-ONLY:
$D419:    POTX
$D41A:    POTY  
$D41B:    OSC3 (Oscillator 3 output)
$D41C:    ENV3 (Envelope 3 output)
```

---

## CONTROL REGISTER BIT MAP

```
$Dx04, $Dx0B, $Dx12: Control Register

Bit 7: NOISE    - Noise waveform
Bit 6: PULSE    - Pulse waveform  
Bit 5: SAWTOOTH - Saw waveform
Bit 4: TRIANGLE - Triangle waveform
Bit 3: TEST     - Reset/hold oscillator
Bit 2: RING     - Ring modulation enable
Bit 1: SYNC     - Hard sync enable
Bit 0: GATE     - Envelope gate

⚠️  Multiple waveforms NOT additive!
⚠️  NOISE + others can lock up (need TEST reset)
```

---

## FREQUENCY CALCULATION

```
PAL:  Fout = Fn × 0.0587 Hz     (Φ2 = 0.985248 MHz)
NTSC: Fout = Fn × 0.0610 Hz     (Φ2 = 1.022727 MHz)

Example: A-4 (440 Hz)
  PAL:  Fn = 7382 ($1CD6)
  NTSC: Fn = 7217 ($1C31)

Range: 0.06 Hz to 3906 Hz (Fn = 1 to 65535)
```

---

## ENVELOPE RATES (Most Common)

```
Value  Attack    Decay/Release
-----  ------    -------------
0      2 ms      6 ms         (Percussive)
2      16 ms     48 ms        (Fast)
5      56 ms     168 ms       (Medium)
8      100 ms    300 ms       (Normal)
11     800 ms    2.4 s        (Slow)
15     8 s       24 s         (Very slow)

All rates EXPONENTIAL, not linear!
Times scale with clock: T_actual = T_table × (1MHz / Φ2)
```

---

## FILTER CUTOFF (Approximate)

```
6581 (470pF):          8580 (22nF):
FC     Freq            FC     Freq
---    ----            ---    ----
$000   30-100 Hz       $000   ~20 Hz
$200   150-400 Hz      $200   ~400 Hz
$400   400-1200 Hz     $400   ~1200 Hz
$600   900-2500 Hz     $600   ~2400 Hz
$7FF   8k-14k Hz       $7FF   ~12 kHz

⚠️  6581: Wide variance!  8580: Consistent
```

---

## COMMON BUG/QUIRKS

```
1. Volume Register Clicks (6581)
   - Writing $D418 creates click
   - Used for 4-bit sample playback
   - Silent on 8580 (DC offset removed)

2. Waveform Combination Silence (6581)
   - Triangle+Saw often silent
   - Varies by chip revision
   - 8580 much better (but not perfect)

3. Noise Lock-Up (Both)
   - Noise + other waveforms can freeze noise
   - Fix: Toggle TEST bit

4. Filter Non-Linearity (6581)
   - Cutoff not proportional to register
   - ±30% variance between chips
   - Makes music sound different per C64

5. Weak Resonance (6581)
   - Maximum resonance barely audible
   - Resistor tolerance issues
   - 8580 has strong resonance
```

---

## SYNC AND RING MOD

```
SYNC: Slave resets when master overflows
  Voice 1 → syncs to Voice 3
  Voice 2 → syncs to Voice 1
  Voice 3 → syncs to Voice 2

RING MOD: XOR of triangle phases (metallic sound)
  Voice 1 → ring with Voice 3
  Voice 2 → ring with Voice 1
  Voice 3 → ring with Voice 2
  
⚠️  Ring mod only affects TRIANGLE waveform!
```

---

## SAMPLE PLAYBACK COMPARISON

```
Method              CPU    Quality  6581   8580
------              ---    -------  ----   ----
Volume Register     100%   4-bit    ★★★★★  ★☆☆☆☆*
Pulse Width         100%   8-bit    ★★☆☆☆  ★★☆☆☆
Test Bit            High   5-6bit   ★★★☆☆  ★★★☆☆
Mahoney (filter)    100%   8-bit    ★★★★★  ★★★★☆
Oscillator (AWE)    <5%    varies   ★★★★☆  ★★★★☆

* Add digi-boost resistor for ★★★★☆
```

---

## MUSICIAN'S PREFERENCE (Survey Data)

```
Revision   Preference   Sound Character
--------   ----------   ---------------
R2         15%          Raw, extreme bass, noisy
R3         30%          Classic 6581, balanced
R4/R4AR    10%          Clean 6581, predictable  
R5 (8580)  45%          Modern, precise, clean

Note: Modern demoscene heavily favors 8580
      Vintage enthusiasts prefer R3
```

---

## EMULATION CHECKLIST

**Minimum Accuracy:**
- [ ] 24-bit phase accumulator per voice
- [ ] Four waveform generators
- [ ] ADSR envelope state machine
- [ ] Programmable filter (LP/BP/HP)
- [ ] Volume control
- [ ] OSC3/ENV3 readback

**Good Accuracy (add):**
- [ ] Sync and ring modulation
- [ ] Waveform combination (lookup tables)
- [ ] Filter resonance
- [ ] Separate 6581/8580 models
- [ ] DC offset simulation

**Cycle-Accurate (add):**
- [ ] Per-cycle phase updates
- [ ] Envelope rate counters
- [ ] Filter settling behavior
- [ ] Per-chip variance simulation
- [ ] Output stage analog modeling

---

## QUICK DEBUGGING

```
Problem: No sound
→ Check: $D418 volume ≠ 0
→ Check: GATE bit set
→ Check: Waveform selected
→ Check: Frequency ≠ 0

Problem: Sound wrong
→ Check: Correct chip revision targeted
→ Check: Filter routed correctly
→ Check: Combined waveforms (avoid on 6581)

Problem: Noise/distortion
→ Check: Filter caps correct value
→ Check: Power supply stable
→ Check: Not over-driving output
```

---

## USEFUL CONSTANTS

```
// Φ2 Clock Frequencies
#define PAL_CLOCK   985248    // Hz
#define NTSC_CLOCK  1022727   // Hz

// Frequency conversion
#define FREQ_SCALE  (CLOCK / 16777216.0)  // Fn to Hz

// Envelope rates (@ 1 MHz, in cycles)
const uint32_t attack_cycles[16] = {
    2000, 8000, 16000, 24000, 38000, 56000, 68000, 80000,
    100000, 250000, 500000, 800000, 1000000, 3000000, 5000000, 8000000
};

// Filter caps effect on cutoff
#define FILTER_SCALE_6581  (1.0 / cap_value_pF)
#define FILTER_SCALE_8580  (1.0 / cap_value_nF)
```

---

## SOUND DESIGN QUICK TIPS

**Bass (6581):**
- Saw or triangle waveform
- Route through low-pass filter
- Resonance=0 (doesn't help on 6581)
- Let filter distortion add character

**Bass (8580):**
- Saw or pulse waveform
- Pulse width modulation adds movement
- Can use filter resonance for punch
- Need to add saturation/distortion in mix

**Leads (Both):**
- Pulse with width modulation
- Filter sweeps for expression
- ADSR with moderate attack/decay

**Pads (8580 better):**
- Triangle + Pulse combination
- Slow attack, long decay, high sustain
- Detuned voices for thickness
- Filter with moderate resonance

**Drums (6581 better):**
- Noise for snare/hi-hat
- Triangle + noise for toms
- Filter sweeps for dynamics
- Sample playback for kicks (volume register)

---

**For Cermu Emulation:**
- Start with basic 8580 (simpler, more consistent)
- Add 6581 later (requires variance modeling)
- Focus on cycle-accurate oscillators first
- Filter can be approximate initially
- Use reSID waveform tables as reference

END OF QUICK REFERENCE
