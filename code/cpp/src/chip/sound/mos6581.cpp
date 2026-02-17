#include "mos6581.h"
#include "../../core/aiemuc.h"
#include <string.h>
#include <stdlib.h>
#include <math.h> // for tanhf
#include <stdio.h>  // For snprintf
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Envelope rate counter comparison values — reSID reference.
// These are the COMPARISON values. The actual period between envelope ticks
// is comparison_value + 1, because the counter counts 0,1,...,value then
// matches — that's (value + 1) cycles between ticks.
// E.g., rate 0 → compare=8, actual period = 9 cycles.
static const uint32_t envelope_rate_periods[16] = {
    8, 31, 62, 94, 148, 219, 266, 312, 391, 976, 1953, 3125, 3906, 11719, 19531, 31250
};

// No decay lookup table needed — real SID uses exponential counter periods.
// The envelope counter is 8-bit (0x00-0xFF).  During decay/release the
// exponential_counter is incremented each rate tick; an actual envelope
// decrement only happens when exponential_counter reaches the current
// exponential_counter_period.  The period changes at these thresholds:
//   envelope >= 0x5D (93) → period 1  (fast decay)
//   envelope >= 0x36 (54) → period 2
//   envelope >= 0x1A (26) → period 4
//   envelope >= 0x0E (14) → period 8
//   envelope >= 0x06 ( 6) → period 16
//   envelope >= 0x00 ( 0) → period 30 (slow decay)
// See reSID EnvelopeGenerator::clock().

// Noise output: reSID-accurate bit extraction from 23-bit LFSR.
// LFSR bits {20,18,14,11,9,5,2,0} map to output bits {11,10,9,8,7,6,5,4}.
// The lower 4 output bits are always zero (grounded on the die).

// Helper: update the exponential counter period based on current envelope level.
// These thresholds match the real SID hardware (reSID reference).
static void voice_update_exponential_period(voice_t* voice) {
    switch (voice->envelope_amplitude) {
        case 0xFF: voice->exponential_counter_period = 1; break;
        case 0x5D: voice->exponential_counter_period = 2; break;
        case 0x36: voice->exponential_counter_period = 4; break;
        case 0x1A: voice->exponential_counter_period = 8; break;
        case 0x0E: voice->exponential_counter_period = 16; break;
        case 0x06: voice->exponential_counter_period = 30; break;
        case 0x00:
            voice->exponential_counter_period = 1;
            voice->envelope_hold_zero = true;
            break;
        default: break; // Keep current period
    }
}

// =============================================================================
// VOICE INTERACTION (SYNC AND RING MODULATION)
// =============================================================================

void voice_apply_sync(voice_t* voice, voice_t* sync_source) {
    if (!voice || !sync_source || !voice->synchronize) return;
    
    // Sync resets accumulator when sync source MSB rises
    if (sync_source->sync_trigger) {
        voice->waveform_accumulator = 0;
    }
}

