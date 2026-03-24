#pragma once

// ============================================================================
// RGBStreamDecoder — GPU reconstruction for RGB-family signals
// ============================================================================
//
// Handles RGB, Digital, and YPbPr signal types.
// Stream format: RGBA8 (4 bytes per sample: r, g, b, flags).
// No palette lookup — the shader reads raw RGB values directly.
//
// Owns: stream texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// No palette dependency.
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/rgb_stream_shader.hpp"
#include "gui/shader/ypbpr_stream_shader.hpp"
#include "gui/shader/stream_shader.hpp"    // for MAX_SCANLINES, STREAM_TEX_WIDTH, compute_scanline_map

#include <cstring>
#include <algorithm>

enum class RGBShaderVariant {
    Standard,   // RGB / Digital — direct pass-through
    YPbPr,      // Y'PbPr component — chroma bandwidth limiting
};

class RGBStreamDecoder : public SignalDecoder {
public:
    explicit RGBStreamDecoder(RGBShaderVariant variant = RGBShaderVariant::Standard)
        : variant_(variant) {}

    bool create() override {
        texture_ = rgb_stream_shader::create_stream_texture(MAX_STREAM_SAMPLES);
        if (!texture_) return false;

        rgb_stream_shader::RGBShaderLocations locs{};
        if (variant_ == RGBShaderVariant::YPbPr)
            shader_ = ypbpr_stream_shader::create_program(&locs);
        else
            shader_ = rgb_stream_shader::create_program(&locs);

        if (!shader_) {
            glDeleteTextures(1, &texture_);
            texture_ = 0;
            return false;
        }

        loc_proj_         = locs.proj_mtx;
        loc_scanline_map_ = locs.scanline_map;
        loc_tex_width_    = locs.stream_tex_width;
        loc_display_h_    = locs.display_height;
        loc_display_w_    = locs.display_width;

        create_fbo();
        return true;
    }

    void destroy() override {
        destroy_fbo();
        if (shader_)  { gl_api::glDeleteProgram(shader_); shader_ = 0; }
        if (texture_) { glDeleteTextures(1, &texture_); texture_ = 0; }
    }

    void upload(const uint8_t* data, uint32_t sample_count) override {
        rgb_stream_shader::upload_stream_texture(texture_, data, sample_count);
    }

    void snapshot(const FrameData& fd, int fb_width) override {
        if (!fd.stream || fd.stream_len == 0) { has_snapshot_ = false; return; }
        const uint8_t* src = static_cast<const uint8_t*>(fd.stream);
        uint32_t n = std::min(fd.stream_len, MAX_STREAM_SAMPLES);
        std::memcpy(stream_buf_, src, n * 4);
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
        rgb_stream_shader::upload_stream_texture(texture_, stream_buf_, snapshot_len_);
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
        gl_api::glUseProgram(0);
    }

    int display_height() const override { return display_height_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }
    GLuint data_texture() const override { return texture_; }

    GLuint stream_texture() const { return texture_; }

protected:
    void bind_textures() override {
        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
    }

    void fill_callback_textures(DecoderCallbackData& cb) const override {
        cb.tex0 = texture_;
    }

private:
    RGBShaderVariant variant_;

    GLuint texture_ = 0;

    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int   display_height_   = 0;

    // Snapshot buffers (owned, written by emu thread, read by GUI thread)
    uint8_t   stream_buf_[MAX_STREAM_SAMPLES * 4]{};
    SyncEvent sync_buf_[MAX_SYNC_EVENTS]{};
    uint32_t  snapshot_len_  = 0;
    uint32_t  sync_count_    = 0;
    int       back_porch_    = 0;
    int       display_width_ = 0;
};
