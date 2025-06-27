#include "mos6581.h" // sid
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Rates in milliseconds for envelope calculations
static const uint16_t rates_in_ms[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};

// TODO : Remove once same result is reached by accurate emulation
static const uint8_t oscillator_start_per_waveform_bits[16] = {
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
};

void* mos6581_system_create(chip_descriptor_t* desc) {
    mos6581_t* sid = (mos6581_t*)calloc(1, sizeof(mos6581_t));
    if (!sid) return NULL;
    sid->desc = desc;
    
    // Initialize voices with CPU clock (assuming 1MHz)
    sid->voice1.cpu_clock = 1000000.0f;
    sid->voice2.cpu_clock = 1000000.0f;
    sid->voice3.cpu_clock = 1000000.0f;
    
    mos6581_reset(sid);
    return sid;
}

void mos6581_system_destroy(void* chip) {
    free(chip);
}

void mos6581_reset(mos6581_t* sid) {
    voice_reset(&sid->voice1);
    voice_reset(&sid->voice2);
    voice_reset(&sid->voice3);

    sid->filter_cutoff_frequency = 0;
    sid->filter_voice1 = false;
    sid->filter_voice2 = false;
    sid->filter_voice3 = false;
    sid->filter_voice4 = false;
    sid->filter_resonance = 0;
    sid->volume = 0;
    sid->low_pass_enabled = false;
    sid->band_pass_enabled = false;
    sid->high_pass_enabled = false;
    sid->voice3_disabled = false;

    sid->filter_voice_count = 0;
    sid->sample_index = 0;
}

uint8_t mos6581_registers_read(void* context, uint16_t address) {
    mos6581_t* sid = (mos6581_t*)context;
    uint32_t r = address & SID_REGS_MASK; // The SID registers are repeated each 32 bytes in the area $d400-$d7ff
    
    switch (r) {
        case 25: // $D419:POTX Read Game Paddle 1 (or 3) Position
            return 0; // TODO
        case 26: // $D41A:POTY Read Game Paddle 2 (or 4) Position
            return 0; // TODO
        case 27: // $D41B:OSC3 Read Oscillator 3/Random Number Generator
            // "always reflects the changing output of the oscillator and is not affected in any way by the Envelope Generator. "
            return  (uint8_t)(sid->voice3.oscillator_waveform >> 4); // "read the upper 8 output bits of Oscillator 3"
        case 28: // $D41C:ENV3 Envelope Generator 3 Output
            // "The Voice 3 Envelope Generator must be gated in order to produce any output from this register."
            return  (uint8_t)(sid->voice3.gated ? sid->voice3.envelope_amplitude >> 8 : 0); // Assume upper 8 output bits
        default: // All other registers are either write-only or (the final 3) unmapped.
            // Return previous bus value when reading write-only registers.
            // This is what makes SID/busvalue/busvalue.prg test succeed.
            return  sid->bus_interface.detached_read(sid->bus_interface.context);
    }
}

