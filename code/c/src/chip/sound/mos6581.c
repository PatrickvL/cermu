#include "mos6581.h"
#include "aiemuc.h"
#include <string.h>
#include <stdlib.h>
#include <math.h> // for tanhf
#include <stdio.h>  // For snprintf
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Accurate envelope rate periods (in cycles) based on hardware measurements
static const uint32_t envelope_rate_periods[16] = {
    9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3906, 11720, 19531, 31251
};

// Exponential decay lookup table for envelope
static const uint8_t envelope_decay_table[256] = {
    1, 30, 30, 30, 30, 30, 30, 16, 16, 16, 16, 16, 16, 16, 16, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

// Noise output bit mapping table
static const uint8_t noise_output_bits[12] = {
    22, 20, 16, 13, 11, 7, 4, 2, 1, 0, 21, 19
};

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
    
    switch (voice->envelope_cycle) {
        case CYCLE_OFF:
            voice->envelope_amplitude = 0;
            break;
            
        case CYCLE_ATTACK:
            if (voice->envelope_amplitude < ENVELOPE_MAX) {
                voice->envelope_amplitude++;
                if (voice->envelope_amplitude == ENVELOPE_MAX) {
                    voice->envelope_cycle = CYCLE_DECAY;
                    voice->envelope_rate_period = voice_rate_to_period(voice, voice->decay_rate);
                }
            }
            break;
            
        case CYCLE_DECAY:
            if (voice->envelope_amplitude > voice->sustain_level) {
                uint8_t decay_amount = envelope_decay_table[voice->envelope_amplitude >> 8];
                if (voice->envelope_amplitude >= decay_amount) {
                    voice->envelope_amplitude -= decay_amount;
                } else {
                    voice->envelope_amplitude = 0;
                }
                
                if (voice->envelope_amplitude <= voice->sustain_level) {
                    voice->envelope_amplitude = voice->sustain_level;
                    voice->envelope_cycle = CYCLE_SUSTAIN;
                }
            }
            break;
            
        case CYCLE_SUSTAIN:
            // Sustain level is held constant
            break;
            
        case CYCLE_RELEASE:
            if (voice->envelope_amplitude > 0) {
                uint8_t release_amount = envelope_decay_table[voice->envelope_amplitude >> 8];
                if (voice->envelope_amplitude >= release_amount) {
                    voice->envelope_amplitude -= release_amount;
                } else {
                    voice->envelope_amplitude = 0;
                }
                
                if (voice->envelope_amplitude == 0) {
                    voice->envelope_cycle = CYCLE_OFF;
                }
            }
            break;
    }
}

void voice_update_envelope(voice_t* voice) {
    if (!voice) return;
    
    // Update envelope rate counter
    voice->envelope_rate_counter++;
    
    if (voice->envelope_rate_counter >= voice->envelope_rate_period) {
        voice->envelope_rate_counter = 0;
        voice_envelope_clock(voice);
    }
}

// =============================================================================
// FILTER IMPLEMENTATION
// =============================================================================

void mos6581_filter_update_cutoff(mos6581_t* sid) {
    if (!sid) return;
    
    filter_state_t* f = &sid->filter_state;
    
    // Calculate cutoff frequency from register value
    float fc = (float)sid->filter_cutoff_frequency;
    
    // Different formulas for different chip revisions
    if (sid->revision <= SID_REVISION_6581_R4AR) {
        // 6581 formula (nonlinear)
        f->cutoff_frequency = 5.5f * fc / FILTER_CUTOFF_MAX;
    } else {
        // 8580 formula (more linear)
        f->cutoff_frequency = 12.5f * fc / FILTER_CUTOFF_MAX;
    }
    
    // Calculate filter coefficient
    f->w0 = (float)(2.0f * M_PI * f->cutoff_frequency / sid->sid_rate);
    
    // Clamp to prevent instability
    if (f->w0 > 1.0f) f->w0 = 1.0f;
}

