#pragma once
/**
 * audio_synth_engine.hpp — Abstract interface for audio-thread synthesis engines
 *
 * Each sound chip that supports running synthesis on a dedicated audio thread
 * implements this interface.  The emulation thread enqueues timestamped
 * register writes via AudioCommandQueue; the audio thread calls
 * process_until() to drain commands and advance synthesis.
 *
 * See docs/AUDIO_THREAD_SEPARATION.md for the full design.
 */

#include <cstdint>

class AudioCommandQueue;

class AudioSynthEngine {
public:
    virtual ~AudioSynthEngine() = default;

    /// Drain queued commands and advance synthesis to target_cycle (CPU cycles).
    /// Called by AudioThread on the audio processing thread.
    virtual void process_until(uint64_t target_cycle) = 0;

    /// Access the command queue.
    /// Producer: emulation thread (enqueues register writes).
    /// Consumer: audio thread (drains via process_until).
    virtual AudioCommandQueue& cmd_queue() = 0;

    /// Reset synthesis state and clear command queue.
    /// Call only when both threads are quiescent (e.g. system reset).
    virtual void reset() = 0;
};
