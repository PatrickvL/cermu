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
// Owns: stream texture, optional palette texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// ============================================================================

#include "gui/decoder/signal_decoder.hpp"
#include "gui/shader/stream_shader.hpp"
#include "gui/shader/svideo_stream_shader.hpp"
#include "gui/shader/artifact_stream_shader.hpp"

enum class CompositeShaderVariant {
    Standard,   // Composite / RGBI — direct palette lookup
    SVideo,     // S-Video — chroma bandwidth limiting
    Artifact,   // NTSC artifact coloring
};

class CompositeStreamDecoder : public SignalDecoder {
public:
    /// If palette_texture is 0, the decoder creates and owns its own.
    /// If non-zero, the decoder uses the external texture (non-owning).
    CompositeStreamDecoder(CompositeShaderVariant variant, GLuint palette_texture = 0)
        : variant_(variant), owns_palette_(palette_texture == 0) {
        palette_texture_ = palette_texture;
    }

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

        if (owns_palette_)
            create_palette_texture();

        create_fbo();
        return true;
    }

    void destroy() override {
        destroy_fbo();
        if (shader_)  { gl_api::glDeleteProgram(shader_); shader_ = 0; }
        if (texture_) { glDeleteTextures(1, &texture_); texture_ = 0; }
        if (owns_palette_)
            destroy_palette_texture();
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

    int display_height() const override { return display_height_; }
    bool ready() const override { return shader_ != 0 && texture_ != 0; }

    // Artifact shader: set the NTSC phase increment (radians per pixel)
    void set_artifact_phase(float phase) { artifact_phase_ = phase; }
    float artifact_phase() const { return artifact_phase_; }
    GLint artifact_phase_loc() const { return artifact_loc_phase_; }

    // Update the palette texture reference (e.g. after recreation)
    void set_palette_texture(GLuint tex) { palette_texture_ = tex; }

    // Access the stream texture (for callers that need it directly)
    GLuint stream_texture() const { return texture_; }

protected:
    void bind_textures() override {
        gl_api::glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
        gl_api::glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, palette_texture_);
        gl_api::glActiveTexture(GL_TEXTURE0);
    }

    void fill_callback_textures(DecoderCallbackData& cb) const override {
        cb.tex0 = texture_;
        cb.tex1 = palette_texture_;
    }

private:
    CompositeShaderVariant variant_;
    bool   owns_palette_    = false;

    GLuint texture_ = 0;

    GLint loc_scanline_map_ = -1;
    GLint loc_tex_width_    = -1;
    GLint loc_display_h_    = -1;
    GLint loc_display_w_    = -1;

    int   display_height_   = 0;

    // Artifact-specific
    GLint artifact_loc_phase_ = -1;
    float artifact_phase_     = 3.14159265f;
};