uint32_t voice_apply_ring_modulation(voice_t* voice, voice_t* ring_source) {
    if (!voice || !ring_source || !voice->ring_modulation) {
        return voice->oscillator_waveform;
    }
    
    // Ring modulation only affects triangle waveform
    if (voice->waveform & WAVEFORM_TRIANGLE) {
        bool ring_msb = (ring_source->waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0;
        
        if (ring_msb) {
            return voice->triangle_output ^ 0xFFF;
        }
    }
    
    return voice->oscillator_waveform;
}

// =============================================================================
// ENVELOPE GENERATION
// =============================================================================

uint32_t voice_rate_to_period(voice_t* voice, int rate) {
    if (!voice) return 0;
    
    // Convert rate (0-15) to period in cycles
    return envelope_rate_periods[rate & 0x0F];
}

void voice_envelope_clock(voice_t* voice) {
    if (!voice) return;
    
    // reSID-accurate 8-bit envelope generator.
    // Attack: envelope_counter increments by 1 each rate tick.
    //         Transitions to decay when reaching 0xFF.
    // Decay/Sustain: envelope_counter decrements by 1, gated by
    //         the exponential counter reaching its period.
    // Release: same decrement logic as decay.
    // hold_zero prevents any further changes once envelope reaches 0.
    
    switch (voice->envelope_cycle) {
        case CYCLE_OFF:
            break;
            
        case CYCLE_ATTACK:
            // Attack always increments (exponential counter not used)
            if (voice->envelope_hold_zero) break;
            voice->envelope_amplitude = (voice->envelope_amplitude + 1) & 0xFF;
            if (voice->envelope_amplitude == 0xFF) {
                voice->envelope_cycle = CYCLE_DECAY;
                voice->envelope_rate_period = voice_rate_to_period(voice, voice->decay_rate);
            }
            break;
            
        case CYCLE_DECAY:
            // reSID: Combined DECAY_SUSTAIN state (no separate sustain).
            // Each rate tick gated by the exponential counter, if envelope !=
            // sustain_level, decrement. Otherwise hold. If sustain is changed
            // while in this state, decay resumes automatically.
            if (voice->envelope_hold_zero) break;
            if (++voice->exponential_counter >= voice->exponential_counter_period) {
                voice->exponential_counter = 0;
                if (voice->envelope_amplitude != voice->sustain_level) {
                    voice->envelope_amplitude = (voice->envelope_amplitude - 1) & 0xFF;
                }
            }
            voice_update_exponential_period(voice);
            // Note: do NOT transition to CYCLE_SUSTAIN. Stay in CYCLE_DECAY
            // so that sustain level changes cause decay to resume.
            break;
            
        case CYCLE_SUSTAIN:
            // Legacy state — kept for save-state compatibility but should not
            // be entered by new code paths. Behaves like CYCLE_DECAY.
            if (voice->envelope_hold_zero) break;
            if (voice->envelope_amplitude != voice->sustain_level) {
                // Sustain level was changed — resume decay behavior
                voice->envelope_cycle = CYCLE_DECAY;
            }
            break;
            
        case CYCLE_RELEASE:
            if (voice->envelope_hold_zero) break;
            if (++voice->exponential_counter >= voice->exponential_counter_period) {
                voice->exponential_counter = 0;
                voice->envelope_amplitude = (voice->envelope_amplitude - 1) & 0xFF;
            }
            voice_update_exponential_period(voice);
            break;
    }
}

void voice_update_envelope(voice_t* voice) {
    if (!voice) return;
    
    // reSID-accurate 15-bit rate counter.
    // The rate counter increments each cycle. When it matches the rate period,
    // the envelope is clocked. The counter is 15-bit and wraps at 0x8000.
    // This wrapping behavior is the root of the "ADSR bug": if the rate period
    // changes to a value below the current counter, the counter must wrap all
    // the way around before the next envelope step, causing a long delay.
    // reSID reference: envelope.h — rate_counter is 15-bit, uses != comparison.
    if (voice->envelope_rate_counter != voice->envelope_rate_period) {
        // Increment with 15-bit wrapping (counter skips 0 on wrap)
        if (++voice->envelope_rate_counter & 0x8000) {
            voice->envelope_rate_counter = 1; // Wrap: 0x7FFF → 0x0001 (skip 0)
        }
        return;
    }
    
    // Rate counter matched — reset to 0 and clock the envelope
    voice->envelope_rate_counter = 0;
    voice_envelope_clock(voice);
}

// =============================================================================
// FILTER IMPLEMENTATION
// =============================================================================

void mos6581_filter_update_cutoff(mos6581_t* sid) {
    if (!sid) return;
    
    filter_state_t* f = &sid->filter_state;
    
    // Calculate cutoff frequency in Hz from the 11-bit register (0-2047).
    float fc = (float)sid->filter_cutoff_frequency;
    float normalized = fc / FILTER_CUTOFF_MAX;  // 0.0 .. 1.0
    float cutoff_hz;
    
    if (sid->revision <= SID_REVISION_6581_R4AR) {
        // 6581: roughly 220 Hz to ~12 kHz (non-linear / quadratic)
        cutoff_hz = 220.0f + normalized * normalized * 11780.0f;
    } else {
        // 8580: roughly 30 Hz to ~12.5 kHz (more linear)
        cutoff_hz = 30.0f + normalized * 12470.0f;
    }
    
    f->cutoff_frequency = cutoff_hz;
    
    // w0 is based on the sample rate since the filter is processed once
    // per output sample (~44.1 kHz), not every CPU cycle.  This gives a
    // ~22x performance improvement over per-cycle filter processing while
    // maintaining good audio quality.
    float rate = sid->sample_rate > 0.0f ? sid->sample_rate : 44100.0f;
    f->w0 = (float)(2.0f * M_PI * cutoff_hz / rate);
    
    // Clamp for SVF stability (must be well below 1.0 at sample rate).
    // At 44.1 kHz, max cutoff ~12 kHz gives w0 ≈ 1.71 unclamped —
    // we cap at 0.45 for safe operation with high resonance.
    if (f->w0 > 0.45f) f->w0 = 0.45f;
}

void mos6581_filter_update_resonance(mos6581_t* sid) {
    if (!sid) return;
    
    filter_state_t* f = &sid->filter_state;
    
    // Calculate resonance damping coefficient (1/Q) for the SVF.
    // In the SVF the feedback term is: hp = input - bp*(1/Q) - lp
    // Higher resonance → lower damping → higher Q → more resonant peak.
    //
    // Real SID 6581: resonance ranges from minimal (res=0) to near
    // self-oscillation (res=15).  We model 1/Q linearly from ~1.7 down to 0.
    // (reSID uses measured per-chip curves, but this is a good approximation.)
    f->resonance = (float)sid->filter_resonance;
    
    // 1/Q: ranges from 1.7 at res=0 (low Q ≈ 0.59) to ~0.0 at res=15 (self-oscillation)
    float res_norm = f->resonance / FILTER_RESONANCE_MAX; // 0..1
    f->q = 1.7f * (1.0f - res_norm);  // This IS 1/Q (damping), not Q itself
    
    // Ensure a tiny minimum to prevent NaN/explosion at max resonance
    if (f->q < 0.01f) f->q = 0.01f;
}

float mos6581_filter_process(mos6581_t* sid, float input) {
    if (!sid || !sid->enable_filter) return input;
    
    filter_state_t* f = &sid->filter_state;
    
    // Two-integrator-loop state variable filter (SVF)
    //   hp = input - bp * (1/Q) - lp
    //   bp += w0 * hp
    //   lp += w0 * bp
    // f->q already stores 1/Q (damping coefficient).
    float hp = input - f->integrator1 * f->q - f->integrator2;
    float bp = f->integrator1 + hp * f->w0;
    float lp = f->integrator2 + f->integrator1 * f->w0;  // use OLD bp for lp update
    
    // Update integrators
    f->integrator1 = bp;
    f->integrator2 = lp;
    
    // Apply distortion for 6581
    if (f->enable_distortion && sid->revision <= SID_REVISION_6581_R4AR) {
        float distortion_amount = f->resonance / FILTER_RESONANCE_MAX * 0.5f;
        lp = tanhf(lp * distortion_amount) / distortion_amount;
        bp = tanhf(bp * distortion_amount) / distortion_amount;
        hp = tanhf(hp * distortion_amount) / distortion_amount;
    }
    
    // Store outputs
    f->low_pass_output = lp;
    f->band_pass_output = bp;
    f->high_pass_output = hp;
    
    // Mix filter outputs
    float output = 0.0f;
    if (sid->low_pass_enabled) output += lp;
    if (sid->band_pass_enabled) output += bp;
    if (sid->high_pass_enabled) output += hp;
    
    return output;
}

void mos6581_filter_reset(mos6581_t* sid) {
    if (!sid) return;
    
    filter_state_t* f = &sid->filter_state;
    
    f->cutoff_frequency = 0.0f;
    f->resonance = 0.0f;
    f->low_pass_output = 0.0f;
    f->band_pass_output = 0.0f;
    f->high_pass_output = 0.0f;
    f->previous_input = 0.0f;
    f->previous_low_pass = 0.0f;
    f->previous_band_pass = 0.0f;
    f->w0 = 0.0f;
    f->q = 0.0f;
    f->integrator1 = 0.0f;
    f->integrator2 = 0.0f;
    f->distortion_level = 0.0f;
    f->enable_distortion = (sid->revision <= SID_REVISION_6581_R4AR);
}

void mos6581_filter_init(mos6581_t* sid) {
    if (!sid) return;
    
    mos6581_filter_reset(sid);
}

// =============================================================================
// ADVANCED FILTER MODELING
// =============================================================================

void mos6581_filter_set_model(mos6581_t* sid, bool use_nonlinear_model) {
    if (!sid) return;
    
    sid->filter_state.enable_distortion = use_nonlinear_model && 
                                         (sid->revision <= SID_REVISION_6581_R4AR);
}

float mos6581_filter_get_cutoff_hz(mos6581_t* sid) {
    if (!sid) return 0.0f;
    
    // cutoff_frequency is now stored directly as Hz
    return sid->filter_state.cutoff_frequency;
}

float mos6581_filter_get_resonance_q(mos6581_t* sid) {
    if (!sid) return 0.0f;
    
    return sid->filter_state.q;
}

// =============================================================================
// FILTER REGISTER WRITERS
// =============================================================================

void mos6581_write_resonance_control_register_value(mos6581_t* sid, uint8_t value) {
    if (!sid) return;
    
    sid->filter_voice1 = (value & 0x01) != 0;
    sid->filter_voice2 = (value & 0x02) != 0;
    sid->filter_voice3 = (value & 0x04) != 0;
    sid->filter_voice4 = (value & 0x08) != 0;
    sid->filter_resonance = (value >> 4) & 0x0F;
    
    mos6581_filter_update_resonance(sid);
}

void mos6581_write_volume_and_filter_select_register_value(mos6581_t* sid, uint8_t value) {
    if (!sid) return;
    
    sid->volume = value & 0x0F;
    sid->low_pass_enabled = (value & 0x10) != 0;
    sid->band_pass_enabled = (value & 0x20) != 0;
    sid->high_pass_enabled = (value & 0x40) != 0;
    sid->voice3_disabled = (value & 0x80) != 0;
}

// =============================================================================
// VOICE MIXING AND OUTPUT
// =============================================================================

uint32_t mos6581_mix_voices(mos6581_t* sid) {
    if (!sid) return 0;
    
    float unfiltered_output = 0.0f;
    float filtered_input = 0.0f;
    
    // Process sync and ring modulation
    voice_apply_sync(&sid->voice1, &sid->voice3);
    voice_apply_sync(&sid->voice2, &sid->voice1);
    voice_apply_sync(&sid->voice3, &sid->voice2);
    
    // Apply ring modulation (affects oscillator output, before envelope)
    uint32_t voice1_output = voice_apply_ring_modulation(&sid->voice1, &sid->voice3);
    uint32_t voice2_output = voice_apply_ring_modulation(&sid->voice2, &sid->voice1);
    uint32_t voice3_output = voice_apply_ring_modulation(&sid->voice3, &sid->voice2);
    
    // Center waveform BEFORE envelope so silent voices produce zero.
    // 12-bit waveform centered: -2048..+2047, × 8-bit envelope 0..255
    const float inv_scale = 1.0f / 522240.0f;
    float v1 = (float)(((int32_t)voice1_output - 2048) * (int32_t)sid->voice1.envelope_amplitude) * inv_scale;
    float v2 = (float)(((int32_t)voice2_output - 2048) * (int32_t)sid->voice2.envelope_amplitude) * inv_scale;
    float v3 = (float)(((int32_t)voice3_output - 2048) * (int32_t)sid->voice3.envelope_amplitude) * inv_scale;
    
    // Route Voice 1
    if (sid->filter_voice1) {
        filtered_input += v1;
    } else {
        unfiltered_output += v1;
    }
    
    // Route Voice 2
    if (sid->filter_voice2) {
        filtered_input += v2;
    } else {
        unfiltered_output += v2;
    }
    
    // Route Voice 3
    // Real SID: voice3_disabled suppresses Voice 3 from the UNFILTERED path only.
    // If Voice 3 is routed through the filter, it always passes regardless of
    // voice3_disabled.  If Voice 3 is NOT filtered AND voice3_disabled is set,
    // it is completely muted.
    if (sid->filter_voice3) {
        filtered_input += v3;
    } else if (!sid->voice3_disabled) {
        unfiltered_output += v3;
    }
    
    // Add external input
    if (sid->filter_voice4) {
        filtered_input += sid->external_input;
    } else {
        unfiltered_output += sid->external_input;
    }
    
    // Apply filter to the summed filtered voices (no averaging!)
    float filtered_output = mos6581_filter_process(sid, filtered_input);
    
    // Sum filtered + unfiltered
    float mixed_output = unfiltered_output + filtered_output;
    
    // Apply volume
    mixed_output *= (float)sid->volume / 15.0f;
    
    // Apply volume bug click (6581 only)
    if (sid->volume_change_click && sid->volume_click_counter > 0) {
        mixed_output += sid->volume_click_amplitude;
        sid->volume_click_counter--;
        if (sid->volume_click_counter == 0) {
            sid->volume_change_click = false;
        }
    }
    
    // Digital boost for 4-bit sample playback
    if (sid->enable_digiboost && sid->volume_change_click) {
        mixed_output *= 4.0f;
    }
    
    // Clamp output
    if (mixed_output > 1.0f) mixed_output = 1.0f;
    if (mixed_output < -1.0f) mixed_output = -1.0f;
    
    return (uint32_t)(mixed_output * 32767.0f + 32768.0f);
}

// =============================================================================
// RING BUFFER IMPLEMENTATION
// =============================================================================

void ring_buffer_init(ring_buffer_t* rb, uint32_t size) {
    if (!rb) return;
    
    // Round size up to next power of 2
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
    
    rb->size = size;
    rb->mask = size - 1;
    rb->buffer = (float*)calloc(size, sizeof(float));
    rb->write_pos = 0;
    rb->read_pos = 0;
}

void ring_buffer_destroy(ring_buffer_t* rb) {
    if (!rb) return;
    
    free(rb->buffer);
    rb->buffer = NULL;
    rb->size = 0;
    rb->mask = 0;
    rb->write_pos = 0;
    rb->read_pos = 0;
}

void ring_buffer_write(ring_buffer_t* rb, float sample) {
    if (!rb || !rb->buffer) return;

    // SPSC safety: only the writer touches write_pos, only the reader touches read_pos.
    // Read read_pos once into a local (volatile ensures we get the latest value).
    uint32_t next = (rb->write_pos + 1) & rb->mask;
    uint32_t rp = rb->read_pos;  // snapshot reader's position
    if (next == rp) {
        // Buffer full — drop this sample rather than touching read_pos
        // (read_pos belongs to the consumer thread).
        return;
    }

    rb->buffer[rb->write_pos] = sample;
    rb->write_pos = next;  // publish (volatile store)
}

bool ring_buffer_empty(ring_buffer_t* rb) {
    if (!rb) return true;
    // Volatile reads ensure we see the latest positions from both threads
    return rb->read_pos == rb->write_pos;
}

float ring_buffer_read(ring_buffer_t* rb) {
    if (!rb || !rb->buffer) return 0.0f;

    // Snapshot write_pos once (volatile load) to avoid TOCTOU with ring_buffer_empty
    uint32_t wp = rb->write_pos;
    uint32_t rp = rb->read_pos;
    if (rp == wp) return 0.0f;  // empty

    float sample = rb->buffer[rp];
    rb->read_pos = (rp + 1) & rb->mask;  // publish (volatile store)
    return sample;
}

uint32_t ring_buffer_available(ring_buffer_t* rb) {
    if (!rb) return 0;
    // Snapshot both positions (volatile loads) for consistent calculation
    uint32_t wp = rb->write_pos;
    uint32_t rp = rb->read_pos;
    return (wp - rp) & rb->mask;
}

// =============================================================================
// COMBINED WAVEFORM INITIALIZATION
// =============================================================================

void mos6581_init_combined_waveforms(mos6581_t* sid) {
    if (!sid) return;
    
    // Allocate combined waveform table
    sid->combined_waveform_table = (uint8_t*)malloc(COMBINED_WAVEFORM_TABLE_SIZE);
    if (!sid->combined_waveform_table) {
        sid->combined_waveform_enabled = false;
        return;
    }
    
    // Initialize with simple AND behavior
    for (uint32_t i = 0; i < COMBINED_WAVEFORM_TABLE_SIZE; i++) {
        sid->combined_waveform_table[i] = 255; // Default to no attenuation
    }
    
    sid->combined_waveform_enabled = true;
}

// =============================================================================
// ADDITIONAL HELPER FUNCTIONS
// =============================================================================

float mos6581_interpolate_sample(mos6581_t* sid, float position) {
    if (!sid) return 0.0f;
    
    // Linear interpolation between samples
    uint32_t index = (uint32_t)position;
    float fraction = position - (float)index;
    
    if (ring_buffer_available(&sid->sample_buffer) < 2) {
        return 0.0f;
    }
    
    // Get two consecutive samples
    float sample1 = ring_buffer_read(&sid->sample_buffer);
    float sample2 = ring_buffer_read(&sid->sample_buffer);
    
    // Put second sample back
    ring_buffer_write(&sid->sample_buffer, sample2);
    
    return sample1 + fraction * (sample2 - sample1);
}

// =============================================================================
// FREQUENCY CALCULATION HELPERS
// =============================================================================

uint16_t mos6581_frequency_to_sid_value(float frequency_hz, bool pal_timing) {
    // Convert frequency in Hz to SID register value
    float clock_freq = pal_timing ? 985248.0f : 1022727.0f;
    float sid_value = frequency_hz * 16777216.0f / clock_freq;
    
    if (sid_value > 65535.0f) sid_value = 65535.0f;
    if (sid_value < 0.0f) sid_value = 0.0f;
    
    return (uint16_t)sid_value;
}

float mos6581_sid_value_to_frequency(uint16_t sid_value, bool pal_timing) {
    // Convert SID register value to frequency in Hz
    float clock_freq = pal_timing ? 985248.0f : 1022727.0f;
    return ((float)sid_value * clock_freq) / 16777216.0f;
}

// =============================================================================
// WAVEFORM GENERATION
// =============================================================================

uint32_t voice_generate_triangle(voice_t* voice) {
    if (!voice) return 0;
    
    uint32_t accumulator = voice->waveform_accumulator;
    
    // Triangle wave: sawtooth XOR with MSB to flip second half
    if (accumulator & WAVEFORM_ACCUMULATOR_MSB) {
        return (accumulator ^ WAVEFORM_ACCUMULATOR_MAX) >> 11;
    } else {
        return accumulator >> 11;
    }
}

uint32_t voice_generate_sawtooth(voice_t* voice) {
    if (!voice) return 0;
    
    // Sawtooth wave: upper 12 bits of accumulator
    return voice->waveform_accumulator >> 12;
}

uint32_t voice_generate_pulse(voice_t* voice) {
    if (!voice) return 0;
    
    // Pulse wave: compare upper 12 bits with pulse width
    uint32_t pulse_threshold = voice->pulse_waveform_width;
    uint32_t accumulator_12bit = voice->waveform_accumulator >> 12;
    
    return (accumulator_12bit >= pulse_threshold) ? 0xFFF : 0;
}

uint32_t voice_generate_noise(voice_t* voice) {
    if (!voice) return 0;
    
    // Clock noise LFSR based on accumulator bit 19
    bool clock_noise = (voice->waveform_accumulator & 0x080000) != 0;
    
    if (clock_noise && !voice->noise_clock_enable) {
        // 23-bit LFSR with feedback taps at bits 22 and 17
        uint32_t feedback = ((voice->noise_lfsr >> 22) ^ (voice->noise_lfsr >> 17)) & 1;
        voice->noise_lfsr = ((voice->noise_lfsr << 1) | feedback) & NOISE_LFSR_MASK;
    }
    
    voice->noise_clock_enable = clock_noise;
    
    // Extract noise output from LFSR using reSID-accurate bit positions.
    // LFSR bits {20,18,14,11,9,5,2,0} → output bits {11,10,9,8,7,6,5,4}.
    // Lower 4 output bits are grounded (always zero).
    uint32_t sr = voice->noise_lfsr;
    uint32_t noise_output =
        ((sr & 0x100000) >> 9)  |  // LFSR bit 20 → output bit 11
        ((sr & 0x040000) >> 8)  |  // LFSR bit 18 → output bit 10
        ((sr & 0x004000) >> 5)  |  // LFSR bit 14 → output bit  9
        ((sr & 0x000800) >> 3)  |  // LFSR bit 11 → output bit  8
        ((sr & 0x000200) >> 2)  |  // LFSR bit  9 → output bit  7
        ((sr & 0x000020) << 1)  |  // LFSR bit  5 → output bit  6
        ((sr & 0x000004) << 3)  |  // LFSR bit  2 → output bit  5
        ((sr & 0x000001) << 4);    // LFSR bit  0 → output bit  4
    
    return noise_output;
}

uint32_t voice_generate_combined_waveform(voice_t* voice) {
    if (!voice || !voice->sid) return 0;
    
    // Combined waveforms use lookup table or simple AND operation
    uint32_t waveform_bits = voice->waveform;
    uint32_t output = 0xFFF; // Start with all bits set
    
    if (waveform_bits & WAVEFORM_TRIANGLE) {
        output &= voice->triangle_output;
    }
    if (waveform_bits & WAVEFORM_SAWTOOTH) {
        output &= voice->sawtooth_output;
    }
    if (waveform_bits & WAVEFORM_PULSE) {
        output &= voice->pulse_output;
    }
    if (waveform_bits & WAVEFORM_NOISE) {
        output &= voice->noise_output;
    }
    
    // Apply combined waveform table if available
    if (voice->sid->combined_waveform_enabled && voice->sid->combined_waveform_table) {
        uint32_t table_index = (voice->waveform_accumulator >> 12) & (COMBINED_WAVEFORM_TABLE_SIZE - 1);
        uint32_t table_value = voice->sid->combined_waveform_table[table_index];
        output = (output * table_value) >> 8;
    }
    
    return output;
}

// =============================================================================
// NOISE WAVEFORM ANALYSIS HELPERS
// =============================================================================

void mos6581_analyze_noise_period(voice_t* voice, uint32_t* period_length, uint32_t* unique_values) {
    if (!voice || !period_length || !unique_values) return;
    
    // Analyze noise waveform period and unique values
    uint32_t initial_lfsr = voice->noise_lfsr;
    uint32_t count = 0;
    bool unique_found[8192] = {false}; // Track unique 13-bit values
    uint32_t unique_count = 0;
    
    do {
        // Generate noise sample
        uint32_t noise_sample = voice_generate_noise(voice);
        uint32_t noise_13bit = noise_sample & 0x1FFF;
        
        if (!unique_found[noise_13bit]) {
            unique_found[noise_13bit] = true;
            unique_count++;
        }
        
        count++;
        
        // Prevent infinite loop
        if (count > 8388607) break;
        
    } while (voice->noise_lfsr != initial_lfsr);
    
    *period_length = count;
    *unique_values = unique_count;
}

// =============================================================================
// VOICE INTERACTION HELPERS
// =============================================================================

void mos6581_setup_voice_routing(mos6581_t* sid) {
    if (!sid) return;
    
    // Voice 1 syncs to Voice 3, ring mods with Voice 3
    // Voice 2 syncs to Voice 1, ring mods with Voice 1  
    // Voice 3 syncs to Voice 2, ring mods with Voice 2
    
    // This is handled in the cycle update function
}

bool mos6581_voice_is_audible(voice_t* voice) {
    if (!voice) return false;
    
    return voice->gated && 
           voice->envelope_amplitude > 0 && 
           voice->waveform != WAVEFORM_NONE &&
           !voice->test;
}

// =============================================================================
// DEBUGGING AND ANALYSIS FUNCTIONS
// =============================================================================

void mos6581_get_voice_state(voice_t* voice, char* buffer, size_t buffer_size) {
    if (!voice || !buffer) return;
    
    snprintf(buffer, buffer_size,
        "Voice %d: Freq=%04X PW=%03X Wave=%02X Gate=%d Env=%04X Acc=%06X",
        voice->voice_index,
        voice->frequency,
        voice->pulse_waveform_width,
        voice->waveform,
        voice->gated ? 1 : 0,
        voice->envelope_amplitude,
        voice->waveform_accumulator
    );
}

void mos6581_get_filter_state(mos6581_t* sid, char* buffer, size_t buffer_size) {
    if (!sid || !buffer) return;
    
    snprintf(buffer, buffer_size,
        "Filter: Cutoff=%04X Res=%X LP=%d BP=%d HP=%d V1=%d V2=%d V3=%d",
        sid->filter_cutoff_frequency,
        sid->filter_resonance,
        sid->low_pass_enabled ? 1 : 0,
        sid->band_pass_enabled ? 1 : 0,
        sid->high_pass_enabled ? 1 : 0,
        sid->filter_voice1 ? 1 : 0,
        sid->filter_voice2 ? 1 : 0,
        sid->filter_voice3 ? 1 : 0
    );
}

uint32_t mos6581_get_total_cycles(mos6581_t* sid) {
    return sid ? sid->total_cycles : 0;
}

uint32_t mos6581_get_samples_generated(mos6581_t* sid) {
    return sid ? sid->samples_generated : 0;
}

// =============================================================================
// VOICE FUNCTIONS
// =============================================================================

void voice_reset(voice_t* voice) {
    if (!voice) return;
    
    // Reset register values
    voice->frequency = 0;
    voice->pulse_waveform_width = 0;
    voice->waveform = WAVEFORM_NONE;
    voice->gated = false;
    voice->synchronize = false;
    voice->ring_modulation = false;
    voice->test = false;
    voice->attack_rate = 0;
    voice->decay_rate = 0;
    voice->sustain_level = 0;
    voice->release_rate = 0;
    
    // Reset internal state
    voice->waveform_accumulator = 0x555555; // VICE: Even bits high on powerup
    voice->envelope_accumulator = 0;
    voice->envelope_cycle = CYCLE_OFF;
    voice->envelope_amplitude = 0;
    voice->envelope_rate_counter = 0;
    voice->envelope_rate_period = 0;
    voice->envelope_hold_zero = false;
    voice->exponential_counter = 0;
    voice->exponential_counter_period = 1;
    
    // Reset waveform outputs
    voice->oscillator_waveform = 0;
    voice->triangle_output = 0;
    voice->sawtooth_output = 0;
    voice->pulse_output = 0;
    voice->combined_output = 0;
    
    // Reset noise state — reSID: shift_register = 0x7FFFFE after reset
    voice->noise_lfsr = 0x7FFFFE; // reSID reference: reset value
    voice->noise_output = 0;
    voice->noise_clock_enable = false;
    
    // Reset sync and ring modulation
    voice->prev_accumulator = 0;
    voice->sync_trigger = false;
    voice->ring_msb = false;
    
    // Reset outputs
    voice->oscillator_output = 0;
    voice->envelope_output = 0;
    voice->result = 0;
}

void voice_clock_cycle(voice_t* voice) {
    if (!voice) return;
    
    // Store previous accumulator for sync detection
    voice->prev_accumulator = voice->waveform_accumulator;
    
    // Update accumulator unless test bit is set
    if (!voice->test) {
        voice->waveform_accumulator = (voice->waveform_accumulator + voice->frequency) & WAVEFORM_ACCUMULATOR_MAX;
    } else {
        // Test bit locks accumulator and resets noise LFSR.
        // reSID: test bit gradually fades all LFSR bits to 1 (0x7FFFFF)
        // over ~35000 cycles (6581). We approximate this as instant.
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = 0x7FFFFF;
    }
    
    // Detect MSB change for sync
    bool msb_rising = ((voice->prev_accumulator & WAVEFORM_ACCUMULATOR_MSB) == 0) && 
                      ((voice->waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0);
    voice->sync_trigger = msb_rising;
    
    // Generate ONLY the waveform(s) that are actually selected.
    // This avoids ~3 redundant waveform computations per voice per cycle.
    uint32_t waveform_output = 0;
    uint8_t wf = voice->waveform;
    
    if (wf == 0) {
        // No waveform selected
        waveform_output = 0;
    } else if ((wf & (wf - 1)) == 0) {
        // Single waveform (power of 2) — generate only what's needed
        switch (wf) {
            case WAVEFORM_TRIANGLE:
                waveform_output = voice_generate_triangle(voice);
                voice->triangle_output = waveform_output;
                break;
            case WAVEFORM_SAWTOOTH:
                waveform_output = voice_generate_sawtooth(voice);
                voice->sawtooth_output = waveform_output;
                break;
            case WAVEFORM_PULSE:
                waveform_output = voice_generate_pulse(voice);
                voice->pulse_output = waveform_output;
                break;
            case WAVEFORM_NOISE:
                waveform_output = voice_generate_noise(voice);
                voice->noise_output = waveform_output;
                break;
            default:
                waveform_output = 0;
                break;
        }
    } else {
        // Combined waveform — generate all needed components
        if (wf & WAVEFORM_TRIANGLE) voice->triangle_output = voice_generate_triangle(voice);
        if (wf & WAVEFORM_SAWTOOTH) voice->sawtooth_output = voice_generate_sawtooth(voice);
        if (wf & WAVEFORM_PULSE)    voice->pulse_output = voice_generate_pulse(voice);
        if (wf & WAVEFORM_NOISE)    voice->noise_output = voice_generate_noise(voice);
        waveform_output = voice_generate_combined_waveform(voice);
    }
    
    voice->oscillator_waveform = waveform_output;
    voice->oscillator_output = (uint8_t)(waveform_output >> 4);
    
    // Update envelope
    voice_update_envelope(voice);
    
    // Apply envelope to waveform
    voice->result = (voice->oscillator_waveform * voice->envelope_amplitude) >> 8;
    
    // Update envelope output register (now 8-bit, direct)
    voice->envelope_output = voice->envelope_amplitude;
}

// =============================================================================
// MAIN CYCLE FUNCTION
// =============================================================================

// Unified bus state threading main cycle function
bus_state_t mos6581_advance_cycle(mos6581_t* sid, bus_state_t bus_state) {
    if (!sid) return bus_state;

    // Clock all three voices every CPU cycle (like real hardware)
    for (int i = 0; i < 3; i++) {
        voice_clock_cycle(sid->voices[i]);
    }

    // Apply oscillator sync (one-cycle-delayed, matching real hardware).
    // Sync source's MSB transition was detected inside voice_clock_cycle;
    // the accumulator reset takes effect next cycle.
    voice_apply_sync(&sid->voice1, &sid->voice3);
    voice_apply_sync(&sid->voice2, &sid->voice1);
    voice_apply_sync(&sid->voice3, &sid->voice2);

    // Generate output samples at the target sample rate (~44.1 kHz).
    // Voice mixing and SVF filter processing happen here — NOT every cycle.
    // This is a ~22x reduction in filter work vs per-cycle processing,
    // which is critical for maintaining 50 fps at ~1 MHz emulation speed.
    if (sid->cpu_clock > 0.0f) {
        sid->sample_accumulator += (double)sid->sample_rate / (double)sid->cpu_clock;

        if (sid->sample_accumulator >= 1.0) {
            sid->sample_accumulator -= 1.0;

            // --- Voice mixing ---
            // Apply ring modulation to get final waveform outputs
            uint32_t v1_wave = voice_apply_ring_modulation(&sid->voice1, &sid->voice3);
            uint32_t v2_wave = voice_apply_ring_modulation(&sid->voice2, &sid->voice1);
            uint32_t v3_wave = voice_apply_ring_modulation(&sid->voice3, &sid->voice2);

            // Center waveform BEFORE envelope so silent voices produce zero.
            // 12-bit waveform centered: -2048..+2047
            // × 8-bit envelope 0..255 → signed product -522240..+521985
            // Normalise so full-scale ≈ ±1.
            const float inv_scale = 1.0f / 522240.0f;
            float v1 = (float)(((int32_t)v1_wave - 2048) * (int32_t)sid->voice1.envelope_amplitude) * inv_scale;
            float v2 = (float)(((int32_t)v2_wave - 2048) * (int32_t)sid->voice2.envelope_amplitude) * inv_scale;
            float v3 = (float)(((int32_t)v3_wave - 2048) * (int32_t)sid->voice3.envelope_amplitude) * inv_scale;

            float filtered_input = 0.0f;
            float unfiltered_output = 0.0f;

            // Route voices to filter or direct output
            if (sid->filter_voice1) filtered_input += v1; else unfiltered_output += v1;
            if (sid->filter_voice2) filtered_input += v2; else unfiltered_output += v2;
            if (sid->filter_voice3) filtered_input += v3;
            else if (!sid->voice3_disabled) unfiltered_output += v3;
            if (sid->filter_voice4) filtered_input += sid->external_input;
            else unfiltered_output += sid->external_input;

            // Apply SVF filter (at sample rate, not per-cycle)
            float filtered_output = mos6581_filter_process(sid, filtered_input);

            // Sum filtered + unfiltered
            float mixed = unfiltered_output + filtered_output;

            // 6581 digi support: add constant DC bias from the voice DACs.
            // In real hardware, this residual bias is always present and
            // gets modulated by volume register changes to produce 4-bit
            // sample playback.  The bias is a fixed analog property —
            // independent of waveform or envelope state.
            if (sid->revision <= SID_REVISION_6581_R4AR) {
                mixed += 0.38f;
            }

            // Apply master volume
            mixed *= (float)sid->volume / 15.0f;

            // DC blocker: removes the constant bias×volume product while
            // preserving fast changes (digi samples).  ~20 Hz high-pass.
            //   y[n] = x[n] - x[n-1] + α · y[n-1],  α = 0.997
            {
                float dc_out = mixed - sid->dc_blocker_prev_in
                             + 0.997f * sid->dc_blocker_prev_out;
                sid->dc_blocker_prev_in = mixed;
                sid->dc_blocker_prev_out = dc_out;
                mixed = dc_out;
            }

            // Clamp to [-1, 1]
            if (mixed > 1.0f) mixed = 1.0f;
            if (mixed < -1.0f) mixed = -1.0f;

            ring_buffer_write(&sid->sample_buffer, mixed);
            sid->samples_generated++;
        }
    }

    sid->cycle_count++;
    sid->total_cycles++;

    return bus_state;
}

// =============================================================================
// SAMPLE GENERATION
// =============================================================================

void mos6581_generate_samples(mos6581_t* sid, float* output, uint32_t sample_count) {
    if (!sid || !output) return;
    
    for (uint32_t i = 0; i < sample_count; i++) {
        if (!ring_buffer_empty(&sid->sample_buffer)) {
            output[i] = ring_buffer_read(&sid->sample_buffer);
        } else {
            output[i] = 0.0f;
        }
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void mos6581_set_revision(mos6581_t* sid, sid_revision_t revision) {
    if (!sid) return;
    
    sid->revision = revision;
    sid->enable_distortion = (revision <= SID_REVISION_6581_R4AR);
    mos6581_filter_reset(sid);
}

void mos6581_set_timing(mos6581_t* sid, bool pal_timing) {
    if (!sid) return;
    
    sid->pal_timing = pal_timing;
    sid->sid_rate = sid->cpu_clock / (pal_timing ? 18.0f : 17.0f);
    mos6581_filter_update_cutoff(sid);
}

void mos6581_set_sample_rate(mos6581_t* sid, float sample_rate) {
    if (!sid) return;
    
    sid->sample_rate = sample_rate;
    // Update filter coefficient since w0 depends on sample rate
    mos6581_filter_update_cutoff(sid);
}

void mos6581_set_cpu_clock(mos6581_t* sid, float clock_hz) {
    if (!sid) return;
    
    sid->cpu_clock = clock_hz;
    for (int i = 0; i < 3; i++) {
        sid->voices[i]->cpu_clock = clock_hz;
    }
    // Update derived timing values
    sid->sid_rate = clock_hz / (sid->pal_timing ? 18.0f : 17.0f);
    mos6581_filter_update_cutoff(sid);
}

int voice_cycles_per_millisecond(voice_t* voice) {
    if (!voice) return 0;
    return (int)(voice->cpu_clock / 1000.0f);
}

// =============================================================================
// ENVELOPE TIMING ANALYSIS
// =============================================================================

uint32_t mos6581_calculate_envelope_time_ms(voice_t* voice, envelope_cycle_t cycle, uint8_t rate) {
    if (!voice) return 0;
    
    uint32_t period = voice_rate_to_period(voice, rate);
    uint32_t cycles_per_ms = voice_cycles_per_millisecond(voice);
    
    switch (cycle) {
        case CYCLE_ATTACK:
            // Attack is linear: 0 to 255 in period cycles per step
            return (255 * period) / cycles_per_ms;
            
        case CYCLE_DECAY:
        case CYCLE_RELEASE:
            // Decay/Release is exponential - approximate time to reach 1/e
            return (uint32_t)(period * 255.0f / cycles_per_ms);
            
        default:
            return 0;
    }
}

// =============================================================================
// VOICE REGISTER WRITERS
// =============================================================================

void voice_write_pulse_waveform_width(voice_t* voice, uint16_t value) {
    if (!voice) return;
    
    voice->pulse_waveform_width = value & PULSE_WIDTH_MAX;
}

void voice_write_voice_control_register_value(voice_t* voice, uint8_t value) {
    if (!voice) return;
    
    bool prev_gated = voice->gated;
    bool prev_test = voice->test;
    
    // Update control bits
    voice->gated = (value & 0x01) != 0;
    voice->synchronize = (value & 0x02) != 0;
    voice->ring_modulation = (value & 0x04) != 0;
    voice->test = (value & 0x08) != 0;
    voice->waveform = (waveform_bits_t)(value >> 4);
    
    // Handle test bit changes
    // Real hardware: test bit only affects the oscillator (zeros accumulator,
    // resets noise LFSR to all 1s). The envelope generator is completely independent.
    // reSID: test bit fades LFSR to 0x7FFFFF over ~35000 cycles; we do it instantly.
    if (voice->test && !prev_test) {
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = 0x7FFFFF;
    }
    
    // Handle gate bit changes
    // Real hardware: the rate counter is NOT reset on gate transition.
    // This is the famous "ADSR bug" — the counter persists, causing variable
    // delay before the first envelope tick after retriggering.
    if (voice->gated != prev_gated) {
        if (voice->gated) {
            voice->envelope_cycle = CYCLE_ATTACK;
            voice->envelope_rate_period = voice_rate_to_period(voice, voice->attack_rate);
            // Gate on clears hold_zero so the envelope can restart
            voice->envelope_hold_zero = false;
        } else {
            voice->envelope_cycle = CYCLE_RELEASE;
            voice->envelope_rate_period = voice_rate_to_period(voice, voice->release_rate);
        }
    }
}

void voice_write_attack_decay_register_value(voice_t* voice, uint8_t value) {
    if (!voice) return;
    
    voice->decay_rate = value & 0x0F;
    voice->attack_rate = (value >> 4) & 0x0F;
    
    // Update current rate period if in corresponding cycle
    if (voice->envelope_cycle == CYCLE_ATTACK) {
        voice->envelope_rate_period = voice_rate_to_period(voice, voice->attack_rate);
    } else if (voice->envelope_cycle == CYCLE_DECAY) {
        voice->envelope_rate_period = voice_rate_to_period(voice, voice->decay_rate);
    }
}

void voice_write_sustain_release_register_value(voice_t* voice, uint8_t value) {
    if (!voice) return;
    
    voice->release_rate = value & 0x0F;
    uint8_t sustain_nibble = (value >> 4) & 0x0F;
    
    // Convert 4-bit sustain to 8-bit by duplicating the nibble.
    // Real SID: 0xF → 0xFF, 0xA → 0xAA, 0x0 → 0x00, etc.
    voice->sustain_level = (sustain_nibble << 4) | sustain_nibble;
    
    // Update current rate period if in release cycle
    if (voice->envelope_cycle == CYCLE_RELEASE) {
        voice->envelope_rate_period = voice_rate_to_period(voice, voice->release_rate);
    }
}

// =============================================================================
// REGISTER ACCESS
// =============================================================================

bus_state_t mos6581_registers_write(void* context, bus_state_t bus_state) {
    mos6581_t* sid = (mos6581_t*)context;
    if (!sid) return bus_state;
    
    uint32_t r = BUS_GET_ADDR(bus_state) & SID_REGS_MASK;
    uint8_t value = BUS_GET_DATA(bus_state);
    sid->bus_value = value; // Store for potential bus reads
    
    if (r < 21) { // Voice registers (0x00-0x14)
        voice_t* voice = sid->voices[r / 7];
        
        switch (r % 7) {
            case 0: // FRELO - Voice frequency control (low byte)
                voice->frequency = (voice->frequency & 0xFF00) | value;
                break;
                
            case 1: // FREHI - Voice frequency control (high byte)
                voice->frequency = (voice->frequency & 0x00FF) | (value << 8);
                break;
                
            case 2: // PWLO - Voice pulse waveform width (low byte)
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0xFF00) | value);
                break;
                
            case 3: // PWHI - Voice pulse waveform width (high nybble)
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0x00FF) | ((value & 0x0F) << 8));
                break;
                
            case 4: // VCREG - Voice control register
                voice_write_voice_control_register_value(voice, value);
                break;
                
            case 5: // ATDCY - Voice attack/decay register
                voice_write_attack_decay_register_value(voice, value);
                break;
                
            case 6: // SUREL - Voice sustain/release register
                voice_write_sustain_release_register_value(voice, value);
                break;
        }
    } else {
        // Global registers (0x15-0x1F)
        switch (r) {
            case 0x15: // CUTLO - Filter cutoff frequency (low 3 bits)
                sid->filter_cutoff_frequency = (sid->filter_cutoff_frequency & 0x7F8) | (value & 0x07);
                mos6581_filter_update_cutoff(sid);
                break;
                
            case 0x16: // CUTHI - Filter cutoff frequency (high 8 bits)
                sid->filter_cutoff_frequency = ((uint16_t)value << 3) | (sid->filter_cutoff_frequency & 0x07);
                mos6581_filter_update_cutoff(sid);
                break;
                
            case 0x17: // RESON - Filter resonance control register
                mos6581_write_resonance_control_register_value(sid, value);
                break;
                
            case 0x18: // SIGVOL - Volume and filter select register
                // Handle volume bug for 6581
                if (sid->revision <= SID_REVISION_6581_R4AR && sid->volume != (value & 0x0F)) {
                    sid->volume_change_click = true;
                    sid->volume_click_amplitude = (float)(value & 0x0F) / 15.0f * 0.1f;
                    sid->volume_click_counter = 1000; // Duration in cycles
                }
                mos6581_write_volume_and_filter_select_register_value(sid, value);
                break;
                
            case 0x19: // POTX - Read-only
            case 0x1A: // POTY - Read-only  
            case 0x1B: // OSC3 - Read-only
            case 0x1C: // ENV3 - Read-only
                break; // Ignore writes to read-only registers
                
            case 0x1D: // Unused
            case 0x1E: // Unused
            case 0x1F: // Unused
                break; // Ignore writes to unused registers
        }
    }
    
    // Store register value for debugging
    if (r < SID_REGS_SIZE) {
        sid->regs[r] = value;
    }
    
    return bus_state;
}