void mos6581_filter_update_resonance(mos6581_t* sid) {
    if (!sid) return;
    
    filter_state_t* f = &sid->filter_state;
    
    // Calculate Q from resonance value
    f->resonance = (float)sid->filter_resonance;
    f->q = 1.0f / (2.0f - f->resonance / FILTER_RESONANCE_MAX * 1.8f);
    
    // Clamp Q to prevent instability
    if (f->q > 10.0f) f->q = 10.0f;
    if (f->q < 0.1f) f->q = 0.1f;
}

float mos6581_filter_process(mos6581_t* sid, float input) {
    if (!sid || !sid->enable_filter) return input;
    
    filter_state_t* f = &sid->filter_state;
    
    // Two-integrator-loop biquad filter
    float hp = input - f->integrator1 * f->q - f->integrator2;
    float bp = f->integrator1 + hp * f->w0;
    float lp = f->integrator2 + bp * f->w0;
    
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
    
    return (float)(sid->filter_state.cutoff_frequency * sid->sid_rate / (2.0f * M_PI));
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
    
    float mixed_output = 0.0f;
    float filtered_input = 0.0f;
    uint32_t unfiltered_count = 0;
    uint32_t filtered_count = 0;
    
    // Process sync and ring modulation
    voice_apply_sync(&sid->voice1, &sid->voice3);
    voice_apply_sync(&sid->voice2, &sid->voice1);
    voice_apply_sync(&sid->voice3, &sid->voice2);
    
    // Apply ring modulation
    uint32_t voice1_output = voice_apply_ring_modulation(&sid->voice1, &sid->voice3);
    uint32_t voice2_output = voice_apply_ring_modulation(&sid->voice2, &sid->voice1);
    uint32_t voice3_output = voice_apply_ring_modulation(&sid->voice3, &sid->voice2);
    
    // Mix voices
    if (!sid->filter_voice1) {
        mixed_output += (float)voice1_output / 4095.0f;
        unfiltered_count++;
    } else {
        filtered_input += (float)voice1_output / 4095.0f;
        filtered_count++;
    }
    
    if (!sid->filter_voice2) {
        mixed_output += (float)voice2_output / 4095.0f;
        unfiltered_count++;
    } else {
        filtered_input += (float)voice2_output / 4095.0f;
        filtered_count++;
    }
    
    if (!sid->filter_voice3 && !sid->voice3_disabled) {
        mixed_output += (float)voice3_output / 4095.0f;
        unfiltered_count++;
    } else if (!sid->voice3_disabled) {
        filtered_input += (float)voice3_output / 4095.0f;
        filtered_count++;
    }
    
    // Add external input if filtered
    if (sid->filter_voice4) {
        filtered_input += sid->external_input;
        filtered_count++;
    } else {
        mixed_output += sid->external_input;
        unfiltered_count++;
    }
    
    // Apply filter to filtered voices
    if (filtered_count > 0) {
        filtered_input /= filtered_count;
        float filtered_output = mos6581_filter_process(sid, filtered_input);
        mixed_output += filtered_output;
    }
    
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
    
    rb->buffer[rb->write_pos] = sample;
    rb->write_pos = (rb->write_pos + 1) & rb->mask;
}

bool ring_buffer_empty(ring_buffer_t* rb) {
    if (!rb) return true;
    return rb->read_pos == rb->write_pos;
}

float ring_buffer_read(ring_buffer_t* rb) {
    if (!rb || !rb->buffer || ring_buffer_empty(rb)) return 0.0f;
    
    float sample = rb->buffer[rb->read_pos];
    rb->read_pos = (rb->read_pos + 1) & rb->mask;
    return sample;
}

