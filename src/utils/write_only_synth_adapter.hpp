#pragma once
/**
 * write_only_synth_adapter.hpp — Generic AudioSynthEngine adapter for
 *                                write-only sound chips
 *
 * Wraps any sound chip that exposes:
 *   - write(uint8_t data)   — single-byte register write
 *   - tick()                — advance one internal clock cycle
 *   - reset()               — reset chip state
 *
 * The adapter owns an AudioCommandQueue and implements AudioSynthEngine so
 * the chip's synthesis runs on the dedicated audio thread.
 *
 * The emulation thread enqueues timestamped register writes via cmd_queue().
 * The audio thread calls process_until() which drains commands (applying
 * writes at the correct cycle) and ticks the chip forward.
 *
 * Template parameter Chip must provide:
 *   - void write(uint8_t data)
 *   - void tick()
 *   - void reset()   (or init())
 *
 * cpu_cycles_per_tick specifies how many CPU cycles elapse per chip tick(),
 * allowing the adapter to convert CPU cycle timestamps into chip ticks.
 * For example, if the CPU runs at 2 MHz and the chip is clocked at 1 MHz,
 * cpu_cycles_per_tick = 2.
 */

#include "core/audio_synth_engine.hpp"
#include "utils/audio_cmd_queue.hpp"

#include <cstdint>

template<typename Chip>
class WriteOnlySynthAdapter : public AudioSynthEngine {
public:
    /// Construct adapter wrapping an existing chip instance.
    /// The adapter does NOT own the chip — the caller (board/system) retains
    /// ownership.
    ///
    /// @param chip              Non-owning pointer to the sound chip
    /// @param cpu_cycles_per_tick  CPU cycles per chip tick (e.g. 2 if chip
    ///                            runs at half CPU clock)
    /// @param queue_capacity    AudioCommandQueue capacity (power of two)
    WriteOnlySynthAdapter(Chip* chip,
                          uint32_t cpu_cycles_per_tick,
                          uint32_t queue_capacity = 4096)
        : chip_(chip)
        , cpu_cycles_per_tick_(cpu_cycles_per_tick)
        , queue_(queue_capacity)
    {}

    // ---- AudioSynthEngine interface ----

    void process_until(uint64_t target_cycle) override {
        // Drain commands whose cycle <= target, interleaving ticks.
        // For each command, advance the chip to the command's cycle first,
        // then apply the write.
        queue_.drain_until(target_cycle, [&](const AudioCommand& cmd) {
            advance_to(cmd.cycle);
            if (cmd.type == static_cast<uint8_t>(AudioCmdType::REGISTER_WRITE)) {
                chip_->write(cmd.value);
            } else if (cmd.type == static_cast<uint8_t>(AudioCmdType::RESET)) {
                chip_->reset();
                current_cycle_ = cmd.cycle;
            }
        });

        // Advance remaining ticks to reach target_cycle.
        advance_to(target_cycle);
    }

    AudioCommandQueue& cmd_queue() override { return queue_; }

    void reset() override {
        queue_.clear();
        chip_->reset();
        current_cycle_ = 0;
    }

private:
    /// Advance the chip from current_cycle_ to target by ticking.
    void advance_to(uint64_t target) {
        while (current_cycle_ + cpu_cycles_per_tick_ <= target) {
            chip_->tick();
            current_cycle_ += cpu_cycles_per_tick_;
        }
    }

    Chip*              chip_;
    uint32_t           cpu_cycles_per_tick_;
    AudioCommandQueue  queue_;
    uint64_t           current_cycle_ = 0;   // audio thread's cycle bookmark
};