bus_state_t mos6581_registers_read(void* context, bus_state_t bus_state) {
    mos6581_t* sid = (mos6581_t*)context;
    if (!sid) return bus_state;
    
    uint32_t r = BUS_GET_ADDR(bus_state) & SID_REGS_MASK;
    
    switch (r) {
        case 0x19: // POTX - Game paddle 1 position
            BUS_SET_DATA(bus_state, sid->pot_x_value);
            break;
            
        case 0x1A: // POTY - Game paddle 2 position
            BUS_SET_DATA(bus_state, sid->pot_y_value);
            break;
            
        case 0x1B: // OSC3 - Oscillator 3 / Random number generator
            BUS_SET_DATA(bus_state, (uint8_t)(sid->voice3.oscillator_waveform >> 4));
            break;
            
        case 0x1C: // ENV3 - Envelope generator 3 output
            // Real hardware: ENV3 always reflects current envelope amplitude,
            // regardless of gate state (the envelope continues during release).
            // Envelope is now 8-bit, return directly.
            BUS_SET_DATA(bus_state, sid->voice3.envelope_amplitude);
            break;
            
        case 0x1D: // Unused register
        case 0x1E: // Unused register
        case 0x1F: // Unused register
            BUS_SET_DATA(bus_state, 0xFF);
            break;
            
        default:
            // Return bus value for write-only registers (already in BUS_GET_DATA(bus_state))
            // No action needed - BUS_GET_DATA(bus_state) already contains what was on the bus
            break;
    }
    
    return bus_state;
}

