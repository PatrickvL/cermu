#include "mos6581.h"
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
void voice_s::update_exponential_period() {
    switch (envelope_amplitude) {
        case 0xFF: exponential_counter_period = 1; break;
        case 0x5D: exponential_counter_period = 2; break;
        case 0x36: exponential_counter_period = 4; break;
        case 0x1A: exponential_counter_period = 8; break;
        case 0x0E: exponential_counter_period = 16; break;
        case 0x06: exponential_counter_period = 30; break;
        case 0x00:
            exponential_counter_period = 1;
            envelope_hold_zero = true;
            break;
        default: break; // Keep current period
    }
}

// =============================================================================
// VOICE INTERACTION (SYNC AND RING MODULATION)
// =============================================================================

void voice_s::apply_sync(voice_t* sync_source, voice_t* sync_source_source) {
    // Sync resets accumulator when sync source MSB rises.
    // reSID chain-sync protection: if the sync source was itself synced on
    // the same cycle (its own source's MSB also rose), the destination is NOT
    // synced. Verified by sampling OSC3 on real hardware.
    if ((control_reg & VCREG_SYNC) && sync_source->sync_trigger) {
        if ((sync_source->control_reg & VCREG_SYNC) && sync_source_source->sync_trigger) {
            return; // chain-sync protection
        }
        waveform_accumulator = 0;
    }
}

// =============================================================================
// ENVELOPE GENERATION
// =============================================================================

uint32_t voice_s::rate_to_period(int rate) {
    return envelope_rate_periods[rate & 0x0F];
}

void voice_s::envelope_clock() {
    // reSID-accurate 8-bit envelope generator.
    // Attack: envelope_counter increments by 1 each rate tick.
    //         Transitions to decay when reaching 0xFF.
    // Decay/Sustain: envelope_counter decrements by 1, gated by
    //         the exponential counter reaching its period.
    // Release: same decrement logic as decay.
    // hold_zero prevents any further changes once envelope reaches 0.
    
    switch (envelope_cycle) {
        case CYCLE_OFF:
            break;
            
        case CYCLE_ATTACK:
            // Attack always increments (exponential counter not used for gating).
            // reSID: the first envelope step in attack also resets the exponential
            // counter. This ensures decay starts with a fresh counter after attack
            // completes. Verified by sampling ENV3 on real hardware.
            if (envelope_hold_zero) break;
            exponential_counter = 0;
            envelope_amplitude = (envelope_amplitude + 1) & 0xFF;
            if (envelope_amplitude == 0xFF) {
                envelope_cycle = CYCLE_DECAY;
                uint8_t decay_rate = sid->regs[voice_index * VOICE_REGS + VOICE_ATDCY] & 0x0F;
                envelope_rate_period = rate_to_period(decay_rate);
            }
            break;
            
        case CYCLE_DECAY:
            // reSID: Combined DECAY_SUSTAIN state (no separate sustain).
            // Each rate tick gated by the exponential counter, if envelope !=
            // sustain_level, decrement. Otherwise hold. If sustain is changed
            // while in this state, decay resumes automatically.
            // reSID uses strict == for exponential counter comparison (not >=).
            if (envelope_hold_zero) break;
            if (++exponential_counter == exponential_counter_period) {
                exponential_counter = 0;
                if (envelope_amplitude != sustain_level) {
                    envelope_amplitude = (envelope_amplitude - 1) & 0xFF;
                }
            }
            update_exponential_period();
            // Note: do NOT transition to CYCLE_SUSTAIN. Stay in CYCLE_DECAY
            // so that sustain level changes cause decay to resume.
            break;
            
        case CYCLE_SUSTAIN:
            // Legacy state — kept for save-state compatibility but should not
            // be entered by new code paths. Behaves like CYCLE_DECAY.
            if (envelope_hold_zero) break;
            if (envelope_amplitude != sustain_level) {
                // Sustain level was changed — resume decay behavior
                envelope_cycle = CYCLE_DECAY;
            }
            break;
            
        case CYCLE_RELEASE:
            if (envelope_hold_zero) break;
            if (++exponential_counter == exponential_counter_period) {
                exponential_counter = 0;
                envelope_amplitude = (envelope_amplitude - 1) & 0xFF;
            }
            update_exponential_period();
            break;
    }
}