uint32_t ring_buffer_available(ring_buffer_t* rb) {
    if (!rb) return 0;
    return (rb->write_pos - rb->read_pos) & rb->mask;
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
    
    // Extract noise output from various LFSR bits
    uint32_t noise_output = 0;
    for (int i = 0; i < 12; i++) {
        uint32_t bit = (voice->noise_lfsr >> noise_output_bits[i]) & 1;
        noise_output |= (bit << i);
    }
    
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
    voice->envelope_next_level = 0;
    
    // Reset waveform outputs
    voice->oscillator_waveform = 0;
    voice->triangle_output = 0;
    voice->sawtooth_output = 0;
    voice->pulse_output = 0;
    voice->combined_output = 0;
    
    // Reset noise state
    voice->noise_lfsr = 0x7FFFF8; // Initial LFSR state
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
        // Test bit locks accumulator and resets noise
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = 0x7FFFF8;
    }
    
    // Detect MSB change for sync
    bool msb_rising = ((voice->prev_accumulator & WAVEFORM_ACCUMULATOR_MSB) == 0) && 
                      ((voice->waveform_accumulator & WAVEFORM_ACCUMULATOR_MSB) != 0);
    voice->sync_trigger = msb_rising;
    
    // Generate waveforms
    voice->triangle_output = voice_generate_triangle(voice);
    voice->sawtooth_output = voice_generate_sawtooth(voice);
    voice->pulse_output = voice_generate_pulse(voice);
    voice->noise_output = voice_generate_noise(voice);
    
    // Select waveform output
    uint32_t waveform_output = 0;
    int waveform_count = aiemuc_popcount(voice->waveform);
    
    if (waveform_count == 0) {
        waveform_output = 0;
    } else if (waveform_count == 1) {
        // Single waveform
        switch (voice->waveform) {
            case WAVEFORM_NONE:
                voice->oscillator_waveform = 0;
                break;
            case WAVEFORM_TRIANGLE:
                waveform_output = voice->triangle_output;
                break;
            case WAVEFORM_SAWTOOTH:
                waveform_output = voice->sawtooth_output;
                break;
            case WAVEFORM_PULSE:
                waveform_output = voice->pulse_output;
                break;
            case WAVEFORM_NOISE:
                waveform_output = voice->noise_output;
                break;
        }
    } else {
        // Combined waveform
        waveform_output = voice_generate_combined_waveform(voice);
    }
    
    voice->oscillator_waveform = waveform_output;
    voice->oscillator_output = (uint8_t)(waveform_output >> 4);
    
    // Update envelope
    voice_update_envelope(voice);
    
    // Apply envelope to waveform
    voice->result = (voice->oscillator_waveform * voice->envelope_amplitude) >> 16;
    
    // Update envelope output register
    voice->envelope_output = (uint8_t)(voice->envelope_amplitude >> 8);
}

// =============================================================================
// MAIN CYCLE FUNCTION
// =============================================================================

// Unified bus state threading main cycle function
bus_state_t mos6581_advance_cycle(mos6581_t* sid, bus_state_t bus_state) {
    if (!sid) return bus_state;

    // Update at SID frequency (PAL: every 18 cycles, NTSC: every 17 cycles)
    uint32_t divisor = sid->pal_timing ? 18 : 17;

    if (sid->cycle_count % divisor == 0) {
        // Update all voices
        for (int i = 0; i < 3; i++) {
            voice_clock_cycle(sid->voices[i]);
        }

        // Mix voices and generate output
        uint32_t mixed_sample = mos6581_mix_voices(sid);

        // Convert to float and store in ring buffer
        float sample = ((float)mixed_sample - 32768.0f) / 32767.0f;
        ring_buffer_write(&sid->sample_buffer, sample);

        sid->samples_generated++;
    }

    sid->cycle_count++;
    sid->total_cycles++;

    // Return possibly updated bus state (for future expansion)
    return bus_state;
}

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
    
    // Check for I/O register access when I/O pending
    if (unlikely(bus_is_io_pending(&bus_state))) {
        // Check if address is within SID range ($D400-$D7FF)
        if ((bus_state.addr & 0x0C00) == 0x0400) {
            bus_clear_io_pending(&bus_state);
            // Handle register access directly
            bool is_read = bus_state.lines & BUS_MASK_RW;
            if (is_read) {
                bus_state = mos6581_registers_read(sid, bus_state);
            } else {
                bus_state = mos6581_registers_write(sid, bus_state);
            }
        }
    }
    
    // Delegate to the existing advance cycle function
    // In the future, this can be expanded to include additional tick-specific logic
    return mos6581_advance_cycle(sid, bus_state);
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
    sid->sid_rate = sid->voice1.cpu_clock / (pal_timing ? 18.0f : 17.0f);
    mos6581_filter_update_cutoff(sid);
}