// =============================================================================
// SYSTEM FUNCTIONS
// =============================================================================

void mos6581_reset(mos6581_t* sid) {
    if (!sid) return;
    
    // Reset all registers
    memset(sid->regs, 0, SID_REGS_SIZE);
    sid->bus_value = 0;
    
    // Reset voices
    for (int i = 0; i < 3; i++) {
        voice_reset(sid->voices[i]);
    }
    
    // Reset filter state
    mos6581_filter_reset(sid);
    
    // Reset SID state
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
    
    // Reset timing
    sid->cycle_count = 0;
    sid->subcycle_count = 0;
    sid->sample_accumulator = 0.0;

    // Flush the sample ring buffer so the audio callback doesn't replay
    // stale data from the previous session.
    sid->sample_buffer.write_pos = 0;
    sid->sample_buffer.read_pos = 0;
    
    // Reset volume bug state
    sid->volume_change_click = false;
    sid->volume_click_amplitude = 0.0f;
    sid->volume_click_counter = 0;
    
    // Reset DC blocker state
    sid->dc_blocker_prev_in = 0.0f;
    sid->dc_blocker_prev_out = 0.0f;
    
    // Reset POT values
    sid->pot_x_value = 0xFF;
    sid->pot_y_value = 0xFF;
    
    // Reset external input
    sid->external_input = 0.0f;
    
    // Update timing-dependent values
    mos6581_set_timing(sid, sid->pal_timing);
}

