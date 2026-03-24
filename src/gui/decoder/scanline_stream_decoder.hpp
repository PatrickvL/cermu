#pragma once

// ============================================================================
// ScanlineStreamDecoder — shared base for scanline-mapped stream decoders
// ============================================================================
//
// Factors out the common pattern shared by CompositeStreamDecoder (2 bytes/
// sample) and RGBStreamDecoder (4 bytes/sample):
//   - Stream texture + scanline-map uniforms
//   - snapshot() / upload_snapshot() double-buffering
//   - update_uniforms() scanline map computation + uniform upload
//   - display_height(), ready(), data_texture() accessors
//
// Template parameter BytesPerSample sizes the snapshot stream buffer.
// Concrete subclasses provide create(), destroy(), upload(),
// bind_textures(), fill_callback_textures(), and optionally override
// update_extra_uniforms() for signal-specific uniforms (e.g. artifact
// phase).
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/stream_shader.hpp"   // MAX_SCANLINES, STREAM_TEX_WIDTH, compute_scanline_map

#include <cstring>
#include <algorithm>

template <int BytesPerSample>
class ScanlineStreamDecoder : public SignalDecoder {
public:
    void snapshot(const FrameData& fd, int fb_width) override {
        if (!fd.stream || fd.stream_len == 0) { has_snapshot_ = false; return; }
        const uint8_t* src = static_cast<const uint8_t*>(fd.stream);
        uint32_t n = std::min(fd.stream_len, MAX_STREAM_SAMPLES);
        std::memcpy(stream_buf_, src, n * BytesPerSample);
        snapshot_len_ = n;
        uint32_t sc = std::min(fd.sync_count, MAX_SYNC_EVENTS);
        std::memcpy(sync_buf_, fd.sync_events, sc * sizeof(SyncEvent));
        sync_count_     = sc;
        back_porch_     = fd.back_porch;
        display_width_  = fd.display_width > 0 ? fd.display_width : fb_width;
        has_snapshot_ = true;
    }

    bool upload_snapshot() override {
        if (!has_snapshot_) return false;
        has_snapshot_ = false;
        upload(stream_buf_, snapshot_len_);
        update_uniforms(sync_buf_, sync_count_, back_porch_,
                        display_width_, snapshot_len_);
        return true;
    }

    void update_uniforms(const SyncEvent* sync, uint32_t sync_count,
                         int back_porch, int display_width,
                         uint32_t stream_len) override {
        int offsets[stream_shader::MAX_SCANLINES];
        display_height_ = stream_shader::compute_scanline_map(
            offsets, stream_shader::MAX_SCANLINES,
            sync, sync_count, back_porch, stream_len);

        gl_api::glUseProgram(shader_);
        gl_api::glUniform1iv(loc_scanline_map_,
                             stream_shader::MAX_SCANLINES, offsets);
        gl_api::glUniform1i(loc_tex_width_,
                            stream_shader::STREAM_TEX_WIDTH);
        gl_api::glUniform1i(loc_display_h_, display_height_);
        gl_api::glUniform1i(loc_display_w_, display_width);
        update_extra_uniforms();
        gl_api::glUseProgram(0);
    }

    int display_height() const override { return display_height_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }
    GLuint data_texture() const override { return texture_; }

    GLuint stream_texture() const { return texture_; }

protected:
    /// Hook for subclass-specific uniforms (called with program already bound).
    virtual void update_extra_uniforms() {}

    GLuint texture_ = 0;

    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int display_height_ = 0;

private:
    // Snapshot buffers (owned, written by emu thread, read by GUI thread)
    uint8_t   stream_buf_[MAX_STREAM_SAMPLES * BytesPerSample]{};
    SyncEvent sync_buf_[MAX_SYNC_EVENTS]{};
    uint32_t  snapshot_len_  = 0;
    uint32_t  sync_count_    = 0;
    int       back_porch_    = 0;
    int       display_width_ = 0;
};
