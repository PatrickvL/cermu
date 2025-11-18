using Emulation.Core;
using Emulation.Hardware.Generic;
using Emulation.Hardware.Part;

namespace Emulation.Chip.Sound;

public class MOS6581(float CPUClock, string name = "SID MOS 6581") : RWDevice(name, 0x400, (int)Pin._CS)
{
    // SID MOS 6581 DIP has 28 pins; Pinout :
    public enum Pin
    {
        CAP1A = 1, Vdd = 28,
        CAP1B = 2, AUDIO_OUT = 27,
        CAP2A = 3, EXT_IN = 26,
        CAP2B = 4, Vcc = 25,
        _RES = 5, POT_X = 24,
        phi2 = 6, POT_Y = 23,
        R_W = 7, D7 = 22,
        _CS = 8, D6 = 21,
        A0 = 9, D5 = 20,
        A1 = 10, D4 = 19,
        A2 = 11, D3 = 18,
        A3 = 12, D2 = 17,
        A4 = 13, D1 = 16,
        GND = 14, D0 = 15,
    }

    // Pins sets
    private static readonly int[] PinsA0toA4 = [
        (int)Pin.A0, (int)Pin.A1, (int)Pin.A2, (int)Pin.A3, (int)Pin.A4];
    private static readonly int[] PinsD0toD7 = [
        (int)Pin.D0, (int)Pin.D1, (int)Pin.D2, (int)Pin.D3,
        (int)Pin.D4, (int)Pin.D5, (int)Pin.D6, (int)Pin.D7];

    // Hardware Pins
    public readonly HWPinn CAP1A = NewPin(Input, "CAP1A", (int)Pin.CAP1A, "???");
    public readonly HWPinn CAP1B = NewPin(Input, "CAP1B", (int)Pin.CAP1B, "???");
    public readonly HWPinn CAP2A = NewPin(Input, "CAP2A", (int)Pin.CAP2A, "???");
    public readonly HWPinn CAP2B = NewPin(Input, "CAP2B", (int)Pin.CAP2B, "???");
    public readonly HWPinn _RES = NewPin(Input, "/RES", (int)Pin._RES, "Reset Input");
    public readonly HWPinn phi2 = NewPin(Clock, "phi2", (int)Pin.phi2, "Clock Input");
    public readonly HWPinn R_W = NewPin(Input, "R/W", (int)Pin.R_W, "Read/Write Input");
    public HWPinn _CS => _CE; // Note : _CS is inherited from RWDevice._CE
    public readonly HWPinn A0toA4 = PinSet("A0..A4", PinsA0toA4, "Address Inputs, 5 bits", Input);
    public readonly HWPinn GND = NewPin(Ground, "GND", (int)Pin.GND, "Ground");
    public readonly HWPinn D0toD7 = PinSet("D0..D7", PinsD0toD7, "Data Bus, 8 bits");
    public readonly HWPinn POT_X = NewPin(Input, "POT_X", (int)Pin.POT_X, "???");
    public readonly HWPinn POT_Y = NewPin(Input, "POT_Y", (int)Pin.POT_Y, "???");
    public readonly HWPinn Vcc = NewPin(Plus5Volt, "Vcc", (int)Pin.Vcc, "Power (Usually +5v)???");
    public readonly HWPinn EXT_IN = NewPin(Input, "EXT_IN", (int)Pin.EXT_IN, "External input???");
    public readonly HWPinn AUDIO_OUT = NewPin(Output, "AUDIO_OUT", (int)Pin.AUDIO_OUT, "Audio output???");
    public readonly HWPinn Vdd = NewPin(Plus5Volt, "Vdd", (int)Pin.Vdd, "Power (Usually +5v)???");

    // Register dimensions
    private const int RegsBits = 5;
    private const int RegsSize = 1 << RegsBits; // 32
    private const int RegsMask = RegsSize - 1; // 31

