#pragma once

// ============================================================================
// CompositeSignalDecoder — GPU reconstruction for composite-family signals
// ============================================================================
//
// Handles Composite, RGBI, and S-Video signal types.
// All share the same signal texture format (RG8: color index + flags) and
// the same scanline-map-based reconstruction approach.  Only the fragment
// shader differs:
//   Standard     — direct palette lookup (signal_shader.hpp)
//   S-Video      — palette + chroma bandwidth limiting (svideo_signal_shader.hpp)
//   Artifact     — NTSC artifact coloring (artifact_signal_shader.hpp)
//                  Not a distinct signal encoding — artifact coloring is a
//                  decoder/renderer interpretation of a Composite signal,
//                  controlled via display pipeline configuration.
//
// Owns: signal texture, optional palette texture.
// Inherits: shader, FBO + quad, render_to_texture, ImGui callback from base.
// ============================================================================

#include "gui/decoder/scanline_signal_decoder.hpp"
#include "gui/shader/signal_shader.hpp"
#include "gui/shader/svideo_signal_shader.hpp"
#include "gui/shader/artifact_signal_shader.hpp"

enum class CompositeShaderVariant {
    Standard,   // Composite / RGBI — direct palette lookup
    SVideo,     // S-Video — chroma bandwidth limiting
    Artifact,   // NTSC artifact coloring
};

class CompositeSignalDecoder : public ScanlineSignalDecoder<2> {
public:
    /// If palette_texture is 0, the decoder creates and owns its own.
    /// If non-zero, the decoder uses the external texture (non-owning).
    CompositeSignalDecoder(CompositeShaderVariant variant, GLuint palette_texture = 0)
        : variant_(variant), owns_palette_(palette_texture == 0) {
        palette_texture_ = palette_texture;
    }

    bool create() override {
        texture_ = signal_shader::create_signal_texture(MAX_SIGNAL_SAMPLES);
        if (!texture_) return false;

        signal_shader::SignalShaderLocations locs{};
        switch (variant_) {
            case CompositeShaderVariant::SVideo:
                shader_ = svideo_signal_shader::create_program(&locs);
                break;
            case CompositeShaderVariant::Artifact:
                shader_ = artifact_signal_shader::create_program(
                    &locs, &artifact_loc_phase_);
                break;
            default:
                shader_ = signal_shader::create_program(&locs);
                break;
        }

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

    // Artifact shader: set the NTSC phase increment (radians per pixel)
    void set_artifact_phase(float phase) { artifact_phase_ = phase; }
    float artifact_phase() const { return artifact_phase_; }
    GLint artifact_phase_loc() const { return artifact_loc_phase_; }

    // Update the palette texture reference (e.g. after recreation)
    void set_palette_texture(GLuint tex) { palette_texture_ = tex; }

protected:
    void upload_to_gpu(const uint8_t* data, uint32_t sample_count) override {
        signal_shader::upload_signal_texture(texture_, data, sample_count);
    }
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

    void update_extra_uniforms() override {
        if (variant_ == CompositeShaderVariant::Artifact
            && artifact_loc_phase_ >= 0) {
            gl_api::glUniform1f(artifact_loc_phase_, artifact_phase_);
        }
    }

private:
    CompositeShaderVariant variant_;
    bool   owns_palette_    = false;

    // Artifact-specific
    GLint artifact_loc_phase_ = -1;
    float artifact_phase_     = 3.14159265f;
};
