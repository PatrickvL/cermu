namespace Emulation.Chip.Sound;

internal class Voice(float CPUClock)
{
    private readonly float _CPUClock = CPUClock;

    public void WritePulseWaveformWidth(u16 v)
    {
        PulseWaveformWidth = v;
        // TODO : Handle more side-effects in internal state?
    }

    public void WriteVoiceControlRegisterValue(u8 v)
    {
        bool currGated = Gated;
        bool currTest = Test;
        // Decode control register bits for $D404:VCREG1/$D40B:VCREG2/$$D412:VCREG3
        Gated = (v & 0x01) > 0;            // VCREG Bit 0: Gate Bit: 1=Start attack/decay/sustain, 0=Start release
        Synchronize = (v & 0x02) > 0;      // VCREG Bit 1: Synchronize Oscillator with other Oscillator frequency; Voice 1:1=Oscillator 3, Voice 2:1=Oscillator 1, Voice 3:1=Oscillator 2
        RingModulation = (v & 0x04) > 0;   // VCREG Bit 2: Ring modulate Oscillators; Voice 1:1=1 and 3,Voice 2:1=2 and 1,Voice 32:1=3 and 2
        Test = (v & 0x08) > 0;             // VCREG Bit 3: Test Bit: 1=Disable Oscillator
        Waveform = (WaveformBits)(v >> 4); // VCREG Bit 4-7: Select waveforms; 4:triangle, 5:sawtooth, 6:pulse, 7:random noise
        // Detect and handle changes
        if (Test & !currTest)
        {
            EnvelopeCycle = Cycle.Off;
            EnvelopeAmplitude = 0;
            OscillatorWaveform = 0;
            // TODO : Reset Noise waveform output
            // TODO : Bring (and hold) Pulse waveform output to DC level
        }
        else
        {
            if (Gated != currGated)
                EnvelopeCycle = Gated ? Cycle.Attack : Cycle.Release;

            // Perform state initialization/transition :
            OscillatorWaveform = (uint)(OscillatorStartPerWaveformBits[(int)Waveform] << 4);
        }
        // TODO : Handle more side-effects in internal state?
    }

    public void WriteAttackDecayRegisterValue(u8 v)
    {
        // Decode input bits for $D405:ATDCY1/$D40C:ATDCY2/$$D413:ATDCY3
        int decayRate = v & 0x0F; // ATDCY Bits 0-3: Select decay cycle duration (0-15)
        int attackRate = v >> 4;  // ATDCY Bits 4-7: Select attack cycle duration (0-15)
        // Transform/lookup rates into envelop deltas, and put these in the lookup table.
        EnvelopeDeltas[(int)Cycle.Decay] = RateToDelta(decayRate) / DecayReleaseDivider;
        EnvelopeDeltas[(int)Cycle.Attack] = RateToDelta(attackRate);
        // TODO : Handle more side-effects in internal state?
    }

    public void WriteSustainReleaseRegisterValue(u8 v)
    {
        // Decode input bits for $D406:SUREL1/$D40D:SUREL2/$$D414:SUREL3
        int releaseRate = v & 0x0F;  // SUREL Bits 0-3: Select release cycle duration (0-15)
        int sustainLevel = v & 0xF0; // SUREL Bits 4-7: Select sustain volume level (0-15)
        // Transform/lookup Release rate into an envelop delta, and put it in the lookup table.
        EnvelopeDeltas[(int)Cycle.Release] = RateToDelta(releaseRate) / DecayReleaseDivider;
        // Adjust SustainLevel to the same 16 bit range as EnvelopeAmplitude (turning 0xF0 into 0xFFFF).
        // Note : VICE does the same, albeit on an 8 bit value.
        SustainLevel = (sustainLevel << 8) | (sustainLevel << 4) | sustainLevel | sustainLevel >> 4;
        // TODO : Handle more side-effects in internal state?
    }

    private int RateToDelta(int rate) // rate:0-15
        // Calculate an accurate delta for given number of milliseconds, based on the duration of a single ClockCycle()
        // The resulting delta is the value that is added each cycle to EnvelopeAmplitude, so that after the selected number
        // of milliseconds, it changed by AmplitudePeak. TODO : Make this more accurate, how?
        => (int)(CyclesPerMillisecond / RatesInmS[rate]); // cyles 

    private int CyclesPerMillisecond => (int)(_CPUClock / 1000f);

