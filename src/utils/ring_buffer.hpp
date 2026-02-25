#pragma once
/**
 * ring_buffer.hpp - Lock-free SPSC ring buffer
 *
 * Single-Producer Single-Consumer design.  One thread writes, another reads.
 * No mutexes — synchronisation is purely via std::atomic load/store with
 * acquire/release ordering.
 *
 * Typical uses:
 *   - Audio sample transport  (RingBuffer<float>)
 *   - Event / command queues  (RingBuffer<Event>)
 *   - Logging pipelines       (RingBuffer<LogEntry>)
 */

#include <atomic>
#include <cstddef>
#include <vector>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buf_(capacity), cap_(capacity) {}

    /// Number of elements available for reading.
    size_t available() const {
        size_t w = write_.load(std::memory_order_acquire);
        size_t r = read_.load(std::memory_order_acquire);
        return (w >= r) ? (w - r) : (cap_ - r + w);
    }

    /// Write up to @p count elements.  Returns number actually written.
    size_t write(const T* data, size_t count) {
        size_t w = write_.load(std::memory_order_relaxed);
        size_t r = read_.load(std::memory_order_acquire);
        size_t free = cap_ - 1 - ((w >= r) ? (w - r) : (cap_ - r + w));
        if (count > free) count = free;
        for (size_t i = 0; i < count; i++)
            buf_[(w + i) % cap_] = data[i];
        write_.store((w + count) % cap_, std::memory_order_release);
        return count;
    }

    /// Read up to @p count elements.  Returns number actually read.
    size_t read(T* data, size_t count) {
        size_t r = read_.load(std::memory_order_relaxed);
        size_t w = write_.load(std::memory_order_acquire);
        size_t avail = (w >= r) ? (w - r) : (cap_ - r + w);
        if (count > avail) count = avail;
        for (size_t i = 0; i < count; i++)
            data[i] = buf_[(r + i) % cap_];
        read_.store((r + count) % cap_, std::memory_order_release);
        return count;
    }

    void reset() {
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<T> buf_;
    size_t cap_;
    std::atomic<size_t> read_{0};
    std::atomic<size_t> write_{0};
};

/// Convenience alias for the audio use-case.
using AudioRingBuffer = RingBuffer<float>;