    public override u8 BusRead(int a)
    {
        uint r = (uint)(a & RegsMask); // The SID registers are repeated each 32 bytes in the area $d400-$d7ff
        u8 v;
        switch (r)
        {
            case 25: // $D419:POTX Read Game Paddle 1 (or 3) Position
                v = 0; // TODO
                break;
            case 26: // $D41A:POTY Read Game Paddle 2 (or 4) Position
                v = 0; // TODO
                break;
            case 27: // $D41B:OSC3 Read Oscillator 3/Random Number Generator
                // "always reflects the changing output of the oscillator and is not affected in any way by the Envelope Generator. "
                v = (u8)(Voice3.OscillatorWaveform >> 4); // "read the upper 8 output bits of Oscillator 3"
                break;
            case 28: // $D41C:ENV3 Envelope Generator 3 Output
                // "The Voice 3 Envelope Generator must be gated in order to produce any output from this register."
                v = (u8)(Voice3.Gated ? Voice3.EnvelopeAmplitude >> 8 : 0); // Assume upper 8 output bits
                break;
            default: // All other registers are either write-only or (the final 3) unmapped.
                // Return previous bus value when reading write-only registers.
                // This is what makes SID/busvalue/busvalue.prg test succeed.
                return (u8)D0toD7.Value;
        }

        // Store bus value on valid register read for later retrieval when reading write-only registers
        D0toD7.Value = v;
        return v;
    }

    public override void BusWrite(int a, u8 v)
    {
        uint r = (uint)(a & 31); // The SID registers are repeated each 32 bytes in the area $d400-$d7ff
        if (r < 21) // Writing to one of the 7 registers for each of the 3 voices?
        {
            // Map register range to a voice selection (code seems faster than reading an array)
            Voice voice = (r / 7) switch
            {
                0 => Voice1,
                1 => Voice2,
                _ => Voice3
            };
            // Perform a write-handler for each specific voice register 
            switch (r % 7)
            {
                case 0: // $D400:FRELO1/$D407:FRELO2/$D40E:FRELO3 Voice Frequency Control (low byte)
                    voice.Frequency = Helpers.LH(Helpers.H(voice.Frequency), v);
                    break;
                case 1: // $D401:FREHI1/$D408:FREHI2/$D40F:FREHI3 Voice Frequency Control (high byte)
                    voice.Frequency = Helpers.LH(v, Helpers.L(voice.Frequency));
                    break;
                case 2: // $D402:PWLO1/$D409:PWLO2/$D410:PWLO3 Voice Pulse Waveform Width (low byte)
                    voice.WritePulseWaveformWidth(Helpers.LH(Helpers.H(voice.PulseWaveformWidth), v));
                    break;
                case 3: // $D403:PWHI1/$D40A:PWHI2/$D411:PWHI3 Voice Pulse Waveform Width (high nybble)
                    voice.WritePulseWaveformWidth(Helpers.LH(v, Helpers.L(voice.PulseWaveformWidth)));
                    break;
                case 4: // $D404:VCREG1/$D40B:VCREG2/$D412:VCREG3 Voice Control Register
                    voice.WriteVoiceControlRegisterValue(v);
                    break;
                case 5: // $D405:ATDCY1/$D40C:ATDCY2/$D413:ATDCY3 Voice Attack/Decay Register
                    voice.WriteAttackDecayRegisterValue(v);
                    break;
                case 6: // $D406:SUREL1/$D40D:SUREL2/$D414:SUREL3 Voice Sustain/Release Control Register
                    voice.WriteSustainReleaseRegisterValue(v);
                    break;
            }
        }
        else
        {
            // Perform a write-handler for some registers, skipping read-only registers, and updating the bus for unmapped registers
            switch (r)
            {
                // Like all voice registers, CUTLO, CUTHI, RESON and SIGVOL are write-only.
                case 21: // $D415:CUTLO Bits 0-2: Low portion of filter cutoff frequency (Bits 5-7: Unused)
                    FilterCutoffFrequency = (FilterCutoffFrequency & 0x7F8) | (v & 0x07);
                    return;
                case 22: // $D416:CUTHI Filter Cutoff Frequency (high byte)
                    FilterCutoffFrequency = ((int)v << 3) | (FilterCutoffFrequency & 0x07);
                    return;
                case 23: // $D417:RESON Filter Resonance Control Register
                    WriteResonanceControlRegisterValue(v);
                    break;
                case 24: // $D418:SIGVOL Volume and Filter Select Register
                    WriteVolumeAndFilterSelectRegisterValue(v);
                    break;

                // POTX to ENV3 are read-only
                case 25: return; // $D419:POTX Read Game Paddle 1 (or 3) Position
                case 26: return; // $D41A:POTY Read Game Paddle 2 (or 4) Position
                case 27: return; // $D41B:OSC3 Read Oscillator 3/Random Number Generator
                case 28: return; // $D41C:ENV3 Envelope Generator 3 Output

                // "The SID chip has been provided with enough addresses for 32 different
                // registers, but as it has only 29, the remaining three addresses are
                // not used. Reading them will always return a value of 255($FF), and
                // writing to them will have no effect."
                // Assume that an attempt to write, also updates the data bus (which is returned
                // when reading a write-only register), so update the bus with this 0xFF here :
                case 29: v = 0xFF; break; // $D41D
                case 30: v = 0xFF; break; // $D41E
                case 31: v = 0xFF; break; // $D41F
            }
        }

        // Store bus value on valid register write for later retrieval when reading write-only registers
        D0toD7.Value = v;
    }