void mos6581_system_destroy(void* chip) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid) return;
    
    ring_buffer_destroy(&sid->sample_buffer);
    free(sid->temp_buffer);
    free(sid->combined_waveform_table);
    free(sid);
}

void mos6581_bus_attach(void* chip, bus_cycle_ops_t* bus_interface) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !bus_interface) return;
    
    sid->bus_interface = *bus_interface;
}

void* mos6581_system_create(chip_descriptor_t* desc) {
    mos6581_t* sid = (mos6581_t*)calloc(1, sizeof(mos6581_t));
    if (!sid) return NULL;
    
    sid->desc = desc;
    
    // Initialize voices with references
    sid->voices[0] = &sid->voice1;
    sid->voices[1] = &sid->voice2;
    sid->voices[2] = &sid->voice3;
    
    // Set voice indices and parent references
    for (int i = 0; i < 3; i++) {
        sid->voices[i]->voice_index = i;
        sid->voices[i]->sid = sid;
        sid->voices[i]->cpu_clock = 985248.0f; // PAL C64 default
    }
    
    // Initialize default settings
    sid->revision = SID_REVISION_6581_R2;
    sid->pal_timing = true;
    sid->sample_rate = 44100.0f;
    sid->cpu_clock = 985248.0f;   // PAL C64 default
    sid->sample_accumulator = 0.0;
    sid->enable_filter = true;
    sid->enable_distortion = true;
    sid->enable_digiboost = true;
    
    // Initialize ring buffer
    ring_buffer_init(&sid->sample_buffer, SAMPLE_BUFFER_SIZE);
    
    // Initialize temporary buffer
    sid->temp_buffer_size = 1024;
    sid->temp_buffer = (float*)malloc(sid->temp_buffer_size * sizeof(float));
    
    // Initialize filter
    mos6581_filter_init(sid);
    
    // Initialize combined waveform tables
    mos6581_init_combined_waveforms(sid);
    
    mos6581_reset(sid);
    return sid;
}

