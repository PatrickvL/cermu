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
};
