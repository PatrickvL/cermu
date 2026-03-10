// =============================================================================
// reSID Instrumented Probe Wrapper
// =============================================================================
// Thin derived class that exposes reSID's protected voice, filter, and
// external-filter internals via public accessors for comparison testing.
//
// No reSID source files are modified — we simply inherit from SID and
// provide public methods that reach into the protected members.
//
// Copyright: see reSID license (GPL v2+).  This file is part of cermu's
// test infrastructure and links against reSID as a static library.
// =============================================================================

#pragma once

#include "sid.h"
#include <cmath>

namespace resid_probe {

// Per-voice snapshot: raw integer outputs from reSID at a single cycle.
struct voice_output_t {
    int output;         // (waveform - wave_zero) * envelope  (20-bit signed)
    int waveform;       // wave.output()  (12-bit unsigned)
    int envelope;       // envelope.output()  (8-bit unsigned)
};

// Per-cycle snapshot of the entire reSID audio pipeline.
struct pipeline_snapshot_t {
    voice_output_t voice[3];
    short filter_output;       // Filter::output() — 16-bit signed, post-mixer
    int   extfilt_output;      // ExternalFilter::output() — ~16-bit signed
    int   raw_output;          // SID::output() — same as extfilt_output
};

// Normalised float version for comparison against cermu's float pipeline.
struct pipeline_float_t {
    float voice[3];            // Normalised to approx [-1, +1] per voice
    float filter_output;       // Normalised to approx [-1, +1]
    float final_output;        // Normalised to approx [-1, +1]
};

// =============================================================================
// Instrumented SID — subclass with public probe access
// =============================================================================

class InstrumentedSID : public reSID::SID {
public:
    // ─── Per-cycle voice outputs (call AFTER clock()) ───────────────

    voice_output_t get_voice_output(int i) {
        voice_output_t v;
        v.output   = voice[i].output();
        v.waveform = voice[i].wave.output();
        v.envelope = voice[i].envelope.output();
        return v;
    }

    // ─── Filter output (call AFTER clock()) ─────────────────────────

    short get_filter_output() {
        return filter.output();
    }

    // ─── External filter output (call AFTER clock()) ────────────────

    int get_extfilt_output() {
        return extfilt.output();
    }

    // ─── Full pipeline snapshot ─────────────────────────────────────

    pipeline_snapshot_t snapshot() {
        pipeline_snapshot_t s;
        for (int i = 0; i < 3; i++)
            s.voice[i] = get_voice_output(i);
        s.filter_output  = get_filter_output();
        s.extfilt_output = get_extfilt_output();
        s.raw_output     = s.extfilt_output;
        return s;
    }

    // ─── Normalised float pipeline for cermu comparison ─────────────
    //
    // reSID voice output range: ~[-2048*255, +2047*255] ≈ ±522,240
    // We normalise per-voice to [-1, +1] using the same 1/(2048*255)
    // scale that cermu uses (but without the 1/3 division, since reSID
    // keeps voices separate until the filter mixer).
    //
    // Filter output is a 16-bit short; normalise to [-1, +1] via /32768.
    // External filter output is also ~16-bit range.

    static constexpr float VOICE_NORM = 1.0f / (2048.0f * 255.0f);
    static constexpr float FILTER_NORM = 1.0f / 32768.0f;

    pipeline_float_t snapshot_float() {
        pipeline_float_t f;
        for (int i = 0; i < 3; i++)
            f.voice[i] = (float)get_voice_output(i).output * VOICE_NORM;
        f.filter_output = (float)get_filter_output() * FILTER_NORM;
        f.final_output  = (float)get_extfilt_output() * FILTER_NORM;
        return f;
    }

    // ─── Accessors for inner state (for advanced comparison) ────────
    //
    // WaveformGenerator and EnvelopeGenerator mark SID as friend, but
    // that friendship doesn't transfer to derived classes.  Use
    // read_state() for accumulator/shift_register/envelope_counter.

    // Scale factor used by reSID's amplify() for sample generation
    int get_scale_factor() { return scaleFactor; }

    // Generate resampled output samples (delegates to parent clock method).
    // Returns number of samples placed in buf.
    int generate_samples(reSID::cycle_count& delta_t, short* buf, int n) {
        return clock(delta_t, buf, n);
    }
};

} // namespace resid_probe