void mos6581_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6581_t* sid = (mos6581_t*)context;
    uint32_t r = address & 31; // The SID registers are repeated each 32 bytes in the area $d400-$d7ff
    
    if (r < 21) { // Writing to one of the 7 registers for each of the 3 voices?
        // Map register range to a voice selection (code seems faster than reading an array)
        voice_t* voice;
        switch (r / 7) {
            case 0: voice = &sid->voice1; break;
            case 1: voice = &sid->voice2; break;
            default: voice = &sid->voice3; break;
        }
        
        // Perform a write-handler for each specific voice register 
        switch (r % 7) {
            case 0: // $D400:FRELO1/$D407:FRELO2/$D40E:FRELO3 Voice Frequency Control (low byte)
                voice->frequency = (voice->frequency & 0xFF00) | value;
                break;
            case 1: // $D401:FREHI1/$D408:FREHI2/$D40F:FREHI3 Voice Frequency Control (high byte)
                voice->frequency = (voice->frequency & 0x00FF) | (value << 8);
                break;
            case 2: // $D402:PWLO1/$D409:PWLO2/$D410:PWLO3 Voice Pulse Waveform Width (low byte)
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0xFF00) | value);
                break;
            case 3: // $D403:PWHI1/$D40A:PWHI2/$D411:PWHI3 Voice Pulse Waveform Width (high nybble)
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0x00FF) | (value << 8));
                break;
            case 4: // $D404:VCREG1/$D40B:VCREG2/$D412:VCREG3 Voice Control Register
                voice_write_voice_control_register_value(voice, value);
                break;
            case 5: // $D405:ATDCY1/$D40C:ATDCY2/$D413:ATDCY3 Voice Attack/Decay Register
                voice_write_attack_decay_register_value(voice, value);
                break;
            case 6: // $D406:SUREL1/$D40D:SUREL2/$D414:SUREL3 Voice Sustain/Release Control Register
                voice_write_sustain_release_register_value(voice, value);
                break;
        }
    } else {
        // Perform a write-handler for some registers, skipping read-only registers, and updating the bus for unmapped registers
        switch (r) {
            // Like all voice registers, CUTLO, CUTHI, RESON and SIGVOL are write-only.
            case 21: // $D415:CUTLO Bits 0-2: Low portion of filter cutoff frequency (Bits 5-7: Unused)
                sid->filter_cutoff_frequency = (sid->filter_cutoff_frequency & 0x7F8) | (value & 0x07);
                return;
            case 22: // $D416:CUTHI Filter Cutoff Frequency (high byte)
                sid->filter_cutoff_frequency = ((int)value << 3) | (sid->filter_cutoff_frequency & 0x07);
                return;
            case 23: // $D417:RESON Filter Resonance Control Register
                mos6581_write_resonance_control_register_value(sid, value);
                break;
            case 24: // $D418:SIGVOL Volume and Filter Select Register
                mos6581_write_volume_and_filter_select_register_value(sid, value);
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
            case 29: value = 0xFF; break; // $D41D
            case 30: value = 0xFF; break; // $D41E
            case 31: value = 0xFF; break; // $D41F
        }
    }
}

void mos6581_write_resonance_control_register_value(mos6581_t* sid, uint8_t v) {
    // Decode $D417:RESON Filter Resonance Control Register
    sid->filter_voice1 = (v & 0x01) > 0;    // RESON_VOICE1   Bit 0: Filter the output of voice 1? 1=yes
    sid->filter_voice2 = (v & 0x02) > 0;    // RESON_VOICE2   Bit 1: Filter the output of voice 2? 1=yes
    sid->filter_voice3 = (v & 0x04) > 0;    // RESON_VOICE3   Bit 2: Filter the output of voice 3? 1=yes
    sid->filter_voice4 = (v & 0x08) > 0;    // RESON_EXTERNAL Bit 3: Filter the output from the external input? 1=yes
    sid->filter_resonance = v >> 4;         // RESON_FILTER   Bits 4-7: Select filter resonance 0-15
    // Update some internal state (counting the number of voices that are to be filtered, so we can average them)
    sid->filter_voice_count = (sid->filter_voice1 ? 1 : 0) + (sid->filter_voice2 ? 1 : 0) + (sid->filter_voice3 ? 1 : 0) + (sid->filter_voice4 ? 1 : 0);
    // TODO : Handle more side-effects in internal state?
}

void mos6581_write_volume_and_filter_select_register_value(mos6581_t* sid, uint8_t v) {
    // Decode $D418:SIGVOL Volume and Filter Select Register
    sid->volume = v & 0x0F;                  // SIGVOL_VOL   Bits 0-3: Select output volume (0-15)
    sid->low_pass_enabled = (v & 0x10) > 0;  // SIGVOL_LOW   Bit 4: Select low-pass filter, 1=low-pass on
    sid->band_pass_enabled = (v & 0x20) > 0; // SIGVOL_BAND  Bit 5: Select band-pass filter, 1=band-pass on
    sid->high_pass_enabled = (v & 0x40) > 0; // SIGVOL_HIGH  Bit 6: Select high-pass filter, 1=high-pass on
    sid->voice3_disabled = (v & 0x80) > 0;   // SIGVOL_V3OFF Bit 7: Disconnect output of voice 3, 1=voice 3 off
    // TODO : Handle more side-effects in internal state?
}

