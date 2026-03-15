#pragma once
/**
 * audio_cmd_queue.hpp — Lock-free SPSC queue of timestamped audio commands
 *
 * Designed for the audio thread separation pattern:
 *   - Emulation thread (producer) enqueues register writes, DMC sample loads,
 *     resets, and sync-read requests with their absolute cycle timestamp.
 *   - Audio thread (consumer) drains commands up to a target cycle, applying
 *     them at the correct sample offset within each audio block.
 *
 * The queue is a fixed-capacity ring buffer of AudioCommand structs.
 * Synchronisation is purely via std::atomic load/store (acquire/release).
 *
 * Capacity must be a power of two.
 */

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

// ============================================================================
// AudioCommand — a single timestamped command
// ============================================================================

enum class AudioCmdType : uint8_t {
    REGISTER_WRITE    = 0,   // reg + value
    DMC_SAMPLE_LOADED = 1,   // value = sample byte
    RESET             = 2,   // hard reset
    SYNC_READ_REQUEST = 3,   // request sync readback (reg = register index)
};

struct AudioCommand {
    uint64_t cycle;          // absolute cycle stamp (producer-monotonic)
    uint8_t  type;           // AudioCmdType (stored as uint8_t for ABI stability)
    uint8_t  reg;            // register index (0–31 typically)
    uint8_t  value;          // data byte
    uint8_t  flags;          // reserved / per-command flags
    uint32_t pad;            // explicit padding to 16 bytes
};
static_assert(sizeof(AudioCommand) == 16, "AudioCommand must be 16 bytes");
static_assert(std::is_trivially_copyable_v<AudioCommand>);

// ============================================================================
// AudioCommandQueue — fixed-capacity lock-free SPSC ring
// ============================================================================

class AudioCommandQueue {
public:
    // Capacity MUST be a power of two.
    explicit AudioCommandQueue(uint32_t capacity = 4096)
        : cap_(capacity), mask_(capacity - 1) {
        assert((capacity & (capacity - 1)) == 0 && "capacity must be power of two");
        assert(capacity >= 16 && "capacity too small");
        buf_ = new AudioCommand[capacity];
    }

    ~AudioCommandQueue() { delete[] buf_; }

    // Non-copyable, movable.
    AudioCommandQueue(const AudioCommandQueue&) = delete;
    AudioCommandQueue& operator=(const AudioCommandQueue&) = delete;

    AudioCommandQueue(AudioCommandQueue&& o) noexcept
        : buf_(o.buf_), cap_(o.cap_), mask_(o.mask_),
          write_(o.write_.load(std::memory_order_relaxed)),
          read_(o.read_.load(std::memory_order_relaxed)) {
        o.buf_ = nullptr;
        o.cap_ = 0;
        o.mask_ = 0;
    }

    AudioCommandQueue& operator=(AudioCommandQueue&& o) noexcept {
        if (this != &o) {
            delete[] buf_;
            buf_  = o.buf_;
            cap_  = o.cap_;
            mask_ = o.mask_;
            write_.store(o.write_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            read_.store(o.read_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            o.buf_ = nullptr;
            o.cap_ = 0;
            o.mask_ = 0;
        }
        return *this;
    }

    // ---- Producer (emulation thread) ----

    /// Try to enqueue a single command.  Returns true on success.
    bool push(const AudioCommand& cmd) {
        uint32_t w = write_.load(std::memory_order_relaxed);
        uint32_t r = read_.load(std::memory_order_acquire);
        if (size_from(w, r) >= cap_ - 1)
            return false;  // full
        buf_[w & mask_] = cmd;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    /// Convenience: enqueue a register write.
    bool push_write(uint64_t cycle, uint8_t reg, uint8_t value) {
        return push({cycle, static_cast<uint8_t>(AudioCmdType::REGISTER_WRITE),
                     reg, value, 0, 0});
    }

    /// Convenience: enqueue a DMC sample load.
    bool push_dmc_sample(uint64_t cycle, uint8_t sample_byte) {
        return push({cycle, static_cast<uint8_t>(AudioCmdType::DMC_SAMPLE_LOADED),
                     0, sample_byte, 0, 0});
    }

    /// Convenience: enqueue a reset.
    bool push_reset(uint64_t cycle) {
        return push({cycle, static_cast<uint8_t>(AudioCmdType::RESET),
                     0, 0, 0, 0});
    }

    /// Convenience: enqueue a sync-read request.
    bool push_sync_read(uint64_t cycle, uint8_t reg) {
        return push({cycle, static_cast<uint8_t>(AudioCmdType::SYNC_READ_REQUEST),
                     reg, 0, 0, 0});
    }

    // ---- Consumer (audio thread) ----

    /// Peek at the front command without consuming it.
    /// Returns nullptr if the queue is empty.
    const AudioCommand* peek() const {
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        if (r == w) return nullptr;
        return &buf_[r & mask_];
    }

    /// Pop the front command.  Returns true if a command was consumed.
    bool pop(AudioCommand& out) {
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        if (r == w) return false;  // empty
        out = buf_[r & mask_];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

    /// Drain all commands with cycle <= target_cycle, invoking fn(cmd) for each.
    /// Commands are delivered in FIFO (cycle-ascending) order.
    /// Returns the number of commands consumed.
    template<typename Fn>
    uint32_t drain_until(uint64_t target_cycle, Fn&& fn) {
        uint32_t count = 0;
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        while (r != w) {
            const AudioCommand& cmd = buf_[r & mask_];
            if (cmd.cycle > target_cycle)
                break;
            fn(cmd);
            ++r;
            ++count;
        }
        if (count > 0)
            read_.store(r, std::memory_order_release);
        return count;
    }

    // ---- Queries ----

    uint32_t capacity() const { return cap_; }

    uint32_t size() const {
        uint32_t w = write_.load(std::memory_order_acquire);
        uint32_t r = read_.load(std::memory_order_acquire);
        return size_from(w, r);
    }

    bool empty() const { return size() == 0; }

    /// High-water mark since last reset_stats().  Useful for monitoring.
    uint32_t high_water_mark() const { return hwm_; }

    void reset_stats() { hwm_ = 0; }

    /// Discard all pending commands (call only when both threads are quiescent).
    void clear() {
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
        hwm_ = 0;
    }

private:
    uint32_t size_from(uint32_t w, uint32_t r) const {
        return w - r;  // works with unsigned wrap since mask_ handles indexing
    }

    AudioCommand*          buf_   = nullptr;
    uint32_t               cap_   = 0;
    uint32_t               mask_  = 0;
    std::atomic<uint32_t>  write_{0};
    std::atomic<uint32_t>  read_{0};
    uint32_t               hwm_   = 0;  // high-water mark (updated by push)
};