// Include GUI implementation if available
#ifdef IMGUI_VERSION
#include "mos6581_gui.h"
#endif

/**
 * Consolidated SID tick function - main entry point for SID cycle processing.
 * Combines advance cycle functionality with I/O bus coordination.
 * This replaces direct calls to mos6581_advance_cycle() in the new architecture.
 *
 * @param chip Pointer to SID chip instance
 * @param bus_state Current bus state
 * @return Updated bus state
 */
bus_state_t mos6581_tick(void* chip, bus_state_t bus_state) {
    mos6581_t* sid = (mos6581_t*)chip;

    // HYBRID APPROACH: SID no longer needs to check for IO pending
    // I/O access is now handled directly by the bus memory tick function
    // through chip callback arrays, eliminating the need for this check

    // Delegate to the existing advance cycle function
    // In the future, this can be expanded to include additional tick-specific logic
    return mos6581_advance_cycle(sid, bus_state);
}

#if 0
// =============================================================================
// WAVEFORM CAPTURE AND ANALYSIS
// =============================================================================

void mos6581_capture_waveform(voice_t* voice, uint16_t* buffer, uint32_t buffer_size) {
    if (!voice || !buffer) return;
    
    uint32_t original_accumulator = voice->waveform_accumulator;
    uint32_t original_frequency = voice->frequency;
    
    // Set up for waveform capture
    voice->frequency = 65535; // Maximum frequency for fast capture
    voice->waveform_accumulator = 0;
    
    for (uint32_t i = 0; i < buffer_size; i++) {
        voice_clock_cycle(voice);
        buffer[i] = (uint16_t)voice->oscillator_waveform;
    }
    
    // Restore original state
    voice->waveform_accumulator = original_accumulator;
    voice->frequency = original_frequency;
}

