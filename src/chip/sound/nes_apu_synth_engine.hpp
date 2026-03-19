#pragma once
/*
 * nes_apu_synth_engine.hpp — AudioSynthEngine adapter for the NES APU
 *
 * Wraps a dedicated nes6502_apu::APU instance that runs on the audio thread.
 * The emulation thread enqueues timestamped register writes and DMC sample
 * bytes via AudioCommandQueue; the audio thread drains the queue, advances
 * the APU cycle-by-cycle, and decimates the output into an AudioRingBuffer.
 *
 * In single-threaded mode this class is not used — the APU runs inline
 * from the CPU mixin's clock_apu() / generate_audio_sample() path.
 *
 * See docs/AUDIO_ARCHITECTURE.md §6 for the full design.
 */

#include "core/audio_synth_engine.hpp"
#include "utils/audio_cmd_queue.hpp"
#include "utils/ring_buffer.hpp"
#include "chip/sound/nes_apu.hpp"

#include <cstdint>

struct AudioPort;  // Forward declaration for analog signal output

class NesApuSynthEngine : public AudioSynthEngine {
public:
    /// @param is_pal     true for PAL (2A07), false for NTSC (2A03)
    /// @param sample_rate  Host audio sample rate (e.g. 44100)
    /// @param ring_buf_capacity  Capacity of the output ring buffer
    NesApuSynthEngine(bool is_pal, uint32_t sample_rate,
                      uint32_t ring_buf_capacity = 8192)
        : apu_(is_pal)
        , ring_buf_(ring_buf_capacity)
        , sample_period_(is_pal ? 33 : 37)
    {
        (void)sample_rate;  // period is fixed for NES; rate only affects host
    }

    // ---- AudioSynthEngine interface ----

    void process_until(uint64_t target_cycle) override {
        // Drain commands whose cycle <= target, interleaving ticks.
        queue_.drain_until(target_cycle, [&](const AudioCommand& cmd) {
            advance_to(cmd.cycle);
            switch (static_cast<AudioCmdType>(cmd.type)) {
            case AudioCmdType::REGISTER_WRITE:
                apu_.write_register(cmd.reg, cmd.value);
                break;
            case AudioCmdType::DMC_SAMPLE_LOADED:
                apu_.dmc_load_sample(cmd.value);
                break;
            case AudioCmdType::RESET:
                apu_.reset_to_power_up_state();
                current_cycle_ = cmd.cycle;
                sample_counter_ = 0;
                break;
            default:
                break;
            }
        });

        // Advance remaining ticks to reach target_cycle.
        advance_to(target_cycle);
    }

    AudioCommandQueue& cmd_queue() override { return queue_; }

    void reset() override {
        queue_.clear();
        apu_.reset_to_power_up_state();
        current_cycle_ = 0;
        sample_counter_ = 0;
    }

    // ---- Sample output ----

    /// Read samples produced by the audio thread.
    /// Called from the SDL audio callback (or get_audio_samples).
    uint32_t audio_read(float* buffer, uint32_t max_samples) {
        return static_cast<uint32_t>(
            ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
    }

    uint32_t audio_available() const {
        return static_cast<uint32_t>(ring_buf_.available());
    }

    /// Update sample period (e.g. on region change).
    void set_region(bool is_pal) {
        sample_period_ = is_pal ? 33 : 37;
    }

    /// Set optional AudioPort for analog signal output.
    void set_audio_port(AudioPort* port) { audio_port_ = port; }

private:
    /// Advance the APU from current_cycle_ to target, ticking once per
    /// CPU cycle and generating decimated audio samples.
    void advance_to(uint64_t target) {
        while (current_cycle_ < target) {
            apu_.tick(0);  // bus_state not used for synthesis
            current_cycle_++;

            if (++sample_counter_ >= sample_period_) {
                sample_counter_ = 0;
                float s = apu_.sample();
                if (audio_port_) {
                    audio_port_->drive_sample(s);
                } else {
                    ring_buf_.write(&s, 1);
                }
            }
        }
    }

    nes6502_apu::APU  apu_;
    AudioCommandQueue queue_;
    AudioRingBuffer   ring_buf_;
    uint64_t          current_cycle_ = 0;
    uint32_t          sample_counter_ = 0;
    uint32_t          sample_period_;       // NTSC=37, PAL=33
    AudioPort*        audio_port_ = nullptr;  // Optional analog signal output
};
