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
    // Sync resets accumulator when sync source MSB rises
    if ((voice->control_reg & VCREG_SYNC) && sync_source->sync_trigger) {
        voice->waveform_accumulator = 0;
    }
}

// =============================================================================
// ENVELOPE GENERATION
// =============================================================================

uint32_t voice_rate_to_period(int rate) {
    return envelope_rate_periods[rate & 0x0F];
}

void voice_envelope_clock(voice_t* voice) {
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
                uint8_t decay_rate = voice->sid->regs[voice->voice_index * VOICE_REGS + VOICE_ATDCY] & 0x0F;
                voice->envelope_rate_period = voice_rate_to_period(decay_rate);
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
    // reSID-accurate 15-bit rate counter.
    // The rate counter increments each cycle. When it matches the rate period,
    // the envelope is clocked. The counter is 15-bit and wraps at 0x8000.
    // This wrapping behavior is the root of the "ADSR bug": if the rate period
    // changes to a value below the current counter, the counter must wrap all
    // the way around before the next envelope step, causing a long delay.
    // reSID reference: envelope.h — rate_counter is 15-bit, uses != comparison.
    if (voice->envelope_rate_counter != voice->envelope_rate_period) {
        // Increment with 15-bit wrapping (counter skips 0 on wrap)
        if (++voice->envelope_rate_counter & ENVELOPE_RATE_OVERFLOW) {
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
    
    // ZDF SVF: g = tan(π * fc / fs).
    // This is the topology-preserving integrator gain that ensures
    // unconditional stability at any cutoff/resonance combination.
    float rate = sid->sample_rate > 0.0f ? sid->sample_rate : 44100.0f;
    
    // Clamp cutoff to just below Nyquist to avoid tan() blowing up
    float max_hz = rate * 0.499f;
    if (cutoff_hz > max_hz) cutoff_hz = max_hz;
    
    f->g = tanf((float)M_PI * cutoff_hz / rate);
    
    // Recompute derived coefficients (depend on both g and k)
    float g = f->g;
    float k = f->k;
    f->a1 = 1.0f / (1.0f + g * (g + k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}

float mos6581_filter_process(mos6581_t* sid, float input) {
    if (!sid->enable_filter) return input;
    
    filter_state_t* f = &sid->filter_state;
    
    // ZDF (zero-delay feedback) topology-preserving SVF
    // (Andy Simper / Cytomic / Vadim Zavalishin).
    //
    // Unlike the naive SVF, this formulation is UNCONDITIONALLY STABLE
    // at any cutoff frequency and resonance setting.  This is critical
    // because we process the filter at ~44.1 kHz, not every CPU cycle
    // at ~1 MHz like reSID — making the naive SVF unstable at high
    // resonance (the root cause of the "diesel engine buzzing" bug).
    //
    // Coefficients a1, a2, a3 are precomputed when cutoff/resonance change.
    
    float v3 = input - f->ic2eq;
    float v1 = f->a1 * f->ic1eq + f->a2 * v3;
    float v2 = f->ic2eq + f->a2 * f->ic1eq + f->a3 * v3;
    
    // Update integrator states
    f->ic1eq = 2.0f * v1 - f->ic1eq;
    f->ic2eq = 2.0f * v2 - f->ic2eq;
    
    // Filter outputs
    float lp = v2;
    float bp = v1;
    float hp = input - f->k * v1 - v2;
    
    // Apply soft-clipping distortion for 6581 (models NMOS op-amp non-linearity).
    // Applied outside the feedback loop to preserve filter stability while
    // still providing the characteristic 6581 "grit."
    if (f->enable_distortion && sid->revision <= SID_REVISION_6581_R4AR) {
        float res_norm = f->resonance / FILTER_RESONANCE_MAX;
        float distortion_amount = res_norm * 0.5f;
        if (distortion_amount > 1e-6f) {
            float inv_k = 1.0f / distortion_amount;
            lp = tanhf(lp * distortion_amount) * inv_k;
            bp = tanhf(bp * distortion_amount) * inv_k;
            hp = tanhf(hp * distortion_amount) * inv_k;
        }
    }
    
    // Store outputs
    f->low_pass_output = lp;
    f->band_pass_output = bp;
    f->high_pass_output = hp;
    
    // Mix filter outputs based on selected filter mode
    float output = 0.0f;
    uint8_t sigvol = sid->regs[SID_REG_SIGVOL];
    if (sigvol & SIGVOL_LP) output += lp;
    if (sigvol & SIGVOL_BP) output += bp;
    if (sigvol & SIGVOL_HP) output += hp;
    
    return output;
}

void mos6581_filter_reset(mos6581_t* sid) {
    filter_state_t* f = &sid->filter_state;
    
    f->cutoff_frequency = 0.0f;
    f->resonance = 0.0f;
    f->low_pass_output = 0.0f;
    f->band_pass_output = 0.0f;
    f->high_pass_output = 0.0f;
    f->g = 0.0f;
    f->k = 1.7f;   // minimum resonance → maximum damping
    f->a1 = 0.0f;
    f->a2 = 0.0f;
    f->a3 = 0.0f;
    f->ic1eq = 0.0f;
    f->ic2eq = 0.0f;
    f->enable_distortion = (sid->revision <= SID_REVISION_6581_R4AR);
}

void mos6581_filter_init(mos6581_t* sid) {
    mos6581_filter_reset(sid);
}

// =============================================================================
// FILTER REGISTER WRITERS
// =============================================================================

void mos6581_write_resonance_control_register_value(mos6581_t* sid, uint8_t value) {
    // Extract resonance nibble for the float computation.
    sid->filter_state.resonance = (float)((value >> RESON_RES_SHIFT) & 0x0F);
    
    // Recompute k and derived SVF coefficients
    filter_state_t* f = &sid->filter_state;
    float res_norm = f->resonance / FILTER_RESONANCE_MAX;
    f->k = 1.7f * (1.0f - res_norm);
    if (f->k < 0.01f) f->k = 0.01f;
    
    float g = f->g;
    float k = f->k;
    f->a1 = 1.0f / (1.0f + g * (g + k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}

// SIGVOL register (0x18): no decoded fields — regs[SID_REG_SIGVOL] stores
// the raw byte. All reads extract bits directly at sample rate.

// =============================================================================
// RING BUFFER IMPLEMENTATION
// =============================================================================

void ring_buffer_init(ring_buffer_t* rb, uint32_t size) {
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
    free(rb->buffer);
    rb->buffer = NULL;
    rb->size = 0;
    rb->mask = 0;
    rb->write_pos = 0;
    rb->read_pos = 0;
}

void ring_buffer_write(ring_buffer_t* rb, float sample) {
    // SPSC safety: only the writer touches write_pos, only the reader touches read_pos.
    // Read read_pos once into a local (volatile ensures we get the latest value).
    uint32_t next = (rb->write_pos + 1) & rb->mask;
    if (next == rb->read_pos) return; // full — drop sample

    rb->buffer[rb->write_pos] = sample;
    rb->write_pos = next;  // publish (volatile store)
}

bool ring_buffer_empty(ring_buffer_t* rb) {
    return rb->read_pos == rb->write_pos;
}

float ring_buffer_read(ring_buffer_t* rb) {
    // Snapshot write_pos once (volatile load) to avoid TOCTOU with ring_buffer_empty
    uint32_t wp = rb->write_pos;
    uint32_t rp = rb->read_pos;
    if (rp == wp) return 0.0f;  // empty

    float sample = rb->buffer[rp];
    rb->read_pos = (rp + 1) & rb->mask;  // publish (volatile store)
    return sample;
}

uint32_t ring_buffer_available(ring_buffer_t* rb) {
    uint32_t wp = rb->write_pos;
    uint32_t rp = rb->read_pos;
    return (wp - rp) & rb->mask;
}

// =============================================================================
// WAVEFORM GENERATION
// =============================================================================

static inline uint32_t voice_generate_triangle(uint32_t accumulator) {
    // Triangle wave: fold at MSB. XOR with max flips the ramp direction.
    uint32_t folded = (accumulator & WAVEFORM_ACCUMULATOR_MSB)
                    ? (accumulator ^ WAVEFORM_ACCUMULATOR_MAX) : accumulator;
    return folded >> OSCILLATOR_SHIFT_TRI;
}

// Extract noise DAC output from LFSR state.
// LFSR bits {20,18,14,11,9,5,2,0} → DAC bits {11,10,9,8,7,6,5,4}.
// Lower 4 DAC bits are grounded (always zero).
static inline uint32_t noise_lfsr_to_output(uint32_t sr) {
    return ((sr & 0x100000) >> 9)   // bit 20 → bit 11
         | ((sr & 0x040000) >> 8)   // bit 18 → bit 10
         | ((sr & 0x004000) >> 5)   // bit 14 → bit  9
         | ((sr & 0x000800) >> 3)   // bit 11 → bit  8
         | ((sr & 0x000200) >> 2)   // bit  9 → bit  7
         | ((sr & 0x000020) << 1)   // bit  5 → bit  6
         | ((sr & 0x000004) << 3)   // bit  2 → bit  5
         | ((sr & 0x000001) << 4);  // bit  0 → bit  4
}

// =============================================================================
// VOICE FUNCTIONS
// =============================================================================

void voice_reset(voice_t* voice) {
    // Reset register values
    voice->frequency = 0;
    voice->pulse_waveform_width = 0;
    voice->control_reg = 0;
    voice->sustain_level = 0;
    
    // Reset internal state
    voice->waveform_accumulator = ACC_POWERUP_VALUE;
    voice->envelope_accumulator = 0;
    voice->envelope_cycle = CYCLE_OFF;
    voice->envelope_amplitude = 0;
    voice->envelope_rate_counter = 0;
    voice->envelope_rate_period = 0;
    voice->envelope_hold_zero = false;
    voice->exponential_counter = 0;
    voice->exponential_counter_period = 1;
    
    // Reset waveform outputs — 0xFFF means "not driving any DAC lines low"
    voice->oscillator_waveform = 0;
    voice->triangle_output = OSCILLATOR_MAX;
    voice->sawtooth_output = OSCILLATOR_MAX;
    voice->pulse_output = OSCILLATOR_MAX;
    voice->combined_output = 0;
    
    // Reset noise state — reSID: shift_register = 0x7FFFFE after reset.
    // Latch noise_output immediately so it is valid before the first clock.
    voice->noise_lfsr = NOISE_LFSR_RESET;
    voice->noise_output = noise_lfsr_to_output(voice->noise_lfsr);
    voice->noise_clock_enable = false;
    
    // Reset sync state
    voice->prev_accumulator = 0;
    voice->sync_trigger = false;
    
    // Reset outputs
    voice->oscillator_output = 0;
    voice->envelope_output = 0;
    voice->result = 0;
}

void voice_clock_cycle(voice_t* voice) {
    // Store previous accumulator for sync detection
    voice->prev_accumulator = voice->waveform_accumulator;
    
    // Update accumulator unless test bit is set
    if (!(voice->control_reg & VCREG_TEST)) {
        voice->waveform_accumulator = (voice->waveform_accumulator + voice->frequency) & WAVEFORM_ACCUMULATOR_MAX;
    } else {
        // Test bit locks accumulator and resets noise LFSR.
        // reSID: test bit gradually fades all LFSR bits to 1 (0x7FFFFF)
        // over ~35000 cycles (6581). We approximate this as instant.
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = NOISE_LFSR_TEST;
        voice->noise_output = noise_lfsr_to_output(voice->noise_lfsr);
    }
    
    // Detect MSB change for sync
    bool msb_rising = ((voice->prev_accumulator & WAVEFORM_ACCUMULATOR_MSB) == 0) && 
                      ((voice->waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0);
    voice->sync_trigger = msb_rising;
    
    // Always clock noise LFSR based on accumulator bit 19 — real hardware
    // clocks it regardless of waveform selection (reSID does this in clock()).
    // Latch noise_output immediately on shift so it is always valid and never
    // needs recalculating on each cycle (LFSR shifts much less often).
    {
        bool clock_noise = (voice->waveform_accumulator & ACC_BIT19) != 0;
        if (clock_noise && !voice->noise_clock_enable) {
            uint32_t feedback = ((voice->noise_lfsr >> 22) ^ (voice->noise_lfsr >> 17)) & 1;
            voice->noise_lfsr = ((voice->noise_lfsr << 1) | feedback) & NOISE_LFSR_MASK;
            voice->noise_output = noise_lfsr_to_output(voice->noise_lfsr);
        }
        voice->noise_clock_enable = clock_noise;
    }
    
    // Update envelope (rate counter + envelope clock)
    voice_update_envelope(voice);
    voice->envelope_output = voice->envelope_amplitude;
}

// Generate waveform outputs with ring modulation baked in (reSID-accurate).
//
// Ring modulation works by XORing the accumulator's MSB (bit 23) with the
// INVERTED MSB of the ring source oscillator before computing the triangle
// waveform.  This shifts the triangle fold-point, creating FM-like effects.
//
// reSID: ring_msb_mask = ((~control>>5) & (control>>2) & 1) << 23
// → active when ring_mod=1 AND sawtooth=0.  Sawtooth's direct DAC path
// overrides the triangle EOR mechanism on real hardware.
//
// The ring-modified accumulator only affects triangle generation.  Sawtooth,
// pulse and noise use the original accumulator (pulse is a comparator and
// sawtooth selection disables the ring MSB mask).
void voice_set_waveform_output(voice_t* voice, voice_t* ring_source) {
    uint8_t wf = voice->control_reg;
    
    // Compute ring modulation MSB mask (reSID: ring_msb_mask).
    // Active only when ring_mod=1 AND sawtooth=0.
    uint32_t ring_msb_mask = ((wf & VCREG_RING) && !(wf & WAVEFORM_SAWTOOTH))
                           ? WAVEFORM_ACCUMULATOR_MSB : 0;
    
    // Ring-modified accumulator: XOR our MSB with ~source_MSB.
    // reSID: accumulator ^ (~sync_source->accumulator & ring_msb_mask)
    uint32_t ring_acc = voice->waveform_accumulator
                      ^ (~ring_source->waveform_accumulator & ring_msb_mask);
    
    // Generate waveform outputs.  Unselected waveforms output 0xFFF so their
    // bits don't pull any DAC lines low in the combined AND — matching real
    // hardware where unselected waveform switches are open.
    voice->triangle_output = (wf & WAVEFORM_TRIANGLE)
        ? voice_generate_triangle(ring_acc) : 0xFFF;
    voice->sawtooth_output = (wf & WAVEFORM_SAWTOOTH)
        ? (voice->waveform_accumulator >> OSCILLATOR_SHIFT_SAW) : OSCILLATOR_MAX;
    voice->pulse_output = (wf & WAVEFORM_PULSE)
        ? (((voice->waveform_accumulator >> OSCILLATOR_SHIFT_SAW) >= voice->pulse_waveform_width) ? OSCILLATOR_MAX : 0) : OSCILLATOR_MAX;
    
    // Combined waveform output: AND of all waveform DAC lines.
    // Disabled waveforms carry 0xFFF and pass through transparently.
    // No waveform selected → floating DAC, simplified as zero.
    uint32_t noise_and = (wf & WAVEFORM_NOISE) ? voice->noise_output : OSCILLATOR_MAX;
    uint32_t waveform_output = (wf & WAVEFORM_MASK)
        ? (voice->triangle_output & voice->sawtooth_output & voice->pulse_output & noise_and)
        : 0;
    
    voice->oscillator_waveform = waveform_output;
    voice->oscillator_output = (uint8_t)(waveform_output >> 4);
    
    // Apply envelope to waveform (voice->result includes ring mod)
    voice->result = (voice->oscillator_waveform * voice->envelope_amplitude) >> 8;
}

// =============================================================================
// MAIN CYCLE FUNCTION
// =============================================================================

// Unified bus state threading main cycle function
inline bus_state_t mos6581_advance_cycle(mos6581_t* sid, bus_state_t bus_state) {
    // reSID per-cycle order:
    //   1. Clock envelopes  (inside voice_clock_cycle)
    //   2. Clock oscillators (accumulator + noise LFSR)
    //   3. Synchronize oscillators (hard sync)
    //   4. Generate waveform output (with ring mod baked in)
    //   5. Clock filter → generate sample

    // Steps 1-2: Clock accumulators, noise, and envelopes
    for (int i = 0; i < 3; i++) {
        voice_clock_cycle(sid->voices[i]);
    }

    // Step 3: Apply oscillator sync.
    // Sync source mapping: voice1←voice3, voice2←voice1, voice3←voice2
    voice_apply_sync(&sid->voice1, &sid->voice3);
    voice_apply_sync(&sid->voice2, &sid->voice1);
    voice_apply_sync(&sid->voice3, &sid->voice2);

    // Step 4: Generate waveform outputs with ring mod baked in.
    // Ring source mapping matches sync: voice1←voice3, etc.
    voice_set_waveform_output(&sid->voice1, &sid->voice3);
    voice_set_waveform_output(&sid->voice2, &sid->voice1);
    voice_set_waveform_output(&sid->voice3, &sid->voice2);

    // Step 5: Generate output samples at the target sample rate (~44.1 kHz).
    // Voice mixing and SVF filter processing happen here — NOT every cycle.
    // This is a ~22x reduction in filter work vs per-cycle processing,
    // which is critical for maintaining 50 fps at ~1 MHz emulation speed.
    if (sid->cpu_clock > 0.0f) {
        sid->sample_accumulator += (double)sid->sample_rate / (double)sid->cpu_clock;

        if (sid->sample_accumulator >= 1.0) {
            sid->sample_accumulator -= 1.0;

            // --- Voice mixing ---
            // oscillator_waveform already includes ring modulation (applied per-cycle).
            // Center waveform BEFORE envelope so silent voices produce zero.
            // 12-bit waveform centered: -2048..+2047
            // × 8-bit envelope 0…255 → signed product -522240..+521985
            // Scale so the SUM of all 3 voices at max ≈ ±1.0 to prevent
            // clipping (real SID mixer has headroom for all voices).
            const float inv_scale = 1.0f / (3.0f * OSCILLATOR_CENTER * ENVELOPE_MAX);
            float v1 = (float)(((int32_t)sid->voice1.oscillator_waveform - OSCILLATOR_CENTER) * (int32_t)sid->voice1.envelope_amplitude) * inv_scale;
            float v2 = (float)(((int32_t)sid->voice2.oscillator_waveform - OSCILLATOR_CENTER) * (int32_t)sid->voice2.envelope_amplitude) * inv_scale;
            float v3 = (float)(((int32_t)sid->voice3.oscillator_waveform - OSCILLATOR_CENTER) * (int32_t)sid->voice3.envelope_amplitude) * inv_scale;

            float filtered_input = 0.0f;
            float unfiltered_output = 0.0f;

            // Route voices to filter or direct output
            uint8_t reson = sid->regs[SID_REG_RESON];
            uint8_t sigvol = sid->regs[SID_REG_SIGVOL];
            if (reson & RESON_FILT1) filtered_input += v1; else unfiltered_output += v1;
            if (reson & RESON_FILT2) filtered_input += v2; else unfiltered_output += v2;
            if (reson & RESON_FILT3) filtered_input += v3;
            else if (!(sigvol & SIGVOL_3OFF)) unfiltered_output += v3;
            if (reson & RESON_FILTEX) filtered_input += sid->external_input;
            else unfiltered_output += sid->external_input;

            // Apply SVF filter (at sample rate, not per-cycle)
            float filtered_output = mos6581_filter_process(sid, filtered_input);

            // Sum filtered + unfiltered
            float mixed = unfiltered_output + filtered_output;

            // 6581 digi support: add constant DC bias from the voice DACs.
            if (sid->revision <= SID_REVISION_6581_R4AR) {
                mixed += SID_6581_DIGI_BIAS;
            }

            // Apply master volume
            mixed *= (float)(sigvol & SIGVOL_VOL_MASK) / SIGVOL_VOL_MAX;

            // DC blocker: removes the constant bias×volume product while
            // preserving fast changes (digi samples).  ~20 Hz high-pass.
            //   y[n] = x[n] - x[n-1] + α · y[n-1],  α = 0.997
            {
                float dc_out = mixed - sid->dc_blocker_prev_in
                             + DC_BLOCKER_ALPHA * sid->dc_blocker_prev_out;
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
        output[i] = ring_buffer_read(&sid->sample_buffer);
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void mos6581_set_revision(mos6581_t* sid, sid_revision_t revision) {
    sid->revision = revision;
    sid->enable_distortion = (revision <= SID_REVISION_6581_R4AR);
    mos6581_filter_reset(sid);
}

void mos6581_set_timing(mos6581_t* sid, bool pal_timing) {
    sid->pal_timing = pal_timing;
    sid->sid_rate = sid->cpu_clock / (pal_timing ? 18.0f : 17.0f);
    mos6581_filter_update_cutoff(sid);
}

void mos6581_set_sample_rate(mos6581_t* sid, float sample_rate) {
    sid->sample_rate = sample_rate;
    // Update filter coefficient since w0 depends on sample rate
    mos6581_filter_update_cutoff(sid);
}

void mos6581_set_cpu_clock(mos6581_t* sid, float clock_hz) {
    sid->cpu_clock = clock_hz;
    for (int i = 0; i < 3; i++) {
        sid->voices[i]->cpu_clock = clock_hz;
    }
    // Update derived timing values
    sid->sid_rate = clock_hz / (sid->pal_timing ? 18.0f : 17.0f);
    mos6581_filter_update_cutoff(sid);
}

int voice_cycles_per_millisecond(voice_t* voice) {
    return (int)(voice->cpu_clock / 1000.0f);
}

// =============================================================================
// ENVELOPE TIMING ANALYSIS
// =============================================================================

uint32_t mos6581_calculate_envelope_time_ms(voice_t* voice, envelope_cycle_t cycle, uint8_t rate) {
    uint32_t period = voice_rate_to_period(rate);
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
    voice->pulse_waveform_width = value & PULSE_WIDTH_MAX;
}

void voice_write_voice_control_register_value(voice_t* voice, uint8_t value) {
    uint8_t prev = voice->control_reg;
    voice->control_reg = value;
    
    // Handle test bit changes
    // Real hardware: test bit only affects the oscillator (zeros accumulator,
    // resets noise LFSR to all 1s). The envelope generator is completely independent.
    // reSID: test bit fades LFSR to 0x7FFFFF over ~35000 cycles; we do it instantly.
    if ((value & VCREG_TEST) && !(prev & VCREG_TEST)) {
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = NOISE_LFSR_TEST;
    }
    
    // Handle gate bit changes
    // Real hardware: the rate counter is NOT reset on gate transition.
    // This is the famous "ADSR bug" — the counter persists, causing variable
    // delay before the first envelope tick after retriggering.
    if ((value ^ prev) & VCREG_GATE) {
        if (value & VCREG_GATE) {
            voice->envelope_cycle = CYCLE_ATTACK;
            uint8_t attack_rate = (voice->sid->regs[voice->voice_index * VOICE_REGS + VOICE_ATDCY] >> 4) & 0x0F;
            voice->envelope_rate_period = voice_rate_to_period(attack_rate);
            // Gate on clears hold_zero so the envelope can restart
            voice->envelope_hold_zero = false;
        } else {
            voice->envelope_cycle = CYCLE_RELEASE;
            uint8_t release_rate = voice->sid->regs[voice->voice_index * VOICE_REGS + VOICE_SUREL] & 0x0F;
            voice->envelope_rate_period = voice_rate_to_period(release_rate);
        }
    }
}

void voice_write_attack_decay_register_value(voice_t* voice, uint8_t value) {
    // Update current rate period if in corresponding cycle.
    if (voice->envelope_cycle == CYCLE_ATTACK) {
        voice->envelope_rate_period = voice_rate_to_period((value >> 4) & 0x0F);
    } else if (voice->envelope_cycle == CYCLE_DECAY) {
        voice->envelope_rate_period = voice_rate_to_period(value & 0x0F);
    }
}

void voice_write_sustain_release_register_value(voice_t* voice, uint8_t value) {
    uint8_t sustain_nibble = (value >> 4) & 0x0F;
    
    // Convert 4-bit sustain to 8-bit by duplicating the nibble.
    // Real SID: 0xF → 0xFF, 0xA → 0xAA, 0x0 → 0x00, etc.
    voice->sustain_level = (sustain_nibble << 4) | sustain_nibble;
    
    // Update current rate period if in release cycle
    if (voice->envelope_cycle == CYCLE_RELEASE) {
        voice->envelope_rate_period = voice_rate_to_period(value & 0x0F);
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
    
    if (r < SID_VOICE_REG_COUNT) { // Voice registers (0x00-0x14)
        voice_t* voice = sid->voices[r / VOICE_REGS];
        
        switch (r % VOICE_REGS) {
            case VOICE_FRELO:
                voice->frequency = (voice->frequency & 0xFF00) | value;
                break;
                
            case VOICE_FREHI:
                voice->frequency = (voice->frequency & 0x00FF) | (value << 8);
                break;
                
            case VOICE_PWLO:
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0xFF00) | value);
                break;
                
            case VOICE_PWHI:
                voice_write_pulse_waveform_width(voice, (voice->pulse_waveform_width & 0x00FF) | ((value & 0x0F) << 8));
                break;
                
            case VOICE_VCREG:
                voice_write_voice_control_register_value(voice, value);
                break;
                
            case VOICE_ATDCY:
                voice_write_attack_decay_register_value(voice, value);
                break;
                
            case VOICE_SUREL:
                voice_write_sustain_release_register_value(voice, value);
                break;
        }
    } else {
        // Global registers
        switch (r) {
            case SID_REG_CUTLO:
                sid->filter_cutoff_frequency = (sid->filter_cutoff_frequency & 0x7F8) | (value & 0x07);
                mos6581_filter_update_cutoff(sid);
                break;
                
            case SID_REG_CUTHI:
                sid->filter_cutoff_frequency = ((uint16_t)value << 3) | (sid->filter_cutoff_frequency & 0x07);
                mos6581_filter_update_cutoff(sid);
                break;
                
            case SID_REG_RESON:
                mos6581_write_resonance_control_register_value(sid, value);
                break;
                
            case SID_REG_SIGVOL:
                // Handle volume bug for 6581
                if (sid->revision <= SID_REVISION_6581_R4AR && (sid->regs[SID_REG_SIGVOL] & SIGVOL_VOL_MASK) != (value & SIGVOL_VOL_MASK)) {
                    sid->volume_change_click = true;
                    sid->volume_click_amplitude = (float)(value & SIGVOL_VOL_MASK) / SIGVOL_VOL_MAX * 0.1f;
                    sid->volume_click_counter = VOLUME_CLICK_DURATION;
                }
                break;
                
            case SID_REG_POTX:
            case SID_REG_POTY:
            case SID_REG_OSC3:
            case SID_REG_ENV3:
                break; // Ignore writes to read-only registers
                
            default:
                break; // Unused registers
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
        case SID_REG_POTX:
            BUS_SET_DATA(bus_state, sid->pot_x_value);
            break;
            
        case SID_REG_POTY:
            BUS_SET_DATA(bus_state, sid->pot_y_value);
            break;
            
        case SID_REG_OSC3:
            BUS_SET_DATA(bus_state, (uint8_t)(sid->voice3.oscillator_waveform >> 4));
            break;
            
        case SID_REG_ENV3:
            // Real hardware: ENV3 always reflects current envelope amplitude,
            // regardless of gate state (the envelope continues during release).
            BUS_SET_DATA(bus_state, sid->voice3.envelope_amplitude);
            break;
            
        case SID_REG_UNUSED_START:
        case SID_REG_UNUSED_START + 1:
        case SID_REG_UNUSED_END:
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
    sid->revision = SID_REVISION_6581_R4AR;
    sid->pal_timing = true;
    sid->sample_rate = 44100.0f;
    sid->cpu_clock = 985248.0f;   // PAL C64 default
    sid->sample_accumulator = 0.0;
    sid->enable_filter = true;
    sid->enable_distortion = true;
    sid->enable_digiboost = true;
    
    // Initialize ring buffer
    ring_buffer_init(&sid->sample_buffer, SAMPLE_BUFFER_SIZE);
    
    // Initialize filter
    mos6581_filter_init(sid);
    
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
    return mos6581_advance_cycle((mos6581_t*)chip, bus_state);
}

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