void mos6581_capture_envelope(voice_t* voice, uint16_t* buffer, uint32_t buffer_size) {
    if (!voice || !buffer) return;
    
    envelope_cycle_t original_cycle = voice->envelope_cycle;
    uint16_t original_amplitude = voice->envelope_amplitude;
    
    // Start envelope from beginning
    voice->envelope_cycle = CYCLE_ATTACK;
    voice->envelope_amplitude = 0;
    voice->envelope_rate_counter = 0;
    
    for (uint32_t i = 0; i < buffer_size; i++) {
        voice_update_envelope(voice);
        buffer[i] = voice->envelope_amplitude;
    }
    
    // Restore original state
    voice->envelope_cycle = original_cycle;
    voice->envelope_amplitude = original_amplitude;
}

// =============================================================================
// PERFORMANCE MONITORING
// =============================================================================

typedef struct {
    uint32_t voice_updates;
    uint32_t filter_updates;
    uint32_t envelope_updates;
    uint32_t waveform_generations;
    uint32_t buffer_overruns;
    uint32_t buffer_underruns;
} mos6581_performance_stats_t;

static mos6581_performance_stats_t perf_stats = {0};

void mos6581_get_performance_stats(mos6581_performance_stats_t* stats) {
    if (stats) {
        *stats = perf_stats;
    }
}

