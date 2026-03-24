#pragma once

// ============================================================================
// DisplayPipeline — GUI-side owner of video frame buffers
// ============================================================================
//
// Owns the double-buffered sample arrays that the video chip writes into
// via VideoPort's stream.  Mirrors real hardware: the monitor owns its
// input buffer (video decoding happens on the receiving end), and the
// signal cable (VideoPort) is just a passive connector.
//
// Buffer lifecycle:
//   1. connect() installs the write buffer and a frame-end swap callback
//      into the target VideoPort.
//   2. The video chip calls stream.drive() which writes into buf_[write_idx_].
//   3. At FrameEnd, cold_path in VideoPort calls on_frame_end(), which
//      swaps write_idx_ and returns the new buffer pointer.  The completed
//      frame remains stable in buf_[write_idx_ ^ 1].
//   4. swap_frame() (called by the system) creates a FrameData pointing
//      to the completed buffer via stream_.completed_base.
//   5. The GUI thread reads FrameData and uploads the completed buffer
//      to a GPU texture.
//
// Disconnect: reverts the VideoPort to its internal fallback buffer.
// Only safe when emulation is paused (no concurrent drive() calls).
//
// This type lives in src/gui/ because it is part of the display
// presentation layer, not the emulated hardware model.
// ============================================================================

#include "core/signal/video_port.hpp"

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
};

template<typename SampleT>
class DisplayPipeline : public DisplayPipelineBase {
public:
    using PortType = VideoPort<VideoOut<SampleT>>;

    DisplayPipeline() {
        buf_[0] = std::make_unique<SampleT[]>(MAX_STREAM_SAMPLES);
        buf_[1] = std::make_unique<SampleT[]>(MAX_STREAM_SAMPLES);
    }

    // ====================================================================
    // Connection — redirect a VideoPort's stream to our double-buffer
    // and install the swap callback.  Only one port at a time.
    // ====================================================================

    void connect(PortType& port) {
        port_ = &port;
        write_idx_ = 0;
        sync_count_[0] = 0;
        sync_count_[1] = 0;
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
    // Buffer access — the completed frame is in the buffer that is NOT
    // currently being written to (write_idx_ ^ 1).
    // Valid from FrameEnd until the next FrameEnd (two swap cycles).
    // ====================================================================

    const SampleT* completed_frame() const noexcept {
        return buf_[write_idx_ ^ 1].get();
    }

    const SyncEvent* completed_sync_events() const noexcept {
        return sync_events_[write_idx_ ^ 1];
    }

    uint32_t completed_sync_count() const noexcept {
        return sync_count_[write_idx_ ^ 1];
    }

    SampleT* write_buffer() noexcept {
        return buf_[write_idx_].get();
    }

private:
    std::unique_ptr<SampleT[]> buf_[2];
    SyncEvent sync_events_[2][MAX_SYNC_EVENTS] = {};
    uint32_t  sync_count_[2] = {};
    int write_idx_ = 0;
    PortType* port_ = nullptr;     // Non-owning back-reference

    // Called by VideoPort::cold_path on FrameEnd — swaps sample and sync
    // buffers, returns the new sample write target.  The completed frame's
    // data (samples + sync events) stays stable in [write_idx_ ^ 1].
    static SampleT* on_frame_end(void* ctx) noexcept {
        auto* self = static_cast<DisplayPipeline*>(ctx);
        self->write_idx_ ^= 1;
        // Redirect port's sync writing to the new buffer
        self->port_->set_active_sync_buffer(
            self->sync_events_[self->write_idx_],
            &self->sync_count_[self->write_idx_]);
        self->sync_count_[self->write_idx_] = 0;
        return self->buf_[self->write_idx_].get();
    }
};

// ============================================================================
// Concrete aliases — match the VideoPort aliases
// ============================================================================

using CompositeDisplayPipeline = DisplayPipeline<CompositeVideoSample>;
using RGBDisplayPipeline       = DisplayPipeline<RGBVideoSample>;
using RGBIDisplayPipeline      = DisplayPipeline<RGBIVideoSample>;
using VectorDisplayPipeline    = DisplayPipeline<VectorVideoSample>;