void mos6581_set_sample_rate(mos6581_t* sid, float sample_rate) {
    if (!sid) return;
    
    sid->sample_rate = sample_rate;
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
            // Attack is linear: 0 to 65535 in period cycles per step
            return (65535 * period) / cycles_per_ms;
            
        case CYCLE_DECAY:
        case CYCLE_RELEASE:
            // Decay/Release is exponential - approximate time to reach 1/e
            return (uint32_t)(period * 255.0f / cycles_per_ms);
            
        default:
            return 0;
    }
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
            bus_state_t preset_bus_state = { .addr = 0xD400 + i, .data = preset->registers[i] };
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
    if (voice->test && !prev_test) {
        voice->envelope_cycle = CYCLE_OFF;
        voice->envelope_amplitude = 0;
        voice->waveform_accumulator = 0;
        voice->noise_lfsr = 0x7FFFF8;
    }
    
    // Handle gate bit changes
    if (voice->gated != prev_gated) {
        if (voice->gated) {
            voice->envelope_cycle = CYCLE_ATTACK;
            voice->envelope_rate_period = voice_rate_to_period(voice, voice->attack_rate);
        } else {
            voice->envelope_cycle = CYCLE_RELEASE;
            voice->envelope_rate_period = voice_rate_to_period(voice, voice->release_rate);
        }
        voice->envelope_rate_counter = 0;
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
    uint8_t sustain_level = (value >> 4) & 0x0F;
    
    // Convert 4-bit sustain level to 16-bit value
    voice->sustain_level = (uint16_t)sustain_level << 12;
    
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
    
    uint32_t r = bus_state.addr & SID_REGS_MASK;
    uint8_t value = bus_state.data;
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
    
    uint32_t r = bus_state.addr & SID_REGS_MASK;
    
    switch (r) {
        case 0x19: // POTX - Game paddle 1 position
            bus_state.data = sid->pot_x_value;
            break;
            
        case 0x1A: // POTY - Game paddle 2 position
            bus_state.data = sid->pot_y_value;
            break;
            
        case 0x1B: // OSC3 - Oscillator 3 / Random number generator
            bus_state.data = (uint8_t)(sid->voice3.oscillator_waveform >> 4);
            break;
            
        case 0x1C: // ENV3 - Envelope generator 3 output
            bus_state.data = (uint8_t)(sid->voice3.gated ? (sid->voice3.envelope_amplitude >> 8) : 0);
            break;
            
        case 0x1D: // Unused register
        case 0x1E: // Unused register
        case 0x1F: // Unused register
            bus_state.data = 0xFF;
            break;
            
        default:
            // Return bus value for write-only registers (already in bus_state.data)
            // No action needed - bus_state.data already contains what was on the bus
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
    
    // Reset volume bug state
    sid->volume_change_click = false;
    sid->volume_click_amplitude = 0.0f;
    sid->volume_click_counter = 0;
    
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
        sid->voices[i]->cpu_clock = 1000000.0f; // Default 1MHz
    }
    
    // Initialize default settings
    sid->revision = SID_REVISION_6581_R2;
    sid->pal_timing = true;
    sid->sample_rate = 44100.0f;
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
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "mos6581_gui.c"
#endif

// =============================================================================
// BUS STATE INTERFACE FUNCTIONS
// =============================================================================

// Implement the missing bus_state_t functions that c64_bus.c expects
bus_state_t mos6581_read(mos6581_t* sid, bus_state_t bus_state) {
    if (!sid) return bus_state;
    
    // Read from SID register using address from bus state
    return mos6581_registers_read(sid, bus_state);
}

bus_state_t mos6581_write(mos6581_t* sid, bus_state_t bus_state) {
    if (!sid) return bus_state;
    
    // Write to SID register using address and data from bus state
    return mos6581_registers_write(sid, bus_state);
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
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6581_render_debug_window,
    .render_settings_window = mos6581_render_settings_window
#endif
};