void mos6581_cycle(mos6581_t* sid) {
    // TODO : Reset logic for _RES pin - check if _RES is low
    // if (_RES.IsLow)
    // {
    //     // Officially only reacts after 10 cycles
    //     _RES.ResetPin();
    //     Reset(); // TODO: Verify
    // }

    // TODO : Check if _CS is high and return if so
    // if (_CS.IsHigh)
    //     return;

    // TODO : Skip some # cycles? (Since output is only 4 Khz), or do that in Voice.ClockCycle()?

    voice_clock_cycle(&sid->voice1);
    voice_clock_cycle(&sid->voice2);
    voice_clock_cycle(&sid->voice3);
    uint32_t voice4_result = 0; // TODO : Fetch

    uint32_t voices_result = 0;
    if (sid->volume > 0) {
        // Accumulate and all voice inputs
        uint32_t unfiltered_voice_input = (sid->filter_voice1 ? 0 : sid->voice1.result) + (sid->filter_voice2 ? 0 : sid->voice2.result) + (sid->filter_voice4 ? 0 : voice4_result);
        uint32_t filtered_voice_input = (sid->filter_voice1 ? sid->voice1.result : 0) + (sid->filter_voice2 ? sid->voice2.result : 0) + (sid->filter_voice4 ? voice4_result : 0);

        uint32_t filtered_voices_count = sid->filter_voice_count;
        uint32_t unfiltered_voices_count = 4 - filtered_voices_count;
        // Take the optionally disabled voice 3 into account separately
        if (sid->voice3_disabled) {
            // .. by deducting it (on the appropriate side)
            if (sid->filter_voice3)
                filtered_voices_count--;
            else
                unfiltered_voices_count--;
        } else {
            // .. by including it in the total (on the appropriate side)
            if (sid->filter_voice3)
                filtered_voice_input += sid->voice3.result;
            else
                unfiltered_voice_input += sid->voice3.result;
        }

        // Calculate the average unfiltered voices, and the average to-be-filter input value :
        uint32_t unfilter_voices = unfiltered_voices_count == 0 ? 0 : unfiltered_voice_input / unfiltered_voices_count;
        uint32_t filter_input = filtered_voices_count == 0 ? 0 : filtered_voice_input / filtered_voices_count;
        // TODO : Apply LowPass, BandPass and HighPass filters to filterInput
        uint32_t filter_output = filter_input;

        // Apply global volume (quite coarse, only 16 levels)
        voices_result = ((filter_output + unfilter_voices) * sid->volume) / (2 * 15);
    }

    // Add filtered and volume-adjusted output to buffer
    sid->sample_buffer[sid->sample_index++] = (uint8_t)voices_result;
    if (sid->sample_index >= SAMPLE_BUFFER_SIZE) {
        // TODO : Flush buffer to host audio out
        sid->sample_index = 0;
    }
}

void mos6581_bus_attach(void* chip, bus_cycle_ops_t* bus_interface) {
    mos6581_t* sid = (mos6581_t*)chip;
    sid->bus_interface = *bus_interface;
}


chip_descriptor_t mos6581_descriptor = {
    .description = "MOS6581 SID Sound Interface Device",
    .create = mos6581_system_create,
    .destroy = mos6581_system_destroy,
    .bus_attach = mos6581_bus_attach,
    .read = mos6581_registers_read,
    .write = mos6581_registers_write,
    .bank_change = NULL,
    .get_rwcb_context = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6581_render_debug_window,
    .render_settings_window = mos6581_render_settings_window
#endif
};

// Voice function implementations

void voice_write_pulse_waveform_width(voice_t* voice, uint16_t v) {
    voice->pulse_waveform_width = v;
    // TODO : Handle more side-effects in internal state?
}

void voice_write_voice_control_register_value(voice_t* voice, uint8_t v) {
    bool curr_gated = voice->gated;
    bool curr_test = voice->test;
    // Decode control register bits for $D404:VCREG1/$D40B:VCREG2/$$D412:VCREG3
    voice->gated = (v & 0x01) > 0;            // VCREG Bit 0: Gate Bit: 1=Start attack/decay/sustain, 0=Start release
    voice->synchronize = (v & 0x02) > 0;      // VCREG Bit 1: Synchronize Oscillator with other Oscillator frequency; Voice 1:1=Oscillator 3, Voice 2:1=Oscillator 1, Voice 3:1=Oscillator 2
    voice->ring_modulation = (v & 0x04) > 0;  // VCREG Bit 2: Ring modulate Oscillators; Voice 1:1=1 and 3,Voice 2:1=2 and 1,Voice 32:1=3 and 2
    voice->test = (v & 0x08) > 0;             // VCREG Bit 3: Test Bit: 1=Disable Oscillator
    voice->waveform = (waveform_bits_t)(v >> 4); // VCREG Bit 4-7: Select waveforms; 4:triangle, 5:sawtooth, 6:pulse, 7:random noise
    // Detect and handle changes
    if (voice->test && !curr_test) {
        voice->envelope_cycle = CYCLE_OFF;
        voice->envelope_amplitude = 0;
        voice->oscillator_waveform = 0;
        // TODO : Reset Noise waveform output
        // TODO : Bring (and hold) Pulse waveform output to DC level
    } else {
        if (voice->gated != curr_gated)
            voice->envelope_cycle = voice->gated ? CYCLE_ATTACK : CYCLE_RELEASE;

        // Perform state initialization/transition :
        voice->oscillator_waveform = (uint32_t)(oscillator_start_per_waveform_bits[(int)voice->waveform] << 4);
    }
    // TODO : Handle more side-effects in internal state?
}

