#include "chip/sound/mos6581.hpp"
#include "chip/sound/sid_waveform_tables.hpp"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>  // For snprintf

// Fast tanh approximation: x / (1 + |x|).
// Monotonic, odd-symmetric, same [-1,+1] range — good enough for SID
// soft-clipping distortion where exactness doesn't matter, and this is
// called ~1M times/sec per filter output (lp/bp/hp).
static inline float fast_tanh(float x) {
    return x / (1.0f + fabsf(x));
}
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
void voice_t::update_exponential_period() {
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

void voice_t::apply_sync(voice_t* sync_source, voice_t* sync_source_source) {
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

uint32_t voice_t::rate_to_period(int rate) {
    return envelope_rate_periods[rate & 0x0F];
}

// Envelope state pipeline change (reSID state_change())
// Models the 2-3 cycle delay for gate transitions observed on real hardware.
void voice_t::envelope_state_change() {
    state_pipeline--;

    switch (envelope_next_state) {
    case CYCLE_ATTACK:
        if (state_pipeline == 0) {
            envelope_cycle = CYCLE_ATTACK;
            // The attack register is correctly activated during second cycle of attack phase
            uint8_t attack_rate = (sid->regs_[voice_index * VOICE_REGS + VOICE_ATDCY] >> 4) & 0x0F;
            envelope_rate_period = rate_to_period(attack_rate);
            envelope_hold_zero = false;
        }
        break;
    case CYCLE_DECAY:
        // DECAY_SUSTAIN transitions are handled elsewhere
        break;
    case CYCLE_RELEASE:
        if ((envelope_cycle == CYCLE_ATTACK && state_pipeline == 0) ||
            (envelope_cycle == CYCLE_DECAY && state_pipeline == 1)) {
            envelope_cycle = CYCLE_RELEASE;
            uint8_t release_rate = sid->regs_[voice_index * VOICE_REGS + VOICE_SUREL] & 0x0F;
            envelope_rate_period = rate_to_period(release_rate);
        }
        break;
    case CYCLE_FREEZED:
        break;
    default:
        break;
    }
}

void voice_t::envelope_clock() {
    // reSID-accurate envelope clocking with full pipeline chain.
    //
    // The reSID envelope uses a multi-stage pipeline:
    //   1. Rate counter matches rate_period → set reset_rate_counter = true
    //   2. Next clock: reset_rate_counter processed → exponential_pipeline set
    //   3. Next clock: exponential_pipeline fires → envelope_pipeline = 1
    //   4. Next clock: envelope_pipeline fires → envelope_counter changes
    //   5. Next clock: env3 latch captures new value
    //
    // This function is called once per clock cycle and manages the entire
    // pipeline chain in the same order as reSID's EnvelopeGenerator::clock().
    
    // Process state pipeline (deferred gate transitions)
    if (state_pipeline) {
        envelope_state_change();
    }
    
    // Stage 2: Process envelope_pipeline (actual envelope counter change)
    if (envelope_pipeline != 0 && (--envelope_pipeline == 0)) {
        if (!envelope_hold_zero) {
            if (envelope_cycle == CYCLE_ATTACK) {
                envelope_amplitude = (envelope_amplitude + 1) & 0xFF;
                if (envelope_amplitude == 0xFF) {
                    envelope_cycle = CYCLE_DECAY;
                    uint8_t decay_rate = sid->regs_[voice_index * VOICE_REGS + VOICE_ATDCY] & 0x0F;
                    envelope_rate_period = rate_to_period(decay_rate);
                }
            } else if (envelope_cycle == CYCLE_DECAY || envelope_cycle == CYCLE_RELEASE) {
                envelope_amplitude = (envelope_amplitude - 1) & 0xFF;
            }
            update_exponential_period();
        }
    }
    
    // Stage 3: Process exponential_pipeline
    if (exponential_pipeline != 0 && (--exponential_pipeline == 0)) {
        exponential_counter = 0;
        if ((envelope_cycle == CYCLE_DECAY && envelope_amplitude != sustain_level) ||
            envelope_cycle == CYCLE_RELEASE) {
            envelope_pipeline = 1;
        }
    }
    // Stage 4: Process reset_rate_counter (deferred from rate counter match)
    else if (reset_rate_counter) {
        envelope_rate_counter = 0;
        reset_rate_counter = false;
        
        if (envelope_cycle == CYCLE_ATTACK) {
            exponential_counter = 0;
            envelope_pipeline = 2;
        } else {
            if (!envelope_hold_zero &&
                ++exponential_counter == exponential_counter_period) {
                // exponential_pipeline delay depends on counter period:
                // period 1 → 1-cycle delay, period > 1 → 2-cycle delay
                exponential_pipeline = (exponential_counter_period != 1) ? 2 : 1;
            }
        }
    }
    
    // Stage 1: Rate counter check
    if (envelope_rate_counter != envelope_rate_period) {
        if (++envelope_rate_counter & 0x8000) {
            ++envelope_rate_counter &= 0x7FFF;
        }
    } else {
        // Rate counter matched — defer processing to next clock
        reset_rate_counter = true;
    }
}

// =============================================================================
// FILTER IMPLEMENTATION
// =============================================================================

// 6581 filter cutoff lookup table — maps the 11-bit register (0–2047) to Hz.
// Built once at first use from the MOS 6581's R-2R DAC transfer function.
// The 6581 DAC has a non-ideal 2R/R ratio of ~2.20 and no termination at bit 0,
// creating the chip's characteristic nonlinear filter sweep curve.
// Table values are: f_min + dac_normalized * (f_max - f_min), where the
// DAC normalization captures the hardware's stepped response.
static float f0_6581[2048];
static bool  f0_6581_ready = false;

static void build_f0_6581() {
    if (f0_6581_ready) return;

    // Build an 11-bit R-2R DAC table with MOS 6581 characteristics.
    // Ported from reSID's dac.cc build_dac_table() (Dag Lem, GPLv2+).
    constexpr int BITS = 11;
    constexpr double _2R_div_R = 2.20;   // Non-ideal 6581 resistor ratio
    constexpr bool   TERM = false;        // Missing termination at bit 0
    constexpr double LEAKAGE = 0.0075;    // Subthreshold MOSFET leakage

    double R  = 1.0;
    double _2R = _2R_div_R * R;
    double vbit[BITS];

    for (int set_bit = 0; set_bit < BITS; set_bit++) {
        double Vn = 1.0;
        double Rn = TERM ? _2R : 1e30;     // No termination → infinite R

        // DAC "tail" resistance by repeated parallel substitution
        for (int bit = 0; bit < set_bit; bit++) {
            if (Rn >= 1e29)
                Rn = R + _2R;
            else
                Rn = R + _2R * Rn / (_2R + Rn);  // R + (2R ∥ Rn)
        }

        // Source transformation for this bit's voltage contribution
        if (Rn >= 1e29) {
            Rn = _2R;
        } else {
            Rn = _2R * Rn / (_2R + Rn);          // 2R ∥ Rn
            Vn = Vn * Rn / _2R;
        }

        // Propagate through remaining bits above this one
        for (int bit = set_bit + 1; bit < BITS; bit++) {
            Rn += R;
            double I = Vn / Rn;
            Rn = _2R * Rn / (_2R + Rn);          // 2R ∥ Rn
            Vn = Rn * I;
        }
        vbit[set_bit] = Vn;
    }

    // Frequency endpoints calibrated to typical 6581 (R4AR 0687 14).
    // These can be tuned per-chip; the DAC shape is the important part.
    constexpr float F_MIN =  220.0f;   // fc=0: ~220 Hz
    constexpr float F_MAX = 14000.0f;  // fc=2047: ~14 kHz

    // Build superposition table and map to frequency
    double dac_max = 0;
    double dac_raw[1 << BITS];
    for (int fc = 0; fc < (1 << BITS); fc++) {
        double Vo = 0;
        int x = fc;
        for (int j = 0; j < BITS; j++) {
            Vo += ((x & 1) ? 1.0 : LEAKAGE) * vbit[j];
            x >>= 1;
        }
        dac_raw[fc] = Vo;
        if (Vo > dac_max) dac_max = Vo;
    }

    for (int fc = 0; fc < (1 << BITS); fc++) {
        float norm = (dac_max > 1e-30) ? (float)(dac_raw[fc] / dac_max) : 0.0f;
        f0_6581[fc] = F_MIN + norm * (F_MAX - F_MIN);
    }

    f0_6581_ready = true;
}

void mos6581_t::filter_update_cutoff() {
    filter_state_t* f = &filter_state;
    
    // Calculate cutoff frequency in Hz from the 11-bit register (0-2047).
    uint16_t fc = filter_cutoff_frequency;
    float cutoff_hz;
    
    if (revision <= SID_REVISION_6581_R4AR) {
        // 6581: use the DAC-accurate lookup table
        build_f0_6581();
        cutoff_hz = f0_6581[fc & 0x7FF];
    } else {
        // 8580: roughly 30 Hz to ~12.5 kHz (more linear DAC, correct termination)
        float normalized = (float)fc / 2048.0f;
        cutoff_hz = 30.0f + normalized * 12470.0f;
    }
    
    f->cutoff_frequency = cutoff_hz;
    
    // ZDF SVF: g = tan(π * fc / fs).
    // This is the topology-preserving integrator gain that ensures
    // unconditional stability at any cutoff/resonance combination.
    //
    // The filter is clocked every CPU cycle (~985 kHz PAL), so we use the
    // CPU clock as the effective sample rate for coefficient calculation.
    float rate = cpu_clock > 0.0f ? cpu_clock : SID_DEFAULT_CPU_CLOCK_PAL;
    
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

#ifdef _MSC_VER
__forceinline
#else
__attribute__((always_inline)) inline
#endif
float mos6581_t::filter_process(float input) {
    if (!enable_filter) return input;
    
    filter_state_t* f = &filter_state;
    
    // ZDF (zero-delay feedback) topology-preserving SVF
    // (Andy Simper / Cytomic / Vadim Zavalishin).
    //
    // Unlike the naive SVF, this formulation is UNCONDITIONALLY STABLE
    // at any cutoff frequency and resonance setting.  The filter is
    // clocked every CPU cycle (~985 kHz PAL), matching reSID's approach
    // for accurate cutoff tracking during rapid filter sweeps.
    
    // 6581 VCR conductance modulation: the real MOS transistor VCR's
    // conductance is signal-dependent — it drops as the integrator
    // voltage departs from the working point.  This effectively lowers
    // the cutoff frequency during resonant peaks, providing natural
    // amplitude limiting on top of the op-amp saturation.
    // Approximate as g_eff = g / (1 + α·V²) where V is the peak
    // integrator state.
    float g, a1, a2, a3;
    if (f->enable_distortion) {
        constexpr float VCR_MOD = 20.0f;
        float p1 = f->ic1eq > 0 ? f->ic1eq : -f->ic1eq;
        float p2 = f->ic2eq > 0 ? f->ic2eq : -f->ic2eq;
        float peak = p1 > p2 ? p1 : p2;
        g = f->g / (1.0f + VCR_MOD * peak * peak);
        float k = f->k;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    } else {
        g = f->g;
        a1 = f->a1;
        a2 = f->a2;
        a3 = f->a3;
    }
    
    float v3 = input - f->ic2eq;
    float v1 = a1 * f->ic1eq + a2 * v3;
    float v2 = f->ic2eq + a2 * f->ic1eq + a3 * v3;
    
    // Update integrator states
    f->ic1eq = 2.0f * v1 - f->ic1eq;
    f->ic2eq = 2.0f * v2 - f->ic2eq;
    
    // 6581 op-amp saturation: the real NMOS inverters clip when the node
    // voltage departs far from the working point (~4.54 V out of 0–10.3 V).
    // Soft-clipping the integrator states INSIDE the feedback loop gives each
    // cycle a naturally bounded input, matching how the real op-amp limits
    // gain and preventing infinite oscillation at high Q.
    // SAT_DRIVE maps the integrator range into tanh's ±1 active zone;
    // SAT_MAKEUP restores the peak swing.  Net effect: linear below ~1.5,
    // soft-clips above ~3.0.
    constexpr float SAT_DRIVE  = 0.33f;
    constexpr float SAT_MAKEUP = 3.0f;
    if (f->enable_distortion) {
        f->ic1eq = fast_tanh(f->ic1eq * SAT_DRIVE) * SAT_MAKEUP;
        f->ic2eq = fast_tanh(f->ic2eq * SAT_DRIVE) * SAT_MAKEUP;
    }
    
    // Filter outputs
    float lp = v2;
    float bp = v1;
    float hp = input - f->k * v1 - v2;
    
    // Store outputs
    f->low_pass_output = lp;
    f->band_pass_output = bp;
    f->high_pass_output = hp;
    
    // Mix filter outputs based on selected filter mode (cached on register write)
    float output = 0.0f;
    if (filter_lp) output += lp;
    if (filter_bp) output += bp;
    if (filter_hp) output += hp;
    
    return output;
}

void mos6581_t::filter_reset() {
    filter_state_t* f = &filter_state;
    
    f->cutoff_frequency = 0.0f;
    f->resonance = 0.0f;
    f->low_pass_output = 0.0f;
    f->band_pass_output = 0.0f;
    f->high_pass_output = 0.0f;
    f->g = 0.0f;
    f->k = 1.0f / 0.707f; // ~1.414 — Butterworth (res=0 default, both models)
    // Note: 6581 at res=0 should be 15/8=1.875, but filter_reset() is followed
    // by a register write that sets the correct revision-dependent k value.
    f->a1 = 0.0f;
    f->a2 = 0.0f;
    f->a3 = 0.0f;
    f->ic1eq = 0.0f;
    f->ic2eq = 0.0f;
    f->enable_distortion = (revision <= SID_REVISION_6581_R4AR);
}

void mos6581_t::filter_init() {
    filter_reset();
}

void mos6581_t::update_cached_audio_constants() {
    // inv_scale: normalise 3 voices at peak excursion to ~±1.
    // Always uses OSCILLATOR_CENTER (half the DAC range) as the reference,
    // regardless of wave_zero_. The wave_zero_ offset creates the correct
    // DC shift within this normalization. This matches how all the calibrated
    // parameters (voice_dc_, output_gain_) were tuned.
    // With 6581's wave_zero=0x380, individual voices can exceed ±0.333 on the
    // positive side (up to ~0.52), but output_gain_=0.33 and final clamp ensure
    // the output stays in [-1, +1].
    inv_scale_ = 1.0f / (3.0f * (float)OSCILLATOR_CENTER * 255.0f);

    // vol_scaled_ combines master_volume/15 with output_gain_ to avoid
    // per-cycle integer→float conversion and multiplication.
    vol_scaled_ = ((float)master_volume / 15.0f) * output_gain_;
}
// =============================================================================
// FILTER REGISTER WRITERS
// =============================================================================

void mos6581_t::write_resonance_control_register_value(uint8_t value) {
    // Cache decoded routing bits for per-cycle use
    filt1  = (value & RESON_FILT1) != 0;
    filt2  = (value & RESON_FILT2) != 0;
    filt3  = (value & RESON_FILT3) != 0;
    filtex = (value & RESON_FILTEX) != 0;

    // Extract resonance nibble for the float computation.
    filter_state.resonance = (float)((value >> RESON_RES_SHIFT) & 0x0F);
    
    // Recompute k (damping = 1/Q) and derived SVF coefficients.
    //
    // 6581: The feedback gain in the real SID is set by a resistor network
    // controlled by the resonance bits.  reSID encodes this as:
    //   _8_div_Q = ~res & 0x0f      (inverted 4-bit nibble)
    // giving k = _8_div_Q / 8.  This yields:
    //   res=0  → k=15/8=1.875  (Q≈0.533, heavily damped)
    //   res=8  → k=7/8=0.875   (Q≈1.14)
    //   res=14 → k=1/8=0.125   (Q=8)
    //   res=15 → k=0            (self-oscillation)
    // The real chip's op-amp saturation naturally limits the resonant peak
    // at high Q.  We model this by soft-clipping the integrator states in
    // filter_process() (see below).
    //
    // 8580: A different resistor ladder gives an exponential Q curve:
    //   1/Q = 2^((4 - res)/8)
    //   res=0  → k≈1.414  (Q≈0.707, Butterworth)
    //   res=15 → k≈0.386  (Q≈2.59, moderate resonance)
    // No self-oscillation, no op-amp saturation needed.
    filter_state_t* f = &filter_state;
    int res_int = (int)f->resonance;  // 0-15
    
    if (revision <= SID_REVISION_6581_R4AR) {
        // 6581: linear resistor ladder model.  Clamp k to a small minimum
        // so the ZDF SVF doesn't produce infinite output when res=15.
        // The saturation model in filter_process() provides the real limiting.
        f->k = (float)(15 - res_int) / 8.0f;
        if (f->k < 0.02f) f->k = 0.02f;  // Q_max ≈ 50
    } else {
        // 8580: exponential resistor ladder, matches reSID's lookup table.
        f->k = powf(2.0f, (4.0f - (float)res_int) / 8.0f);
    }
    
    float g = f->g;
    float k = f->k;
    f->a1 = 1.0f / (1.0f + g * (g + k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}

// SIGVOL register (0x18): decoded fields cached in voice3_off, filter_lp/bp/hp,
// master_volume — updated on register write for per-cycle use.

// =============================================================================
// WAVEFORM GENERATION
// =============================================================================

static inline uint32_t voice_generate_triangle(uint32_t accumulator) {
    // Triangle wave: fold at MSB. XOR with max flips the ramp direction.
    uint32_t folded = (accumulator & WAVEFORM_ACCUMULATOR_MSB)
                    ? (accumulator ^ WAVEFORM_ACCUMULATOR_MAX) : accumulator;
    return folded >> 11;
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

void voice_t::reset() {
    // Reset register values
    frequency = 0;
    pulse_waveform_width = 0;
    control_reg = 0;
    sustain_level = 0;
    
    // Internal state — reSID: accumulator and envelope_counter are NOT
    // changed on reset (only set once at power-on in the constructor).
    // waveform_accumulator and envelope_amplitude are preserved.
    envelope_accumulator = 0;
    envelope_cycle = CYCLE_RELEASE;     // reSID: state = RELEASE after reset
    envelope_rate_counter = 0;
    envelope_rate_period = rate_to_period(0);  // release rate 0
    envelope_hold_zero = false;
    exponential_counter = 0;
    exponential_counter_period = 1;
    envelope_pipeline = 0;
    exponential_pipeline = 0;
    reset_rate_counter = false;
    state_pipeline = 0;
    envelope_next_state = CYCLE_RELEASE;
    
    // Reset waveform outputs
    oscillator_waveform = 0;
    triangle_output = OSCILLATOR_MAX;
    sawtooth_output = OSCILLATOR_MAX;
    pulse_output = OSCILLATOR_MAX;      // reSID: pulse_output = 0xfff after reset
    combined_output = 0;
    
    // Reset noise state — reSID: shift_register = 0x7FFFFE after reset,
    // then clocked once. Latch noise_output immediately.
    noise_lfsr = 0x7FFFFE;
    noise_output = noise_lfsr_to_output(noise_lfsr);
    noise_clock_enable = false;
    shift_pipeline = 0;
    shift_register_reset = 0;
    
    // Reset sync state
    prev_accumulator = 0;
    sync_trigger = false;
    
    // Reset outputs
    oscillator_output = 0;
    envelope_output = 0;
    result = 0;

    // Refresh cached waveform lookup state
    update_cached_waveform_state();
}

// Pre-compute waveform table pointer and bitmasks from control_reg / model.
// Called on control register write, model change, and reset.
void voice_t::update_cached_waveform_state() {
    uint8_t wf = control_reg;
    int wf_index = (wf >> 4) & 0x7;
    cached_wave_table = sid_tables::model_wave[model_index][wf_index];
    cached_ring_msb_mask = ((wf & VCREG_RING) && !(wf & WAVEFORM_SAWTOOTH))
                         ? WAVEFORM_ACCUMULATOR_MSB : 0;
    cached_no_pulse_mask = (wf & WAVEFORM_PULSE) ? 0x000 : 0xFFF;
    cached_no_noise_mask = (wf & WAVEFORM_NOISE) ? 0x000 : 0xFFF;
    cached_wf_mask = wf & WAVEFORM_MASK;
}

void voice_t::clock_cycle() {
    // Store previous accumulator for sync detection
    prev_accumulator = waveform_accumulator;
    
    // Update accumulator unless test bit is set
    if (control_reg & VCREG_TEST) {
        // Test bit: lock accumulator at 0, force pulse high.
        // reSID: shift_register gradually fades to 0x7FFFFF over many cycles
        // via a countdown timer. We model the same countdown.
        if (shift_register_reset && !--shift_register_reset) {
            noise_lfsr = 0x7FFFFF;  // All bits → 1
            noise_output = noise_lfsr_to_output(noise_lfsr);
        }
        // Pulse output is forced high while test bit is set (reSID behavior)
        pulse_output = OSCILLATOR_MAX;
    } else {
        // Normal operation: advance accumulator
        waveform_accumulator = (waveform_accumulator + frequency) & WAVEFORM_ACCUMULATOR_MAX;
    }
    
    // Detect MSB change for sync
    bool msb_rising = ((prev_accumulator & WAVEFORM_ACCUMULATOR_MSB) == 0) && 
                      ((waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0);
    sync_trigger = msb_rising;
    
    // Noise LFSR shift with 2-cycle pipeline delay (reSID-accurate).
    // When accumulator bit 19 rises, set shift_pipeline = 2.
    // Decrement each cycle; when it reaches 0, clock the shift register.
    // When test bit is active, the clock() returns early in reSID — no shifting.
    if (!(control_reg & VCREG_TEST)) {
        bool bit19_now = (waveform_accumulator & ACC_BIT19) != 0;
        bool bit19_was = (prev_accumulator & ACC_BIT19) != 0;
        if (bit19_now && !bit19_was) {
            // Rising edge of bit 19 — start pipeline
            shift_pipeline = 2;
        } else if (shift_pipeline && !--shift_pipeline) {
            // Pipeline expired — clock the shift register
            uint32_t feedback = ((noise_lfsr >> 22) ^ (noise_lfsr >> 17)) & 1;
            noise_lfsr = ((noise_lfsr << 1) | feedback) & NOISE_LFSR_MASK;
            noise_output = noise_lfsr_to_output(noise_lfsr);
        }
    }
    
    // Sample ENV3 latch BEFORE any envelope changes (reSID: env3 = envelope_counter
    // is captured at the start of each clock, before rate counter and state processing).
    envelope_output = envelope_amplitude;
    
    // Update envelope — full pipeline chain runs inside envelope_clock()
    envelope_clock();
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
void voice_t::set_waveform_output(voice_t* ring_source) {
    const uint8_t wf = control_reg;

    if (cached_wf_mask) {
        // Lookup index: ring-modified accumulator, upper 12 bits.
        // reSID: ix = (accumulator ^ (~sync_source->accumulator & ring_msb_mask)) >> 12
        const int ix = (waveform_accumulator
                  ^ (~ring_source->waveform_accumulator & cached_ring_msb_mask)) >> 12;

        // Combined waveform output from cached table pointer and pre-computed masks.
        uint32_t waveform_output = cached_wave_table[ix]
                                 & (cached_no_pulse_mask | pulse_output)
                                 & (cached_no_noise_mask | noise_output);
        
        oscillator_waveform = waveform_output;
        oscillator_output = (uint8_t)(waveform_output >> 4);

        // 6581-specific: accumulator MSB driven low by combined waveform output.
        // When sawtooth is selected along with any other waveform(s), the top
        // bit of the accumulator can be pulled low. This is a side-effect of
        // the analog waveform bus being connected back to the accumulator on
        // the 6581 die.
        const uint8_t wf_sel = (wf >> 4) & 0xF;
        if ((wf_sel & 0x2) && (wf_sel & 0xD) && model_index == 0) {
            waveform_accumulator &= (waveform_output << 12) | 0x7FFFFF;
        }

        // Combined waveforms write to the shift register (reSID behavior).
        // When waveform > 0x8 (noise + any other), the noise shift register
        // gets bits written back, eventually zeroing out.
        const int wf_index = (wf >> 4) & 0x7;
        if ((wf_index > 0) && (wf & WAVEFORM_NOISE) && !(wf & VCREG_TEST) && shift_pipeline != 1) {
            // Write waveform output back into noise shift register
            // This causes combined noise waveforms to decay to zero
            noise_lfsr &= 0x7fffff;
            noise_lfsr |= ((waveform_output & (1 << 11)) ? (1 << 20) : 0);
            noise_lfsr |= ((waveform_output & (1 <<  8)) ? (1 << 18) : 0);
            noise_lfsr |= ((waveform_output & (1 <<  5)) ? (1 << 14) : 0);
            noise_lfsr |= ((waveform_output & (1 <<  3)) ? (1 << 11) : 0);
            noise_lfsr |= ((waveform_output & (1 <<  1)) ? (1 <<  9) : 0);
            noise_lfsr |= ((waveform_output & (1 <<  0)) ? (1 <<  5) : 0);
            noise_output = noise_lfsr_to_output(noise_lfsr);
        }
    } else {
        // No waveform selected → floating DAC, simplified as zero.
        oscillator_waveform = 0;
        oscillator_output = 0;
    }
    
    // Apply envelope to waveform (result includes ring mod)
    result = (oscillator_waveform * envelope_amplitude) >> 8;
    
    // Pulse output pipeline: the result of the pulse width compare is delayed
    // one cycle (reSID behavior). Compute for the NEXT cycle's use.
    // When test bit is set, pulse output stays forced high (handled in clock_cycle).
    if (!(wf & VCREG_TEST)) {
        pulse_output = (wf & WAVEFORM_PULSE)
            ? (((waveform_accumulator >> 12) >= pulse_waveform_width) ? OSCILLATOR_MAX : 0)
            : OSCILLATOR_MAX;
    }
}

// =============================================================================
// MAIN CYCLE FUNCTION
// =============================================================================

// Unified bus state threading main cycle function
inline bus_state_t mos6581_t::advance_cycle(bus_state_t bus_state) {
    // reSID per-cycle order:
    //   1. Clock envelopes  (inside voice_clock_cycle)
    //   2. Clock oscillators (accumulator + noise LFSR)
    //   3. Synchronize oscillators (hard sync)
    //   4. Generate waveform output (with ring mod baked in)
    //   5. Clock filter → generate sample

    // Steps 1-2: Clock accumulators, noise, and envelopes
    voice1.clock_cycle();
    voice2.clock_cycle();
    voice3.clock_cycle();

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
    // Volume is applied per-cycle (not per-sample) so that rapid $D418 writes
    // correctly modulate the voice DC offset — this is how 6581 "digi" playback
    // works.  The accumulated post-filter output is averaged at sample time.
    {
        // Compute instantaneous centred voice outputs (waveform × envelope).
        // 12-bit waveform offset by wave_zero_ (revision-dependent) × 8-bit envelope.
        // 6581: wave_zero=0x380 → asymmetric centering, large positive DC bias.
        // 8580: wave_zero=0x800 → symmetric centering, no DC offset.
        // inv_scale_ normalises 3 voices at peak excursion to ~[-1, +1],
        // accounting for the asymmetric range when wave_zero ≠ 0x800.
        float v1 = (float)((int32_t)voice1.oscillator_waveform - wave_zero_)
                 * (float)voice1.envelope_amplitude * inv_scale_;
        float v2 = (float)((int32_t)voice2.oscillator_waveform - wave_zero_)
                 * (float)voice2.envelope_amplitude * inv_scale_;
        float v3 = (float)((int32_t)voice3.oscillator_waveform - wave_zero_)
                 * (float)voice3.envelope_amplitude * inv_scale_;

        // Route voices to filtered / unfiltered paths (cached on register write).
        float filtered_input = 0.0f;
        float unfiltered_output = 0.0f;
        if (filt1) filtered_input += v1; else unfiltered_output += v1;
        if (filt2) filtered_input += v2; else unfiltered_output += v2;
        if (filt3) filtered_input += v3;
        else if (!voice3_off) unfiltered_output += v3;
        if (filtex) filtered_input += external_input;
        else unfiltered_output += external_input;

        // Clock the ZDF SVF filter at CPU rate.
        float filtered_output = filter_process(filtered_input);

        // Apply master volume per-cycle using cached vol_scaled_.
        // vol_scaled_ = (master_volume / 15) * output_gain_, updated on register write.
        // voice_dc_: 6581 mixer DC bias for digi playback (see header comments).
        float total = (unfiltered_output + filtered_output + voice_dc_)
                    * vol_scaled_;

        // CIC-3 (3rd-order Cascaded Integrator-Comb) decimation filter.
        // Three cascaded running sums produce a 3rd-order B-spline window
        // with -39 dB first sidelobe (vs -13 dB for plain box filter).
        // Cost: two extra additions per cycle.
        cic_s1 += total;
        cic_s2 += cic_s1;
        cic_s3 += cic_s2;
    }
    sample_cycle_count++;

    // Step 5: Generate output samples at the target sample rate (~44.1 kHz).
    // The CIC-3 integrate-and-dump produces a well-filtered decimated output
    // with only ~2 extra additions per cycle vs the old box filter.
    // Note: sample_rate_ratio is 0 when cpu_clock is unset, so no samples
    // are generated until timing is configured — no explicit guard needed.
    sample_accumulator += sample_rate_ratio;

    if (sample_accumulator >= 1.0f) {
        sample_accumulator -= 1.0f;

        // CIC-3 output: triple running sum normalized by N*(N+1)*(N+2)/6,
        // where N is the number of CPU cycles in this sample period.
        // This equals three cascaded box filters (integrate-and-dump),
        // yielding a smooth B-spline decimation window.
        const float N = (sample_cycle_count > 0) ? (float)sample_cycle_count : 1.0f;
        float norm = N * (N + 1.0f) * (N + 2.0f) / 6.0f;
        float mixed = cic_s3 / norm;

        // Reset integrators for next sample period
        cic_s1 = 0.0f;
        cic_s2 = 0.0f;
        cic_s3 = 0.0f;
        sample_cycle_count = 0;

        // DC blocker: removes the constant bias×volume product while
        // preserving fast changes (digi samples).  ~20 Hz high-pass.
        //   y[n] = x[n] - x[n-1] + α · y[n-1],  α = 0.997
        {
            float dc_out = mixed - dc_blocker_prev_in
                         + 0.997f * dc_blocker_prev_out;
            dc_blocker_prev_in = mixed;
            dc_blocker_prev_out = dc_out;
            mixed = dc_out;
        }

        // Clamp to [-1, 1]
        if (mixed > 1.0f) mixed = 1.0f;
        if (mixed < -1.0f) mixed = -1.0f;

        sample_buffer.write(&mixed, 1);
        samples_generated++;
    }

    cycle_count++;
    total_cycles++;

    return bus_state;
}

// =============================================================================
// SAMPLE GENERATION
// =============================================================================

void mos6581_t::generate_samples(float* output, uint32_t sample_count) {
    if (!output) return;
    
    uint32_t got = static_cast<uint32_t>(
        sample_buffer.read(output, static_cast<size_t>(sample_count)));
    // Silence-fill any remaining slots (preserves old read()-returns-0.0f behavior)
    for (uint32_t i = got; i < sample_count; i++) {
        output[i] = 0.0f;
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void mos6581_t::set_revision(sid_revision_t rev) {
    revision = rev;
    enable_distortion = (rev <= SID_REVISION_6581_R4AR);
    bool is_6581 = (rev <= SID_REVISION_6581_R4AR);
    wave_zero_ = is_6581 ? 0x380 : 0x800;
    voice_dc_ = is_6581 ? 1.5f : 0.0f;
    output_gain_ = is_6581 ? 0.33f : 1.0f;
    update_cached_audio_constants();
    // Cache model index in each voice for per-cycle waveform table lookup
    int mi = is_6581 ? 0 : 1;
    voice1.model_index = mi;
    voice2.model_index = mi;
    voice3.model_index = mi;
    voice1.update_cached_waveform_state();
    voice2.update_cached_waveform_state();
    voice3.update_cached_waveform_state();
    filter_reset();
}

void mos6581_t::set_timing(bool pal) {
    pal_timing = pal;
    sid_rate = cpu_clock / (pal ? 18.0f : 17.0f);
    filter_update_cutoff();
}

void mos6581_t::set_sample_rate(float rate) {
    sample_rate = rate;
    if (cpu_clock > 0.0f)
        sample_rate_ratio = sample_rate / cpu_clock;
    // Update filter coefficient since w0 depends on sample rate
    filter_update_cutoff();
}

void mos6581_t::set_cpu_clock(float clock_hz) {
    cpu_clock = clock_hz;
    if (cpu_clock > 0.0f)
        sample_rate_ratio = sample_rate / cpu_clock;
    for (int i = 0; i < 3; i++) {
        voices[i]->cpu_clock = clock_hz;
    }
    // Update derived timing values
    sid_rate = clock_hz / (pal_timing ? 18.0f : 17.0f);
    filter_update_cutoff();
}

int voice_t::cycles_per_millisecond() {
    return (int)(cpu_clock / 1000.0f);
}

// =============================================================================
// ENVELOPE TIMING ANALYSIS
// =============================================================================

uint32_t mos6581_t::calculate_envelope_time_ms(voice_t* v, envelope_cycle_t cycle, uint8_t rate) {
    uint32_t period = voice_t::rate_to_period(rate);
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

void voice_t::write_pulse_waveform_width(uint16_t value) {
    pulse_waveform_width = value & 0xFFF;
}

// Number of cycles for shift register to fully reset to 0x7FFFFF.
// reSID: ~0x8000 cycles on 6581, different on 8580.
static const uint32_t SHIFT_REGISTER_FADE_6581 = 0x8000;
static const uint32_t SHIFT_REGISTER_FADE_8580 = 0x950000;

void voice_t::write_control_register_value(uint8_t value) {
    uint8_t prev = control_reg;
    control_reg = value;

    // Refresh cached waveform table pointer and bitmasks
    update_cached_waveform_state();
    
    // Handle test bit rising edge
    // reSID: accumulator is cleared, shift register begins fading to 0x7FFFFF
    // over many cycles (countdown). Pulse output is forced high.
    if ((value & VCREG_TEST) && !(prev & VCREG_TEST)) {
        waveform_accumulator = 0;
        shift_pipeline = 0;  // Flush any pending shift
        // Start countdown for gradual LFSR fade to all 1s
        bool is_6581 = sid && (sid->revision <= SID_REVISION_6581_R4AR);
        shift_register_reset = is_6581 ? SHIFT_REGISTER_FADE_6581 : SHIFT_REGISTER_FADE_8580;
        pulse_output = OSCILLATOR_MAX;  // Test bit forces pulse high
    }
    // Handle test bit falling edge
    // reSID: shift register is clocked once and noise output updated.
    else if (!(value & VCREG_TEST) && (prev & VCREG_TEST)) {
        // bit0 = (~shift_register >> 17) & 1
        uint32_t bit0 = (~noise_lfsr >> 17) & 1;
        noise_lfsr = ((noise_lfsr << 1) | bit0) & NOISE_LFSR_MASK;
        noise_output = noise_lfsr_to_output(noise_lfsr);
        shift_register_reset = 0;
    }
    
    // Handle gate bit changes using reSID state_pipeline mechanism.
    // Real hardware delays state transitions by 2-3 cycles via internal pipeline.
    // The rate counter is NOT reset on gate transition (the "ADSR bug").
    if ((value ^ prev) & VCREG_GATE) {
        if (value & VCREG_GATE) {
            // Gate on: Start attack via pipeline.
            // The decay register is "accidentally" activated during first cycle of attack phase.
            envelope_next_state = CYCLE_ATTACK;
            envelope_cycle = CYCLE_DECAY;  // "Accidentally" activate decay state first
            uint8_t decay_rate = sid->regs_[voice_index * VOICE_REGS + VOICE_ATDCY] & 0x0F;
            envelope_rate_period = rate_to_period(decay_rate);
            state_pipeline = 2;
            if (reset_rate_counter || exponential_pipeline == 2) {
                envelope_pipeline = (exponential_counter_period == 1 || exponential_pipeline == 2) ? 2 : 4;
            } else if (exponential_pipeline == 1) {
                state_pipeline = 3;
            }
        } else {
            // Gate off: Start release via pipeline.
            envelope_next_state = CYCLE_RELEASE;
            state_pipeline = (envelope_pipeline > 0) ? 3 : 2;
        }
    }

    // reSID: update waveform output after any control register write.
    // This recomputes pulse_output pipeline and combined waveform DAC,
    // critical for correct behavior after test bit transitions.
    if (value & WAVEFORM_MASK) {
        voice_t* ring_source = sid->voices[(voice_index + 2) % 3];
        set_waveform_output(ring_source);
    }
}

void voice_t::write_attack_decay_register_value(uint8_t value) {
    // Update current rate period if in corresponding cycle.
    if (envelope_cycle == CYCLE_ATTACK) {
        envelope_rate_period = rate_to_period((value >> 4) & 0x0F);
    } else if (envelope_cycle == CYCLE_DECAY) {
        envelope_rate_period = rate_to_period(value & 0x0F);
    }
}

void voice_t::write_sustain_release_register_value(uint8_t value) {
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

bus_state_t mos6581_t::registers_write(void* context, bus_state_t bus_state) {
    mos6581_t* sid = (mos6581_t*)context;
    if (!sid) return bus_state;
    
    uint32_t r = BUS_GET_ADDR(bus_state) & sid_constants::REGS_MASK;
    uint8_t value = BUS_GET_DATA(bus_state);
    sid->bus_value = value; // Store for potential bus reads
    
    if (r < (3 * VOICE_REGS)) { // Voice registers (0x00-0x14)
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
                // Cache decoded SIGVOL bits for per-cycle use
                sid->voice3_off   = (value & SIGVOL_3OFF) != 0;
                sid->filter_lp    = (value & SIGVOL_LP) != 0;
                sid->filter_bp    = (value & SIGVOL_BP) != 0;
                sid->filter_hp    = (value & SIGVOL_HP) != 0;
                sid->master_volume = value & SIGVOL_VOL_MASK;
                sid->vol_scaled_ = ((float)sid->master_volume / 15.0f)
                                 * sid->output_gain_;
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
    if (r < sid_constants::REGS_SIZE) {
        sid->regs_[r] = value;
    }

    // Write-capture callback (for SID register logging / test harnesses)
    if (unlikely(sid->write_capture_fn != nullptr)) {
        sid->write_capture_fn(sid->write_capture_ctx,
                              sid->total_cycles, (uint8_t)r, value);
    }
    
    return bus_state;
}

bus_state_t mos6581_t::registers_read(void* context, bus_state_t bus_state) {
    mos6581_t* sid = (mos6581_t*)context;
    if (!sid) return bus_state;
    
    uint32_t r = BUS_GET_ADDR(bus_state) & sid_constants::REGS_MASK;
    
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
            // Real hardware: ENV3 reads a latched value captured at the start
            // of each clock cycle, before the envelope counter is modified.
            // This matches reSID's env3 latch behavior.
            BUS_SET_DATA(bus_state, sid->voice3.envelope_output);
            break;
            
        case SID_REG_UNUSED_START:
        case SID_REG_UNUSED_START + 1:
        case 0x1F:
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

void mos6581_t::init() {
    // Initialize combined waveform lookup tables (once, thread-safe via static guard)
    sid_tables::init_waveform_tables();
    
    // Initialize voices with references
    voices[0] = &voice1;
    voices[1] = &voice2;
    voices[2] = &voice3;
    
    // Set voice indices and parent references
    for (int i = 0; i < 3; i++) {
        voices[i]->voice_index = i;
        voices[i]->sid = this;
        voices[i]->cpu_clock = SID_DEFAULT_CPU_CLOCK_PAL; // PAL C64 default
        // Power-on values — only set here, NOT on reset (reSID behavior).
        // Real hardware: accumulator even bits high, envelope odd bits high.
        voices[i]->waveform_accumulator = 0x555555;  // 0x555555
        voices[i]->envelope_amplitude = 0xAA;                 // Odd bits high
    }
    
    // Initialize default settings
    revision = SID_REVISION_6581_R4AR;
    pal_timing = true;
    sample_rate = 44100.0f;
    cpu_clock = SID_DEFAULT_CPU_CLOCK_PAL;   // PAL C64 default
    enable_filter = true;
    enable_distortion = true;
    enable_digiboost = true;
    
    // Initialize filter
    filter_init();
    
    // Precompute cached audio constants
    update_cached_audio_constants();
    
    reset();
#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
}

// ============================================================================
// Debug field registration (ChipDebugRegistry)
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void mos6581_t::register_debug_fields() {
    using S = const mos6581_t;
    auto& r = debug_registry_;
    r.set_registers(regs_, sid_constants::REGS_SIZE, SID_REG_INFO, 0xD400);
    r.set_decl_entries(SID_DECL_ENTRIES.data(), SID_DECL_ENTRIES.size());

    static constexpr const char* wf_names[] = {
        "None", "Triangle", "Sawtooth", "Saw+Tri",
        "Pulse", "Pulse+Tri", "Pulse+Saw", "Pulse+Saw+Tri", "Noise"
    };
    static constexpr const char* env_names[] = {
        "Attack", "Decay", "Sustain", "Release", "Off"
    };

    // DECL walk handles control register bitfields (Gate/Sync/Ring/Test),
    // attack/decay/sustain/release, filter routing, resonance, volume,
    // filter modes, and Voice 3 Off.

    // ---- Voice 1 (internal state) ----
    r.category("Voice 1", false)
     .value("Frequency", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.frequency; }, 16)
     .value("Pulse Width", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.pulse_waveform_width; }, 12)
     .state("Waveform", +[](const ChipBase* c) -> uint32_t {
         return (static_cast<S*>(c)->voice1.control_reg >> 4) & 0x0F;
     }, wf_names, 9)
     .value("Envelope Amp", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.envelope_amplitude; }, 8)
     .value("Oscillator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.oscillator_waveform; }, 12)
     .signed_value("Result", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<S*>(c)->voice1.result); }, 24)
     .value("Accumulator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.waveform_accumulator; }, 24)
     .state("Env Cycle", +[](const ChipBase* c) -> uint32_t {
         return static_cast<uint32_t>(static_cast<S*>(c)->voice1.envelope_cycle);
     }, env_names, 5)
     .value("Exp Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.exponential_counter; }, 8)
     .value("Sustain Level", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice1.sustain_level; }, 8);

    // ---- Voice 2 (internal state) ----
    r.category("Voice 2", false)
     .value("Frequency", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.frequency; }, 16)
     .value("Pulse Width", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.pulse_waveform_width; }, 12)
     .state("Waveform", +[](const ChipBase* c) -> uint32_t {
         return (static_cast<S*>(c)->voice2.control_reg >> 4) & 0x0F;
     }, wf_names, 9)
     .value("Envelope Amp", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.envelope_amplitude; }, 8)
     .value("Oscillator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.oscillator_waveform; }, 12)
     .signed_value("Result", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<S*>(c)->voice2.result); }, 24)
     .value("Accumulator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.waveform_accumulator; }, 24)
     .state("Env Cycle", +[](const ChipBase* c) -> uint32_t {
         return static_cast<uint32_t>(static_cast<S*>(c)->voice2.envelope_cycle);
     }, env_names, 5)
     .value("Exp Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.exponential_counter; }, 8)
     .value("Sustain Level", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice2.sustain_level; }, 8);

    // ---- Voice 3 (internal state) ----
    r.category("Voice 3", false)
     .value("Frequency", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.frequency; }, 16)
     .value("Pulse Width", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.pulse_waveform_width; }, 12)
     .state("Waveform", +[](const ChipBase* c) -> uint32_t {
         return (static_cast<S*>(c)->voice3.control_reg >> 4) & 0x0F;
     }, wf_names, 9)
     .value("Envelope Amp", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.envelope_amplitude; }, 8)
     .value("Oscillator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.oscillator_waveform; }, 12)
     .signed_value("Result", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<S*>(c)->voice3.result); }, 24)
     .value("Accumulator", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.waveform_accumulator; }, 24)
     .state("Env Cycle", +[](const ChipBase* c) -> uint32_t {
         return static_cast<uint32_t>(static_cast<S*>(c)->voice3.envelope_cycle);
     }, env_names, 5)
     .value("Exp Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.exponential_counter; }, 8)
     .value("Sustain Level", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->voice3.sustain_level; }, 8);

    // ---- Filter & Global (combined cutoff not in DECL) ----
    r.category("Filter & Global")
     .value("Filter Cutoff", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->filter_cutoff_frequency; }, 11)
     .value("Buffer Samples", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->sample_buffer.available(); }, 16);
}
#endif // CERMU_HAS_CHIP_DEBUG

void mos6581_t::reset() {
    // Reset all registers
    memset(regs_, 0, num_regs_);
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
    sample_accumulator = 0.0f;
    sample_cycle_count = 0;
    
    // Reset CIC-3 decimation filter state
    cic_s1 = 0.0f;
    cic_s2 = 0.0f;
    cic_s3 = 0.0f;

    // Flush the sample ring buffer so the audio callback doesn't replay
    // stale data from the previous session.
    sample_buffer.reset();
    
    // Reset decoded SIGVOL fields + cached volume
    voice3_off = false;
    filter_lp = false;
    filter_bp = false;
    filter_hp = false;
    master_volume = 0;
    vol_scaled_ = 0.0f;

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

// Destructor
mos6581_t::~mos6581_t() {
}

/**
 * Consolidated SID tick function - main entry point for SID cycle processing.
 */
bus_state_t mos6581_t::tick(bus_state_t bus_state) {
    bus_state = advance_cycle(bus_state);
#ifdef CERMU_HAS_GUI
    bus_snapshot_ = bus_state;
#endif
    return bus_state;
}
