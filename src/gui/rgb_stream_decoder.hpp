#pragma once

// ============================================================================
// RGBStreamDecoder — GPU reconstruction for RGB-family signals
// ============================================================================
//
// Handles RGB, Digital, and YPbPr signal types.
// Stream format: RGBA8 (4 bytes per sample: r, g, b, flags).
// No palette lookup — the shader reads raw RGB values directly.
//
// Owns: shader program, stream texture, uniform locations.
// No palette dependency.
// ============================================================================

#include "gui/signal_decoder.hpp"
#include "gui/rgb_stream_shader.hpp"
#include "gui/ypbpr_stream_shader.hpp"
#include "gui/stream_shader.hpp"    // for MAX_SCANLINES, STREAM_TEX_WIDTH, compute_scanline_map

#include <imgui.h>

enum class RGBShaderVariant {
    Standard,   // RGB / Digital — direct pass-through
    YPbPr,      // Y'PbPr component — chroma bandwidth limiting
};

// ImGui draw callback data — must be POD for ImDrawList::AddCallback.
struct RGBCallbackData {
    GLuint shader;
    GLint  loc_proj;
    GLuint stream_tex;
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
        return true;
    }

    void destroy() override {
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

    void bind_for_fbo(int width, int height) override {
        gl_api::glUseProgram(shader_);

        float L = 0, R = static_cast<float>(width);
        float T = static_cast<float>(height), B = 0;
        const float ortho[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };
        gl_api::glUniformMatrix4fv(loc_proj_, 1, GL_FALSE, &ortho[0][0]);

        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
    }

    void bind_for_imgui(ImDrawList* draw_list) override {
        RGBCallbackData cb = { shader_, loc_proj_, texture_ };
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    int display_height() const override { return display_height_; }
    GLuint program() const override { return shader_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }

    GLuint stream_texture() const { return texture_; }

private:
    RGBShaderVariant variant_;

    GLuint shader_  = 0;
    GLuint texture_ = 0;

    GLint loc_proj_         = -1;
    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int   display_height_   = 0;

    static void imgui_bind_callback(const ImDrawList*, const ImDrawCmd* cmd) {
        auto* d = static_cast<const RGBCallbackData*>(cmd->UserCallbackData);
        gl_api::glUseProgram(d->shader);

        ImDrawData* draw_data = ImGui::GetDrawData();
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        if (R == L || B == T) return;
        const float ortho[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };
        gl_api::glUniformMatrix4fv(d->loc_proj, 1, GL_FALSE, &ortho[0][0]);

        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, d->stream_tex);
    }
};