void voice_write_attack_decay_register_value(voice_t* voice, uint8_t v) {
    // Decode input bits for $D405:ATDCY1/$D40C:ATDCY2/$$D413:ATDCY3
    int decay_rate = v & 0x0F; // ATDCY Bits 0-3: Select decay cycle duration (0-15)
    int attack_rate = v >> 4;  // ATDCY Bits 4-7: Select attack cycle duration (0-15)
    // Transform/lookup rates into envelop deltas, and put these in the lookup table.
    voice->envelope_deltas[CYCLE_DECAY] = voice_rate_to_delta(voice, decay_rate) / DECAY_RELEASE_DIVIDER;
    voice->envelope_deltas[CYCLE_ATTACK] = voice_rate_to_delta(voice, attack_rate);
    // TODO : Handle more side-effects in internal state?
}

void voice_write_sustain_release_register_value(voice_t* voice, uint8_t v) {
    // Decode input bits for $D406:SUREL1/$D40D:SUREL2/$$D414:SUREL3
    int release_rate = v & 0x0F;  // SUREL Bits 0-3: Select release cycle duration (0-15)
    int sustain_level = v & 0xF0; // SUREL Bits 4-7: Select sustain volume level (0-15)
    // Transform/lookup Release rate into an envelop delta, and put it in the lookup table.
    voice->envelope_deltas[CYCLE_RELEASE] = voice_rate_to_delta(voice, release_rate) / DECAY_RELEASE_DIVIDER;
    // Adjust SustainLevel to the same 16 bit range as EnvelopeAmplitude (turning 0xF0 into 0xFFFF).
    // Note : VICE does the same, albeit on an 8 bit value.
    voice->sustain_level = (uint16_t)((sustain_level << 8) | (sustain_level << 4) | sustain_level | (sustain_level >> 4));
    // TODO : Handle more side-effects in internal state?
}

int voice_rate_to_delta(voice_t* voice, int rate) {
    // rate:0-15
    // Calculate an accurate delta for given number of milliseconds, based on the duration of a single ClockCycle()
    // The resulting delta is the value that is added each cycle to EnvelopeAmplitude, so that after the selected number
    // of milliseconds, it changed by AmplitudePeak. TODO : Make this more accurate, how?
    return (int)(voice_cycles_per_millisecond(voice) / rates_in_ms[rate]); // cycles
}

int voice_cycles_per_millisecond(voice_t* voice) {
    return (int)(voice->cpu_clock / 1000.0f);
}

