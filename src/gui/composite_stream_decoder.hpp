#pragma once

// ============================================================================
// CompositeStreamDecoder — GPU reconstruction for composite-family signals
// ============================================================================
//
// Handles Composite, RGBI, S-Video, and CompositeArtifact signal types.
// All share the same stream texture format (RG8: color index + flags) and
// the same scanline-map-based reconstruction approach.  Only the fragment
// shader differs:
//   Standard     — direct palette lookup (stream_shader.hpp)
//   S-Video      — palette + chroma bandwidth limiting (svideo_stream_shader.hpp)
//   Artifact     — NTSC encode/decode artifact coloring (artifact_stream_shader.hpp)
//
// Owns: shader program, stream texture, uniform locations.
// Does NOT own: palette texture (shared, passed to bind methods).
// ============================================================================

#include "gui/signal_decoder.hpp"
#include "gui/stream_shader.hpp"
#include "gui/svideo_stream_shader.hpp"
#include "gui/artifact_stream_shader.hpp"

#include <imgui.h>

enum class CompositeShaderVariant {
    Standard,   // Composite / RGBI — direct palette lookup
    SVideo,     // S-Video — chroma bandwidth limiting
    Artifact,   // NTSC artifact coloring
};

// ImGui draw callback data — must be POD for ImDrawList::AddCallback.
struct CompositeCallbackData {
    GLuint shader;
    GLint  loc_proj;
    GLuint palette_tex;
    GLuint stream_tex;
};

class CompositeStreamDecoder : public SignalDecoder {
public:
    CompositeStreamDecoder(CompositeShaderVariant variant, GLuint palette_texture)
        : variant_(variant), palette_texture_(palette_texture) {}

    bool create() override {
        texture_ = stream_shader::create_stream_texture(MAX_STREAM_SAMPLES);
        if (!texture_) return false;

        stream_shader::StreamShaderLocations locs{};
        switch (variant_) {
            case CompositeShaderVariant::SVideo:
                shader_ = svideo_stream_shader::create_program(&locs);
                break;
            case CompositeShaderVariant::Artifact:
                shader_ = artifact_stream_shader::create_program(
                    &locs, &artifact_loc_phase_);
                break;
            default:
                shader_ = stream_shader::create_program(&locs);
                break;
        }

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
        stream_shader::upload_stream_texture(texture_, data, sample_count);
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

        if (variant_ == CompositeShaderVariant::Artifact
            && artifact_loc_phase_ >= 0) {
            gl_api::glUniform1f(artifact_loc_phase_, artifact_phase_);
        }

        gl_api::glUseProgram(0);
    }

    void bind_for_fbo(int width, int height) override {
        gl_api::glUseProgram(shader_);

        // Ortho projection matching FBO dimensions (standard GL orientation)
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
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        gl_api::glActiveTexture(GL_TEXTURE0);
    }

    void bind_for_imgui(ImDrawList* draw_list) override {
        CompositeCallbackData cb = {
            shader_, loc_proj_, palette_texture_, texture_
        };
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    int display_height() const override { return display_height_; }
    GLuint program() const override { return shader_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }

    // Artifact shader: set the NTSC phase increment (radians per pixel)
    void set_artifact_phase(float phase) { artifact_phase_ = phase; }
    float artifact_phase() const { return artifact_phase_; }

    // Update the palette texture reference (e.g. after recreation)
    void set_palette_texture(GLuint tex) { palette_texture_ = tex; }

    // Access the stream texture (for callers that need it directly)
    GLuint stream_texture() const { return texture_; }

private:
    CompositeShaderVariant variant_;

    GLuint shader_  = 0;
    GLuint texture_ = 0;
    GLuint palette_texture_ = 0;    // Non-owning

    GLint loc_proj_         = -1;
    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int   display_height_   = 0;

    // Artifact-specific
    GLint artifact_loc_phase_ = -1;
    float artifact_phase_     = 3.14159265f;

    // ImGui draw callback — binds composite stream shader + textures.
    static void imgui_bind_callback(const ImDrawList*, const ImDrawCmd* cmd) {
        auto* d = static_cast<const CompositeCallbackData*>(cmd->UserCallbackData);
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
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d->palette_tex);
        gl_api::glActiveTexture(GL_TEXTURE0);
    }
};
