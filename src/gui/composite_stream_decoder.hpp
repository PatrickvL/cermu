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
// Owns: shader program, stream texture, FBO + quad, uniform locations.
// Does NOT own: palette texture (shared, passed at construct time).
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

        // Create FBO + fullscreen quad for render_to_texture
        create_fbo_resources();

        return true;
    }

    void destroy() override {
        destroy_fbo_resources();
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

    GLuint render_to_texture(int width, int height) override {
        if (!fbo_ || !fbo_tex_) return 0;
        resize_fbo(width, height);

        // Save GL state
        GLint prev_fbo = 0, prev_viewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);

        // Render into FBO
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Bind shader + textures with ortho projection
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
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        gl_api::glActiveTexture(GL_TEXTURE0);

        // Draw fullscreen quad
        gl_api::glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gl_api::glBindVertexArray(0);

        gl_api::glUseProgram(0);

        // Restore GL state
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1],
                   prev_viewport[2], prev_viewport[3]);

        return fbo_tex_;
    }

    void bind_for_imgui(ImDrawList* draw_list) override {
        CompositeCallbackData cb = {
            shader_, loc_proj_, palette_texture_, texture_
        };
        draw_list->AddCallback(imgui_bind_callback, &cb, sizeof(cb));
    }

    int display_height() const override { return display_height_; }
    GLuint program() const override { return shader_; }
    GLuint output_texture() const override { return fbo_tex_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }

    // Artifact shader: set the NTSC phase increment (radians per pixel)
    void set_artifact_phase(float phase) { artifact_phase_ = phase; }
    float artifact_phase() const { return artifact_phase_; }
    GLint artifact_phase_loc() const { return artifact_loc_phase_; }

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

    // FBO resources (owned)
    GLuint fbo_       = 0;
    GLuint fbo_tex_   = 0;
    GLuint quad_vao_  = 0;
    GLuint quad_vbo_  = 0;
    int    fbo_w_     = 0;
    int    fbo_h_     = 0;

    void create_fbo_resources() {
        // FBO texture
        glGenTextures(1, &fbo_tex_);
        glBindTexture(GL_TEXTURE_2D, fbo_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // FBO
        gl_api::glGenFramebuffers(1, &fbo_);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        gl_api::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, fbo_tex_, 0);
        gl_api::glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Fullscreen quad (ImGui vertex layout: pos.xy, uv.xy, col)
        struct QuadVertex { float x, y, u, v; uint32_t col; };
        QuadVertex quad[6] = {
            {0, 0, 0, 0, 0xFFFFFFFF}, {1, 0, 1, 0, 0xFFFFFFFF},
            {1, 1, 1, 1, 0xFFFFFFFF}, {0, 0, 0, 0, 0xFFFFFFFF},
            {1, 1, 1, 1, 0xFFFFFFFF}, {0, 1, 0, 1, 0xFFFFFFFF},
        };
        gl_api::glGenVertexArrays(1, &quad_vao_);
        gl_api::glGenBuffers(1, &quad_vbo_);
        gl_api::glBindVertexArray(quad_vao_);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl_api::glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        gl_api::glEnableVertexAttribArray(0);
        gl_api::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(QuadVertex), (void*)0);
        gl_api::glEnableVertexAttribArray(1);
        gl_api::glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(QuadVertex), (void*)(2 * sizeof(float)));
        gl_api::glEnableVertexAttribArray(2);
        gl_api::glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                      sizeof(QuadVertex), (void*)(4 * sizeof(float)));
        gl_api::glBindVertexArray(0);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);

        fbo_w_ = 0;
        fbo_h_ = 0;
    }

    void destroy_fbo_resources() {
        if (fbo_)      { gl_api::glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
        if (fbo_tex_)  { glDeleteTextures(1, &fbo_tex_); fbo_tex_ = 0; }
        if (quad_vao_) { gl_api::glDeleteVertexArrays(1, &quad_vao_); quad_vao_ = 0; }
        if (quad_vbo_) { gl_api::glDeleteBuffers(1, &quad_vbo_); quad_vbo_ = 0; }
        fbo_w_ = fbo_h_ = 0;
    }

    void resize_fbo(int w, int h) {
        if (w == fbo_w_ && h == fbo_h_) return;
        fbo_w_ = w;
        fbo_h_ = h;

        // Resize FBO texture
        glBindTexture(GL_TEXTURE_2D, fbo_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Update quad vertices
        struct QuadVertex { float x, y, u, v; uint32_t col; };
        QuadVertex quad[6] = {
            {0, 0,         0, 0, 0xFFFFFFFF}, {(float)w, 0,         1, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, 0,         0, 0, 0xFFFFFFFF},
            {(float)w, (float)h, 1, 1, 0xFFFFFFFF}, {0, (float)h,  0, 1, 0xFFFFFFFF},
        };
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl_api::glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
        gl_api::glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

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