    public void ClockCycle()
    {
        // TODO : Skip some # cyles? (Since output is only 4 Khz), or do that in MOS6581.ClockCycle()?

        // Transition the accumulator (source: VICE)
        uint nextAccumulator = (WaveformAccumulator + Frequency) & WaveformAccumulatorMax;
        uint bitsAccumulator = ~WaveformAccumulator & nextAccumulator;
        WaveformAccumulator = nextAccumulator;
        // Convert 24 bit (12.12 fixed point) WaveformAccumulator to into a 12 bit (8.4 fixed point)
        // OscillatorWaveform value, depending the selected WaveformBits.
        OscillatorWaveform = Waveform switch
        {
            WaveformBits.None => 0,
            // Note, that any form of fixed-point counting results in rounding errors (also, websid code comments mention sampling interval aliassing)
            WaveformBits.Triangle => (uint)(WaveformAccumulator ^ ((WaveformAccumulator & WaveformAccumulatorMSB) > 0 ? WaveformAccumulatorMax : 0)) >> (12 - 1),
            WaveformBits.Sawtooth => WaveformAccumulator >> 12,
            WaveformBits.Pulse => (uint)((WaveformAccumulator >> 8) > PulseWaveformWidth ? OscillatorMax : 0),
            WaveformBits.Noise => (uint)Random.Shared.Next(OscillatorMax + 1), // TODO : Improve
            _ => WaveformAccumulator >> 12, // TODO : "multiple enabled waveforms AND together"?
        };

        // Handle ADSR increases and transitions to next cycle, by fetching the active
        // envelopeDelta for the active EnvelopeCycle from the EnvelopeDeltas lookup table.
        // Note : Delta is positive for Attack, negative for Decay and Release, and zero for Sustain and Off cycles:
        int envelopeDelta = EnvelopeDeltas[(int)EnvelopeCycle];
        // A positive delta is used only by the Attack cycle.
        if (envelopeDelta > 0)
        {
            EnvelopeAmplitude += envelopeDelta;
            // Attack must not exceed the peak valud
            if (EnvelopeAmplitude >= AmplitudePeak)
            {
                EnvelopeAmplitude = AmplitudePeak;
                // Transition to the next cycle (Decay) and limit that to SustainLevel
                EnvelopeCycle = Cycle.Decay;
                EnvelopeNextLevel = SustainLevel;
            }
        }

        // Sustain and Off cycles have an envelopeDelta of zero, so won't change EnvelopeAmplitude (nor
        // EnvelopeCycle, which is set to Off or Attack or Release by WriteVoiceControlRegisterValue).

        // Handle negative delta on EnvelopeAmplitude, as done by Decay and Release.
        if (envelopeDelta < 0)
        {
            EnvelopeAmplitude += envelopeDelta;
            // Don't go below SustainLevel (at the end of the Decay cycle), or zero (at the end of the Release cycle).
            if (EnvelopeAmplitude <= EnvelopeNextLevel)
            {
                EnvelopeAmplitude = EnvelopeNextLevel;
                // Transition from current to next cycle; From 1 (Decay) to 2 (Sustain), or from 3 (Release) to 4 (Off).
                // (This shift avoids the need to check for and avoid decreasing below zero)
                EnvelopeCycle = (Cycle)((int)EnvelopeCycle + 1);
                // When this was hit when underflowing SustainLevel (and thus changing cycle Decay to Sustain),
                // the next level to limit underflow on is the zero level (at the end of the Release cycle).
                EnvelopeNextLevel = 0;
                // Note, that this code won't be hit again after Release cycle transitions to Off,
                // because the Off 'cycle' uses an envelopeDelta of zero.
            }
        }

        // The resulting value for this voice is the combination between tone oscillator/
        // waveform generator output and (ADSR) envelope generator amplitude output.
        // Note, this combination is called "Amplitude Modulator" (so herhaps remove Amplitude from EnvelopeAmplitude name?)
        Result = (OscillatorWaveform * (uint)EnvelopeAmplitude) / WaveformAccumulatorMax / 8; // remove 8 bit fraction from EnvelopeAmplitude
        // TODO : Probably need to do above differently
    }