    private void WriteResonanceControlRegisterValue(u8 v)
    {
        // Decode $D417:RESON Filter Resonance Control Register
        FilterVoice1 = (v & 0x01) > 0;    // RESON_VOICE1   Bit 0: Filter the output of voice 1? 1=yes
        FilterVoice2 = (v & 0x02) > 0;    // RESON_VOICE2   Bit 1: Filter the output of voice 2? 1=yes
        FilterVoice3 = (v & 0x04) > 0;    // RESON_VOICE3   Bit 2: Filter the output of voice 3? 1=yes
        FilterVoice4 = (v & 0x08) > 0;    // RESON_EXTERNAL Bit 3: Filter the output from the external input? 1=yes
        FilterResonance = (uint)(v >> 4); // RESON_FILTER   Bits 4-7: Select filter resonance 0-15
        // Update some internal state (counting the number of voices that are to be filtered, so we can average them)
        FilterVoiceCount = (uint)((FilterVoice1 ? 1 : 0) + (FilterVoice2 ? 1 : 0) + (FilterVoice3 ? 1 : 0) + (FilterVoice4 ? 1 : 0));
        // TODO : Handle more side-effects in internal state?
    }

    private void WriteVolumeAndFilterSelectRegisterValue(u8 v)
    {
        // Decode $D418:SIGVOL Volume and Filter Select Register
        Volume = (uint)(v & 0x0F);        // SIGVOL_VOL   Bits 0-3: Select output volume (0-15)
        LowPassEnabled = (v & 0x10) > 0;  // SIGVOL_LOW   Bit 4: Select low-pass filter, 1=low-pass on
        BandPassEnabled = (v & 0x20) > 0; // SIGVOL_BAND  Bit 5: Select band-pass filter, 1=band-pass on
        HighPassEnabled = (v & 0x40) > 0; // SIGVOL_HIGH  Bit 6: Select high-pass filter, 1=high-pass on
        Voice3Disabled = (v & 0x80) > 0;  // SIGVOL_V3OFF Bit 7: Disconnect output of voice 3, 1=voice 3 off
        // TODO : Handle more side-effects in internal state?
    }