void voice_s::update_envelope() {
    // reSID-accurate 15-bit rate counter.
    // The rate counter increments each cycle. When it matches the rate period,
    // the envelope is clocked. The counter is 15-bit and wraps at 0x8000.
    // This wrapping behavior is the root of the "ADSR bug": if the rate period
    // changes to a value below the current counter, the counter must wrap all
    // the way around before the next envelope step, causing a long delay.
    // reSID reference: envelope.h — rate_counter is 15-bit, uses != comparison.
    if (envelope_rate_counter != envelope_rate_period) {
        // Increment with 15-bit wrapping (counter skips 0 on wrap)
        if (++envelope_rate_counter & ENVELOPE_RATE_OVERFLOW) {
            envelope_rate_counter = 1; // Wrap: 0x7FFF → 0x0001 (skip 0)
        }
        return;
    }
    
    // Rate counter matched — reset to 0 and clock the envelope
    envelope_rate_counter = 0;
    envelope_clock();
}

// =============================================================================
// FILTER IMPLEMENTATION
// =============================================================================

void mos6581_s::filter_update_cutoff() {
    filter_state_t* f = &filter_state;
    
    // Calculate cutoff frequency in Hz from the 11-bit register (0-2047).
    float fc = (float)filter_cutoff_frequency;
    float normalized = fc / FILTER_CUTOFF_MAX;  // 0.0 .. 1.0
    float cutoff_hz;
    
    if (revision <= SID_REVISION_6581_R4AR) {
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
    //
    // The filter is clocked every CPU cycle (~985 kHz PAL), so we use the
    // CPU clock as the effective sample rate for coefficient calculation.
    float rate = cpu_clock > 0.0f ? cpu_clock : 985248.0f;
    
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

float mos6581_s::filter_process(float input) {
    if (!enable_filter) return input;
    
    filter_state_t* f = &filter_state;
    
    // ZDF (zero-delay feedback) topology-preserving SVF
    // (Andy Simper / Cytomic / Vadim Zavalishin).
    //
    // Unlike the naive SVF, this formulation is UNCONDITIONALLY STABLE
    // at any cutoff frequency and resonance setting.  The filter is
    // clocked every CPU cycle (~985 kHz PAL), matching reSID's approach
    // for accurate cutoff tracking during rapid filter sweeps.
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
    if (f->enable_distortion && revision <= SID_REVISION_6581_R4AR) {
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
    uint8_t sigvol = regs[SID_REG_SIGVOL];
    if (sigvol & SIGVOL_LP) output += lp;
    if (sigvol & SIGVOL_BP) output += bp;
    if (sigvol & SIGVOL_HP) output += hp;
    
    return output;
}

void mos6581_s::filter_reset() {
    filter_state_t* f = &filter_state;
    
    f->cutoff_frequency = 0.0f;
    f->resonance = 0.0f;
    f->low_pass_output = 0.0f;
    f->band_pass_output = 0.0f;
    f->high_pass_output = 0.0f;
    f->g = 0.0f;
    f->k = 1.0f / 0.707f; // ~1.414 — Butterworth (no resonance)
    f->a1 = 0.0f;
    f->a2 = 0.0f;
    f->a3 = 0.0f;
    f->ic1eq = 0.0f;
    f->ic2eq = 0.0f;
    f->enable_distortion = (revision <= SID_REVISION_6581_R4AR);
}

void mos6581_s::filter_init() {
    filter_reset();
}

// =============================================================================
// FILTER REGISTER WRITERS
// =============================================================================

void mos6581_s::write_resonance_control_register_value(uint8_t value) {
    // Extract resonance nibble for the float computation.
    filter_state.resonance = (float)((value >> RESON_RES_SHIFT) & 0x0F);
    
    // Recompute k and derived SVF coefficients.
    // The 6581's resonance is controlled by a VCR (voltage-controlled resistor)
    // whose nonlinear characteristics limit the effective Q factor to ~10-20.
    // reSID models this through circuit-level simulation; we approximate with
    // a direct Q mapping: Q_min ≈ 0.707 (Butterworth) to Q_max ≈ 15.
    // Previous mapping (k = 1.7*(1-res/15), clamped to 0.01) gave Q_max = 100,
    // which caused enormous resonant gain (+40 dB) on filter sweeps → "pieuw".
    filter_state_t* f = &filter_state;
    float res_norm = f->resonance / FILTER_RESONANCE_MAX;
    float Q = 0.707f + res_norm * 14.3f;  // Q range [0.707, 15.0]
    f->k = 1.0f / Q;
    
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

void ring_buffer_s::init(uint32_t size) {
    // Round size up to next power of 2
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
    
    this->size = size;
    mask = size - 1;
    buffer = (float*)calloc(size, sizeof(float));
    write_pos = 0;
    read_pos = 0;
}

void ring_buffer_s::destroy() {
    free(buffer);
    buffer = NULL;
    size = 0;
    mask = 0;
    write_pos = 0;
    read_pos = 0;
}

void ring_buffer_s::write(float sample) {
    // SPSC safety: only the writer touches write_pos, only the reader touches read_pos.
    // Read read_pos once into a local (volatile ensures we get the latest value).
    uint32_t next = (write_pos + 1) & mask;
    if (next == read_pos) return; // full — drop sample

    buffer[write_pos] = sample;
    write_pos = next;  // publish (volatile store)
}

bool ring_buffer_s::empty() {
    return read_pos == write_pos;
}

float ring_buffer_s::read() {
    // Snapshot write_pos once (volatile load) to avoid TOCTOU with empty()
    uint32_t wp = write_pos;
    uint32_t rp = read_pos;
    if (rp == wp) return 0.0f;  // empty

    float sample = buffer[rp];
    read_pos = (rp + 1) & mask;  // publish (volatile store)
    return sample;
}

uint32_t ring_buffer_s::available() {
    uint32_t wp = write_pos;
    uint32_t rp = read_pos;
    return (wp - rp) & mask;
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

void voice_s::reset() {
    // Reset register values
    frequency = 0;
    pulse_waveform_width = 0;
    control_reg = 0;
    sustain_level = 0;
    
    // Reset internal state
    waveform_accumulator = ACC_POWERUP_VALUE;
    envelope_accumulator = 0;
    envelope_cycle = CYCLE_OFF;
    envelope_amplitude = 0;
    envelope_rate_counter = 0;
    envelope_rate_period = 0;
    envelope_hold_zero = false;
    exponential_counter = 0;
    exponential_counter_period = 1;
    
    // Reset waveform outputs — 0xFFF means "not driving any DAC lines low"
    oscillator_waveform = 0;
    triangle_output = OSCILLATOR_MAX;
    sawtooth_output = OSCILLATOR_MAX;
    pulse_output = OSCILLATOR_MAX;
    combined_output = 0;
    
    // Reset noise state — reSID: shift_register = 0x7FFFFE after reset.
    // Latch noise_output immediately so it is valid before the first clock.
    noise_lfsr = NOISE_LFSR_RESET;
    noise_output = noise_lfsr_to_output(noise_lfsr);
    noise_clock_enable = false;
    
    // Reset sync state
    prev_accumulator = 0;
    sync_trigger = false;
    
    // Reset outputs
    oscillator_output = 0;
    envelope_output = 0;
    result = 0;
}

void voice_s::clock_cycle() {
    // Store previous accumulator for sync detection
    prev_accumulator = waveform_accumulator;
    
    // Update accumulator unless test bit is set
    if (!(control_reg & VCREG_TEST)) {
        waveform_accumulator = (waveform_accumulator + frequency) & WAVEFORM_ACCUMULATOR_MAX;
    } else {
        // Test bit locks accumulator and resets noise LFSR.
        // reSID: test bit gradually fades all LFSR bits to 1 (0x7FFFFF)
        // over ~35000 cycles (6581). We approximate this as instant.
        waveform_accumulator = 0;
        noise_lfsr = NOISE_LFSR_TEST;
        noise_output = noise_lfsr_to_output(noise_lfsr);
    }
    
    // Detect MSB change for sync
    bool msb_rising = ((prev_accumulator & WAVEFORM_ACCUMULATOR_MSB) == 0) && 
                      ((waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0);
    sync_trigger = msb_rising;
    
    // Always clock noise LFSR based on accumulator bit 19 — real hardware
    // clocks it regardless of waveform selection (reSID does this in clock()).
    // Latch noise_output immediately on shift so it is always valid and never
    // needs recalculating on each cycle (LFSR shifts much less often).
    {
        bool clock_noise = (waveform_accumulator & ACC_BIT19) != 0;
        if (clock_noise && !noise_clock_enable) {
            uint32_t feedback = ((noise_lfsr >> 22) ^ (noise_lfsr >> 17)) & 1;
            noise_lfsr = ((noise_lfsr << 1) | feedback) & NOISE_LFSR_MASK;
            noise_output = noise_lfsr_to_output(noise_lfsr);
        }
        noise_clock_enable = clock_noise;
    }
    
    // Update envelope (rate counter + envelope clock)
    update_envelope();
    envelope_output = envelope_amplitude;
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
void voice_s::set_waveform_output(voice_t* ring_source) {
    uint8_t wf = control_reg;
    
    // Compute ring modulation MSB mask (reSID: ring_msb_mask).
    // Active only when ring_mod=1 AND sawtooth=0.
    uint32_t ring_msb_mask = ((wf & VCREG_RING) && !(wf & WAVEFORM_SAWTOOTH))
                           ? WAVEFORM_ACCUMULATOR_MSB : 0;
    
    // Ring-modified accumulator: XOR our MSB with ~source_MSB.
    // reSID: accumulator ^ (~sync_source->accumulator & ring_msb_mask)
    uint32_t ring_acc = waveform_accumulator
                      ^ (~ring_source->waveform_accumulator & ring_msb_mask);
    
    // Generate waveform outputs.  Unselected waveforms output 0xFFF so their
    // bits don't pull any DAC lines low in the combined AND — matching real
    // hardware where unselected waveform switches are open.
    triangle_output = (wf & WAVEFORM_TRIANGLE)
        ? voice_generate_triangle(ring_acc) : OSCILLATOR_MAX;
    sawtooth_output = (wf & WAVEFORM_SAWTOOTH)
        ? (waveform_accumulator >> OSCILLATOR_SHIFT_SAW) : OSCILLATOR_MAX;
    pulse_output = (wf & WAVEFORM_PULSE)
        ? (((waveform_accumulator >> OSCILLATOR_SHIFT_SAW) >= pulse_waveform_width) ? OSCILLATOR_MAX : 0) : OSCILLATOR_MAX;
    
    // Combined waveform output: AND of all waveform DAC lines.
    // Disabled waveforms carry 0xFFF and pass through transparently.
    // No waveform selected → floating DAC, simplified as zero.
    uint32_t noise_and = (wf & WAVEFORM_NOISE) ? noise_output : OSCILLATOR_MAX;
    uint32_t waveform_output = (wf & WAVEFORM_MASK)
        ? (triangle_output & sawtooth_output & pulse_output & noise_and)
        : 0;
    
    oscillator_waveform = waveform_output;
    oscillator_output = (uint8_t)(waveform_output >> 4);
    
    // Apply envelope to waveform (result includes ring mod)
    result = (oscillator_waveform * envelope_amplitude) >> 8;
}

// =============================================================================
// MAIN CYCLE FUNCTION
// =============================================================================

// Unified bus state threading main cycle function
inline bus_state_t mos6581_s::advance_cycle(bus_state_t bus_state) {
    // reSID per-cycle order:
    //   1. Clock envelopes  (inside voice_clock_cycle)
    //   2. Clock oscillators (accumulator + noise LFSR)
    //   3. Synchronize oscillators (hard sync)
    //   4. Generate waveform output (with ring mod baked in)
    //   5. Clock filter → generate sample

    // Steps 1-2: Clock accumulators, noise, and envelopes
    for (int i = 0; i < 3; i++) {
        voices[i]->clock_cycle();
    }

    // Step 3: Apply oscillator sync.
    // Sync source mapping: voice1←voice3, voice2←voice1, voice3←voice2
    voice1.apply_sync(&voice3, &voice2);
    voice2.apply_sync(&voice1, &voice3);
    voice3.apply_sync(&voice2, &voice1);

    // Step 4: Generate waveform outputs with ring mod baked in.
    // Ring source mapping matches sync: voice1←voice3, etc.
    voice1.set_waveform_output(&voice3);
    voice2.set_waveform_output(&voice1);
    voice3.set_waveform_output(&voice2);

    // Step 4.5: Per-cycle voice mixing, filter processing, and output accumulation.
    //
    // Like reSID, we clock the filter EVERY CPU CYCLE (~985 kHz for PAL).
    // This is critical because songs modulate the filter cutoff rapidly; sample-rate
    // filter processing (44.1 kHz) creates audible stepping/wobbling artifacts.
    //
    // The accumulated post-filter output is averaged at sample time to produce
    // band-limited 44.1 kHz samples (box-filter anti-aliasing).
    {
        // Compute instantaneous centred voice outputs (waveform × envelope).
        // 12-bit waveform centred to [-2048, +2047] × 8-bit envelope [0, 255].
        const float inv_scale = 1.0f / (3.0f * OSCILLATOR_CENTER * ENVELOPE_MAX);
        float v1 = (float)((int32_t)voice1.oscillator_waveform - OSCILLATOR_CENTER)
                 * (float)voice1.envelope_amplitude * inv_scale;
        float v2 = (float)((int32_t)voice2.oscillator_waveform - OSCILLATOR_CENTER)
                 * (float)voice2.envelope_amplitude * inv_scale;
        float v3 = (float)((int32_t)voice3.oscillator_waveform - OSCILLATOR_CENTER)
                 * (float)voice3.envelope_amplitude * inv_scale;

        // Route voices to filtered / unfiltered paths (read register bits).
        float filtered_input = 0.0f;
        float unfiltered_output = 0.0f;
        uint8_t reson = regs[SID_REG_RESON];
        uint8_t sigvol = regs[SID_REG_SIGVOL];
        if (reson & RESON_FILT1) filtered_input += v1; else unfiltered_output += v1;
        if (reson & RESON_FILT2) filtered_input += v2; else unfiltered_output += v2;
        if (reson & RESON_FILT3) filtered_input += v3;
        else if (!(sigvol & SIGVOL_3OFF)) unfiltered_output += v3;
        if (reson & RESON_FILTEX) filtered_input += external_input;
        else unfiltered_output += external_input;

        // Clock the ZDF SVF filter at CPU rate.
        float filtered_output = filter_process(filtered_input);

        // Accumulate post-filter mixed output for box-filter downsampling.
        output_acc += (double)(unfiltered_output + filtered_output);
    }
    sample_cycle_count++;

    // Step 5: Generate output samples at the target sample rate (~44.1 kHz).
    // Average the accumulated per-cycle output, apply master volume and DC blocker.
    if (cpu_clock > 0.0f) {
        sample_accumulator += (double)sample_rate / (double)cpu_clock;

        if (sample_accumulator >= 1.0) {
            sample_accumulator -= 1.0;

            // Average the accumulated filter output over the sample period.
            const float cyc = (sample_cycle_count > 0) ? (float)sample_cycle_count : 1.0f;
            float mixed = (float)(output_acc / (double)cyc);

            // Reset accumulators for next sample period
            output_acc = 0.0;
            sample_cycle_count = 0;

            // 6581 digi support: add constant DC bias from the voice DACs.
            if (revision <= SID_REVISION_6581_R4AR) {
                mixed += SID_6581_DIGI_BIAS;
            }

            // Apply master volume
            uint8_t sigvol = regs[SID_REG_SIGVOL];
            mixed *= (float)(sigvol & SIGVOL_VOL_MASK) / SIGVOL_VOL_MAX;

            // DC blocker: removes the constant bias×volume product while
            // preserving fast changes (digi samples).  ~20 Hz high-pass.
            //   y[n] = x[n] - x[n-1] + α · y[n-1],  α = 0.997
            {
                float dc_out = mixed - dc_blocker_prev_in
                             + DC_BLOCKER_ALPHA * dc_blocker_prev_out;
                dc_blocker_prev_in = mixed;
                dc_blocker_prev_out = dc_out;
                mixed = dc_out;
            }

            // Clamp to [-1, 1]
            if (mixed > 1.0f) mixed = 1.0f;
            if (mixed < -1.0f) mixed = -1.0f;

            sample_buffer.write(mixed);
            samples_generated++;
        }
    }

    cycle_count++;
    total_cycles++;

    return bus_state;
}

// =============================================================================
// SAMPLE GENERATION
// =============================================================================

void mos6581_s::generate_samples(float* output, uint32_t sample_count) {
    if (!output) return;
    
    for (uint32_t i = 0; i < sample_count; i++) {
        output[i] = sample_buffer.read();
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void mos6581_s::set_revision(sid_revision_t rev) {
    revision = rev;
    enable_distortion = (rev <= SID_REVISION_6581_R4AR);
    filter_reset();
}

void mos6581_s::set_timing(bool pal) {
    pal_timing = pal;
    sid_rate = cpu_clock / (pal ? 18.0f : 17.0f);
    filter_update_cutoff();
}

void mos6581_s::set_sample_rate(float rate) {
    sample_rate = rate;
    // Update filter coefficient since w0 depends on sample rate
    filter_update_cutoff();
}

void mos6581_s::set_cpu_clock(float clock_hz) {
    cpu_clock = clock_hz;
    for (int i = 0; i < 3; i++) {
        voices[i]->cpu_clock = clock_hz;
    }
    // Update derived timing values
    sid_rate = clock_hz / (pal_timing ? 18.0f : 17.0f);
    filter_update_cutoff();
}

int voice_s::cycles_per_millisecond() {
    return (int)(cpu_clock / 1000.0f);
}

// =============================================================================
// ENVELOPE TIMING ANALYSIS
// =============================================================================

uint32_t mos6581_s::calculate_envelope_time_ms(voice_t* v, envelope_cycle_t cycle, uint8_t rate) {
    uint32_t period = voice_s::rate_to_period(rate);
    uint32_t cycles_per_ms = v->cycles_per_millisecond();
    
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

void voice_s::write_pulse_waveform_width(uint16_t value) {
    pulse_waveform_width = value & PULSE_WIDTH_MAX;
}

void voice_s::write_control_register_value(uint8_t value) {
    uint8_t prev = control_reg;
    control_reg = value;
    
    // Handle test bit changes
    // Real hardware: test bit only affects the oscillator (zeros accumulator,
    // resets noise LFSR to all 1s). The envelope generator is completely independent.
    // reSID: test bit fades LFSR to 0x7FFFFF over ~35000 cycles; we do it instantly.
    if ((value & VCREG_TEST) && !(prev & VCREG_TEST)) {
        waveform_accumulator = 0;
        noise_lfsr = NOISE_LFSR_TEST;
    }
    
    // Handle gate bit changes
    // Real hardware: the rate counter is NOT reset on gate transition.
    // This is the famous "ADSR bug" — the counter persists, causing variable
    // delay before the first envelope tick after retriggering.
    if ((value ^ prev) & VCREG_GATE) {
        if (value & VCREG_GATE) {
            envelope_cycle = CYCLE_ATTACK;
            uint8_t attack_rate = (sid->regs[voice_index * VOICE_REGS + VOICE_ATDCY] >> 4) & 0x0F;
            envelope_rate_period = rate_to_period(attack_rate);
            // Gate on clears hold_zero so the envelope can restart
            envelope_hold_zero = false;
        } else {
            envelope_cycle = CYCLE_RELEASE;
            uint8_t release_rate = sid->regs[voice_index * VOICE_REGS + VOICE_SUREL] & 0x0F;
            envelope_rate_period = rate_to_period(release_rate);
        }
    }
}

void voice_s::write_attack_decay_register_value(uint8_t value) {
    // Update current rate period if in corresponding cycle.
    if (envelope_cycle == CYCLE_ATTACK) {
        envelope_rate_period = rate_to_period((value >> 4) & 0x0F);
    } else if (envelope_cycle == CYCLE_DECAY) {
        envelope_rate_period = rate_to_period(value & 0x0F);
    }
}

void voice_s::write_sustain_release_register_value(uint8_t value) {
    uint8_t sustain_nibble = (value >> 4) & 0x0F;
    
    // Convert 4-bit sustain to 8-bit by duplicating the nibble.
    // Real SID: 0xF → 0xFF, 0xA → 0xAA, 0x0 → 0x00, etc.
    sustain_level = (sustain_nibble << 4) | sustain_nibble;
    
    // Update current rate period if in release cycle
    if (envelope_cycle == CYCLE_RELEASE) {
        envelope_rate_period = rate_to_period(value & 0x0F);
    }
}

// =============================================================================
// REGISTER ACCESS
// =============================================================================

bus_state_t mos6581_s::registers_write(void* context, bus_state_t bus_state) {
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
                voice->write_pulse_waveform_width((voice->pulse_waveform_width & 0xFF00) | value);
                break;
                
            case VOICE_PWHI:
                voice->write_pulse_waveform_width((voice->pulse_waveform_width & 0x00FF) | ((value & 0x0F) << 8));
                break;
                
            case VOICE_VCREG:
                voice->write_control_register_value(value);
                break;
                
            case VOICE_ATDCY:
                voice->write_attack_decay_register_value(value);
                break;
                
            case VOICE_SUREL:
                voice->write_sustain_release_register_value(value);
                break;
        }
    } else {
        // Global registers
        switch (r) {
            case SID_REG_CUTLO:
                sid->filter_cutoff_frequency = (sid->filter_cutoff_frequency & 0x7F8) | (value & 0x07);
                sid->filter_update_cutoff();
                break;
                
            case SID_REG_CUTHI:
                sid->filter_cutoff_frequency = ((uint16_t)value << 3) | (sid->filter_cutoff_frequency & 0x07);
                sid->filter_update_cutoff();
                break;
                
            case SID_REG_RESON:
                sid->write_resonance_control_register_value(value);
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

bus_state_t mos6581_s::registers_read(void* context, bus_state_t bus_state) {
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

void mos6581_s::init() {
    // Initialize voices with references
    voices[0] = &voice1;
    voices[1] = &voice2;
    voices[2] = &voice3;
    
    // Set voice indices and parent references
    for (int i = 0; i < 3; i++) {
        voices[i]->voice_index = i;
        voices[i]->sid = this;
        voices[i]->cpu_clock = 985248.0f; // PAL C64 default
    }
    
    // Initialize default settings
    revision = SID_REVISION_6581_R4AR;
    pal_timing = true;
    sample_rate = 44100.0f;
    cpu_clock = 985248.0f;   // PAL C64 default
    enable_filter = true;
    enable_distortion = true;
    enable_digiboost = true;
    
    // Initialize ring buffer
    sample_buffer.init(SAMPLE_BUFFER_SIZE);
    
    // Initialize filter
    filter_init();
    
    reset();
}

void mos6581_s::reset() {
    // Reset all registers
    memset(regs, 0, SID_REGS_SIZE);
    bus_value = 0;
    
    // Reset voices
    for (int i = 0; i < 3; i++) {
        voices[i]->reset();
    }
    
    // Reset filter state
    filter_reset();
    
    // Reset SID state
    filter_cutoff_frequency = 0;
    
    // Reset timing
    cycle_count = 0;
    subcycle_count = 0;
    sample_accumulator = 0.0;
    sample_cycle_count = 0;
    output_acc = 0.0;

    // Flush the sample ring buffer so the audio callback doesn't replay
    // stale data from the previous session.
    sample_buffer.write_pos = 0;
    sample_buffer.read_pos = 0;
    
    // Reset volume bug state
    volume_change_click = false;
    volume_click_amplitude = 0.0f;
    volume_click_counter = 0;
    
    // Reset DC blocker state
    dc_blocker_prev_in = 0.0f;
    dc_blocker_prev_out = 0.0f;
    
    // Reset POT values
    pot_x_value = 0xFF;
    pot_y_value = 0xFF;
    
    // Reset external input
    external_input = 0.0f;
    
    // Update timing-dependent values
    set_timing(pal_timing);
}

// Destructor — clean up dynamically allocated ring buffer
mos6581_s::~mos6581_s() {
    sample_buffer.destroy();
}

// ChipBase identity
ChipIdentity mos6581_s::chip_identity() const {
    return ChipIdentity{"MOS6581", "MOS Technology"};
}

/**
 * Consolidated SID tick function - main entry point for SID cycle processing.
 */
bus_state_t mos6581_s::tick(bus_state_t bus_state) {
    return advance_cycle(bus_state);
}