    public void Reset()
    {
        Frequency = 0;
        PulseWaveformWidth = 0;
        Gated = false;
        Synchronize = false;
        RingModulation = false;
        Test = false;
        Waveform = WaveformBits.None;

        EnvelopeAmplitude = 0xFE << 8;
        OscillatorWaveform = 0;
        Result = 0;

        WaveformAccumulator = 0x555555; // Vice: Accumulator's even bits are high on powerup
        EnvelopeCycle = Cycle.Off;
        EnvelopeNextLevel = 0;
        SustainLevel = 0;
        EnvelopeDeltas[(int)Cycle.Attack] = 0;
        EnvelopeDeltas[(int)Cycle.Decay] = 0;
        EnvelopeDeltas[(int)Cycle.Release] = 0;
    }

    // Write-only voice register values :

    internal u16 Frequency = 0; // A 16 bit number. Updated by MOS6581.BusWrite() for FRELO and FREHI registers
    internal u16 PulseWaveformWidth = 0; // A 16 bit number. Updated by WritePulseWaveformWidth()
    // Values updated by WriteVoiceControlRegisterValue()
    internal bool Gated = false;
    private bool Synchronize = false;
    private bool RingModulation = false;
    private bool Test = false;
    private WaveformBits Waveform = WaveformBits.None;

    // Outside readable variables (albeit after shifting)

    internal int EnvelopeAmplitude; // A 16 bit (8.8 fixed-point) number. Reading $D41C (ENV3) returns upper 8 bits of Voice3.
    internal uint OscillatorWaveform; // A 12 bit (8.4 fixed-point) number. Reading $D41B (OSC3) returns upper 8 bits of Voice3.
    internal uint Result = 0; // Updated by ClockCycle() - TODO : Return it instead?

    // Internal state

    private uint WaveformAccumulator; // A 24 bit (12.12 fixed-point) number, updated by ClockCycle()
    // TODO : Add a LFSR (Linear Feedback Shift Register) for noise generation purposes
    private Cycle EnvelopeCycle = Cycle.Off; // Updated by WriteVoiceControlRegisterValue() and ClockCycle()
    private int EnvelopeNextLevel; // Updated by ClockCycle()
    private int SustainLevel; // Updated by WriteSustainReleaseRegisterValue()
    private readonly int[] EnvelopeDeltas = [ // Indexed with (int)EnvelopeCycle (signed for negative delta's)
        0, // Attack, updated by WriteAttackDecayRegisterValue()
        0, // Decay, updated by WriteAttackDecayRegisterValue()
        0, // Sustain, always stays at zero (but ClockCycle() copies SustainLevel into EnvelopeNextLevel)
        0, // Release, updated by WriteSustainReleaseRegisterValue()
        0, // Off, always stays at zero
    ];

    private enum Cycle { Attack, Decay, Sustain, Release, Off }; // Note : Used as in index into EnvelopeDeltas[]
    private enum WaveformBits
    {
        None = 0,
        Triangle = 1,
        Sawtooth = 2,
        SawtoothAndTriangle = 3,
        Pulse = 4,
        PulseAndTriangle = 5,
        PulseAndSawtooth = 6,
        PulseSawtoothAndTriangle = 6,
        Noise = 8,
        // TODO : 9 to 15 are also combinations of Noise with a selection of Pulse, Sawtooth and Triangle.
    }

    private const int WaveformAccumulatorMSB = 0x8000000; // Bit 23 (24th counting from 0)
    private const int WaveformAccumulatorMax = 0xFFFFFFF; // The highest 24 bit (12.12 fixed-point) number
    private const int OscillatorMax = 0xFFF; // The highest 12 bit (8.4 fixed-point) number
    private const int AmplitudePeak = 0xFFFF; // The highest 16 bit (8.8 fixed-point) number

    private static readonly u16[] RatesInmS = [2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000];
    private const int DecayReleaseDivider = -3; // Decay and Release are decreases, and take 3 times longer than RatesinMS[Attack]

    private readonly static u8[] OscillatorStartPerWaveformBits = // TODO : Remove once same result is reached by accurate emulation
    [
        0,    // 0x0. :
        0xAA, // 0x1. : VCREG3_TRIANGLE
        0x55, // 0x2. : VCREG3_SAWTOOTH
        0,    // 0x3. : ??
        0xFF, // 0x4. : VCREG3_PULSE
        0,    // 0x5. : ??
        0,    // 0x6. : ??
        0,    // 0x7. : ??
        0xFE, // 0x8. : VCREG3_NOISE
        0,    // 0x9. : ??
        0,    // 0xA. : ??
        0,    // 0xB. : ??
        0,    // 0xC. : ??
        0,    // 0xD. : ??
        0,    // 0xE. : ??
        0,    // 0xF. : ??
    ];
};
