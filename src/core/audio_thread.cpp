/**
 * audio_thread.cpp — AudioThread implementation
 */

#include "core/audio_thread.hpp"
#include "core/audio_synth_engine.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

// ============================================================================
// Lifecycle
// ============================================================================

AudioThread::~AudioThread() {
    stop();
}

void AudioThread::start() {
    if (running_.load(std::memory_order_relaxed))
        return;

    last_processed_cycle_ = 0;
    target_cycle_.store(0, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&AudioThread::thread_func, this);
}

void AudioThread::stop() {
    if (!running_.load(std::memory_order_relaxed))
        return;

    running_.store(false, std::memory_order_release);
    wake_cv_.notify_one();

    if (thread_.joinable())
        thread_.join();
}

// ============================================================================
// Engine management
// ============================================================================

void AudioThread::register_engine(AudioSynthEngine* engine) {
    assert(!running_.load(std::memory_order_relaxed) &&
           "register_engine() must be called while the thread is stopped");
    if (engine)
        engines_.push_back(engine);
}

void AudioThread::unregister_engine(AudioSynthEngine* engine) {
    assert(!running_.load(std::memory_order_relaxed) &&
           "unregister_engine() must be called while the thread is stopped");
    engines_.erase(
        std::remove(engines_.begin(), engines_.end(), engine),
        engines_.end());
}

void AudioThread::clear_engines() {
    assert(!running_.load(std::memory_order_relaxed) &&
           "clear_engines() must be called while the thread is stopped");
    engines_.clear();
}

// ============================================================================
// Signaling
// ============================================================================

void AudioThread::signal_progress(uint64_t cycle) {
    target_cycle_.store(cycle, std::memory_order_release);
    wake_cv_.notify_one();
}

// ============================================================================
// Thread function
// ============================================================================

void AudioThread::thread_func() {
    using namespace std::chrono_literals;

    while (running_.load(std::memory_order_relaxed)) {
        // Wait for new work or timeout (avoids starvation).
        {
            std::unique_lock<std::mutex> lock(wake_mutex_);
            wake_cv_.wait_for(lock, 5ms, [this] {
                return !running_.load(std::memory_order_relaxed) ||
                       target_cycle_.load(std::memory_order_acquire)
                           > last_processed_cycle_;
            });
        }

        if (!running_.load(std::memory_order_relaxed))
            break;

        uint64_t target = target_cycle_.load(std::memory_order_acquire);
        if (target <= last_processed_cycle_)
            continue;

        // Process all registered engines up to the target cycle.
        for (auto* engine : engines_)
            engine->process_until(target);

        last_processed_cycle_ = target;
    }
}