    public void ClockCycle()
    {
        if (_RES.IsLow)
        {
            // Officially only reacts after 10 cycles
            _RES.ResetPin();
            Reset(); // TODO: Verify
        }

        if (_CS.IsHigh)
            return;

        // TODO : Skip some # cyles? (Since output is only 4 Khz), or do that in Voice.ClockCycle()?

        Voice1.ClockCycle();
        Voice2.ClockCycle();
        Voice3.ClockCycle();
        uint Voice4_Result = 0; // TODO : Fetch

        uint voicesResult = 0;
        if (Volume > 0)
        {
            // Accumulate and all voice inputs
            uint unfilteredVoiceInput = (FilterVoice1 ? 0 : Voice1.Result) + (FilterVoice2 ? 0 : Voice2.Result) + (FilterVoice4 ? 0 : Voice4_Result);
            uint filteredVoiceInput = (FilterVoice1 ? Voice1.Result : 0) + (FilterVoice2 ? Voice2.Result : 0) + (FilterVoice4 ? Voice4_Result : 0);

            uint filteredVoicesCount = FilterVoiceCount;
            uint unfilteredVoicesCount = 4 - filteredVoicesCount;
            // Take the optionally disabled voice 3 into account separately
            if (Voice3Disabled)
            {
                // .. by deducting it (on the appropriate side)
                if (FilterVoice3)
                    filteredVoicesCount--;
                else
                    unfilteredVoicesCount--;
            }
            else
            {
                // .. by including it in the total (on the appropriate side)
                if (FilterVoice3)
                    filteredVoiceInput += Voice3.Result;
                else
                    unfilteredVoiceInput += Voice3.Result;
            }

            // Calculate the average unfiltered voices, and the average to-be-filter input value :
            uint unfilterVoices = unfilteredVoicesCount == 0 ? 0 : unfilteredVoiceInput / unfilteredVoicesCount;
            uint filterInput = filteredVoicesCount == 0 ? 0 : filteredVoiceInput / filteredVoicesCount;
            // TODO : Apply LowPass, BandPass and HighPass filters to filterInput
            uint filterOutput = filterInput;

            // Apply global volume (quite coarse, only 16 levels)
            voicesResult = ((filterOutput + unfilterVoices) * Volume) / (2 * 15);
        }

        // Add filtered and volume-adjusted output to buffer
        SampleBuffer[SampleIndex++] = (u8)voicesResult;
        if (SampleIndex >= SampleBufferSize)
        {
            // TODO : Flush buffer to host audio out
            SampleIndex = 0;
        }
    }

    public void Reset()
    {
        Voice1.Reset();
        Voice2.Reset();
        Voice3.Reset();

        FilterCutoffFrequency = 0;
        FilterVoice1 = false;
        FilterVoice2 = false;
        FilterVoice3 = false;
        FilterVoice4 = false;
        FilterResonance = 0;
        Volume = 0;
        LowPassEnabled = false;
        BandPassEnabled = false;
        HighPassEnabled = false;
        Voice3Disabled = false;

        FilterVoiceCount = 0;
        SampleIndex = 0;
    }

    // Write-only register values :

    private readonly Voice Voice1 = new(CPUClock);
    private readonly Voice Voice2 = new(CPUClock);
    private readonly Voice Voice3 = new(CPUClock);

    private int FilterCutoffFrequency = 0; // An 11 bit number. Updated by BusWrite(), CUTLO and CUTHI cases.
    private bool FilterVoice1 = false;
    private bool FilterVoice2 = false;
    private bool FilterVoice3 = false;
    private bool FilterVoice4 = false;
    private uint FilterResonance = 0; // 0-15
    private uint Volume = 0; // 0-15
    private bool LowPassEnabled = false;
    private bool BandPassEnabled = false;
    private bool HighPassEnabled = false;
    private bool Voice3Disabled = false;

    // Internal state

    private uint FilterVoiceCount = 0;
    internal uint SampleIndex = 0;
    internal u8[] SampleBuffer = new u8[SampleBufferSize];
    internal const int SampleBufferSize = 128 * 1024;
}
