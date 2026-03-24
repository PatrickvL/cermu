#pragma once

// ============================================================================
// RGBSignalDecoder — GPU reconstruction for RGB-family signals
// ============================================================================
//
// Handles RGB, Digital, and YPbPr signal types.
// Stream format: RGBA8 (4 bytes per sample: r, g, b, flags).
// No palette lookup — the shader reads raw RGB values directly.
//
// Owns: signal texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// No palette dependency.
// ============================================================================

#include "gui/decoder/scanline_signal_decoder.hpp"
#include "gui/shader/rgb_signal_shader.hpp"
#include "gui/shader/ypbpr_signal_shader.hpp"

enum class RGBShaderVariant {
    Standard,   // RGB / Digital — direct pass-through
    YPbPr,      // Y'PbPr component — chroma bandwidth limiting
};

class RGBSignalDecoder : public ScanlineSignalDecoder<4> {
public:
    explicit RGBSignalDecoder(RGBShaderVariant variant = RGBShaderVariant::Standard)
        : variant_(variant) {}

    bool create() override {
        texture_ = rgb_signal_shader::create_signal_texture(MAX_SIGNAL_SAMPLES);
        if (!texture_) return false;

        rgb_signal_shader::RGBShaderLocations locs{};
        if (variant_ == RGBShaderVariant::YPbPr)
            shader_ = ypbpr_signal_shader::create_program(&locs);
        else
            shader_ = rgb_signal_shader::create_program(&locs);

        if (!shader_) {
            glDeleteTextures(1, &texture_);
            texture_ = 0;
            return false;
        }

        loc_proj_         = locs.proj_mtx;
        loc_scanline_map_ = locs.scanline_map;
        loc_tex_width_    = locs.signal_tex_width;
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

protected:
    void upload_to_gpu(const uint8_t* data, uint32_t sample_count) override {
        rgb_signal_shader::upload_signal_texture(texture_, data, sample_count);
    }
    void bind_textures() override {
        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
    }

    void fill_callback_textures(DecoderCallbackData& cb) const override {
        cb.tex0 = texture_;
    }

private:
    RGBShaderVariant variant_;
};