void mos6581_reset_performance_stats(void) {
    memset(&perf_stats, 0, sizeof(perf_stats));
}

// =============================================================================
// PRESET MANAGEMENT
// =============================================================================

typedef struct {
    char name[64];
    uint8_t registers[SID_REGS_SIZE];
    sid_revision_t revision;
    bool pal_timing;
} mos6581_preset_t;

void mos6581_save_preset(mos6581_t* sid, mos6581_preset_t* preset, const char* name) {
    if (!sid || !preset || !name) return;
    
    strncpy(preset->name, name, sizeof(preset->name) - 1);
    preset->name[sizeof(preset->name) - 1] = '\0';
    
    memcpy(preset->registers, sid->regs, SID_REGS_SIZE);
    preset->revision = sid->revision;
    preset->pal_timing = sid->pal_timing;
}

void mos6581_load_preset(mos6581_t* sid, const mos6581_preset_t* preset) {
    if (!sid || !preset) return;
    
    // Set revision and timing first
    mos6581_set_revision(sid, preset->revision);
    mos6581_set_timing(sid, preset->pal_timing);
    
    // Load all registers
    for (uint32_t i = 0; i < SID_REGS_SIZE; i++) {
        if (i < 0x19 || i > 0x1C) { // Skip read-only registers
            bus_state_t preset_bus_state = BUS_STATE(0xD400 + i, preset->registers[i], 0);
            mos6581_registers_write(sid, preset_bus_state);
        }
    }
}

// =============================================================================
// MEMORY MANAGEMENT HELPERS
// =============================================================================

size_t mos6581_get_memory_usage(mos6581_t* sid) {
    if (!sid) return 0;
    
    size_t total = sizeof(mos6581_t);
    total += sid->sample_buffer.size * sizeof(float);
    total += sid->temp_buffer_size * sizeof(float);
    total += COMBINED_WAVEFORM_TABLE_SIZE;
    
    return total;
}

void mos6581_optimize_memory(mos6581_t* sid) {
    if (!sid) return;
    
    // Optimize ring buffer size based on usage
    uint32_t max_usage = ring_buffer_available(&sid->sample_buffer);
    if (max_usage < sid->sample_buffer.size / 4) {
        // Shrink buffer if underutilized
        uint32_t new_size = max_usage * 2;
        if (new_size < 1024) new_size = 1024;
        
        ring_buffer_destroy(&sid->sample_buffer);
        ring_buffer_init(&sid->sample_buffer, new_size);
    }
}
#endif

// =============================================================================
// CHIP DESCRIPTOR
// =============================================================================

chip_descriptor_t mos6581_descriptor = {
    .description = "MOS6581 SID Sound Interface Device",
    .create = mos6581_system_create,
    .destroy = mos6581_system_destroy,
    .bus_attach = (void (*)(void *, void *))mos6581_bus_attach,
    .bank_change = NULL,
#ifdef IMGUI_VERSION
    .render_debug_window = mos6581_render_debug_window,
    .render_settings_window = mos6581_render_settings_window
#endif
};
