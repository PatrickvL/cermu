#pragma once

// ============================================================================
// ScanlineSignalDecoder — shared base for scanline-mapped signal decoders
// ============================================================================
//
// Factors out the common pattern shared by CompositeSignalDecoder (2 bytes/
// sample) and RGBSignalDecoder (4 bytes/sample):
//   - Stream texture + scanline-map uniforms
//   - Zero-copy snapshot (stores FrameData pointers, no memcpy)
//   - Scanline map computation + uniform upload
//   - display_height(), ready(), data_texture() accessors
//
// Template parameter BytesPerSample sizes the snapshot signal buffer.
// Concrete subclasses provide create(), destroy(), upload_to_gpu(),
// bind_textures(), fill_callback_textures(), and optionally override
// update_extra_uniforms() for signal-specific uniforms (e.g. artifact
// phase).
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/signal_shader.hpp"   // MAX_SCANLINES, SIGNAL_TEX_WIDTH, compute_scanline_map

#include <algorithm>

template <int BytesPerSample>
class ScanlineSignalDecoder : public SignalDecoder {
public:
    void snapshot(const FrameData& fd, int fb_width) override {
        if (!fd.signal_output || fd.signal_output_len == 0) { has_snapshot_ = false; return; }
        snapshot_fd_ = fd;
        if (snapshot_fd_.display_width <= 0)
            snapshot_fd_.display_width = fb_width;
        has_snapshot_ = true;
    }

    bool upload_snapshot() override {
        if (!has_snapshot_) return false;
        has_snapshot_ = false;
        const uint8_t* src = static_cast<const uint8_t*>(snapshot_fd_.signal_output);
        uint32_t n = std::min(snapshot_fd_.signal_output_len, MAX_SIGNAL_SAMPLES);
        upload_to_gpu(src, n);
        compute_and_upload_uniforms(snapshot_fd_.sync_events,
                                    snapshot_fd_.sync_count,
                                    snapshot_fd_.back_porch,
                                    snapshot_fd_.display_width, n);
        return true;
    }

    int display_height() const override { return display_height_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }
    GLuint data_texture() const override { return texture_; }

    GLuint signal_texture() const { return texture_; }

protected:
    /// Hook for subclass-specific uniforms (called with program already bound).
    virtual void update_extra_uniforms() {}

    /// Upload raw signal bytes to the GPU texture.
    /// Concrete subclasses call the appropriate shader namespace function.
    virtual void upload_to_gpu(const uint8_t* data, uint32_t sample_count) = 0;

    GLuint texture_ = 0;

    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int display_height_ = 0;

private:
    void compute_and_upload_uniforms(const SyncEvent* sync, uint32_t sync_count,
                                     int back_porch, int display_width,
                                     uint32_t signal_output_len) {
        int offsets[signal_shader::MAX_SCANLINES];
        display_height_ = signal_shader::compute_scanline_map(
            offsets, signal_shader::MAX_SCANLINES,
            sync, sync_count, back_porch, signal_output_len);

        gl_api::glUseProgram(shader_);
        gl_api::glUniform1iv(loc_scanline_map_,
                             signal_shader::MAX_SCANLINES, offsets);
        gl_api::glUniform1i(loc_tex_width_,
                            signal_shader::SIGNAL_TEX_WIDTH);
        gl_api::glUniform1i(loc_display_h_, display_height_);
        gl_api::glUniform1i(loc_display_w_, display_width);
        update_extra_uniforms();
        gl_api::glUseProgram(0);
    }

    // Snapshot — stored FrameData (pointers into DisplayPipeline's
    // triple-buffered slot, protected by claim/release).
    FrameData snapshot_fd_{};
};