void voice_clock_cycle(voice_t* voice) {
    // TODO : Skip some # cycles? (Since output is only 4 Khz), or do that in MOS6581.ClockCycle()?

    // Transition the accumulator (source: VICE)
    uint32_t next_accumulator = (voice->waveform_accumulator + voice->frequency) & WAVEFORM_ACCUMULATOR_MAX;
    uint32_t bits_accumulator = ~voice->waveform_accumulator & next_accumulator;
    (void)bits_accumulator; // Note: Currently unused, matching C# structure
    voice->waveform_accumulator = next_accumulator;
    // Convert 24 bit (12.12 fixed point) WaveformAccumulator to into a 12 bit (8.4 fixed point)
    // OscillatorWaveform value, depending the selected WaveformBits.
    switch (voice->waveform) {
        case WAVEFORM_NONE:
            voice->oscillator_waveform = 0;
            break;
        // Note, that any form of fixed-point counting results in rounding errors (also, websid code comments mention sampling interval aliassing)
        case WAVEFORM_TRIANGLE:
            voice->oscillator_waveform = (uint32_t)(voice->waveform_accumulator ^ ((voice->waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) > 0 ? WAVEFORM_ACCUMULATOR_MAX : 0)) >> (12 - 1);
            break;
        case WAVEFORM_SAWTOOTH:
            voice->oscillator_waveform = voice->waveform_accumulator >> 12;
            break;
        case WAVEFORM_PULSE:
            voice->oscillator_waveform = (uint32_t)((voice->waveform_accumulator >> 8) > voice->pulse_waveform_width ? OSCILLATOR_MAX : 0);
            break;
        case WAVEFORM_NOISE:
            voice->oscillator_waveform = (uint32_t)(rand() % (OSCILLATOR_MAX + 1)); // TODO : Improve
            break;
        default:
            voice->oscillator_waveform = voice->waveform_accumulator >> 12; // TODO : "multiple enabled waveforms AND together"?
            break;
    }

    // Handle ADSR increases and transitions to next cycle, by fetching the active
    // envelopeDelta for the active EnvelopeCycle from the EnvelopeDeltas lookup table.
    // Note : Delta is positive for Attack, negative for Decay and Release, and zero for Sustain and Off cycles:
    int envelope_delta = voice->envelope_deltas[voice->envelope_cycle];
    // A positive delta is used only by the Attack cycle.
    if (envelope_delta > 0) {
        voice->envelope_amplitude += (uint16_t)envelope_delta;
        // Attack must not exceed the peak valud
        if (voice->envelope_amplitude >= AMPLITUDE_PEAK) {
            voice->envelope_amplitude = AMPLITUDE_PEAK;
            // Transition to the next cycle (Decay) and limit that to SustainLevel
            voice->envelope_cycle = CYCLE_DECAY;
            voice->envelope_next_level = voice->sustain_level;
        }
    }

    // Sustain and Off cycles have an envelopeDelta of zero, so won't change EnvelopeAmplitude (nor
    // EnvelopeCycle, which is set to Off or Attack or Release by WriteVoiceControlRegisterValue).

    // Handle negative delta on EnvelopeAmplitude, as done by Decay and Release.
    if (envelope_delta < 0) {
        voice->envelope_amplitude += (uint16_t)envelope_delta;
        // Don't go below SustainLevel (at the end of the Decay cycle), or zero (at the end of the Release cycle).
        if (voice->envelope_amplitude <= voice->envelope_next_level) {
            voice->envelope_amplitude = voice->envelope_next_level;
            // Transition from current to next cycle; From 1 (Decay) to 2 (Sustain), or from 3 (Release) to 4 (Off).
            // (This shift avoids the need to check for and avoid decreasing below zero)
            voice->envelope_cycle = (envelope_cycle_t)((int)voice->envelope_cycle + 1);
            // When this was hit when underflowing SustainLevel (and thus changing cycle Decay to Sustain),
            // the next level to limit underflow on is the zero level (at the end of the Release cycle).
            voice->envelope_next_level = 0;
            // Note, that this code won't be hit again after Release cycle transitions to Off,
            // because the Off 'cycle' uses an envelopeDelta of zero.
        }
    }

    // The resulting value for this voice is the combination between tone oscillator/
    // waveform generator output and (ADSR) envelope generator amplitude output.
    // Note, this combination is called "Amplitude Modulator" (so herhaps remove Amplitude from EnvelopeAmplitude name?)
    voice->result = (voice->oscillator_waveform * (uint32_t)voice->envelope_amplitude) / WAVEFORM_ACCUMULATOR_MAX / 8; // remove 8 bit fraction from EnvelopeAmplitude
    // TODO : Probably need to do above differently
}

void voice_reset(voice_t* voice) {
    voice->frequency = 0;
    voice->pulse_waveform_width = 0;
    voice->gated = false;
    voice->synchronize = false;
    voice->ring_modulation = false;
    voice->test = false;
    voice->waveform = WAVEFORM_NONE;

    voice->envelope_amplitude = 0xFE << 8;
    voice->oscillator_waveform = 0;
    voice->result = 0;

    voice->waveform_accumulator = 0x555555; // Vice: Accumulator's even bits are high on powerup
    voice->envelope_cycle = CYCLE_OFF;
    voice->envelope_next_level = 0;
    voice->sustain_level = 0;
    voice->envelope_deltas[CYCLE_ATTACK] = 0;
    voice->envelope_deltas[CYCLE_DECAY] = 0;
    voice->envelope_deltas[CYCLE_RELEASE] = 0;
}

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "mos6581_gui.c"
#endif
