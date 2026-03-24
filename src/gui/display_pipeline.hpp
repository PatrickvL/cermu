#pragma once

// ============================================================================
// DisplayPipeline — GUI-side owner of video frame buffers
// ============================================================================
//
// Owns a triple-buffered sample array that the video chip writes into
// via VideoPort's stream.  Mirrors real hardware: the monitor owns its
// input buffer (video decoding happens on the receiving end), and the
// signal cable (VideoPort) is just a passive connector.
//
// Buffer roles (three slots, rotating):
//   WRITE   — the emu thread is currently writing into this slot.
//   READY   — the last completed frame; available for the GUI to claim.
//   FREE    — not in use; available as the next write target.
//
// The triple buffer enables zero-copy snapshot:
//   1. connect() installs the write buffer and a frame-end swap callback.
//   2. The video chip calls stream.drive(), writing into the WRITE slot.
//   3. At FrameEnd, on_frame_end() promotes WRITE→READY and picks a
//      FREE slot as the new WRITE target.
//   4. The emu thread calls claim() under fb_mutex_ to mark the READY
//      slot as in-use by the GUI.  While claimed, on_frame_end() will
//      not reuse that slot — it picks the remaining FREE slot instead.
//   5. The GUI thread reads directly from the claimed buffer (zero-copy
//      GPU upload), then calls release() under fb_mutex_.
//
// Disconnect: reverts the VideoPort to its internal fallback buffer.
// Only safe when emulation is paused (no concurrent drive() calls).
//
// This type lives in src/gui/ because it is part of the display
// presentation layer, not the emulated hardware model.
// ============================================================================

#include "core/signal/video_port.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

// ============================================================================
// Base class for type-erased ownership in SessionGUI
// ============================================================================

class DisplayPipelineBase {
public:
    virtual ~DisplayPipelineBase() = default;
    virtual void disconnect() = 0;
    virtual bool connected() const noexcept = 0;

    /// Claim the completed frame — prevents it from being overwritten
    /// by the next FrameEnd swap.  Call under fb_mutex_ from emu thread.
    /// Must be paired with release().
    virtual void claim() noexcept = 0;

    /// Release the claimed frame — allows the buffer to be reused.
    /// Call under fb_mutex_ from GUI thread after upload is complete.
    virtual void release() noexcept = 0;

    /// Whether a frame is currently claimed.
    virtual bool claimed() const noexcept = 0;
};

template<typename SampleT>
class DisplayPipeline : public DisplayPipelineBase {
public:
    using PortType = VideoPort<VideoOut<SampleT>>;

    DisplayPipeline() {
        buf_[0] = std::make_unique<SampleT[]>(MAX_STREAM_SAMPLES);
        buf_[1] = std::make_unique<SampleT[]>(MAX_STREAM_SAMPLES);
        buf_[2] = std::make_unique<SampleT[]>(MAX_STREAM_SAMPLES);
    }

    // ====================================================================
    // Connection — redirect a VideoPort's stream to our triple-buffer
    // and install the swap callback.  Only one port at a time.
    // ====================================================================

    void connect(PortType& port) {
        port_ = &port;
        write_idx_ = 0;
        ready_idx_ = -1;
        display_idx_.store(-1, std::memory_order_relaxed);
        sync_count_[0] = 0;
        sync_count_[1] = 0;
        sync_count_[2] = 0;
        port.set_active_buffer(buf_[write_idx_].get());
        port.set_active_sync_buffer(sync_events_[write_idx_], &sync_count_[write_idx_]);
        port.set_frame_end_callback(&DisplayPipeline::on_frame_end, this);
    }

    // Disconnect — revert the port to its internal buffer.
    // Only safe when emulation is paused.
    void disconnect() override {
        if (port_) {
            port_->reset_to_internal_buffer();
            port_ = nullptr;
        }
    }

    bool connected() const noexcept override { return port_ != nullptr; }

    // ====================================================================
    // Claim / release — protect the ready buffer from being overwritten.
    // ====================================================================

    void claim() noexcept override {
        int r = ready_idx_;
        if (r >= 0)
            display_idx_.store(r, std::memory_order_release);
    }

    void release() noexcept override {
        display_idx_.store(-1, std::memory_order_release);
    }

    bool claimed() const noexcept override {
        return display_idx_.load(std::memory_order_acquire) >= 0;
    }

private:
    static constexpr int NUM_BUFFERS = 3;

    std::unique_ptr<SampleT[]> buf_[NUM_BUFFERS];
    SyncEvent sync_events_[NUM_BUFFERS][MAX_SYNC_EVENTS] = {};
    uint32_t  sync_count_[NUM_BUFFERS] = {};

    int write_idx_ = 0;            // Slot currently being written by emu
    int ready_idx_ = -1;           // Last completed frame (-1 = none yet)
    std::atomic<int> display_idx_{-1}; // Slot claimed by GUI (-1 = not claimed)

    PortType* port_ = nullptr;     // Non-owning back-reference

    // Find a free slot (not new_ready, not display).
    int find_free_slot(int new_ready) const noexcept {
        int claimed = display_idx_.load(std::memory_order_acquire);
        for (int i = 0; i < NUM_BUFFERS; ++i) {
            if (i != new_ready && i != claimed)
                return i;
        }
        // Should never happen — 3 slots, max 2 in use.
        return 0;
    }

    // Called by VideoPort::cold_path on FrameEnd — promotes the write
    // buffer to ready and picks a free slot as the new write target.
    // Runs on emu thread without fb_mutex_.
    static SampleT* on_frame_end(void* ctx) noexcept {
        auto* self = static_cast<DisplayPipeline*>(ctx);

        int new_ready = self->write_idx_;
        int new_write = self->find_free_slot(new_ready);

        self->ready_idx_ = new_ready;
        self->write_idx_ = new_write;

        // Redirect port's sync writing to the new buffer
        self->port_->set_active_sync_buffer(
            self->sync_events_[new_write],
            &self->sync_count_[new_write]);
        self->sync_count_[new_write] = 0;

        return self->buf_[new_write].get();
    }
};

// ============================================================================
// Concrete aliases — match the VideoPort aliases
// ============================================================================

using CompositeDisplayPipeline = DisplayPipeline<CompositeVideoSample>;
using RGBDisplayPipeline       = DisplayPipeline<RGBVideoSample>;
using RGBIDisplayPipeline      = DisplayPipeline<RGBIVideoSample>;
using VectorDisplayPipeline    = DisplayPipeline<VectorVideoSample>;
