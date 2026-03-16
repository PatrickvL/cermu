#pragma once
/**
 * audio_thread.hpp — Dedicated audio synthesis thread
 *
 * Owns a std::thread that processes one or more AudioSynthEngine instances.
 * The emulation thread signals progress (CPU cycle advances) and the audio
 * thread catches up, draining command queues and producing samples.
 *
 * Engines are registered/unregistered only while the thread is stopped.
 * The signal_progress() call is lock-free (atomic store + CV notify).
 *
 * See docs/AUDIO_THREAD_SEPARATION.md for the full design.
 */

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class AudioSynthEngine;

class AudioThread {
public:
    AudioThread() = default;
    ~AudioThread();

    // Non-copyable, non-movable (owns thread).
    AudioThread(const AudioThread&) = delete;
    AudioThread& operator=(const AudioThread&) = delete;
    AudioThread(AudioThread&&) = delete;
    AudioThread& operator=(AudioThread&&) = delete;

    // ---- Engine management (call only while stopped) ----

    /// Register a synthesis engine.  The AudioThread does NOT own the engine.
    void register_engine(AudioSynthEngine* engine);

    /// Unregister a previously registered engine.
    void unregister_engine(AudioSynthEngine* engine);

    /// Remove all registered engines.
    void clear_engines();

    // ---- Thread lifecycle ----

    void start();
    void stop();
    bool running() const { return running_.load(std::memory_order_relaxed); }

    // ---- Signaling (call from emulation thread) ----

    /// Signal that emulation has advanced to the given CPU cycle.
    /// The audio thread will process all engines up to this cycle.
    /// Lock-free on the fast path (atomic store + CV notify).
    void signal_progress(uint64_t cycle);

private:
    void thread_func();

    std::vector<AudioSynthEngine*> engines_;   // non-owning
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> target_cycle_{0};    // latest cycle from emu thread
    uint64_t last_processed_cycle_{0};         // audio thread's bookmark

    std::mutex wake_mutex_;
    std::condition_variable wake_cv_;
};
